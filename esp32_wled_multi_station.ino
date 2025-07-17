/*
 * ESP32 WLED Multi-Station Touch Controller
 * 
 * This system coordinates 3 ESP32 stations that communicate with each other
 * - All 3 stations must have their touch sensors activated simultaneously
 * - Only when all 3 people are touching their sensors will the puzzle be solved
 * - WLED preset changes when puzzle is solved
 * - Stations communicate via ESP-NOW for low latency
 * - Fallback to WiFi/UDP communication if needed
 * 
 * Hardware Requirements per station:
 * - ESP32 DevKit
 * - Touch sensor (capacitive or resistive)
 * - Status LED (optional)
 * - WLED controller on network (can be shared or individual)
 * 
 * Station Configuration:
 * - Station 1: Master (controls WLED presets)
 * - Station 2: Slave
 * - Station 3: Slave
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <WiFiUdp.h>

// Station Configuration - CHANGE THIS FOR EACH STATION
#define STATION_ID 1                    // Set to 1, 2, or 3 for each station
#define IS_MASTER (STATION_ID == 1)     // Station 1 is the master

// WiFi Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// WLED Configuration (only used by master station)
const char* wledIP = "192.168.1.100";
const int wledPort = 80;

// Pin Configuration
#define TOUCH_PIN 4                     // Touch sensor pin
#define STATUS_LED_PIN 2                // Built-in LED for status
#define EXTERNAL_LED_PIN 5              // External LED for better visibility

// Touch Sensor Configuration
#define TOUCH_THRESHOLD 30              // Touch threshold for capacitive sensors
#define TOUCH_DEBOUNCE_TIME 100         // Debounce time in milliseconds
#define TOUCH_HOLD_TIME 50              // Minimum hold time for valid touch
#define PUZZLE_SOLVE_TIME 2000          // Time all sensors must be held (2 seconds)

// Communication Configuration
#define ESP_NOW_CHANNEL 1               // ESP-NOW channel
#define UDP_PORT 12345                  // UDP port for fallback communication
#define HEARTBEAT_INTERVAL 1000         // Send status every 1 second
#define TIMEOUT_THRESHOLD 3000          // Consider station offline after 3 seconds

// WLED Preset Configuration
const int PRESET_COUNT = 10;
int currentPreset = 1;
const int INITIAL_PRESET = 0;

// ESP-NOW MAC addresses of other stations (you'll need to find these)
uint8_t station1_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Replace with actual MAC
uint8_t station2_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Replace with actual MAC
uint8_t station3_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Replace with actual MAC

// Communication structures
typedef struct {
  uint8_t stationId;
  bool touchActive;
  unsigned long timestamp;
  float batteryLevel; // Optional: for battery monitoring
} StationStatus;

typedef struct {
  uint8_t command;           // 0=status, 1=puzzle_solved, 2=reset, 3=preset_change
  uint8_t presetNumber;
  unsigned long timestamp;
} CommandMessage;

// System state
StationStatus localStatus;
StationStatus stationStates[4]; // Index 0 unused, 1-3 for stations
bool wifiConnected = false;
bool espNowInitialized = false;
unsigned long lastHeartbeat = 0;
unsigned long puzzleStartTime = 0;
bool puzzleActive = false;
bool puzzleSolved = false;

// Touch sensor state
bool currentTouchState = false;
bool lastTouchState = false;
unsigned long lastTouchTime = 0;
unsigned long touchStartTime = 0;

// LED control
unsigned long lastLEDUpdate = 0;
bool ledState = false;

// UDP for fallback communication
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Multi-Station Touch Controller Starting...");
  Serial.print("Station ID: ");
  Serial.println(STATION_ID);
  Serial.print("Role: ");
  Serial.println(IS_MASTER ? "MASTER" : "SLAVE");
  
  // Initialize pins
  pinMode(TOUCH_PIN, INPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(EXTERNAL_LED_PIN, OUTPUT);
  
  // Initialize local status
  localStatus.stationId = STATION_ID;
  localStatus.touchActive = false;
  localStatus.timestamp = millis();
  localStatus.batteryLevel = 100.0; // Placeholder
  
  // Initialize all station states
  for (int i = 1; i <= 3; i++) {
    stationStates[i].stationId = i;
    stationStates[i].touchActive = false;
    stationStates[i].timestamp = 0;
    stationStates[i].batteryLevel = 100.0;
  }
  
  // Initialize WiFi
  initializeWiFi();
  
  // Initialize ESP-NOW
  initializeESPNow();
  
  // Initialize UDP for fallback
  if (wifiConnected) {
    udp.begin(UDP_PORT);
    Serial.print("UDP listening on port: ");
    Serial.println(UDP_PORT);
  }
  
  // Master-specific initialization
  if (IS_MASTER && wifiConnected) {
    initializeWLED();
  }
  
  // Print MAC address for configuration
  Serial.print("This station's MAC address: ");
  Serial.println(WiFi.macAddress());
  
  Serial.println("System Ready!");
  printInstructions();
}

void loop() {
  unsigned long currentTime = millis();
  
  // Read touch sensor
  readTouchSensor();
  
  // Send heartbeat
  if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = currentTime;
  }
  
  // Check for timeouts on other stations
  checkStationTimeouts();
  
  // Handle puzzle logic
  handlePuzzleLogic();
  
  // Update LEDs
  updateLEDs();
  
  // Handle serial commands
  handleSerialCommands();
  
  delay(10);
}

void initializeWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_AP_STA); // Both station and AP mode for ESP-NOW
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    Serial.println("");
    Serial.println("WiFi connection failed, continuing with ESP-NOW only");
  }
}

void initializeESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  espNowInitialized = true;
  Serial.println("ESP-NOW initialized successfully");
  
  // Register callback functions
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);
  
  // Add peers (other stations)
  addESPNowPeers();
}

void addESPNowPeers() {
  esp_now_peer_info_t peerInfo;
  peerInfo.channel = ESP_NOW_CHANNEL;
  peerInfo.encrypt = false;
  
  // Add other stations as peers
  for (int i = 1; i <= 3; i++) {
    if (i != STATION_ID) { // Don't add ourselves
      uint8_t* mac;
      switch (i) {
        case 1: mac = station1_mac; break;
        case 2: mac = station2_mac; break;
        case 3: mac = station3_mac; break;
      }
      
      memcpy(peerInfo.peer_addr, mac, 6);
      
      if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        Serial.print("Added peer station ");
        Serial.println(i);
      } else {
        Serial.print("Failed to add peer station ");
        Serial.println(i);
      }
    }
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: Handle send confirmation
}

void onDataReceived(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(StationStatus)) {
    StationStatus receivedStatus;
    memcpy(&receivedStatus, incomingData, sizeof(receivedStatus));
    
    // Update station state
    if (receivedStatus.stationId >= 1 && receivedStatus.stationId <= 3) {
      stationStates[receivedStatus.stationId] = receivedStatus;
      stationStates[receivedStatus.stationId].timestamp = millis();
      
      Serial.print("Received status from station ");
      Serial.print(receivedStatus.stationId);
      Serial.print(" - Touch: ");
      Serial.println(receivedStatus.touchActive ? "ACTIVE" : "INACTIVE");
    }
  } else if (len == sizeof(CommandMessage)) {
    CommandMessage receivedCommand;
    memcpy(&receivedCommand, incomingData, sizeof(receivedCommand));
    handleReceivedCommand(receivedCommand);
  }
}

void readTouchSensor() {
  // Read touch sensor - adjust based on your sensor type
  int touchValue = digitalRead(TOUCH_PIN);
  
  // For capacitive touch sensors, use:
  // int touchValue = touchRead(TOUCH_PIN);
  // currentTouchState = touchValue < TOUCH_THRESHOLD;
  
  currentTouchState = (touchValue == HIGH); // Adjust based on sensor type
  
  unsigned long currentTime = millis();
  
  // Debouncing
  if (currentTouchState != lastTouchState) {
    if (currentTime - lastTouchTime >= TOUCH_DEBOUNCE_TIME) {
      lastTouchState = currentTouchState;
      lastTouchTime = currentTime;
      
      if (currentTouchState) {
        touchStartTime = currentTime;
        Serial.print("Station ");
        Serial.print(STATION_ID);
        Serial.println(" touch ACTIVATED");
      } else {
        Serial.print("Station ");
        Serial.print(STATION_ID);
        Serial.println(" touch DEACTIVATED");
      }
    }
  }
  
  // Update local status
  localStatus.touchActive = currentTouchState;
  localStatus.timestamp = currentTime;
}

void sendHeartbeat() {
  if (espNowInitialized) {
    // Send via ESP-NOW to all peers
    esp_now_send(NULL, (uint8_t*)&localStatus, sizeof(localStatus));
  }
  
  // Also send via UDP as fallback
  if (wifiConnected) {
    sendUDPHeartbeat();
  }
  
  // Update our own state
  stationStates[STATION_ID] = localStatus;
}

void sendUDPHeartbeat() {
  // Broadcast UDP message
  udp.beginPacket("255.255.255.255", UDP_PORT);
  udp.write((uint8_t*)&localStatus, sizeof(localStatus));
  udp.endPacket();
}

void checkStationTimeouts() {
  unsigned long currentTime = millis();
  
  for (int i = 1; i <= 3; i++) {
    if (i != STATION_ID) {
      if (currentTime - stationStates[i].timestamp > TIMEOUT_THRESHOLD) {
        if (stationStates[i].touchActive) {
          stationStates[i].touchActive = false;
          Serial.print("Station ");
          Serial.print(i);
          Serial.println(" timed out - marking as inactive");
        }
      }
    }
  }
}

void handlePuzzleLogic() {
  bool allStationsActive = true;
  
  // Check if all stations have active touches
  for (int i = 1; i <= 3; i++) {
    if (!stationStates[i].touchActive) {
      allStationsActive = false;
      break;
    }
  }
  
  unsigned long currentTime = millis();
  
  if (allStationsActive && !puzzleActive) {
    // All stations just became active
    puzzleActive = true;
    puzzleStartTime = currentTime;
    puzzleSolved = false;
    
    Serial.println("=== PUZZLE STARTED ===");
    Serial.println("All stations active! Hold for " + String(PUZZLE_SOLVE_TIME / 1000) + " seconds...");
    
    // Send puzzle start command to all stations
    sendPuzzleCommand(0); // 0 = puzzle started
    
  } else if (!allStationsActive && puzzleActive) {
    // Someone released their touch
    puzzleActive = false;
    puzzleSolved = false;
    
    Serial.println("=== PUZZLE FAILED ===");
    Serial.println("Someone released their touch!");
    
    // Send puzzle failed command
    sendPuzzleCommand(2); // 2 = puzzle reset/failed
    
  } else if (allStationsActive && puzzleActive && !puzzleSolved) {
    // Check if puzzle solve time has been reached
    if (currentTime - puzzleStartTime >= PUZZLE_SOLVE_TIME) {
      puzzleSolved = true;
      puzzleActive = false;
      
      Serial.println("=== PUZZLE SOLVED! ===");
      
      // Only master controls WLED
      if (IS_MASTER) {
        advanceWLEDPreset();
      }
      
      // Send puzzle solved command to all stations
      sendPuzzleCommand(1); // 1 = puzzle solved
      
      // Flash celebration
      celebrationFlash();
    }
  }
}

void sendPuzzleCommand(uint8_t command) {
  CommandMessage cmd;
  cmd.command = command;
  cmd.presetNumber = currentPreset;
  cmd.timestamp = millis();
  
  if (espNowInitialized) {
    esp_now_send(NULL, (uint8_t*)&cmd, sizeof(cmd));
  }
}

void handleReceivedCommand(CommandMessage cmd) {
  switch (cmd.command) {
    case 0: // Puzzle started
      Serial.println("Received: Puzzle started");
      break;
    case 1: // Puzzle solved
      Serial.println("Received: Puzzle solved!");
      celebrationFlash();
      break;
    case 2: // Puzzle failed/reset
      Serial.println("Received: Puzzle failed");
      break;
    case 3: // Preset changed
      Serial.print("Received: Preset changed to ");
      Serial.println(cmd.presetNumber);
      break;
  }
}

void initializeWLED() {
  if (!wifiConnected) return;
  
  Serial.println("Initializing WLED connection...");
  
  HTTPClient http;
  String url = "http://" + String(wledIP) + "/json/info";
  
  http.begin(url);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.println("WLED connection established!");
    setWLEDPreset(INITIAL_PRESET);
  } else {
    Serial.println("Failed to connect to WLED");
  }
  
  http.end();
}

void setWLEDPreset(int presetNumber) {
  if (!wifiConnected) return;
  
  HTTPClient http;
  String url = "http://" + String(wledIP) + "/win&PL=" + String(presetNumber);
  
  Serial.print("Setting WLED preset ");
  Serial.println(presetNumber);
  
  http.begin(url);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.println("WLED preset set successfully");
  } else {
    Serial.print("Error setting WLED preset: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

void advanceWLEDPreset() {
  currentPreset++;
  if (currentPreset > PRESET_COUNT) {
    currentPreset = 1;
  }
  
  setWLEDPreset(currentPreset);
  
  // Notify other stations of preset change
  sendPuzzleCommand(3); // 3 = preset change
}

void updateLEDs() {
  unsigned long currentTime = millis();
  
  // Status LED patterns
  if (puzzleActive) {
    // Fast blink during puzzle
    if (currentTime - lastLEDUpdate >= 100) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
      digitalWrite(EXTERNAL_LED_PIN, ledState);
      lastLEDUpdate = currentTime;
    }
  } else if (currentTouchState) {
    // Solid on when touched
    digitalWrite(STATUS_LED_PIN, HIGH);
    digitalWrite(EXTERNAL_LED_PIN, HIGH);
  } else {
    // Slow blink when idle
    if (currentTime - lastLEDUpdate >= 1000) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
      digitalWrite(EXTERNAL_LED_PIN, LOW); // External LED off when idle
      lastLEDUpdate = currentTime;
    }
  }
}

void celebrationFlash() {
  // Flash LEDs rapidly for celebration
  for (int i = 0; i < 6; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    digitalWrite(EXTERNAL_LED_PIN, HIGH);
    delay(100);
    digitalWrite(STATUS_LED_PIN, LOW);
    digitalWrite(EXTERNAL_LED_PIN, LOW);
    delay(100);
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "status") {
      printSystemStatus();
    } else if (command == "test") {
      testCommunication();
    } else if (command.startsWith("preset ")) {
      if (IS_MASTER) {
        int preset = command.substring(7).toInt();
        if (preset >= 0 && preset <= PRESET_COUNT) {
          currentPreset = preset;
          setWLEDPreset(preset);
        }
      } else {
        Serial.println("Only master station can set presets");
      }
    } else if (command == "help") {
      printHelp();
    } else if (command == "mac") {
      Serial.println("MAC Address: " + WiFi.macAddress());
    }
  }
}

void testCommunication() {
  Serial.println("Testing communication with other stations...");
  
  // Send test message
  CommandMessage testCmd;
  testCmd.command = 255; // Test command
  testCmd.presetNumber = STATION_ID;
  testCmd.timestamp = millis();
  
  if (espNowInitialized) {
    esp_now_send(NULL, (uint8_t*)&testCmd, sizeof(testCmd));
    Serial.println("Test message sent via ESP-NOW");
  }
}

void printSystemStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("Station ID: ");
  Serial.println(STATION_ID);
  Serial.print("Role: ");
  Serial.println(IS_MASTER ? "MASTER" : "SLAVE");
  Serial.print("WiFi: ");
  Serial.println(wifiConnected ? "CONNECTED" : "DISCONNECTED");
  Serial.print("ESP-NOW: ");
  Serial.println(espNowInitialized ? "INITIALIZED" : "FAILED");
  
  Serial.println("\n--- Station States ---");
  for (int i = 1; i <= 3; i++) {
    Serial.print("Station ");
    Serial.print(i);
    Serial.print(": Touch=");
    Serial.print(stationStates[i].touchActive ? "ACTIVE" : "INACTIVE");
    Serial.print(", Last seen=");
    Serial.print((millis() - stationStates[i].timestamp) / 1000);
    Serial.println("s ago");
  }
  
  Serial.print("\nPuzzle Active: ");
  Serial.println(puzzleActive ? "YES" : "NO");
  Serial.print("Puzzle Solved: ");
  Serial.println(puzzleSolved ? "YES" : "NO");
  
  if (IS_MASTER) {
    Serial.print("Current WLED Preset: ");
    Serial.println(currentPreset);
  }
  
  Serial.println("=====================\n");
}

void printHelp() {
  Serial.println("\n=== COMMANDS ===");
  Serial.println("status   - Show system status");
  Serial.println("test     - Test communication");
  Serial.println("mac      - Show MAC address");
  if (IS_MASTER) {
    Serial.println("preset X - Set WLED preset (master only)");
  }
  Serial.println("help     - Show this help");
  Serial.println("================\n");
}

void printInstructions() {
  Serial.println("\n=== INSTRUCTIONS ===");
  Serial.println("1. All 3 people must place their hands on their touch sensors");
  Serial.println("2. Hold for " + String(PUZZLE_SOLVE_TIME / 1000) + " seconds simultaneously");
  Serial.println("3. WLED preset will advance when puzzle is solved");
  Serial.println("4. If anyone releases early, puzzle resets");
  Serial.println("===================\n");
}