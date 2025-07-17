/*
 * ESP32 Multi-Sensor Puzzle Solver
 * 
 * This system uses multiple sensors to solve different types of puzzles:
 * - Color sequence puzzles (using color sensor)
 * - Distance/proximity puzzles (using ultrasonic sensor)
 * - Light pattern puzzles (using photoresistor)
 * - Motion/gesture puzzles (using accelerometer/gyroscope)
 * - Temperature puzzles (using temperature sensor)
 * - Sound pattern puzzles (using microphone)
 * 
 * Hardware Requirements:
 * - ESP32 DevKit
 * - TCS3200/TCS34725 Color Sensor
 * - HC-SR04 Ultrasonic Sensor
 * - MPU6050 Accelerometer/Gyroscope
 * - DS18B20 Temperature Sensor
 * - Photoresistor (LDR)
 * - Microphone Module
 * - RGB LED Strip (WS2812B)
 * - Buzzer
 * - OLED Display (SSD1306)
 */

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MPU6050.h>
#include <ArduinoJson.h>

// Pin Definitions
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 18
#define PHOTORESISTOR_PIN 34
#define MICROPHONE_PIN 35
#define TEMP_SENSOR_PIN 4
#define BUZZER_PIN 19
#define LED_STRIP_PIN 21
#define LED_COUNT 16

// Color Sensor Pins (TCS3200)
#define COLOR_S0 26
#define COLOR_S1 27
#define COLOR_S2 14
#define COLOR_S3 12
#define COLOR_OUT 13

// Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// System Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
CRGB leds[LED_COUNT];
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);
MPU6050 mpu;

// Puzzle States
enum PuzzleType {
  COLOR_SEQUENCE,
  DISTANCE_CALIBRATION,
  LIGHT_PATTERN,
  MOTION_GESTURE,
  TEMPERATURE_TARGET,
  SOUND_PATTERN,
  MULTI_SENSOR_COMBO
};

struct PuzzleState {
  PuzzleType currentPuzzle;
  bool isActive;
  bool isSolved;
  int attempts;
  unsigned long startTime;
  unsigned long timeLimit;
};

struct SensorData {
  // Color sensor
  int redValue, greenValue, blueValue;
  String detectedColor;
  
  // Distance sensor
  float distance;
  
  // Light sensor
  int lightLevel;
  
  // Motion sensor
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;
  
  // Temperature sensor
  float temperature;
  
  // Sound sensor
  int soundLevel;
  bool soundDetected;
};

// Global Variables
PuzzleState puzzle;
SensorData sensors;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 100; // Read sensors every 100ms

// Puzzle Solutions Storage
String colorSequenceSolution[] = {"RED", "BLUE", "GREEN", "YELLOW"};
int colorSequenceProgress = 0;
float targetDistance = 15.0; // cm
float targetTemperature = 25.0; // Celsius
int lightThreshold = 500;
bool gestureDetected = false;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Puzzle Solver Initializing...");
  
  initializePins();
  initializeDisplay();
  initializeLEDs();
  initializeSensors();
  
  // Initialize puzzle state
  puzzle.currentPuzzle = COLOR_SEQUENCE;
  puzzle.isActive = false;
  puzzle.isSolved = false;
  puzzle.attempts = 0;
  puzzle.timeLimit = 300000; // 5 minutes
  
  displayWelcomeScreen();
  Serial.println("System Ready!");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Read sensors at regular intervals
  if (currentTime - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readAllSensors();
    lastSensorRead = currentTime;
  }
  
  // Handle puzzle logic
  if (puzzle.isActive && !puzzle.isSolved) {
    handleCurrentPuzzle();
    checkTimeLimit();
  }
  
  // Check for puzzle start/reset commands
  handleSerialCommands();
  
  // Update display
  updateDisplay();
  
  delay(50);
}

void initializePins() {
  // Ultrasonic sensor
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  
  // Color sensor
  pinMode(COLOR_S0, OUTPUT);
  pinMode(COLOR_S1, OUTPUT);
  pinMode(COLOR_S2, OUTPUT);
  pinMode(COLOR_S3, OUTPUT);
  pinMode(COLOR_OUT, INPUT);
  
  // Set color sensor frequency scaling to 20%
  digitalWrite(COLOR_S0, HIGH);
  digitalWrite(COLOR_S1, LOW);
  
  // Other pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PHOTORESISTOR_PIN, INPUT);
  pinMode(MICROPHONE_PIN, INPUT);
}

void initializeDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void initializeLEDs() {
  FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(50);
  FastLED.clear();
  FastLED.show();
}

void initializeSensors() {
  // Initialize temperature sensor
  tempSensor.begin();
  
  // Initialize MPU6050
  Wire.begin();
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
  }
}

void readAllSensors() {
  // Read color sensor
  readColorSensor();
  
  // Read ultrasonic sensor
  sensors.distance = readUltrasonicDistance();
  
  // Read light sensor
  sensors.lightLevel = analogRead(PHOTORESISTOR_PIN);
  
  // Read motion sensor
  readMotionSensor();
  
  // Read temperature sensor
  tempSensor.requestTemperatures();
  sensors.temperature = tempSensor.getTempCByIndex(0);
  
  // Read sound sensor
  sensors.soundLevel = analogRead(MICROPHONE_PIN);
  sensors.soundDetected = sensors.soundLevel > 2000; // Adjust threshold as needed
}

void readColorSensor() {
  // Read Red
  digitalWrite(COLOR_S2, LOW);
  digitalWrite(COLOR_S3, LOW);
  sensors.redValue = pulseIn(COLOR_OUT, LOW);
  
  // Read Green
  digitalWrite(COLOR_S2, HIGH);
  digitalWrite(COLOR_S3, HIGH);
  sensors.greenValue = pulseIn(COLOR_OUT, LOW);
  
  // Read Blue
  digitalWrite(COLOR_S2, LOW);
  digitalWrite(COLOR_S3, HIGH);
  sensors.blueValue = pulseIn(COLOR_OUT, LOW);
  
  // Determine color
  sensors.detectedColor = determineColor(sensors.redValue, sensors.greenValue, sensors.blueValue);
}

String determineColor(int red, int green, int blue) {
  // Simple color detection logic - adjust thresholds based on your sensor
  if (red < green && red < blue) {
    return "RED";
  } else if (green < red && green < blue) {
    return "GREEN";
  } else if (blue < red && blue < green) {
    return "BLUE";
  } else if (red < 50 && green < 50 && blue < 50) {
    return "BLACK";
  } else if (red > 200 && green > 200 && blue > 200) {
    return "WHITE";
  } else if (red < 100 && green < 100) {
    return "YELLOW";
  }
  return "UNKNOWN";
}

float readUltrasonicDistance() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  
  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2; // Convert to cm
  
  return distance;
}

void readMotionSensor() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  sensors.accelX = ax / 16384.0;
  sensors.accelY = ay / 16384.0;
  sensors.accelZ = az / 16384.0;
  sensors.gyroX = gx / 131.0;
  sensors.gyroY = gy / 131.0;
  sensors.gyroZ = gz / 131.0;
}

void handleCurrentPuzzle() {
  switch (puzzle.currentPuzzle) {
    case COLOR_SEQUENCE:
      handleColorSequencePuzzle();
      break;
    case DISTANCE_CALIBRATION:
      handleDistancePuzzle();
      break;
    case LIGHT_PATTERN:
      handleLightPatternPuzzle();
      break;
    case MOTION_GESTURE:
      handleMotionPuzzle();
      break;
    case TEMPERATURE_TARGET:
      handleTemperaturePuzzle();
      break;
    case SOUND_PATTERN:
      handleSoundPatternPuzzle();
      break;
    case MULTI_SENSOR_COMBO:
      handleMultiSensorPuzzle();
      break;
  }
}

void handleColorSequencePuzzle() {
  static unsigned long lastColorTime = 0;
  static String lastColor = "";
  
  if (millis() - lastColorTime > 1000 && sensors.detectedColor != "UNKNOWN" && sensors.detectedColor != lastColor) {
    lastColor = sensors.detectedColor;
    lastColorTime = millis();
    
    if (sensors.detectedColor == colorSequenceSolution[colorSequenceProgress]) {
      colorSequenceProgress++;
      playSuccessSound();
      updateLEDProgress(colorSequenceProgress, 4);
      
      if (colorSequenceProgress >= 4) {
        puzzleSolved();
      }
    } else {
      colorSequenceProgress = 0;
      playErrorSound();
      FastLED.clear();
      FastLED.show();
      puzzle.attempts++;
    }
  }
}

void handleDistancePuzzle() {
  float tolerance = 2.0; // cm
  if (abs(sensors.distance - targetDistance) <= tolerance) {
    static unsigned long stableTime = 0;
    if (stableTime == 0) {
      stableTime = millis();
    } else if (millis() - stableTime > 3000) { // Hold for 3 seconds
      puzzleSolved();
    }
  } else {
    // Reset stable time if not in range
    static unsigned long stableTime = 0;
    stableTime = 0;
  }
  
  // Visual feedback - closer = more LEDs
  int numLEDs = map(constrain(sensors.distance, 5, 50), 5, 50, LED_COUNT, 0);
  FastLED.clear();
  for (int i = 0; i < numLEDs; i++) {
    leds[i] = CRGB::Blue;
  }
  FastLED.show();
}

void handleLightPatternPuzzle() {
  // Pattern: Dark -> Light -> Dark -> Light (each for 2 seconds)
  static int patternStep = 0;
  static unsigned long stepStartTime = 0;
  static bool patternStarted = false;
  
  if (!patternStarted) {
    stepStartTime = millis();
    patternStarted = true;
  }
  
  unsigned long stepDuration = millis() - stepStartTime;
  bool shouldBeDark = (patternStep % 2) == 0;
  bool isCurrentlyDark = sensors.lightLevel < lightThreshold;
  
  if (stepDuration > 2000) { // 2 seconds per step
    if ((shouldBeDark && isCurrentlyDark) || (!shouldBeDark && !isCurrentlyDark)) {
      patternStep++;
      stepStartTime = millis();
      
      if (patternStep >= 4) {
        puzzleSolved();
      }
    } else {
      // Pattern broken, restart
      patternStep = 0;
      stepStartTime = millis();
      puzzle.attempts++;
    }
  }
}

void handleMotionPuzzle() {
  // Detect specific gesture: Shake (high acceleration) then still (low acceleration)
  static bool shakeDetected = false;
  static unsigned long shakeTime = 0;
  
  float totalAccel = sqrt(sensors.accelX * sensors.accelX + 
                         sensors.accelY * sensors.accelY + 
                         sensors.accelZ * sensors.accelZ);
  
  if (totalAccel > 2.0 && !shakeDetected) { // Shake detected
    shakeDetected = true;
    shakeTime = millis();
    playTone(1000, 200);
  } else if (shakeDetected && totalAccel < 1.2) { // Now still
    if (millis() - shakeTime > 1000 && millis() - shakeTime < 5000) {
      puzzleSolved();
    } else if (millis() - shakeTime > 5000) {
      shakeDetected = false; // Reset if too much time passed
    }
  }
}

void handleTemperaturePuzzle() {
  float tolerance = 1.0; // Celsius
  if (abs(sensors.temperature - targetTemperature) <= tolerance) {
    static unsigned long stableTime = 0;
    if (stableTime == 0) {
      stableTime = millis();
    } else if (millis() - stableTime > 5000) { // Hold for 5 seconds
      puzzleSolved();
    }
  } else {
    static unsigned long stableTime = 0;
    stableTime = 0;
  }
  
  // Visual feedback - temperature indicator
  int colorTemp = map(constrain(sensors.temperature, 15, 35), 15, 35, 0, 255);
  fill_solid(leds, LED_COUNT, CHSV(240 - colorTemp, 255, 100)); // Blue to red
  FastLED.show();
}

void handleSoundPatternPuzzle() {
  // Pattern: 3 claps with specific timing
  static int clapCount = 0;
  static unsigned long lastClapTime = 0;
  static unsigned long firstClapTime = 0;
  
  if (sensors.soundDetected) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastClapTime > 500) { // Debounce claps
      clapCount++;
      lastClapTime = currentTime;
      
      if (clapCount == 1) {
        firstClapTime = currentTime;
      }
      
      playTone(800, 100); // Acknowledge clap
      
      if (clapCount == 3) {
        unsigned long totalTime = currentTime - firstClapTime;
        if (totalTime >= 2000 && totalTime <= 4000) { // 2-4 seconds total
          puzzleSolved();
        } else {
          clapCount = 0; // Reset if timing is wrong
          puzzle.attempts++;
        }
      }
    }
  }
  
  // Reset if too much time passed
  if (millis() - lastClapTime > 6000) {
    clapCount = 0;
  }
}

void handleMultiSensorPuzzle() {
  // Combination puzzle: Right color + right distance + right temperature
  bool colorMatch = sensors.detectedColor == "GREEN";
  bool distanceMatch = abs(sensors.distance - targetDistance) <= 2.0;
  bool tempMatch = abs(sensors.temperature - targetTemperature) <= 1.0;
  
  static unsigned long allMatchTime = 0;
  
  if (colorMatch && distanceMatch && tempMatch) {
    if (allMatchTime == 0) {
      allMatchTime = millis();
    } else if (millis() - allMatchTime > 3000) { // Hold all conditions for 3 seconds
      puzzleSolved();
    }
    
    // All conditions met - green LEDs
    fill_solid(leds, LED_COUNT, CRGB::Green);
  } else {
    allMatchTime = 0;
    
    // Partial matches - different colors
    FastLED.clear();
    if (colorMatch) leds[0] = CRGB::Green;
    if (distanceMatch) leds[1] = CRGB::Blue;
    if (tempMatch) leds[2] = CRGB::Red;
  }
  
  FastLED.show();
}

void puzzleSolved() {
  puzzle.isSolved = true;
  puzzle.isActive = false;
  
  // Celebration sequence
  playCelebrationSequence();
  displayCelebration();
  
  Serial.println("Puzzle Solved!");
  Serial.print("Attempts: ");
  Serial.println(puzzle.attempts);
  Serial.print("Time: ");
  Serial.print((millis() - puzzle.startTime) / 1000);
  Serial.println(" seconds");
}

void checkTimeLimit() {
  if (millis() - puzzle.startTime > puzzle.timeLimit) {
    puzzle.isActive = false;
    playFailureSound();
    displayTimeUp();
    Serial.println("Time's up! Puzzle failed.");
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("start")) {
      int puzzleNum = command.substring(6).toInt();
      startPuzzle((PuzzleType)puzzleNum);
    } else if (command == "status") {
      printStatus();
    } else if (command == "sensors") {
      printSensorData();
    } else if (command == "reset") {
      resetPuzzle();
    } else if (command == "help") {
      printHelp();
    }
  }
}

void startPuzzle(PuzzleType type) {
  puzzle.currentPuzzle = type;
  puzzle.isActive = true;
  puzzle.isSolved = false;
  puzzle.attempts = 0;
  puzzle.startTime = millis();
  
  // Reset puzzle-specific variables
  colorSequenceProgress = 0;
  
  FastLED.clear();
  FastLED.show();
  
  Serial.print("Starting puzzle: ");
  Serial.println(getPuzzleName(type));
  displayPuzzleStart(type);
}

void resetPuzzle() {
  puzzle.isActive = false;
  puzzle.isSolved = false;
  puzzle.attempts = 0;
  colorSequenceProgress = 0;
  
  FastLED.clear();
  FastLED.show();
  
  Serial.println("Puzzle reset");
  displayWelcomeScreen();
}

String getPuzzleName(PuzzleType type) {
  switch (type) {
    case COLOR_SEQUENCE: return "Color Sequence";
    case DISTANCE_CALIBRATION: return "Distance Calibration";
    case LIGHT_PATTERN: return "Light Pattern";
    case MOTION_GESTURE: return "Motion Gesture";
    case TEMPERATURE_TARGET: return "Temperature Target";
    case SOUND_PATTERN: return "Sound Pattern";
    case MULTI_SENSOR_COMBO: return "Multi-Sensor Combo";
    default: return "Unknown";
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  
  if (puzzle.isActive) {
    display.println(getPuzzleName(puzzle.currentPuzzle));
    display.print("Attempts: ");
    display.println(puzzle.attempts);
    
    unsigned long elapsed = (millis() - puzzle.startTime) / 1000;
    display.print("Time: ");
    display.print(elapsed);
    display.println("s");
    
    // Show current sensor values
    display.print("Color: ");
    display.println(sensors.detectedColor);
    display.print("Dist: ");
    display.print(sensors.distance, 1);
    display.println("cm");
    display.print("Temp: ");
    display.print(sensors.temperature, 1);
    display.println("C");
  } else if (puzzle.isSolved) {
    display.println("PUZZLE SOLVED!");
    display.print("Time: ");
    display.print((millis() - puzzle.startTime) / 1000);
    display.println("s");
    display.print("Attempts: ");
    display.println(puzzle.attempts);
  } else {
    display.println("ESP32 Puzzle Solver");
    display.println("Ready for puzzle!");
    display.println("Send 'help' for commands");
  }
  
  display.display();
}

void displayWelcomeScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("PUZZLE");
  display.println("SOLVER");
  display.setTextSize(1);
  display.println("Multi-Sensor System");
  display.display();
  delay(2000);
}

void displayPuzzleStart(PuzzleType type) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Starting:");
  display.println(getPuzzleName(type));
  display.display();
  delay(1000);
}

void displayCelebration() {
  for (int i = 0; i < 3; i++) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("SOLVED!");
    display.display();
    delay(500);
    display.clearDisplay();
    display.display();
    delay(500);
  }
}

void displayTimeUp() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("TIME'S");
  display.println("UP!");
  display.display();
}

void updateLEDProgress(int progress, int total) {
  int ledsToLight = map(progress, 0, total, 0, LED_COUNT);
  FastLED.clear();
  for (int i = 0; i < ledsToLight; i++) {
    leds[i] = CRGB::Green;
  }
  FastLED.show();
}

void playCelebrationSequence() {
  // Rainbow LED effect
  for (int hue = 0; hue < 255; hue += 5) {
    fill_rainbow(leds, LED_COUNT, hue, 255 / LED_COUNT);
    FastLED.show();
    delay(50);
  }
  
  // Success sound
  playTone(523, 200); // C
  playTone(659, 200); // E
  playTone(784, 200); // G
  playTone(1047, 400); // C
}

void playSuccessSound() {
  playTone(1000, 200);
}

void playErrorSound() {
  playTone(200, 500);
}

void playFailureSound() {
  for (int i = 0; i < 3; i++) {
    playTone(150, 200);
    delay(100);
  }
}

void playTone(int frequency, int duration) {
  tone(BUZZER_PIN, frequency, duration);
  delay(duration);
  noTone(BUZZER_PIN);
}

void printStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("Current Puzzle: ");
  Serial.println(getPuzzleName(puzzle.currentPuzzle));
  Serial.print("Active: ");
  Serial.println(puzzle.isActive ? "Yes" : "No");
  Serial.print("Solved: ");
  Serial.println(puzzle.isSolved ? "Yes" : "No");
  Serial.print("Attempts: ");
  Serial.println(puzzle.attempts);
  
  if (puzzle.isActive) {
    unsigned long elapsed = (millis() - puzzle.startTime) / 1000;
    Serial.print("Elapsed Time: ");
    Serial.print(elapsed);
    Serial.println(" seconds");
  }
}

void printSensorData() {
  Serial.println("\n=== SENSOR DATA ===");
  Serial.print("Color: ");
  Serial.print(sensors.detectedColor);
  Serial.print(" (R:");
  Serial.print(sensors.redValue);
  Serial.print(" G:");
  Serial.print(sensors.greenValue);
  Serial.print(" B:");
  Serial.print(sensors.blueValue);
  Serial.println(")");
  
  Serial.print("Distance: ");
  Serial.print(sensors.distance);
  Serial.println(" cm");
  
  Serial.print("Light Level: ");
  Serial.println(sensors.lightLevel);
  
  Serial.print("Temperature: ");
  Serial.print(sensors.temperature);
  Serial.println(" °C");
  
  Serial.print("Acceleration: X:");
  Serial.print(sensors.accelX, 2);
  Serial.print(" Y:");
  Serial.print(sensors.accelY, 2);
  Serial.print(" Z:");
  Serial.println(sensors.accelZ, 2);
  
  Serial.print("Sound Level: ");
  Serial.print(sensors.soundLevel);
  Serial.print(" (Detected: ");
  Serial.print(sensors.soundDetected ? "Yes" : "No");
  Serial.println(")");
}

void printHelp() {
  Serial.println("\n=== COMMANDS ===");
  Serial.println("start 0 - Color Sequence Puzzle");
  Serial.println("start 1 - Distance Calibration Puzzle");
  Serial.println("start 2 - Light Pattern Puzzle");
  Serial.println("start 3 - Motion Gesture Puzzle");
  Serial.println("start 4 - Temperature Target Puzzle");
  Serial.println("start 5 - Sound Pattern Puzzle");
  Serial.println("start 6 - Multi-Sensor Combo Puzzle");
  Serial.println("status  - Show current status");
  Serial.println("sensors - Show sensor readings");
  Serial.println("reset   - Reset current puzzle");
  Serial.println("help    - Show this help");
}