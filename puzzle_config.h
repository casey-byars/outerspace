/*
 * Puzzle Configuration Header
 * 
 * This file contains all configurable parameters for the ESP32 Puzzle Solver
 * Modify these values to customize puzzle behavior and sensor thresholds
 */

#ifndef PUZZLE_CONFIG_H
#define PUZZLE_CONFIG_H

// Timing Configuration (in milliseconds)
#define DEFAULT_PUZZLE_TIMEOUT 300000    // 5 minutes
#define SENSOR_READ_INTERVAL 100         // Read sensors every 100ms
#define COLOR_DEBOUNCE_TIME 1000         // Wait 1 second between color readings
#define CLAP_DEBOUNCE_TIME 500           // Wait 500ms between clap detections
#define STABLE_HOLD_TIME 3000            // Hold stable condition for 3 seconds
#define TEMP_STABLE_TIME 5000            // Temperature must be stable for 5 seconds
#define PATTERN_STEP_TIME 2000           // Each light pattern step is 2 seconds
#define GESTURE_TIMEOUT 5000             // Gesture must complete within 5 seconds
#define CLAP_PATTERN_TIMEOUT 6000        // Sound pattern timeout

// Sensor Thresholds
#define LIGHT_THRESHOLD 500              // Threshold for light/dark detection
#define SOUND_THRESHOLD 2000             // Threshold for sound detection
#define SHAKE_THRESHOLD 2.0              // Acceleration threshold for shake detection
#define STILL_THRESHOLD 1.2              // Acceleration threshold for stillness
#define DISTANCE_TOLERANCE 2.0           // Distance tolerance in cm
#define TEMPERATURE_TOLERANCE 1.0        // Temperature tolerance in Celsius

// Target Values
#define TARGET_DISTANCE 15.0             // Target distance in cm
#define TARGET_TEMPERATURE 25.0          // Target temperature in Celsius

// Color Detection Thresholds (adjust based on your TCS3200 sensor)
#define COLOR_BLACK_THRESHOLD 50         // Below this value is considered black
#define COLOR_WHITE_THRESHOLD 200        // Above this value is considered white
#define COLOR_YELLOW_RED_THRESHOLD 100   // For yellow detection (red and green low)
#define COLOR_YELLOW_GREEN_THRESHOLD 100

// LED Configuration
#define LED_BRIGHTNESS 50                // LED strip brightness (0-255)
#define CELEBRATION_HUE_STEP 5           // Step size for rainbow celebration effect
#define CELEBRATION_DELAY 50             // Delay between rainbow steps

// Audio Configuration
#define SUCCESS_TONE_FREQ 1000           // Success sound frequency
#define SUCCESS_TONE_DURATION 200        // Success sound duration
#define ERROR_TONE_FREQ 200              // Error sound frequency
#define ERROR_TONE_DURATION 500          // Error sound duration
#define CLAP_ACK_FREQ 800                // Clap acknowledgment frequency
#define CLAP_ACK_DURATION 100            // Clap acknowledgment duration
#define GESTURE_ACK_FREQ 1000            // Gesture acknowledgment frequency
#define GESTURE_ACK_DURATION 200         // Gesture acknowledgment duration

// Celebration melody (frequency, duration pairs)
#define CELEBRATION_NOTE_C 523
#define CELEBRATION_NOTE_E 659
#define CELEBRATION_NOTE_G 784
#define CELEBRATION_NOTE_C_HIGH 1047
#define CELEBRATION_NOTE_DURATION_SHORT 200
#define CELEBRATION_NOTE_DURATION_LONG 400

// Color Sequence Puzzle Configuration
const String DEFAULT_COLOR_SEQUENCE[] = {"RED", "BLUE", "GREEN", "YELLOW"};
#define COLOR_SEQUENCE_LENGTH 4

// Sound Pattern Configuration
#define REQUIRED_CLAP_COUNT 3
#define MIN_CLAP_SEQUENCE_TIME 2000      // Minimum time for clap sequence
#define MAX_CLAP_SEQUENCE_TIME 4000      // Maximum time for clap sequence

// Light Pattern Configuration
#define LIGHT_PATTERN_STEPS 4            // Number of steps in light pattern
// Pattern: Dark -> Light -> Dark -> Light

// Multi-Sensor Combo Configuration
#define COMBO_REQUIRED_COLOR "GREEN"     // Required color for combo puzzle
#define COMBO_HOLD_TIME 3000             // Time to hold all conditions

// Display Configuration
#define DISPLAY_UPDATE_INTERVAL 100      // Update display every 100ms
#define WELCOME_SCREEN_DURATION 2000     // Show welcome screen for 2 seconds
#define PUZZLE_START_DISPLAY_TIME 1000   // Show puzzle start screen for 1 second
#define CELEBRATION_BLINK_DURATION 500   // Celebration blink duration

// WiFi Configuration (optional - for future expansion)
#define WIFI_TIMEOUT 10000               // WiFi connection timeout
#define WIFI_RETRY_INTERVAL 5000         // Retry WiFi connection every 5 seconds

// Puzzle Difficulty Levels
enum DifficultyLevel {
  EASY = 0,
  MEDIUM = 1,
  HARD = 2,
  EXPERT = 3
};

// Difficulty-based timeout adjustments
const unsigned long DIFFICULTY_TIMEOUTS[] = {
  600000,  // EASY: 10 minutes
  300000,  // MEDIUM: 5 minutes
  180000,  // HARD: 3 minutes
  120000   // EXPERT: 2 minutes
};

// Difficulty-based tolerance adjustments
const float DIFFICULTY_DISTANCE_TOLERANCE[] = {
  3.0,     // EASY: ±3cm
  2.0,     // MEDIUM: ±2cm
  1.0,     // HARD: ±1cm
  0.5      // EXPERT: ±0.5cm
};

const float DIFFICULTY_TEMP_TOLERANCE[] = {
  2.0,     // EASY: ±2°C
  1.0,     // MEDIUM: ±1°C
  0.5,     // HARD: ±0.5°C
  0.2      // EXPERT: ±0.2°C
};

// Advanced Color Sequences by Difficulty
const String EASY_COLOR_SEQUENCE[] = {"RED", "BLUE"};
const String MEDIUM_COLOR_SEQUENCE[] = {"RED", "BLUE", "GREEN"};
const String HARD_COLOR_SEQUENCE[] = {"RED", "BLUE", "GREEN", "YELLOW"};
const String EXPERT_COLOR_SEQUENCE[] = {"RED", "BLUE", "GREEN", "YELLOW", "RED", "GREEN"};

const int COLOR_SEQUENCE_LENGTHS[] = {2, 3, 4, 6};

// Gesture complexity by difficulty
const float DIFFICULTY_SHAKE_THRESHOLDS[] = {
  1.5,     // EASY: Lower threshold
  2.0,     // MEDIUM: Normal threshold
  2.5,     // HARD: Higher threshold
  3.0      // EXPERT: Very high threshold
};

// Sound pattern complexity by difficulty
const int DIFFICULTY_CLAP_COUNTS[] = {
  2,       // EASY: 2 claps
  3,       // MEDIUM: 3 claps
  4,       // HARD: 4 claps
  5        // EXPERT: 5 claps
};

// Puzzle type weights for random selection
const float PUZZLE_TYPE_WEIGHTS[] = {
  1.0,     // COLOR_SEQUENCE
  0.8,     // DISTANCE_CALIBRATION
  0.6,     // LIGHT_PATTERN
  0.7,     // MOTION_GESTURE
  0.5,     // TEMPERATURE_TARGET
  0.6,     // SOUND_PATTERN
  0.3      // MULTI_SENSOR_COMBO (harder)
};

// Debug Configuration
#define DEBUG_MODE true                  // Enable/disable debug output
#define DEBUG_SENSOR_OUTPUT false        // Enable/disable sensor debug output
#define DEBUG_PUZZLE_LOGIC false         // Enable/disable puzzle logic debug

// Pin Validation Ranges
#define MIN_ANALOG_PIN 32
#define MAX_ANALOG_PIN 39
#define MIN_DIGITAL_PIN 0
#define MAX_DIGITAL_PIN 39

// Error Codes
#define ERROR_SENSOR_INIT_FAILED -1
#define ERROR_DISPLAY_INIT_FAILED -2
#define ERROR_WIFI_FAILED -3
#define ERROR_INVALID_PUZZLE_TYPE -4
#define ERROR_SENSOR_READ_FAILED -5

// Success Codes
#define SUCCESS_PUZZLE_SOLVED 1
#define SUCCESS_SYSTEM_READY 0

#endif // PUZZLE_CONFIG_H