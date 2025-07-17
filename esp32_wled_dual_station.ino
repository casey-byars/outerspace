/*
 * ESP32 WLED Dual-Station Touch Controller v3.0
 * 
 * Three-puzzle cooperative system for 2 people:
 * PUZZLE 1: Both people touch sensors simultaneously for 2 seconds
 * PUZZLE 2: Both people complete magnetic ball mazes using wands
 * PUZZLE 3: Video playback on tablet/iPad with interaction detection
 * 
 * Hardware per station:
 * - ESP32 DevKit
 * - Touch sensor (GPIO4)
 * - Servo motor for latch (GPIO16)
 * - Hall sensor for maze completion (GPIO34)
 * - Status LEDs (GPIO2, GPIO5)
 * - Buzzer (GPIO19)
 * - Magnetic wand (released by servo latch)
 * - Metal ball maze with magnetic completion zone
 * - Tablet/iPad for video display (communicates via HTTP)
 * 
 * Puzzle Flow:
 * 1. Initial state: WLED preset 0 (glow)
 * 2. Solve puzzle 1 → WLED preset 1 + open latches
 * 3. Solve puzzle 2 → WLED preset 2 + trigger videos
 * 4. Complete puzzle 3 → WLED preset 3 + final celebration
 * 5. System can cycle through more puzzles/presets
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <SPIFFS.h>

// Station Configuration - CHANGE THIS FOR EACH STATION
#define STATION_ID 1                    // Set to 1 or 2 for each station
#define IS_MASTER (STATION_ID == 1)     // Station 1 is the master

// WiFi Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// WLED Configuration (only used by master station)
const char* wledIP = "192.168.1.100";
const int wledPort = 80;

// Video/Tablet Configuration
const char* tabletIP1 = "192.168.1.201";    // iPad/tablet IP for station 1
const char* tabletIP2 = "192.168.1.202";    // iPad/tablet IP for station 2
const int tabletPort = 8080;                // Port for video control API

// Pin Configuration
#define TOUCH_PIN 4                     // Touch sensor pin
#define SERVO_PIN 16                    // Servo motor pin for latch
#define HALL_SENSOR_PIN 34              // Hall sensor pin (analog)
#define STATUS_LED_PIN 2                // Built-in LED for status
#define EXTERNAL_LED_PIN 5              // External LED for better visibility
#define BUZZER_PIN 19                   // Buzzer for audio feedback
#define VIDEO_TRIGGER_PIN 21            // Optional: GPIO pin to trigger local video

// Touch Sensor Configuration
#define TOUCH_THRESHOLD 30              // Touch threshold for capacitive sensors
#define TOUCH_DEBOUNCE_TIME 100         // Debounce time in milliseconds
#define PUZZLE1_SOLVE_TIME 2000         // Time all sensors must be held (2 seconds)

// Hall Sensor Configuration
#define HALL_THRESHOLD 100              // Hall sensor threshold (adjust based on magnet strength)
#define HALL_DEBOUNCE_TIME 500          // Debounce time for hall sensor
#define MAZE_COMPLETION_TIME 1000       // Time ball must stay in completion zone

// Video Configuration
#define VIDEO_DURATION 30000            // Expected video duration (30 seconds)
#define VIDEO_COMPLETION_THRESHOLD 0.9  // 90% completion to consider video watched
#define VIDEO_INTERACTION_TIMEOUT 60000 // Max time to wait for video interaction (60 seconds)

// Servo Configuration
#define SERVO_CLOSED_ANGLE 0            // Servo angle when latch is closed
#define SERVO_OPEN_ANGLE 90             // Servo angle when latch is open
#define SERVO_MOVE_DELAY 1000           // Time to wait for servo movement

// Communication Configuration
#define ESP_NOW_CHANNEL 1               // ESP-NOW channel
#define UDP_PORT 12345                  // UDP port for fallback communication
#define WEB_SERVER_PORT 80              // Web server port for tablet communication
#define HEARTBEAT_INTERVAL 1000         // Send status every 1 second
#define TIMEOUT_THRESHOLD 3000          // Consider station offline after 3 seconds

// WLED Preset Configuration
const int PRESET_COUNT = 10;
int currentPreset = 0;
const int INITIAL_PRESET = 0;           // Initial glow
const int PUZZLE1_COMPLETE_PRESET = 1;  // After puzzle 1
const int PUZZLE2_COMPLETE_PRESET = 2;  // After puzzle 2
const int PUZZLE3_COMPLETE_PRESET = 3;  // After puzzle 3

// ESP-NOW MAC addresses of other station (you'll need to find this)
uint8_t station1_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Replace with actual MAC
uint8_t station2_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Replace with actual MAC

// Communication structures
typedef struct {
  uint8_t stationId;
  bool touchActive;
  bool mazeComplete;
  bool latchOpen;
  bool videoStarted;
  bool videoComplete;
  float videoProgress;
  unsigned long timestamp;
  float batteryLevel;
} StationStatus;

typedef struct {
  uint8_t command;           // 0=status, 1=puzzle1_solved, 2=reset, 3=preset_change, 4=open_latches, 5=puzzle2_solved, 6=start_videos, 7=puzzle3_solved
  uint8_t presetNumber;
  bool openLatch;
  bool startVideo;
  unsigned long timestamp;
} CommandMessage;

// Puzzle states
enum PuzzleState {
  PUZZLE_IDLE,
  PUZZLE1_ACTIVE,
  PUZZLE1_COMPLETE,
  PUZZLE2_ACTIVE,
  PUZZLE2_COMPLETE,
  PUZZLE3_ACTIVE,
  PUZZLE3_COMPLETE,
  ALL_PUZZLES_COMPLETE
};

// Video states
enum VideoState {
  VIDEO_IDLE,
  VIDEO_STARTING,
  VIDEO_PLAYING,
  VIDEO_PAUSED,
  VIDEO_COMPLETED,
  VIDEO_ERROR
};

// System state
StationStatus localStatus;
StationStatus stationStates[3]; // Index 0 unused, 1-2 for stations
bool wifiConnected = false;
bool espNowInitialized = false;
unsigned long lastHeartbeat = 0;
PuzzleState currentPuzzleState = PUZZLE_IDLE;
VideoState currentVideoState = VIDEO_IDLE;
unsigned long puzzleStartTime = 0;
unsigned long videoStartTime = 0;

// Touch sensor state
bool currentTouchState = false;
bool lastTouchState = false;
unsigned long lastTouchTime = 0;

// Hall sensor state
bool currentMazeComplete = false;
bool lastMazeComplete = false;
unsigned long lastHallTime = 0;
unsigned long mazeCompleteStartTime = 0;

// Video tracking
bool videoStarted = false;
bool videoComplete = false;
float videoProgress = 0.0;
unsigned long lastVideoUpdate = 0;

// Servo control
Servo latchServo;
bool latchOpen = false;

// LED control
unsigned long lastLEDUpdate = 0;
bool ledState = false;

// Communication objects
WiFiUDP udp;
WebServer webServer(WEB_SERVER_PORT);

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Dual-Station Touch Controller v3.0 Starting...");
  Serial.print("Station ID: ");
  Serial.println(STATION_ID);
  Serial.print("Role: ");
  Serial.println(IS_MASTER ? "MASTER" : "SLAVE");
  
  // Initialize SPIFFS for web interface
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed");
  }
  
  // Initialize pins
  pinMode(TOUCH_PIN, INPUT);
  pinMode(HALL_SENSOR_PIN, INPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(EXTERNAL_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIDEO_TRIGGER_PIN, OUTPUT);
  
  // Initialize servo
  latchServo.attach(SERVO_PIN);
  closeLatch(); // Start with latch closed
  
  // Initialize local status
  localStatus.stationId = STATION_ID;
  localStatus.touchActive = false;
  localStatus.mazeComplete = false;
  localStatus.latchOpen = false;
  localStatus.videoStarted = false;
  localStatus.videoComplete = false;
  localStatus.videoProgress = 0.0;
  localStatus.timestamp = millis();
  localStatus.batteryLevel = 100.0;
  
  // Initialize station states (only 2 stations now)
  for (int i = 1; i <= 2; i++) {
    stationStates[i].stationId = i;
    stationStates[i].touchActive = false;
    stationStates[i].mazeComplete = false;
    stationStates[i].latchOpen = false;
    stationStates[i].videoStarted = false;
    stationStates[i].videoComplete = false;
    stationStates[i].videoProgress = 0.0;
    stationStates[i].timestamp = 0;
    stationStates[i].batteryLevel = 100.0;
  }
  
  // Initialize WiFi
  initializeWiFi();
  
  // Initialize ESP-NOW
  initializeESPNow();
  
  // Initialize web server for video control
  initializeWebServer();
  
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
  
  // Print connection information
  Serial.print("This station's MAC address: ");
  Serial.println(WiFi.macAddress());
  if (wifiConnected) {
    Serial.print("Web server at: http://");
    Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.println(WEB_SERVER_PORT);
  }
  
  Serial.println("System Ready!");
  printInstructions();
}

void loop() {
  unsigned long currentTime = millis();
  
  // Handle web server requests
  webServer.handleClient();
  
  // Read sensors
  readTouchSensor();
  readHallSensor();
  
  // Update video progress tracking
  updateVideoProgress();
  
  // Send heartbeat
  if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = currentTime;
  }
  
  // Check for timeouts on other station
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

void initializeWebServer() {
  // Set up web server endpoints for video control
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/video/start", HTTP_POST, handleVideoStart);
  webServer.on("/video/progress", HTTP_POST, handleVideoProgress);
  webServer.on("/video/complete", HTTP_POST, handleVideoComplete);
  webServer.on("/video/pause", HTTP_POST, handleVideoPause);
  webServer.on("/status", HTTP_GET, handleWebStatus);
  webServer.onNotFound(handleNotFound);
  
  webServer.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = generateVideoControlHTML();
  webServer.send(200, "text/html", html);
}

void handleVideoStart() {
  Serial.println("Video start requested via web API");
  videoStarted = true;
  videoComplete = false;
  videoProgress = 0.0;
  videoStartTime = millis();
  currentVideoState = VIDEO_PLAYING;
  localStatus.videoStarted = true;
  localStatus.videoComplete = false;
  localStatus.videoProgress = 0.0;
  
  webServer.send(200, "application/json", "{\"status\":\"started\"}");
  playTone(800, 200); // Video start feedback
}

void handleVideoProgress() {
  if (webServer.hasArg("progress")) {
    float progress = webServer.arg("progress").toFloat();
    videoProgress = constrain(progress, 0.0, 1.0);
    localStatus.videoProgress = videoProgress;
    lastVideoUpdate = millis();
    
    Serial.print("Video progress updated: ");
    Serial.print(videoProgress * 100);
    Serial.println("%");
  }
  
  webServer.send(200, "application/json", "{\"status\":\"updated\"}");
}

void handleVideoComplete() {
  Serial.println("Video completion reported via web API");
  videoComplete = true;
  videoProgress = 1.0;
  currentVideoState = VIDEO_COMPLETED;
  localStatus.videoComplete = true;
  localStatus.videoProgress = 1.0;
  
  webServer.send(200, "application/json", "{\"status\":\"completed\"}");
  playTone(1200, 500); // Video completion feedback
}

void handleVideoPause() {
  Serial.println("Video paused via web API");
  currentVideoState = VIDEO_PAUSED;
  webServer.send(200, "application/json", "{\"status\":\"paused\"}");
}

void handleWebStatus() {
  DynamicJsonDocument doc(1024);
  doc["stationId"] = STATION_ID;
  doc["puzzleState"] = currentPuzzleState;
  doc["videoState"] = currentVideoState;
  doc["videoProgress"] = videoProgress;
  doc["videoStarted"] = videoStarted;
  doc["videoComplete"] = videoComplete;
  
  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleNotFound() {
  webServer.send(404, "text/plain", "Not found");
}

String generateVideoControlHTML() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Station )" + String(STATION_ID) + R"( Video Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        .status { background: #e7f3ff; padding: 15px; border-radius: 5px; margin: 10px 0; }
        .button { background: #007cba; color: white; padding: 15px 30px; border: none; border-radius: 5px; font-size: 16px; margin: 5px; cursor: pointer; }
        .button:hover { background: #005a8a; }
        .progress-bar { width: 100%; height: 20px; background: #ddd; border-radius: 10px; overflow: hidden; margin: 10px 0; }
        .progress-fill { height: 100%; background: #4caf50; transition: width 0.3s; }
        .video-container { margin: 20px 0; }
        #videoElement { width: 100%; max-width: 600px; height: auto; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Station )" + String(STATION_ID) + R"( - Puzzle 3 Video</h1>
        
        <div class="status">
            <h3>Status: <span id="status">Ready</span></h3>
            <p>Video Progress: <span id="progress">0%</span></p>
            <div class="progress-bar">
                <div class="progress-fill" id="progressBar" style="width: 0%"></div>
            </div>
        </div>
        
        <div class="video-container">
            <video id="videoElement" controls>
                <source src="/video/puzzle3_station)" + String(STATION_ID) + R"(.mp4" type="video/mp4">
                <p>Your browser doesn't support video playback. Please use a modern browser or tablet app.</p>
            </video>
        </div>
        
        <div>
            <button class="button" onclick="startVideo()">Start Video</button>
            <button class="button" onclick="pauseVideo()">Pause Video</button>
            <button class="button" onclick="markComplete()">Mark Complete</button>
        </div>
        
        <div class="status">
            <h4>Instructions:</h4>
            <p>1. This video will automatically start when Puzzle 2 is completed</p>
            <p>2. Watch the entire video to complete Puzzle 3</p>
            <p>3. The system tracks your progress automatically</p>
            <p>4. Both stations must complete their videos to advance</p>
        </div>
    </div>

    <script>
        const video = document.getElementById('videoElement');
        const statusEl = document.getElementById('status');
        const progressEl = document.getElementById('progress');
        const progressBar = document.getElementById('progressBar');
        
        let videoStarted = false;
        
        video.addEventListener('play', function() {
            if (!videoStarted) {
                fetch('/video/start', { method: 'POST' });
                videoStarted = true;
                statusEl.textContent = 'Playing';
            }
        });
        
        video.addEventListener('pause', function() {
            fetch('/video/pause', { method: 'POST' });
            statusEl.textContent = 'Paused';
        });
        
        video.addEventListener('timeupdate', function() {
            const progress = video.currentTime / video.duration;
            updateProgress(progress);
            
            fetch('/video/progress', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'progress=' + progress
            });
        });
        
        video.addEventListener('ended', function() {
            fetch('/video/complete', { method: 'POST' });
            statusEl.textContent = 'Completed';
            updateProgress(1.0);
        });
        
        function updateProgress(progress) {
            const percent = Math.round(progress * 100);
            progressEl.textContent = percent + '%';
            progressBar.style.width = percent + '%';
        }
        
        function startVideo() {
            video.play();
        }
        
        function pauseVideo() {
            video.pause();
        }
        
        function markComplete() {
            fetch('/video/complete', { method: 'POST' });
            statusEl.textContent = 'Manually Completed';
            updateProgress(1.0);
        }
    </script>
</body>
</html>)";
  
  return html;
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
  
  // Add the other station as peer
  uint8_t* mac = (STATION_ID == 1) ? station2_mac : station1_mac;
  int otherStationId = (STATION_ID == 1) ? 2 : 1;
  
  memcpy(peerInfo.peer_addr, mac, 6);
  
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.print("Added peer station ");
    Serial.println(otherStationId);
  } else {
    Serial.print("Failed to add peer station ");
    Serial.println(otherStationId);
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: Handle send confirmation
}

void onDataReceived(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(StationStatus)) {
    StationStatus receivedStatus;
    memcpy(&receivedStatus, incomingData, sizeof(receivedStatus));
    
    if (receivedStatus.stationId >= 1 && receivedStatus.stationId <= 2) {
      stationStates[receivedStatus.stationId] = receivedStatus;
      stationStates[receivedStatus.stationId].timestamp = millis();
      
      Serial.print("Received status from station ");
      Serial.print(receivedStatus.stationId);
      Serial.print(" - Touch: ");
      Serial.print(receivedStatus.touchActive ? "ACTIVE" : "INACTIVE");
      Serial.print(", Maze: ");
      Serial.print(receivedStatus.mazeComplete ? "COMPLETE" : "INCOMPLETE");
      Serial.print(", Video: ");
      Serial.print(receivedStatus.videoComplete ? "COMPLETE" : "INCOMPLETE");
      Serial.print(" (");
      Serial.print(receivedStatus.videoProgress * 100, 1);
      Serial.println("%)");
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
        playTone(1000, 100);
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
  bool hallDetected = hallValue < HALL_THRESHOLD;
  
  unsigned long currentTime = millis();
  
  if (hallDetected && !currentMazeComplete) {
    if (currentTime - lastHallTime >= HALL_DEBOUNCE_TIME) {
      mazeCompleteStartTime = currentTime;
      currentMazeComplete = true;
      Serial.print("Station ");
      Serial.print(STATION_ID);
      Serial.println(" - Ball detected in completion zone!");
      playTone(800, 200);
    }
  } else if (!hallDetected && currentMazeComplete) {
    currentMazeComplete = false;
    Serial.print("Station ");
    Serial.print(STATION_ID);
    Serial.println(" - Ball left completion zone");
  }
  
  if (currentMazeComplete && !lastMazeComplete) {
    if (currentTime - mazeCompleteStartTime >= MAZE_COMPLETION_TIME) {
      lastMazeComplete = true;
      Serial.print("Station ");
      Serial.print(STATION_ID);
      Serial.println(" - MAZE COMPLETED!");
      playTone(1200, 500);
    }
  } else if (!currentMazeComplete) {
    lastMazeComplete = false;
  }
  
  localStatus.mazeComplete = lastMazeComplete;
  lastHallTime = currentTime;
}

void updateVideoProgress() {
  // Auto-update video progress if playing
  if (currentVideoState == VIDEO_PLAYING && videoStarted && !videoComplete) {
    unsigned long currentTime = millis();
    if (currentTime - videoStartTime > 0) {
      float autoProgress = (float)(currentTime - videoStartTime) / VIDEO_DURATION;
      if (autoProgress > videoProgress) {
        videoProgress = constrain(autoProgress, 0.0, 1.0);
        localStatus.videoProgress = videoProgress;
      }
    }
  }
  
  localStatus.videoStarted = videoStarted;
  localStatus.videoComplete = videoComplete;
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
  
  // Check the other station (only 2 stations now)
  int otherStation = (STATION_ID == 1) ? 2 : 1;
  
  if (currentTime - stationStates[otherStation].timestamp > TIMEOUT_THRESHOLD) {
    if (stationStates[otherStation].touchActive || stationStates[otherStation].mazeComplete || stationStates[otherStation].videoStarted) {
      stationStates[otherStation].touchActive = false;
      stationStates[otherStation].mazeComplete = false;
      // Don't reset video state on timeout - videos might be long
      Serial.print("Station ");
      Serial.print(otherStation);
      Serial.println(" timed out - marking touch/maze as inactive");
    }
  }
}

void handlePuzzleLogic() {
  switch (currentPuzzleState) {
    case PUZZLE_IDLE:
    case PUZZLE1_ACTIVE:
      handlePuzzle1Logic();
      break;
      
    case PUZZLE1_COMPLETE:
    case PUZZLE2_ACTIVE:
      handlePuzzle2Logic();
      break;
      
    case PUZZLE2_COMPLETE:
    case PUZZLE3_ACTIVE:
      handlePuzzle3Logic();
      break;
      
    case PUZZLE3_COMPLETE:
      // Could add more puzzles here
      break;
      
    case ALL_PUZZLES_COMPLETE:
      // System complete
      break;
  }
}

void handlePuzzle1Logic() {
  // Check if both stations have active touches
  bool bothStationsTouch = stationStates[1].touchActive && stationStates[2].touchActive;
  
  unsigned long currentTime = millis();
  
  if (bothStationsTouch && currentPuzzleState == PUZZLE_IDLE) {
    currentPuzzleState = PUZZLE1_ACTIVE;
    puzzleStartTime = currentTime;
    
    Serial.println("=== PUZZLE 1 STARTED ===");
    Serial.println("Both stations touching! Hold for " + String(PUZZLE1_SOLVE_TIME / 1000) + " seconds...");
    
    sendPuzzleCommand(0);
    
  } else if (!bothStationsTouch && currentPuzzleState == PUZZLE1_ACTIVE) {
    currentPuzzleState = PUZZLE_IDLE;
    
    Serial.println("=== PUZZLE 1 FAILED ===");
    Serial.println("Someone released their touch!");
    
    sendPuzzleCommand(2);
    
  } else if (bothStationsTouch && currentPuzzleState == PUZZLE1_ACTIVE) {
    if (currentTime - puzzleStartTime >= PUZZLE1_SOLVE_TIME) {
      currentPuzzleState = PUZZLE1_COMPLETE;
      
      Serial.println("=== PUZZLE 1 SOLVED! ===");
      Serial.println("Opening latches for magnetic wands...");
      
      openLatch();
      sendPuzzleCommand(4);
      
      if (IS_MASTER) {
        setWLEDPreset(PUZZLE1_COMPLETE_PRESET);
      }
      
      sendPuzzleCommand(1);
      celebrationFlash();
      
      Serial.println("=== PUZZLE 2 READY ===");
      Serial.println("Use magnetic wands to guide balls through mazes!");
    }
  }
}

void handlePuzzle2Logic() {
  // Check if both stations have completed mazes
  bool bothStationsMazeComplete = stationStates[1].mazeComplete && stationStates[2].mazeComplete;
  
  if (bothStationsMazeComplete && currentPuzzleState == PUZZLE1_COMPLETE) {
    currentPuzzleState = PUZZLE2_ACTIVE;
    puzzleStartTime = millis();
    
    Serial.println("=== PUZZLE 2 ACTIVE ===");
    Serial.println("Both mazes have balls in completion zones!");
    
  } else if (bothStationsMazeComplete && currentPuzzleState == PUZZLE2_ACTIVE) {
    currentPuzzleState = PUZZLE2_COMPLETE;
    
    Serial.println("=== PUZZLE 2 SOLVED! ===");
    Serial.println("Both mazes completed successfully!");
    
    if (IS_MASTER) {
      setWLEDPreset(PUZZLE2_COMPLETE_PRESET);
    }
    
    sendPuzzleCommand(5);
    celebrationFlash();
    
    // Start videos on both stations
    Serial.println("=== STARTING PUZZLE 3 VIDEOS ===");
    sendPuzzleCommand(6); // Start videos
    startVideo();
    
  } else if (!bothStationsMazeComplete && currentPuzzleState == PUZZLE2_ACTIVE) {
    currentPuzzleState = PUZZLE1_COMPLETE;
    
    Serial.println("=== PUZZLE 2 RESET ===");
    Serial.println("A ball left its completion zone!");
  }
}

void handlePuzzle3Logic() {
  // Check if both stations have completed videos
  bool bothStationsVideoComplete = stationStates[1].videoComplete && stationStates[2].videoComplete;
  
  if (bothStationsVideoComplete && currentPuzzleState == PUZZLE2_COMPLETE) {
    currentPuzzleState = PUZZLE3_ACTIVE;
    Serial.println("=== PUZZLE 3 ACTIVE ===");
    Serial.println("Both videos started!");
    
  } else if (bothStationsVideoComplete && currentPuzzleState == PUZZLE3_ACTIVE) {
    currentPuzzleState = PUZZLE3_COMPLETE;
    
    Serial.println("=== PUZZLE 3 SOLVED! ===");
    Serial.println("Both videos completed successfully!");
    
    if (IS_MASTER) {
      setWLEDPreset(PUZZLE3_COMPLETE_PRESET);
    }
    
    sendPuzzleCommand(7); // Puzzle 3 solved
    celebrationFlash();
    
    // Check if more puzzles exist
    if (currentPreset < PRESET_COUNT) {
      Serial.println("=== READY FOR NEXT CHALLENGE ===");
      currentPuzzleState = PUZZLE_IDLE;
      resetForNextRound();
    } else {
      currentPuzzleState = ALL_PUZZLES_COMPLETE;
      Serial.println("=== ALL PUZZLES COMPLETE! ===");
      finalCelebration();
    }
  }
}

void startVideo() {
  Serial.println("Starting video for Puzzle 3...");
  digitalWrite(VIDEO_TRIGGER_PIN, HIGH);
  delay(100);
  digitalWrite(VIDEO_TRIGGER_PIN, LOW);
  
  // Also trigger via HTTP if tablet has API
  if (wifiConnected) {
    triggerTabletVideo();
  }
  
  videoStarted = true;
  videoStartTime = millis();
  currentVideoState = VIDEO_PLAYING;
  localStatus.videoStarted = true;
  
  playTone(600, 300); // Video start notification
}

void triggerTabletVideo() {
  String tabletIP = (STATION_ID == 1) ? tabletIP1 : tabletIP2;
  
  HTTPClient http;
  String url = "http://" + tabletIP + ":" + String(tabletPort) + "/play";
  
  http.begin(url);
  http.setTimeout(5000); // 5 second timeout
  
  int httpResponseCode = http.POST("");
  
  if (httpResponseCode > 0) {
    Serial.print("Tablet video trigger successful: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Tablet video trigger failed: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

void sendPuzzleCommand(uint8_t command) {
  CommandMessage cmd;
  cmd.command = command;
  cmd.presetNumber = currentPreset;
  cmd.openLatch = latchOpen;
  cmd.startVideo = (command == 6);
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
    case 6: // Start videos
      Serial.println("Received: Starting video");
      startVideo();
      break;
    case 7: // Puzzle 3 solved
      Serial.println("Received: Puzzle 3 solved!");
      celebrationFlash();
      break;
  }
}

void resetForNextRound() {
  closeLatch();
  videoStarted = false;
  videoComplete = false;
  videoProgress = 0.0;
  currentVideoState = VIDEO_IDLE;
  localStatus.videoStarted = false;
  localStatus.videoComplete = false;
  localStatus.videoProgress = 0.0;
}

void finalCelebration() {
  for (int i = 0; i < 10; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    digitalWrite(EXTERNAL_LED_PIN, HIGH);
    playTone(1000 + (i * 100), 150);
    delay(150);
    digitalWrite(STATUS_LED_PIN, LOW);
    digitalWrite(EXTERNAL_LED_PIN, LOW);
    delay(150);
  }
}

void openLatch() {
  if (!latchOpen) {
    Serial.println("Opening latch...");
    latchServo.write(SERVO_OPEN_ANGLE);
    latchOpen = true;
    localStatus.latchOpen = true;
    playTone(500, 200);
    delay(SERVO_MOVE_DELAY);
  }
}

void closeLatch() {
  if (latchOpen) {
    Serial.println("Closing latch...");
    latchServo.write(SERVO_CLOSED_ANGLE);
    latchOpen = false;
    localStatus.latchOpen = false;
    playTone(300, 200);
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
  
  sendPuzzleCommand(3); // Preset change
}

void updateLEDs() {
  unsigned long currentTime = millis();
  
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
    case PUZZLE3_ACTIVE:
      // Pulse based on video progress
      if (currentTime - lastLEDUpdate >= 100) {
        int brightness = (int)(videoProgress * 255);
        analogWrite(EXTERNAL_LED_PIN, brightness);
        digitalWrite(STATUS_LED_PIN, videoComplete ? HIGH : ledState);
        ledState = !ledState;
        lastLEDUpdate = currentTime;
      }
      break;
      
    case PUZZLE3_COMPLETE:
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
    } else if (command == "video") {
      startVideo();
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
  currentVideoState = VIDEO_IDLE;
  closeLatch();
  videoStarted = false;
  videoComplete = false;
  videoProgress = 0.0;
  localStatus.videoStarted = false;
  localStatus.videoComplete = false;
  localStatus.videoProgress = 0.0;
  
  if (IS_MASTER) {
    setWLEDPreset(INITIAL_PRESET);
  }
  sendPuzzleCommand(2); // Reset command
}

void testCommunication() {
  Serial.println("Testing communication with other station...");
  
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
    case PUZZLE3_ACTIVE: Serial.println("PUZZLE 3 ACTIVE"); break;
    case PUZZLE3_COMPLETE: Serial.println("PUZZLE 3 COMPLETE"); break;
    case ALL_PUZZLES_COMPLETE: Serial.println("ALL COMPLETE"); break;
  }
  
  Serial.print("Video State: ");
  switch (currentVideoState) {
    case VIDEO_IDLE: Serial.println("IDLE"); break;
    case VIDEO_STARTING: Serial.println("STARTING"); break;
    case VIDEO_PLAYING: Serial.println("PLAYING"); break;
    case VIDEO_PAUSED: Serial.println("PAUSED"); break;
    case VIDEO_COMPLETED: Serial.println("COMPLETED"); break;
    case VIDEO_ERROR: Serial.println("ERROR"); break;
  }
  
  Serial.print("Video Progress: ");
  Serial.print(videoProgress * 100, 1);
  Serial.println("%");
  
  Serial.print("Latch: ");
  Serial.println(latchOpen ? "OPEN" : "CLOSED");
  
  Serial.println("\n--- Station States ---");
  for (int i = 1; i <= 2; i++) {
    Serial.print("Station ");
    Serial.print(i);
    Serial.print(": Touch=");
    Serial.print(stationStates[i].touchActive ? "ACTIVE" : "INACTIVE");
    Serial.print(", Maze=");
    Serial.print(stationStates[i].mazeComplete ? "COMPLETE" : "INCOMPLETE");
    Serial.print(", Video=");
    Serial.print(stationStates[i].videoComplete ? "COMPLETE" : "INCOMPLETE");
    Serial.print(" (");
    Serial.print(stationStates[i].videoProgress * 100, 1);
    Serial.print("%), Last seen=");
    Serial.print((millis() - stationStates[i].timestamp) / 1000);
    Serial.println("s ago");
  }
  
  if (IS_MASTER) {
    Serial.print("Current WLED Preset: ");
    Serial.println(currentPreset);
  }
  
  if (wifiConnected) {
    Serial.print("Web Interface: http://");
    Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.println(WEB_SERVER_PORT);
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
  Serial.println("video    - Start video manually");
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
  Serial.println("- Both people touch sensors simultaneously");
  Serial.println("- Hold for " + String(PUZZLE1_SOLVE_TIME / 1000) + " seconds");
  Serial.println("- Latches will open when solved");
  Serial.println("");
  Serial.println("PUZZLE 2: Magnetic Maze");
  Serial.println("- Take magnetic wands from opened latches");
  Serial.println("- Guide metal balls through mazes");
  Serial.println("- Get balls to completion zones");
  Serial.println("- Both mazes must be completed");
  Serial.println("");
  Serial.println("PUZZLE 3: Video Challenge");
  Serial.println("- Videos will start automatically");
  Serial.println("- Watch/interact with videos on tablets");
  Serial.println("- Both videos must be completed");
  Serial.println("- Progress tracked automatically");
  Serial.println("==========================\n");
}