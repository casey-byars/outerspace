/*
 * ESP32 WLED Touch Controller
 * 
 * This system controls WLED lighting presets using a touch sensor
 * - Initial glow effect on startup
 * - Touch sensor to cycle through WLED presets
 * - HTTP requests to WLED controller
 * - Debouncing for reliable touch detection
 * 
 * Hardware Requirements:
 * - ESP32 DevKit
 * - Touch sensor (capacitive or resistive)
 * - WLED controller on same network
 * 
 * WLED API Documentation:
 * - Preset activation: http://[wled-ip]/win&PL=[preset_number]
 * - JSON API: http://[wled-ip]/json/state
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Configuration
const char* ssid = "YOUR_WIFI_SSID";           // Replace with your WiFi SSID
const char* password = "YOUR_WIFI_PASSWORD";   // Replace with your WiFi password

// WLED Configuration
const char* wledIP = "192.168.1.100";          // Replace with your WLED controller IP
const int wledPort = 80;                       // WLED default port

// Pin Configuration
#define TOUCH_PIN 4                             // Touch sensor pin (GPIO4)
#define STATUS_LED_PIN 2                        // Built-in LED for status indication

// Touch Sensor Configuration
#define TOUCH_THRESHOLD 30                      // Touch threshold (adjust based on your sensor)
#define TOUCH_DEBOUNCE_TIME 500                 // Debounce time in milliseconds
#define TOUCH_HOLD_TIME 50                      // Minimum hold time for valid touch

// WLED Preset Configuration
const int PRESET_COUNT = 10;                   // Number of presets to cycle through
int currentPreset = 1;                         // Start with preset 1
const int INITIAL_PRESET = 0;                  // Preset for initial glow (0 = default)

// System State
bool wifiConnected = false;
bool wledConnected = false;
unsigned long lastTouchTime = 0;
bool lastTouchState = false;
bool systemInitialized = false;

// Status tracking
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_UPDATE_INTERVAL = 30000; // Check status every 30 seconds
unsigned long wifiReconnectAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 10000; // Try to reconnect every 10 seconds

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 WLED Touch Controller Starting...");
  
  // Initialize pins
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  
  // Initialize WiFi
  connectToWiFi();
  
  // Initialize WLED connection and set initial glow
  if (wifiConnected) {
    initializeWLED();
  }
  
  Serial.println("System Ready!");
  Serial.println("Touch the sensor to cycle through WLED presets");
  systemInitialized = true;
}

void loop() {
  unsigned long currentTime = millis();
  
  // Check for serial commands
  serialEvent();
  
  // Check WiFi connection status
  if (currentTime - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
    checkConnections();
    lastStatusUpdate = currentTime;
  }
  
  // Handle WiFi reconnection if needed
  if (!wifiConnected && (currentTime - wifiReconnectAttempt >= WIFI_RECONNECT_INTERVAL)) {
    Serial.println("Attempting to reconnect to WiFi...");
    connectToWiFi();
    wifiReconnectAttempt = currentTime;
  }
  
  // Handle touch sensor input
  if (wifiConnected && wledConnected) {
    handleTouchInput();
  }
  
  // Update status LED
  updateStatusLED();
  
  delay(10); // Small delay to prevent excessive CPU usage
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
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
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    Serial.println("");
    Serial.println("Failed to connect to WiFi");
  }
}

void initializeWLED() {
  Serial.println("Initializing WLED connection...");
  
  // Test WLED connection
  if (testWLEDConnection()) {
    wledConnected = true;
    Serial.println("WLED connection established!");
    
    // Set initial glow preset
    Serial.println("Setting initial glow preset...");
    setWLEDPreset(INITIAL_PRESET);
    
    delay(1000); // Give WLED time to process
    
    // Get current WLED status
    getWLEDStatus();
  } else {
    wledConnected = false;
    Serial.println("Failed to connect to WLED controller");
  }
}

bool testWLEDConnection() {
  HTTPClient http;
  String url = "http://" + String(wledIP) + "/json/info";
  
  http.begin(url);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("WLED Info Response: " + response);
    http.end();
    return true;
  } else {
    Serial.print("Error connecting to WLED: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}

void setWLEDPreset(int presetNumber) {
  if (!wifiConnected || !wledConnected) {
    Serial.println("Cannot set preset: Not connected to WLED");
    return;
  }
  
  HTTPClient http;
  String url = "http://" + String(wledIP) + "/win&PL=" + String(presetNumber);
  
  Serial.print("Setting WLED preset ");
  Serial.print(presetNumber);
  Serial.print(" - URL: ");
  Serial.println(url);
  
  http.begin(url);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("WLED Response: " + response);
    
    // Update current preset tracking
    if (presetNumber != INITIAL_PRESET) {
      currentPreset = presetNumber;
    }
  } else {
    Serial.print("Error setting WLED preset: ");
    Serial.println(httpResponseCode);
    
    // If we get an error, mark WLED as disconnected
    wledConnected = false;
  }
  
  http.end();
}

void getWLEDStatus() {
  if (!wifiConnected || !wledConnected) {
    return;
  }
  
  HTTPClient http;
  String url = "http://" + String(wledIP) + "/json/state";
  
  http.begin(url);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    
    // Parse JSON response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    
    bool isOn = doc["on"];
    int brightness = doc["bri"];
    int preset = doc["ps"];
    
    Serial.println("=== WLED Status ===");
    Serial.println("Power: " + String(isOn ? "ON" : "OFF"));
    Serial.println("Brightness: " + String(brightness));
    Serial.println("Current Preset: " + String(preset));
    Serial.println("==================");
    
  } else {
    Serial.print("Error getting WLED status: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

void handleTouchInput() {
  int touchValue = digitalRead(TOUCH_PIN);
  unsigned long currentTime = millis();
  
  // For capacitive touch sensors, you might want to use touchRead() instead:
  // int touchValue = touchRead(TOUCH_PIN);
  // bool touchDetected = touchValue < TOUCH_THRESHOLD;
  
  bool touchDetected = (touchValue == HIGH); // Adjust based on your sensor type
  
  // Debouncing logic
  if (touchDetected && !lastTouchState) {
    if (currentTime - lastTouchTime >= TOUCH_DEBOUNCE_TIME) {
      // Valid touch detected
      lastTouchTime = currentTime;
      lastTouchState = true;
      
      Serial.println("Touch detected! Cycling to next preset...");
      
      // Cycle to next preset
      currentPreset++;
      if (currentPreset > PRESET_COUNT) {
        currentPreset = 1; // Loop back to preset 1
      }
      
      setWLEDPreset(currentPreset);
      
      // Provide feedback
      flashStatusLED(3, 100); // Flash LED 3 times quickly
    }
  } else if (!touchDetected) {
    lastTouchState = false;
  }
}

void checkConnections() {
  // Check WiFi status
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("WiFi connection lost");
  } else if (!wifiConnected) {
    wifiConnected = true;
    Serial.println("WiFi reconnected");
    
    // Reinitialize WLED if we reconnected
    initializeWLED();
  }
  
  // Test WLED connection if WiFi is connected
  if (wifiConnected && !wledConnected) {
    Serial.println("Testing WLED connection...");
    if (testWLEDConnection()) {
      wledConnected = true;
      Serial.println("WLED reconnected");
    }
  }
}

void updateStatusLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  unsigned long currentTime = millis();
  
  if (wifiConnected && wledConnected) {
    // Solid on when everything is connected
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else if (wifiConnected && !wledConnected) {
    // Fast blink when WiFi connected but WLED not connected
    if (currentTime - lastBlink >= 250) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
      lastBlink = currentTime;
    }
  } else {
    // Slow blink when WiFi not connected
    if (currentTime - lastBlink >= 1000) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
      lastBlink = currentTime;
    }
  }
}

void flashStatusLED(int flashes, int delayTime) {
  for (int i = 0; i < flashes; i++) {
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(delayTime);
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(delayTime);
  }
}

// Serial command interface for testing and configuration
void serialEvent() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("preset ")) {
      int presetNum = command.substring(7).toInt();
      if (presetNum >= 0 && presetNum <= PRESET_COUNT) {
        setWLEDPreset(presetNum);
      } else {
        Serial.println("Invalid preset number. Use 0-" + String(PRESET_COUNT));
      }
    } else if (command == "status") {
      printSystemStatus();
    } else if (command == "wled") {
      getWLEDStatus();
    } else if (command == "test") {
      testWLEDConnection();
    } else if (command == "reconnect") {
      connectToWiFi();
      if (wifiConnected) {
        initializeWLED();
      }
    } else if (command == "help") {
      printHelp();
    } else {
      Serial.println("Unknown command. Type 'help' for available commands.");
    }
  }
}

void printSystemStatus() {
  Serial.println("\n=== System Status ===");
  Serial.println("WiFi Connected: " + String(wifiConnected ? "YES" : "NO"));
  if (wifiConnected) {
    Serial.println("IP Address: " + WiFi.localIP().toString());
    Serial.println("Signal Strength: " + String(WiFi.RSSI()) + " dBm");
  }
  Serial.println("WLED Connected: " + String(wledConnected ? "YES" : "NO"));
  Serial.println("WLED IP: " + String(wledIP));
  Serial.println("Current Preset: " + String(currentPreset));
  Serial.println("Touch Pin: " + String(TOUCH_PIN));
  Serial.println("Touch Value: " + String(digitalRead(TOUCH_PIN)));
  Serial.println("System Uptime: " + String(millis() / 1000) + " seconds");
  Serial.println("====================\n");
}

void printHelp() {
  Serial.println("\n=== Available Commands ===");
  Serial.println("preset [0-" + String(PRESET_COUNT) + "] - Set specific WLED preset");
  Serial.println("status     - Show system status");
  Serial.println("wled       - Show WLED status");
  Serial.println("test       - Test WLED connection");
  Serial.println("reconnect  - Reconnect WiFi and WLED");
  Serial.println("help       - Show this help");
  Serial.println("==========================\n");
}

