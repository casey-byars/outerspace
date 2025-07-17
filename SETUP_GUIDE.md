# ESP32 Dual-Station WLED Touch Controller v3.0 Setup Guide

## Overview
This system creates a three-puzzle cooperative experience where 2 people must work together. Puzzle 1 requires simultaneous touch activation, which opens servo latches containing magnetic wands. Puzzle 2 requires using these wands to guide metal balls through mazes to completion zones detected by hall sensors. Puzzle 3 involves watching videos on tablets with progress tracking.

## Hardware Requirements

### Per Station:
- **ESP32 DevKit** (any variant)
- **Touch Sensor** (capacitive or resistive)
- **Servo Motor** (SG90 or similar for latch mechanism)
- **Hall Sensor** (A3144 or similar for maze completion detection)
- **Buzzer** (optional, for audio feedback)
- **External LED** (optional, for better visual feedback)
- **Magnetic Wand** (neodymium magnet on stick/handle)
- **Metal Ball** (steel ball bearing, ~8-10mm diameter)
- **Maze Structure** (wood/acrylic with channels and completion zone)
- **Latch Mechanism** (servo-controlled door/gate for wand storage)
- **Resistors** (220Ω for LEDs, pull-ups if needed)
- **Breadboard or PCB** for connections
- **Power Supply** (USB or battery pack)

### Shared:
- **WLED Controller** (ESP32/ESP8266 running WLED firmware)
- **LED Strip** (addressable RGB strip like WS2812B)
- **WiFi Network** (for WLED communication)

## Wiring Diagram

### Touch Sensor Connection:
```
ESP32 GPIO4 → Touch Sensor Signal
3.3V → Touch Sensor VCC
GND → Touch Sensor GND
```

### Servo Motor Connection:
```
ESP32 GPIO16 → Servo Signal (Orange/Yellow wire)
5V → Servo VCC (Red wire)
GND → Servo GND (Brown/Black wire)
```

### Hall Sensor Connection:
```
ESP32 GPIO34 → Hall Sensor Signal (analog)
3.3V → Hall Sensor VCC
GND → Hall Sensor GND
```

### LED Connections:
```
ESP32 GPIO2 → Built-in LED (status)
ESP32 GPIO5 → External LED + (through 220Ω resistor)
GND → External LED -
```

### Buzzer Connection (Optional):
```
ESP32 GPIO19 → Buzzer +
GND → Buzzer -
```

## Software Setup

### 1. Install Required Libraries
In Arduino IDE, install these libraries:
- **ESP32** board package
- **ArduinoJson** by Benoit Blanchon
- **ESP32Servo** by Kevin Harrington
- **WiFi** (included with ESP32 package)
- **HTTPClient** (included with ESP32 package)

### 2. Configure Each Station

#### Station 1 (Master):
```cpp
#define STATION_ID 1
```

#### Station 2 (Slave):
```cpp
#define STATION_ID 2
```

### 3. Get MAC Addresses

1. Upload the code to each ESP32
2. Open Serial Monitor for each station
3. Note the MAC address printed during startup
4. Update the MAC address arrays in the code:

```cpp
uint8_t station1_mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // Station 1 MAC
uint8_t station2_mac[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}; // Station 2 MAC
```

### 4. Configure Network Settings

Update these settings in the code:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* wledIP = "192.168.1.100";  // Your WLED controller IP
```

### 5. Configure Touch Sensor

Adjust based on your sensor type:

#### For Digital Touch Sensors:
```cpp
bool touchDetected = (touchValue == HIGH); // or LOW, depending on sensor
```

#### For Capacitive Touch (ESP32 built-in):
```cpp
int touchValue = touchRead(TOUCH_PIN);
bool touchDetected = touchValue < TOUCH_THRESHOLD;
```

## WLED Setup

### 1. Install WLED
- Flash WLED firmware to an ESP32/ESP8266
- Connect to LED strip
- Configure via web interface

### 2. Create Presets
1. Access WLED web interface
2. Go to **Presets** section
3. Create 10 different lighting effects
4. Save as presets 1-10

### Example Presets:
- Preset 0: Soft White Glow (initial state)
- Preset 1: Green Pulse (puzzle 1 complete)
- Preset 2: Blue Fire Effect (puzzle 2 complete)
- Preset 3: Rainbow Chase (puzzle 3 complete)
- Preset 4: Multi-color Strobe
- Preset 5: Purple Breathe
- etc.

## Physical Construction

### Magnetic Maze Design:
1. **Base Material**: 6mm acrylic or wood base (minimum 20cm x 20cm)
2. **Maze Walls**: 3-5mm thick strips creating channels for ball movement
3. **Ball Path**: Channels should be 12-15mm wide for 8-10mm steel balls
4. **Completion Zone**: 25mm diameter circle at maze end with embedded hall sensor
5. **Start Position**: Elevated entry point with ball holder
6. **Magnetic Wand**: Neodymium magnet (10-15mm) on 15-20cm handle

### Servo Latch Mechanism:
1. **Latch Box**: Small compartment (5cm x 3cm x 3cm) for wand storage
2. **Servo Mount**: SG90 servo with arm controlling sliding door/gate
3. **Door Material**: Light plastic or thin wood that servo can easily move
4. **Wand Holder**: Foam insert or clips to secure magnetic wand
5. **Access Opening**: Large enough for easy wand retrieval

### Hall Sensor Placement:
1. **Sensor Position**: Mounted under completion zone, flush with surface
2. **Detection Range**: 5-10mm from metal ball when in zone
3. **Shielding**: Use non-magnetic materials around sensor
4. **Calibration**: Test with actual ball to determine threshold values

## System Configuration

### Timing Settings (adjustable in code):
```cpp
#define PUZZLE1_SOLVE_TIME 2000         // Puzzle 1 hold time: 2 seconds
#define TOUCH_DEBOUNCE_TIME 100         // Touch debounce: 100ms
#define HALL_DEBOUNCE_TIME 500          // Hall sensor debounce: 500ms
#define MAZE_COMPLETION_TIME 1000       // Ball must stay in zone: 1 second
#define HEARTBEAT_INTERVAL 1000         // Communication: 1 second
#define TIMEOUT_THRESHOLD 3000          // Station timeout: 3 seconds
```

### Servo Settings:
```cpp
#define SERVO_CLOSED_ANGLE 0            // Latch closed position
#define SERVO_OPEN_ANGLE 90             // Latch open position
#define SERVO_MOVE_DELAY 1000           // Time for servo movement
```

### Hall Sensor Settings:
```cpp
#define HALL_THRESHOLD 100              // Adjust based on magnet strength
```

### LED Patterns:
- **Slow Blink**: Station idle (puzzle 1 ready)
- **Solid**: Touch sensor active
- **Fast Blink**: Puzzle 1 in progress
- **Status LED Solid**: Puzzle 1 complete (puzzle 2 ready)
- **External LED**: Shows maze completion status
- **Rapid Flash**: Puzzle solved (celebration)

## Testing Procedure

### 1. Individual Station Test
1. Power on one station
2. Check Serial Monitor output
3. Verify WiFi connection
4. Test touch sensor response
5. Confirm LED indicators work

### 2. Communication Test
1. Power on both stations
2. Use Serial command `status` to check station states
3. Use Serial command `test` to verify ESP-NOW communication
4. Touch sensors and verify the other station receives updates

### 3. WLED Integration Test
1. Ensure WLED controller is powered and connected
2. From master station, use `preset X` command
3. Verify WLED responds with correct preset

### 4. Full System Test
1. Both stations powered and communicating
2. Have both people touch their sensors simultaneously
3. Hold for required duration (default 2 seconds)
4. Verify WLED preset advances and latches open
5. Complete mazes to advance to video puzzle
6. Verify videos start and progress is tracked
7. Verify final celebration when all puzzles complete

## Troubleshooting

### Common Issues:

#### "ESP-NOW initialization failed"
- Check if WiFi mode is set to WIFI_AP_STA
- Verify ESP32 board package is up to date

#### "Stations not communicating"
- Verify MAC addresses are correct
- Check if stations are on same WiFi channel
- Ensure all stations have ESP-NOW initialized

#### "WLED not responding"
- Check WLED IP address
- Verify WLED is on same network
- Test WLED web interface manually

#### "Touch sensor not responding"
- Check wiring connections
- Adjust TOUCH_THRESHOLD value
- Verify sensor type (digital vs capacitive)

#### "Puzzle not solving"
- Check if all stations show "ACTIVE" status
- Verify timing requirements are met
- Check for communication timeouts

### Serial Commands:

| Command | Description |
|---------|-------------|
| `status` | Show system status and station states |
| `test` | Send test communication message |
| `mac` | Display station MAC address |
| `preset X` | Set WLED preset (master only) |
| `help` | Show available commands |

## Advanced Configuration

### Custom Puzzle Timing:
```cpp
#define PUZZLE_SOLVE_TIME 5000  // Require 5 seconds hold time
```

### Multiple WLED Controllers:
Each station can control its own WLED by changing:
```cpp
const char* wledIP = "192.168.1.10X";  // Different IP per station
```

### Battery Operation:
- Use ESP32 deep sleep between operations
- Monitor battery level via ADC
- Send low battery warnings

### Range Extension:
- Use ESP-NOW repeater stations
- Implement mesh networking
- Add external antennas

## Safety Notes

- Use appropriate power supplies
- Ensure proper grounding
- Test all connections before powering on
- Monitor for overheating during extended use
- Keep backup configurations

## Support

For issues or modifications:
1. Check serial monitor output for error messages
2. Verify all connections and settings
3. Test components individually before full system test
4. Consider interference from other 2.4GHz devices