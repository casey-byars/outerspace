# Quick Start Setup Guide: Kinect Photo Booth

## Prerequisites Checklist

### Hardware Setup
- [ ] **Kinect v2 for Windows** connected via USB 3.0
- [ ] **AC Power Adapter** plugged in (green LED should be on)
- [ ] **Windows 10/11 PC** with dedicated GPU
- [ ] **External Monitor** for photo booth display
- [ ] **Adequate Space** (3-4 meters in front of Kinect)

### Software Installation
- [ ] **Kinect SDK 2.0** downloaded and installed
- [ ] **TouchDesigner** (latest version) installed
- [ ] **GPU Drivers** updated

## Step-by-Step Implementation

### Phase 1: Basic Kinect Testing (30 minutes)

#### 1. Verify Kinect Connection
1. Open **Kinect Studio v2.0** (comes with SDK)
2. Check if Kinect is detected
3. Test color and depth streams
4. Verify body tracking works

#### 2. Create TouchDesigner Project
1. Open TouchDesigner
2. Create new project
3. Add three **Kinect TOP** operators:
   ```
   kinect_color: Hardware Version 2, Image: Color
   kinect_depth: Hardware Version 2, Image: Depth  
   kinect_player: Hardware Version 2, Image: Player Index
   ```
4. Set all to **Mirror Image: On**
5. Test live feeds

#### 3. Basic UI Setup
1. Add **Container COMP** for main display
2. Add **Button COMP** for start trigger
3. Create simple layout with live preview

### Phase 2: Depth Processing (45 minutes)

#### 1. Add Depth Filtering
1. Create **GLSL TOP** named `depth_filter`
2. Copy bilateral filter shader from template
3. Connect `kinect_depth` to input
4. Test filtered depth output

#### 2. Create Depth Masking
1. Add **Math TOP** for depth thresholding
2. Add **Threshold TOP** for binary masks
3. Test manual depth range selection

#### 3. Player Detection Integration
1. Use **Player Index** stream for body detection
2. Combine with depth data for smart masking
3. Test person detection accuracy

### Phase 3: Photo Capture System (60 minutes)

#### 1. State Management Setup
1. Create **DAT Execute** for photo booth logic
2. Copy PhotoBoothState class from template
3. Initialize global state variables
4. Test state transitions

#### 2. Countdown System
1. Add **Timer CHOP** for countdown timing
2. Create **Text TOP** for countdown display
3. Implement countdown logic with visual feedback
4. Test timing accuracy

#### 3. Photo Storage
1. Create **Movie File Out TOP** for saving photos
2. Set up file naming system (session ID + photo number)
3. Implement depth data saving (.EXR format)
4. Test file output functionality

### Phase 4: Compositing System (90 minutes)

#### 1. Multi-Layer Setup
1. Create four **Cache TOPs** for storing photos
2. Set up depth reference storage system
3. Implement depth-based layer sorting
4. Test photo storage and retrieval

#### 2. Mask Generation
1. Create depth masks for each photo
2. Implement tolerance-based depth ranges
3. Add edge smoothing for natural blending
4. Test mask quality

#### 3. Layer Compositing
1. Set up **Composite TOP** chain
2. Implement depth-based layer ordering
3. Add blending modes for smooth transitions
4. Test final composite output

### Phase 5: User Interface Polish (45 minutes)

#### 1. Visual Feedback
1. Add progress indicators (Photo 1/4, etc.)
2. Create instruction text system
3. Implement visual countdown effects
4. Add state-based UI updates

#### 2. Control Integration
1. Connect buttons to state machine
2. Add keyboard shortcuts for testing
3. Implement error handling
4. Test user interaction flow

#### 3. Final Output Display
1. Create final image display system
2. Add save/print button functionality
3. Implement session reset capability
4. Test complete user experience

## Testing Protocol

### 1. Hardware Validation
```bash
# Test checklist:
□ Kinect LED is green
□ Color stream displays correctly
□ Depth stream shows depth data
□ Player index detects person
□ No USB/power issues
```

### 2. Depth Processing Test
```bash
# Validation steps:
□ Depth filtering reduces noise
□ Thresholding creates clean masks
□ Person detection is accurate
□ Edge quality is acceptable
```

### 3. Photo Capture Test
```bash
# Session test:
□ Countdown displays correctly
□ Photos capture at right time
□ Depth data saves properly
□ State transitions work
□ File naming is correct
```

### 4. Compositing Test
```bash
# Layer test:
□ Masks align with photos
□ Depth sorting works correctly
□ Blending looks natural
□ No obvious artifacts
□ Final image quality good
```

## Common Setup Issues & Solutions

### Issue 1: Kinect Not Detected
**Symptoms**: No video feed, error messages
**Solutions**:
- Check USB 3.0 connection
- Verify AC power adapter
- Reinstall Kinect SDK
- Try different USB port
- Check Windows Device Manager

### Issue 2: Poor Depth Quality
**Symptoms**: Noisy depth data, holes in depth map
**Solutions**:
- Improve lighting conditions
- Remove reflective surfaces
- Adjust Kinect angle/position
- Increase bilateral filter strength
- Use median filtering

### Issue 3: Body Tracking Issues
**Symptoms**: Player index not detecting person
**Solutions**:
- Ensure person is 1.2-3.5m from Kinect
- Check for full body visibility
- Improve contrast with background
- Verify SDK body tracking is enabled
- Test with different poses

### Issue 4: Performance Problems
**Symptoms**: Low frame rate, lag, stuttering
**Solutions**:
- Reduce processing resolution
- Optimize GLSL shaders
- Use GPU acceleration
- Close unnecessary programs
- Monitor GPU/CPU usage

### Issue 5: Compositing Artifacts
**Symptoms**: Hard edges, misaligned layers
**Solutions**:
- Adjust depth tolerance values
- Implement edge feathering
- Use bilateral filtering
- Check depth calibration
- Fine-tune threshold values

## Calibration Tips

### 1. Optimal Kinect Positioning
```
Height: 1.5-2.0 meters above ground
Angle: Slight downward tilt (10-15°)
Distance: 2-4 meters clear space in front
Background: Non-reflective, contrasting with subjects
```

### 2. Depth Range Calibration
```python
# Auto-calibration routine
def calibrate_depth_ranges():
    # Have person stand in each position
    # Record depth values
    # Calculate optimal thresholds
    # Store calibration data
    pass
```

### 3. Lighting Optimization
```
Natural lighting: Best for color quality
Even lighting: Reduces depth noise
Avoid: Direct sunlight, strong backlighting
LED panels: Good for consistent results
```

## Performance Benchmarks

### Target Performance Metrics
```
Frame Rate: 30 FPS (minimum 24 FPS)
Capture Time: <3 seconds per photo
Processing Time: <10 seconds for compositing
Memory Usage: <4GB GPU memory
CPU Usage: <60% during processing
```

### Hardware Recommendations
```
Minimum:
- Intel i5 / AMD Ryzen 5
- 8GB RAM
- GTX 1060 / RX 580
- USB 3.0

Recommended:
- Intel i7 / AMD Ryzen 7
- 16GB RAM
- RTX 3060 / RX 6600 XT
- Multiple USB 3.0 controllers
```

## Deployment Checklist

### Pre-Installation
- [ ] Hardware compatibility verified
- [ ] Software dependencies installed
- [ ] Network/internet access for updates
- [ ] Backup power solution considered
- [ ] Physical installation space prepared

### Installation Day
- [ ] Hardware setup and testing
- [ ] Software installation and configuration
- [ ] Calibration and fine-tuning
- [ ] User acceptance testing
- [ ] Documentation and training provided

### Post-Installation
- [ ] Monitor performance metrics
- [ ] Gather user feedback
- [ ] Plan maintenance schedule
- [ ] Document any issues/solutions
- [ ] Prepare update/upgrade path

## Next Steps & Enhancements

### Phase 6: Advanced Features
- Automatic background replacement
- Real-time preview effects
- Social media integration
- Print queue management
- Analytics and usage tracking

### Phase 7: Professional Features
- Multi-camera synchronization
- Professional lighting integration
- Green screen capabilities
- Video recording mode
- Remote monitoring and control

### Phase 8: Commercial Deployment
- Kiosk mode interface
- Payment integration
- Cloud storage and backup
- Fleet management
- Support and maintenance

---

This quick start guide should get you from zero to a working photo booth system in about 4-5 hours of focused work. Start with Phase 1 to verify your hardware setup, then work through each phase systematically. Don't skip the testing phases - they're crucial for identifying issues early!

The key to success is building incrementally and testing each component thoroughly before moving to the next phase. Good luck with your project!