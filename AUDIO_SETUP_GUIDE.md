# Audio Setup Guide for ESP32 Puzzle System

## Overview
This guide covers setting up comprehensive audio functionality for your ESP32 puzzle system. The system supports multiple audio methods:

1. **ESP32 Buzzer Audio** - Sound effects and simple melodies using built-in buzzer
2. **DFPlayer Mini** - High-quality MP3 playback for background music and complex sounds
3. **Tablet Audio** - Web-based audio synchronized with the ESP32 stations
4. **Combined System** - Using all three methods together for rich audio experience

## Hardware Requirements

### Option 1: Basic Audio (Buzzer Only)
- **ESP32 DevKit** (already included)
- **Buzzer** - 5V active buzzer (GPIO19)
- **Resistor** - 220Ω (if buzzer doesn't have built-in resistor)

### Option 2: Enhanced Audio (DFPlayer Mini)
- **DFPlayer Mini MP3 Player Module** (~$2-5)
- **MicroSD Card** (Class 10, 32GB max)
- **Speaker** - 4Ω 3W (or 8Ω 5W)
- **Jumper Wires**
- **Breadboard or PCB**

### Option 3: Premium Audio (Tablet-based)
- **iPad/Android Tablet** per station
- **Portable Bluetooth Speaker** (optional)
- **WiFi Network** for ESP32-tablet communication

## Wiring Diagrams

### Basic Buzzer Setup
```
ESP32        Buzzer
GPIO19   →   Positive (+)
GND      →   Negative (-)
```

### DFPlayer Mini Setup
```
ESP32         DFPlayer Mini
3.3V      →   VCC
GND       →   GND
GPIO17    →   RX
GPIO16    →   TX
           
DFPlayer Mini    Speaker
SPK_1        →   Positive (+)
SPK_2        →   Negative (-)

DFPlayer Mini    MicroSD
SD Slot      →   Insert formatted SD card
```

### Complete Wiring Diagram
```
ESP32 DevKit C
┌─────────────────┐
│ 3V3         VIN │
│ GND         GND │──┐
│ GPIO4    GPIO13 │  │
│ GPIO2    GPIO12 │  │
│ GPIO15   GPIO14 │  │
│ GPIO16   GPIO27 │──┼── DFPlayer TX
│ GPIO17   GPIO26 │──┼── DFPlayer RX  
│ GPIO5    GPIO25 │  │
│ GPIO18   GPIO33 │  │
│ GPIO19   GPIO32 │──┼── Buzzer +
│ GPIO21   GPIO35 │  │
│ GPIO22   GPIO34 │  │
│ GPIO23     EN   │  │
└─────────────────┘  │
                     │
      ┌──────────────┼── GND
      │              │
   Buzzer         DFPlayer Mini
   ┌────┐         ┌──────────┐
   │ +  │         │ VCC  GND │
   │ -  │         │ RX   TX  │
   └────┘         │ SPK1 SPK2│
                  └──────────┘
                       │   │
                   Speaker │
                   ┌───────┼──┐
                   │ +     -  │
                   └──────────┘
```

## Software Setup

### 1. ESP32 Code Configuration

Enable DFPlayer Mini support in `esp32_wled_dual_station.ino`:

```cpp
// Uncomment these lines if DFPlayer Mini is connected:
#include <DFRobotDFPlayerMini.h>

// In setup() function, uncomment:
if (DFPLAYER_RX_PIN != -1 && DFPLAYER_TX_PIN != -1) {
  Serial2.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (!myDFPlayer.begin(Serial2)) {
    Serial.println("DFPlayer Mini failed to initialize.");
  } else {
    Serial.println("DFPlayer Mini initialized successfully!");
    myDFPlayer.volume(20); // Set volume (0-30)
  }
}

// In audio functions, uncomment DFPlayer commands:
void playBackgroundMusic(int trackNumber) {
  if (myDFPlayer.isPlaying()) {
    myDFPlayer.stop();
    delay(100);
  }
  myDFPlayer.play(trackNumber);
}
```

### 2. Audio File Preparation

#### DFPlayer Mini SD Card Structure
```
SD Card Root/
├── 0001.mp3  # Track 1: Ambient/waiting music
├── 0002.mp3  # Track 2: Puzzle 1 music  
├── 0003.mp3  # Track 3: Puzzle 2 music
├── 0004.mp3  # Track 4: Puzzle 3/video music
├── 0005.mp3  # Track 5: Victory/completion music
├── 0006.mp3  # Track 6: Error/failure sound
├── 0007.mp3  # Track 7: Success fanfare
└── 0008.mp3  # Track 8: Final celebration
```

**Important Notes:**
- Files MUST be named 0001.mp3, 0002.mp3, etc.
- Use MP3 format, 44.1kHz sample rate recommended
- Files should be 32kbps-320kbps bitrate
- Maximum file size: ~100MB per file
- SD card must be FAT32 formatted

#### Tablet Audio Structure
```
tablet_audio/
├── audio/
│   ├── ambient.mp3       # Background ambient music
│   ├── puzzle1.mp3       # Puzzle 1 music
│   ├── puzzle2.mp3       # Puzzle 2 music  
│   ├── puzzle3.mp3       # Puzzle 3/video music
│   ├── complete.mp3      # Puzzle completion music
│   ├── victory.mp3       # Final victory music
│   └── effects/
│       ├── start.mp3     # Video start sound
│       ├── success.mp3   # Success sound
│       ├── complete.mp3  # Completion sound
│       └── notification.mp3 # Notification sound
└── video/
    ├── puzzle3_station1.mp4  # Video for station 1
    └── puzzle3_station2.mp4  # Video for station 2
```

### 3. Audio Track Recommendations

#### Track 1: Ambient/Waiting Music
- **Style**: Mysterious, atmospheric
- **Length**: 2-5 minutes (loops)
- **Volume**: Quiet background level
- **Examples**: Soft synth pads, nature sounds, ambient drones

#### Track 2: Puzzle 1 Music  
- **Style**: Building tension, cooperative energy
- **Length**: 1-3 minutes
- **Volume**: Medium level
- **Examples**: Rhythmic percussion, teamwork themes

#### Track 3: Puzzle 2 Music
- **Style**: Focused concentration, mechanical
- **Length**: 3-5 minutes (maze solving time)
- **Volume**: Medium level  
- **Examples**: Clock ticking, gear sounds, puzzle-solving music

#### Track 4: Puzzle 3/Video Music
- **Style**: Cinematic, important information
- **Length**: Matches video length
- **Volume**: Lower (don't compete with video)
- **Examples**: Documentary style, subtle background score

#### Track 5: Victory Music
- **Style**: Triumphant celebration
- **Length**: 30 seconds - 2 minutes
- **Volume**: High energy
- **Examples**: Fanfares, epic orchestral, celebration music

## Audio Control Features

### ESP32 Station Features
- **Startup Sound** - 3-tone ascending melody when system initializes
- **Touch Feedback** - Short beep when sensors are touched
- **Puzzle Progress** - Different sounds for each puzzle completion
- **Maze Feedback** - Ball detection and completion sounds
- **Latch Sounds** - Audio feedback when magnetic latches open/close
- **Final Celebration** - Epic victory melody sequence

### Tablet Features
- **Volume Control** - Master volume slider for all audio
- **Background Music** - Continuous ambient tracks during puzzles
- **Sound Effects** - Notification and interaction sounds
- **Video Sync** - Audio coordinated with video playback
- **Status Audio** - Different music for each puzzle state

### Synchronized Features
- **State Transitions** - Audio changes when puzzles advance
- **Cross-Platform** - ESP32 controls tablet audio via HTTP
- **Real-time Updates** - Volume and track changes sync across devices
- **Fallback Support** - Graceful degradation if connections fail

## Setup Instructions

### Step 1: Hardware Assembly
1. Connect buzzer to GPIO19 with 220Ω resistor
2. If using DFPlayer Mini:
   - Connect VCC to 3.3V, GND to GND
   - Connect RX to GPIO17, TX to GPIO16
   - Connect speaker to SPK1/SPK2
   - Insert prepared SD card

### Step 2: Audio File Preparation
1. Create audio files according to specifications above
2. For DFPlayer: Copy to SD card with exact naming convention
3. For tablets: Set up web server or local storage with audio files

### Step 3: Software Configuration
1. Update ESP32 code with DFPlayer support (if using)
2. Set audio file paths in tablet HTML app
3. Test audio initialization in setup() function
4. Verify audio commands work via Serial monitor

### Step 4: Volume Calibration
1. Set DFPlayer volume: `myDFPlayer.volume(20)` (0-30 range)
2. Adjust buzzer volume via PWM if needed
3. Set tablet master volume to comfortable level
4. Test all audio levels during actual puzzle solving

### Step 5: Testing
1. **Startup Test**: Verify startup sound plays correctly
2. **Touch Test**: Check touch feedback sounds
3. **Puzzle Test**: Test music changes between puzzle states
4. **Sync Test**: Verify ESP32 can control tablet audio
5. **Completion Test**: Test final celebration audio sequence

## Troubleshooting

### DFPlayer Mini Issues
- **No Sound**: Check wiring, SD card format, file naming
- **Poor Quality**: Verify MP3 bitrate, check speaker connections
- **Playback Errors**: Ensure files are numbered correctly (0001.mp3)
- **Initialization Fail**: Check Serial2 baud rate (9600), verify connections

### Tablet Audio Issues
- **Web Audio Blocked**: Requires user interaction on mobile devices
- **HTTP Audio Fails**: Check CORS settings, verify ESP32 IP address
- **Volume Problems**: Check master volume, individual track levels
- **Sync Issues**: Verify network connectivity, check HTTP endpoints

### General Audio Issues
- **Buzzer Silent**: Check GPIO19 connection, verify code compilation
- **Audio Lag**: Reduce audio file sizes, check processing delays
- **Memory Issues**: Use PROGMEM for large audio arrays if needed
- **Power Problems**: Ensure adequate power supply for all components

## Audio File Sources

### Free Resources
- **Freesound.org** - Creative Commons audio clips
- **Zapsplat.com** - Free tier with registration
- **YouTube Audio Library** - Free music and sound effects
- **BBC Sound Effects** - Large collection of free sounds

### Recommended Tools
- **Audacity** - Free audio editing software
- **FFmpeg** - Command-line audio conversion
- **Online Audio Converter** - Web-based MP3 conversion
- **Music Loops Generator** - For seamless background tracks

### Audio Specifications
- **Format**: MP3 for compatibility
- **Sample Rate**: 44.1kHz recommended
- **Bit Rate**: 128kbps for speech, 192kbps+ for music
- **Channels**: Mono acceptable, stereo preferred
- **Normalization**: -6dB peak maximum to prevent clipping

## Advanced Features

### Dynamic Volume Control
```cpp
void adjustVolumeByPuzzleState() {
  switch(currentPuzzleState) {
    case PUZZLE1_ACTIVE:
      setAudioVolume(25); // Higher for action
      break;
    case PUZZLE2_ACTIVE:
      setAudioVolume(15); // Lower for concentration
      break;
    case PUZZLE3_ACTIVE:
      setAudioVolume(10); // Quiet during video
      break;
  }
}
```

### Audio Triggers
```cpp
void triggerContextualAudio() {
  if (timeRemaining < 60) {
    playBackgroundMusic(6); // Urgency track
  } else if (hintsUsed > 3) {
    playBackgroundMusic(7); // Helpful encouragement
  }
}
```

### Tablet Audio API
```javascript
// Advanced tablet audio control
audioManager.playSequence([
  { track: 'notification', volume: 0.8 },
  { delay: 1000 },
  { track: 'puzzle2', volume: 0.6, fadeIn: 2000 }
]);
```

## Cost Breakdown

### Budget Setup ($5-15)
- ESP32 buzzer only
- Basic sound effects
- Tablet-based background music

### Standard Setup ($15-35)
- ESP32 buzzer + DFPlayer Mini
- SD card + small speaker
- Full audio feature set

### Premium Setup ($50-100)
- DFPlayer Mini + quality speakers
- Professional audio files
- Tablets with Bluetooth speakers
- Custom audio production

## Maintenance

### Regular Tasks
- **Check Audio Files**: Verify SD card integrity monthly
- **Update Tracks**: Refresh music to maintain interest
- **Volume Calibration**: Adjust for different environments
- **Battery Monitoring**: Check power levels affect audio quality

### Troubleshooting Checklist
- [ ] All connections secure
- [ ] SD card properly inserted and formatted
- [ ] Audio files properly named and formatted
- [ ] Volume levels appropriate for environment
- [ ] Network connectivity for tablet sync
- [ ] No electromagnetic interference affecting audio

This audio system will create an immersive and engaging experience for your puzzle participants, with rich soundscapes that enhance the cooperative gaming experience!