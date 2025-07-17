/*
 * ESP32 WLED Multi-Station Touch Controller v2.0
 * 
 * Two-puzzle cooperative system:
 * PUZZLE 1: All 3 people touch sensors simultaneously for 2 seconds
 * PUZZLE 2: All 3 people complete magnetic ball mazes using wands
 * 
 * Hardware per station:
 * - ESP32 DevKit
 * - Touch sensor (GPIO4)
 * - Servo motor for latch (GPIO16)
 * - Hall sensor for maze completion (GPIO34)
 * - Status LEDs (GPIO2, GPIO5)
 * - Magnetic wand (released by servo latch)
 * - Metal ball maze with magnetic completion zone
 * 
 * Puzzle Flow:
 * 1. Initial state: WLED preset 0 (glow)
 * 2. Solve puzzle 1 → WLED preset 1 + open latches
 * 3. Players get wands and complete mazes
 * 4. Solve puzzle 2 → WLED preset 2 + celebration
 * 5. System can cycle through more puzzles/presets
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

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
#define SERVO_PIN 16                    // Servo motor pin for latch
#define HALL_SENSOR_PIN 34              // Hall sensor pin (analog)
#define STATUS_LED_PIN 2                // Built-in LED for status
#define EXTERNAL_LED_PIN 5              // External LED for better visibility
#define BUZZER_PIN 19                   // Buzzer for audio feedback (optional)

// Touch Sensor Configuration
#define TOUCH_THRESHOLD 30              // Touch threshold for capacitive sensors
#define TOUCH_DEBOUNCE_TIME 100         // Debounce time in milliseconds
#define PUZZLE1_SOLVE_TIME 2000         // Time all sensors must be held (2 seconds)

// Hall Sensor Configuration
#define HALL_THRESHOLD 100              // Hall sensor threshold (adjust based on magnet strength)
#define HALL_DEBOUNCE_TIME 500          // Debounce time for hall sensor
#define MAZE_COMPLETION_TIME 1000       // Time ball must stay in completion zone

// Servo Configuration
#define SERVO_CLOSED_ANGLE 0            // Servo angle when latch is closed
#define SERVO_OPEN_ANGLE 90             // Servo angle when latch is open
#define SERVO_MOVE_DELAY 1000           // Time to wait for servo movement

// Communication Configuration
#define ESP_NOW_CHANNEL 1               // ESP-NOW channel
#define UDP_PORT 12345                  // UDP port for fallback communication
#define HEARTBEAT_INTERVAL 1000         // Send status every 1 second
#define TIMEOUT_THRESHOLD 3000          // Consider station offline after 3 seconds

// WLED Preset Configuration
const int PRESET_COUNT = 10;
int currentPreset = 0;
const int INITIAL_PRESET = 0;           // Initial glow
const int PUZZLE1_COMPLETE_PRESET = 1;  // After puzzle 1
const int PUZZLE2_COMPLETE_PRESET = 2;  // After puzzle 2

// ESP-NOW MAC addresses of other stations (you'll need to find these)
uint8_t station1_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Replace with actual MAC
uint8_t station2_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Replace with actual MAC
uint8_t station3_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Replace with actual MAC

// Communication structures
typedef struct {
  uint8_t stationId;
  bool touchActive;
  bool mazeComplete;
  bool latchOpen;
  unsigned long timestamp;
  float batteryLevel;
} StationStatus;

typedef struct {
  uint8_t command;           // 0=status, 1=puzzle1_solved, 2=reset, 3=preset_change, 4=open_latches, 5=puzzle2_solved
  uint8_t presetNumber;
  bool openLatch;
  unsigned long timestamp;
} CommandMessage;

// Puzzle states
enum PuzzleState {
  PUZZLE_IDLE,
  PUZZLE1_ACTIVE,
  PUZZLE1_COMPLETE,
  PUZZLE2_ACTIVE,
  PUZZLE2_COMPLETE,
  ALL_PUZZLES_COMPLETE
};

// System state
StationStatus localStatus;
StationStatus stationStates[4]; // Index 0 unused, 1-3 for stations
bool wifiConnected = false;
bool espNowInitialized = false;
unsigned long lastHeartbeat = 0;
PuzzleState currentPuzzleState = PUZZLE_IDLE;
unsigned long puzzleStartTime = 0;

// Touch sensor state
bool currentTouchState = false;
bool lastTouchState = false;
unsigned long lastTouchTime = 0;

// Hall sensor state
bool currentMazeComplete = false;
bool lastMazeComplete = false;
unsigned long lastHallTime = 0;
unsigned long mazeCompleteStartTime = 0;

// Servo control
Servo latchServo;
bool latchOpen = false;

// LED control
unsigned long lastLEDUpdate = 0;
bool ledState = false;

// UDP for fallback communication
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Multi-Station Touch Controller v2.0 Starting...");
  Serial.print("Station ID: ");
  Serial.println(STATION_ID);
  Serial.print("Role: ");
  Serial.println(IS_MASTER ? "MASTER" : "SLAVE");
  
  // Initialize pins
  pinMode(TOUCH_PIN, INPUT);
  pinMode(HALL_SENSOR_PIN, INPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(EXTERNAL_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize servo
  latchServo.attach(SERVO_PIN);
  closeLatch(); // Start with latch closed
  
  // Initialize local status
  localStatus.stationId = STATION_ID;
  localStatus.touchActive = false;
  localStatus.mazeComplete = false;
  localStatus.latchOpen = false;
  localStatus.timestamp = millis();
  localStatus.batteryLevel = 100.0;
  
  // Initialize all station states
  for (int i = 1; i <= 3; i++) {
    stationStates[i].stationId = i;
    stationStates[i].touchActive = false;
    stationStates[i].mazeComplete = false;
    stationStates[i].latchOpen = false;
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
  
  // Read sensors
  readTouchSensor();
  readHallSensor();
  
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
  
  WiFi.mode(WIFI_AP_STA);
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
  
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);
  
  addESPNowPeers();
}

void addESPNowPeers() {
  esp_now_peer_info_t peerInfo;
  peerInfo.channel = ESP_NOW_CHANNEL;
  peerInfo.encrypt = false;
  
  for (int i = 1; i <= 3; i++) {
    if (i != STATION_ID) {
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
    
    if (receivedStatus.stationId >= 1 && receivedStatus.stationId <= 3) {
      stationStates[receivedStatus.stationId] = receivedStatus;
      stationStates[receivedStatus.stationId].timestamp = millis();
      
      Serial.print("Received status from station ");
      Serial.print(receivedStatus.stationId);
      Serial.print(" - Touch: ");
      Serial.print(receivedStatus.touchActive ? "ACTIVE" : "INACTIVE");
      Serial.print(", Maze: ");
      Serial.println(receivedStatus.mazeComplete ? "COMPLETE" : "INCOMPLETE");
    }
  } else if (len == sizeof(CommandMessage)) {
    CommandMessage receivedCommand;
    memcpy(&receivedCommand, incomingData, sizeof(receivedCommand));
    handleReceivedCommand(receivedCommand);
  }
}

void readTouchSensor() {
  int touchValue = digitalRead(TOUCH_PIN);
  currentTouchState = (touchValue == HIGH);
  
  unsigned long currentTime = millis();
  
  if (currentTouchState != lastTouchState) {
    if (currentTime - lastTouchTime >= TOUCH_DEBOUNCE_TIME) {
      lastTouchState = currentTouchState;
      lastTouchTime = currentTime;
      
      if (currentTouchState) {
        Serial.print("Station ");
        Serial.print(STATION_ID);
        Serial.println(" touch ACTIVATED");
        playTone(1000, 100); // Touch feedback
      } else {
        Serial.print("Station ");
        Serial.print(STATION_ID);
        Serial.println(" touch DEACTIVATED");
      }
    }
  }
  
  localStatus.touchActive = currentTouchState;
}

void readHallSensor() {
  int hallValue = analogRead(HALL_SENSOR_PIN);
  bool hallDetected = hallValue < HALL_THRESHOLD; // Adjust based on your sensor
  
  unsigned long currentTime = millis();
  
  if (hallDetected && !currentMazeComplete) {
    if (currentTime - lastHallTime >= HALL_DEBOUNCE_TIME) {
      mazeCompleteStartTime = currentTime;
      currentMazeComplete = true;
      Serial.print("Station ");
      Serial.print(STATION_ID);
      Serial.println(" - Ball detected in completion zone!");
      playTone(800, 200); // Ball detected feedback
    }
  } else if (!hallDetected && currentMazeComplete) {
    // Ball moved away from completion zone
    currentMazeComplete = false;
    Serial.print("Station ");
    Serial.print(STATION_ID);
    Serial.println(" - Ball left completion zone");
  }
  
  // Check if ball has been in completion zone long enough
  if (currentMazeComplete && !lastMazeComplete) {
    if (currentTime - mazeCompleteStartTime >= MAZE_COMPLETION_TIME) {
      lastMazeComplete = true;
      Serial.print("Station ");
      Serial.print(STATION_ID);
      Serial.println(" - MAZE COMPLETED!");
      playTone(1200, 500); // Maze completion sound
    }
  } else if (!currentMazeComplete) {
    lastMazeComplete = false;
  }
  
  localStatus.mazeComplete = lastMazeComplete;
  lastHallTime = currentTime;
}

void sendHeartbeat() {
  localStatus.timestamp = millis();
  
  if (espNowInitialized) {
    esp_now_send(NULL, (uint8_t*)&localStatus, sizeof(localStatus));
  }
  
  if (wifiConnected) {
    sendUDPHeartbeat();
  }
  
  stationStates[STATION_ID] = localStatus;
}

void sendUDPHeartbeat() {
  udp.beginPacket("255.255.255.255", UDP_PORT);
  udp.write((uint8_t*)&localStatus, sizeof(localStatus));
  udp.endPacket();
}

void checkStationTimeouts() {
  unsigned long currentTime = millis();
  
  for (int i = 1; i <= 3; i++) {
    if (i != STATION_ID) {
      if (currentTime - stationStates[i].timestamp > TIMEOUT_THRESHOLD) {
        if (stationStates[i].touchActive || stationStates[i].mazeComplete) {
          stationStates[i].touchActive = false;
          stationStates[i].mazeComplete = false;
          Serial.print("Station ");
          Serial.print(i);
          Serial.println(" timed out - marking as inactive");
        }
      }
    }
  }
}

void handlePuzzleLogic() {
  unsigned long currentTime = millis();
  
  switch (currentPuzzleState) {
    case PUZZLE_IDLE:
      handlePuzzle1Logic();
      break;
      
    case PUZZLE1_ACTIVE:
      handlePuzzle1Logic();
      break;
      
    case PUZZLE1_COMPLETE:
      handlePuzzle2Logic();
      break;
      
    case PUZZLE2_ACTIVE:
      handlePuzzle2Logic();
      break;
      
    case PUZZLE2_COMPLETE:
      // Could add more puzzles here
      break;
      
    case ALL_PUZZLES_COMPLETE:
      // System complete, could reset or wait for manual reset
      break;
  }
}

void handlePuzzle1Logic() {
  bool allStationsTouch = true;
  
  // Check if all stations have active touches
  for (int i = 1; i <= 3; i++) {
    if (!stationStates[i].touchActive) {
      allStationsTouch = false;
      break;
    }
  }
  
  unsigned long currentTime = millis();
  
  if (allStationsTouch && currentPuzzleState == PUZZLE_IDLE) {
    // Start puzzle 1
    currentPuzzleState = PUZZLE1_ACTIVE;
    puzzleStartTime = currentTime;
    
    Serial.println("=== PUZZLE 1 STARTED ===");
    Serial.println("All stations touching! Hold for " + String(PUZZLE1_SOLVE_TIME / 1000) + " seconds...");
    
    sendPuzzleCommand(0); // Puzzle 1 started
    
  } else if (!allStationsTouch && currentPuzzleState == PUZZLE1_ACTIVE) {
    // Someone released touch during puzzle 1
    currentPuzzleState = PUZZLE_IDLE;
    
    Serial.println("=== PUZZLE 1 FAILED ===");
    Serial.println("Someone released their touch!");
    
    sendPuzzleCommand(2); // Puzzle failed/reset
    
  } else if (allStationsTouch && currentPuzzleState == PUZZLE1_ACTIVE) {
    // Check if puzzle 1 solve time reached
    if (currentTime - puzzleStartTime >= PUZZLE1_SOLVE_TIME) {
      currentPuzzleState = PUZZLE1_COMPLETE;
      
      Serial.println("=== PUZZLE 1 SOLVED! ===");
      Serial.println("Opening latches for magnetic wands...");
      
      // Open latches on all stations
      openLatch();
      sendPuzzleCommand(4); // Open latches command
      
      // Advance WLED preset
      if (IS_MASTER) {
        setWLEDPreset(PUZZLE1_COMPLETE_PRESET);
      }
      
      sendPuzzleCommand(1); // Puzzle 1 solved
      celebrationFlash();
      
      Serial.println("=== PUZZLE 2 READY ===");
      Serial.println("Use magnetic wands to guide balls through mazes!");
    }
  }
}

void handlePuzzle2Logic() {
  bool allStationsMazeComplete = true;
  
  // Check if all stations have completed mazes
  for (int i = 1; i <= 3; i++) {
    if (!stationStates[i].mazeComplete) {
      allStationsMazeComplete = false;
      break;
    }
  }
  
  if (allStationsMazeComplete && currentPuzzleState == PUZZLE1_COMPLETE) {
    // Start checking puzzle 2
    currentPuzzleState = PUZZLE2_ACTIVE;
    puzzleStartTime = millis();
    
    Serial.println("=== PUZZLE 2 ACTIVE ===");
    Serial.println("All mazes have balls in completion zones!");
    
  } else if (allStationsMazeComplete && currentPuzzleState == PUZZLE2_ACTIVE) {
    // Puzzle 2 solved!
    currentPuzzleState = PUZZLE2_COMPLETE;
    
    Serial.println("=== PUZZLE 2 SOLVED! ===");
    Serial.println("All mazes completed successfully!");
    
    // Advance WLED preset
    if (IS_MASTER) {
      setWLEDPreset(PUZZLE2_COMPLETE_PRESET);
    }
    
    sendPuzzleCommand(5); // Puzzle 2 solved
    celebrationFlash();
    
    // Check if more puzzles exist
    if (currentPreset < PRESET_COUNT) {
      Serial.println("=== READY FOR NEXT CHALLENGE ===");
      currentPuzzleState = PUZZLE_IDLE; // Reset for next puzzle cycle
      closeLatch(); // Close latches for next round
    } else {
      currentPuzzleState = ALL_PUZZLES_COMPLETE;
      Serial.println("=== ALL PUZZLES COMPLETE! ===");
    }
    
  } else if (!allStationsMazeComplete && currentPuzzleState == PUZZLE2_ACTIVE) {
    // Someone's ball left the completion zone
    currentPuzzleState = PUZZLE1_COMPLETE; // Back to puzzle 2 ready state
    
    Serial.println("=== PUZZLE 2 RESET ===");
    Serial.println("A ball left its completion zone!");
  }
}

void sendPuzzleCommand(uint8_t command) {
  CommandMessage cmd;
  cmd.command = command;
  cmd.presetNumber = currentPreset;
  cmd.openLatch = latchOpen;
  cmd.timestamp = millis();
  
  if (espNowInitialized) {
    esp_now_send(NULL, (uint8_t*)&cmd, sizeof(cmd));
  }
}

void handleReceivedCommand(CommandMessage cmd) {
  switch (cmd.command) {
    case 0: // Puzzle 1 started
      Serial.println("Received: Puzzle 1 started");
      break;
    case 1: // Puzzle 1 solved
      Serial.println("Received: Puzzle 1 solved!");
      celebrationFlash();
      break;
    case 2: // Puzzle failed/reset
      Serial.println("Received: Puzzle failed/reset");
      break;
    case 3: // Preset changed
      Serial.print("Received: Preset changed to ");
      Serial.println(cmd.presetNumber);
      break;
    case 4: // Open latches
      Serial.println("Received: Opening latch");
      openLatch();
      break;
    case 5: // Puzzle 2 solved
      Serial.println("Received: Puzzle 2 solved!");
      celebrationFlash();
      break;
  }
}

void openLatch() {
  if (!latchOpen) {
    Serial.println("Opening latch...");
    latchServo.write(SERVO_OPEN_ANGLE);
    latchOpen = true;
    localStatus.latchOpen = true;
    playTone(500, 200); // Latch opening sound
    delay(SERVO_MOVE_DELAY);
  }
}

void closeLatch() {
  if (latchOpen) {
    Serial.println("Closing latch...");
    latchServo.write(SERVO_CLOSED_ANGLE);
    latchOpen = false;
    localStatus.latchOpen = false;
    playTone(300, 200); // Latch closing sound
    delay(SERVO_MOVE_DELAY);
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
    currentPreset = presetNumber;
  } else {
    Serial.print("Error setting WLED preset: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
  
  // Notify other stations
  sendPuzzleCommand(3); // Preset change
}

void updateLEDs() {
  unsigned long currentTime = millis();
  
  // LED patterns based on puzzle state
  switch (currentPuzzleState) {
    case PUZZLE_IDLE:
      // Slow blink when idle
      if (currentTime - lastLEDUpdate >= 1000) {
        ledState = !ledState;
        digitalWrite(STATUS_LED_PIN, ledState);
        digitalWrite(EXTERNAL_LED_PIN, currentTouchState ? HIGH : LOW);
        lastLEDUpdate = currentTime;
      }
      break;
      
    case PUZZLE1_ACTIVE:
      // Fast blink during puzzle 1
      if (currentTime - lastLEDUpdate >= 100) {
        ledState = !ledState;
        digitalWrite(STATUS_LED_PIN, ledState);
        digitalWrite(EXTERNAL_LED_PIN, ledState);
        lastLEDUpdate = currentTime;
      }
      break;
      
    case PUZZLE1_COMPLETE:
    case PUZZLE2_ACTIVE:
      // Solid when puzzle 1 complete/puzzle 2 active
      digitalWrite(STATUS_LED_PIN, HIGH);
      digitalWrite(EXTERNAL_LED_PIN, lastMazeComplete ? HIGH : LOW);
      break;
      
    case PUZZLE2_COMPLETE:
    case ALL_PUZZLES_COMPLETE:
      // Celebration pattern
      if (currentTime - lastLEDUpdate >= 200) {
        ledState = !ledState;
        digitalWrite(STATUS_LED_PIN, ledState);
        digitalWrite(EXTERNAL_LED_PIN, ledState);
        lastLEDUpdate = currentTime;
      }
      break;
  }
}

void celebrationFlash() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    digitalWrite(EXTERNAL_LED_PIN, HIGH);
    playTone(1000 + (i * 200), 100);
    delay(100);
    digitalWrite(STATUS_LED_PIN, LOW);
    digitalWrite(EXTERNAL_LED_PIN, LOW);
    delay(100);
  }
}

void playTone(int frequency, int duration) {
  tone(BUZZER_PIN, frequency, duration);
  delay(duration);
  noTone(BUZZER_PIN);
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
          setWLEDPreset(preset);
        }
      } else {
        Serial.println("Only master station can set presets");
      }
    } else if (command == "open") {
      openLatch();
    } else if (command == "close") {
      closeLatch();
    } else if (command == "reset") {
      resetSystem();
    } else if (command == "help") {
      printHelp();
    } else if (command == "mac") {
      Serial.println("MAC Address: " + WiFi.macAddress());
    }
  }
}

void resetSystem() {
  Serial.println("Resetting system...");
  currentPuzzleState = PUZZLE_IDLE;
  closeLatch();
  if (IS_MASTER) {
    setWLEDPreset(INITIAL_PRESET);
  }
  sendPuzzleCommand(2); // Reset command
}

void testCommunication() {
  Serial.println("Testing communication with other stations...");
  
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
  
  Serial.print("Puzzle State: ");
  switch (currentPuzzleState) {
    case PUZZLE_IDLE: Serial.println("IDLE"); break;
    case PUZZLE1_ACTIVE: Serial.println("PUZZLE 1 ACTIVE"); break;
    case PUZZLE1_COMPLETE: Serial.println("PUZZLE 1 COMPLETE"); break;
    case PUZZLE2_ACTIVE: Serial.println("PUZZLE 2 ACTIVE"); break;
    case PUZZLE2_COMPLETE: Serial.println("PUZZLE 2 COMPLETE"); break;
    case ALL_PUZZLES_COMPLETE: Serial.println("ALL COMPLETE"); break;
  }
  
  Serial.print("Latch: ");
  Serial.println(latchOpen ? "OPEN" : "CLOSED");
  
  Serial.println("\n--- Station States ---");
  for (int i = 1; i <= 3; i++) {
    Serial.print("Station ");
    Serial.print(i);
    Serial.print(": Touch=");
    Serial.print(stationStates[i].touchActive ? "ACTIVE" : "INACTIVE");
    Serial.print(", Maze=");
    Serial.print(stationStates[i].mazeComplete ? "COMPLETE" : "INCOMPLETE");
    Serial.print(", Latch=");
    Serial.print(stationStates[i].latchOpen ? "OPEN" : "CLOSED");
    Serial.print(", Last seen=");
    Serial.print((millis() - stationStates[i].timestamp) / 1000);
    Serial.println("s ago");
  }
  
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
  Serial.println("open     - Open latch manually");
  Serial.println("close    - Close latch manually");
  Serial.println("reset    - Reset system to initial state");
  if (IS_MASTER) {
    Serial.println("preset X - Set WLED preset (master only)");
  }
  Serial.println("help     - Show this help");
  Serial.println("================\n");
}

void printInstructions() {
  Serial.println("\n=== PUZZLE INSTRUCTIONS ===");
  Serial.println("PUZZLE 1: Touch & Hold");
  Serial.println("- All 3 people touch sensors simultaneously");
  Serial.println("- Hold for " + String(PUZZLE1_SOLVE_TIME / 1000) + " seconds");
  Serial.println("- Latches will open when solved");
  Serial.println("");
  Serial.println("PUZZLE 2: Magnetic Maze");
  Serial.println("- Take magnetic wands from opened latches");
  Serial.println("- Guide metal balls through mazes");
  Serial.println("- Get balls to completion zones");
  Serial.println("- All 3 mazes must be completed");
  Serial.println("==========================\n");
}