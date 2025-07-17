# ESP32 Multi-Station WLED Touch Controller Setup Guide

## Overview
This system creates a cooperative puzzle where 3 people must simultaneously touch their sensors for a set duration to advance WLED lighting presets. The stations communicate via ESP-NOW for low latency coordination.

## Hardware Requirements

### Per Station:
- **ESP32 DevKit** (any variant)
- **Touch Sensor** (capacitive or resistive)
- **External LED** (optional, for better visual feedback)
- **Resistors** (if needed for LED current limiting)
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

### LED Connections:
```
ESP32 GPIO2 → Built-in LED (status)
ESP32 GPIO5 → External LED + (through 220Ω resistor)
GND → External LED -
```

## Software Setup

### 1. Install Required Libraries
In Arduino IDE, install these libraries:
- **ESP32** board package
- **ArduinoJson** by Benoit Blanchon
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

#### Station 3 (Slave):
```cpp
#define STATION_ID 3
```

### 3. Get MAC Addresses

1. Upload the code to each ESP32
2. Open Serial Monitor for each station
3. Note the MAC address printed during startup
4. Update the MAC address arrays in the code:

```cpp
uint8_t station1_mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // Station 1 MAC
uint8_t station2_mac[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}; // Station 2 MAC
uint8_t station3_mac[] = {0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC}; // Station 3 MAC
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
- Preset 1: Solid Red
- Preset 2: Rainbow Chase
- Preset 3: Blue Breathe
- Preset 4: Multi-color Strobe
- Preset 5: Fire Effect
- etc.

## System Configuration

### Timing Settings (adjustable in code):
```cpp
#define PUZZLE_SOLVE_TIME 2000          // Hold time: 2 seconds
#define TOUCH_DEBOUNCE_TIME 100         // Debounce: 100ms
#define HEARTBEAT_INTERVAL 1000         // Communication: 1 second
#define TIMEOUT_THRESHOLD 3000          // Station timeout: 3 seconds
```

### LED Patterns:
- **Slow Blink**: Station idle
- **Solid**: Touch sensor active
- **Fast Blink**: Puzzle in progress
- **Rapid Flash**: Puzzle solved (celebration)

## Testing Procedure

### 1. Individual Station Test
1. Power on one station
2. Check Serial Monitor output
3. Verify WiFi connection
4. Test touch sensor response
5. Confirm LED indicators work

### 2. Communication Test
1. Power on all three stations
2. Use Serial command `status` to check station states
3. Use Serial command `test` to verify ESP-NOW communication
4. Touch sensors and verify other stations receive updates

### 3. WLED Integration Test
1. Ensure WLED controller is powered and connected
2. From master station, use `preset X` command
3. Verify WLED responds with correct preset

### 4. Full System Test
1. All stations powered and communicating
2. Have all three people touch their sensors simultaneously
3. Hold for required duration (default 2 seconds)
4. Verify WLED preset advances
5. Verify celebration flash on all stations

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