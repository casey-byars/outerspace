# TouchDesigner + Kinect v2 Depth-Based Photo Booth

## Project Overview

This guide will help you create an interactive photo booth using TouchDesigner and a Kinect v2 that captures multiple photos and uses depth information to create composite images where people can appear to stand in front of or behind themselves across up to 4 different photos.

## Hardware Requirements

### Essential Hardware
- **Kinect v2 for Windows** (not Xbox version)
- Windows 10/11 PC with USB 3.0 port
- Dedicated GPU (NVIDIA/AMD recommended for better performance)
- External monitor/display for the photo booth interface
- Optional: External camera flash or LED lighting

### Kinect v2 Setup Notes
- Requires USB 3.0 connection and AC power adapter
- Optimal distance: 1.2-3.5 meters from subjects
- Field of view: ~70° horizontal, ~60° vertical
- Depth range: 0.5-4.5 meters

## Software Requirements

### Installation Order
1. **Windows 10/11** (with latest updates)
2. **Kinect SDK 2.0** - Download from Microsoft
3. **TouchDesigner** (latest version)
4. **Kinect Runtime** or full SDK

### TouchDesigner Components Needed
- Kinect TOP (for color and depth streams)
- Kinect Azure TOP (if using newer Azure Kinect)
- Various image processing TOPs
- UI components for photo booth interface

## System Architecture

### Core Components

#### 1. Depth Capture System
```
Kinect TOP → Depth Stream (512x424, 16-bit)
           → Color Stream (1920x1080, RGB)
           → Player Index (for body detection)
```

#### 2. Photo Capture Workflow
```
Session Start → Countdown → Photo 1 → Process → Photo 2 → Process → 
Photo 3 → Process → Photo 4 → Process → Composite → Save → Display
```

#### 3. Depth-Based Compositing
```
Photo + Depth Data → Depth Threshold Masks → Layer Separation → 
Composite Blending → Final Image
```

## Core Implementation

### 1. Kinect Setup in TouchDesigner

#### Basic Kinect Configuration
```
Kinect TOP Parameters:
- Hardware Version: Version 2
- Image: Color (for photos)
- Camera Resolution: 1920x1080
- Mirror Image: On (for natural interaction)
```

#### Depth Stream Setup
```
Kinect TOP (Depth) Parameters:
- Image: Depth
- Output as 32-bit float for processing
- Range: 0.5-4.0 meters (adjustable)
```

### 2. Depth-Based Masking System

#### Depth Threshold Creation
```
Depth Input → Math TOP → Threshold TOP → Mask Generation
```

**Math TOP Settings:**
- Operation: Subtract
- Value: Reference depth (where person was standing)
- Output: Difference map

**Threshold TOP Settings:**
- Low/High thresholds to create binary masks
- Separate foreground/background layers

#### Multiple Depth Zones
Create 3-4 depth zones for layering:
1. **Near Zone** (0.5-1.5m): Closest to camera
2. **Mid Zone** (1.5-2.5m): Middle distance  
3. **Far Zone** (2.5-3.5m): Furthest from camera
4. **Background** (3.5m+): Static background

### 3. Photo Capture Logic

#### State Machine Implementation
```python
# Python state management
class PhotoBoothState:
    IDLE = 0
    COUNTDOWN = 1
    CAPTURE = 2
    PROCESSING = 3
    COMPOSITE = 4
    DISPLAY = 5
    
current_state = PhotoBoothState.IDLE
photo_count = 0
max_photos = 4
```

#### Capture Sequence
```
For each photo (1-4):
1. Show countdown (3-2-1)
2. Capture color + depth simultaneously
3. Store depth reference for that position
4. Process and store masked versions
5. Move to next photo or compositing
```

### 4. Advanced Depth Processing

#### Depth Refinement Techniques
```
Raw Depth → Bilateral Filter → Median Filter → 
Edge Smoothing → Final Depth Map
```

**Bilateral Filter** (via GLSL TOP):
- Preserves edges while smoothing noise
- Essential for clean depth-based masking

**Median Filter**:
- Removes depth noise and holes
- Use 3x3 or 5x5 kernel

#### Background Subtraction
```
Current Depth - Reference Background → Foreground Mask
```

### 5. Compositing Strategy

#### Layer-Based Compositing
```
Layer 1 (Bottom): Background/Far Zone
Layer 2: Mid-Far Zone  
Layer 3: Mid-Near Zone
Layer 4 (Top): Near Zone
```

#### Blending Methods
- **Hard Masks**: Clean depth cuts (may look artificial)
- **Feathered Masks**: Soft edges (more natural)
- **Alpha Blending**: Gradual transitions between layers

### 6. User Interface Design

#### Photo Booth Interface Elements
```
Main Display:
├── Live Camera Preview
├── Countdown Timer
├── Photo Progress (1/4, 2/4, etc.)
├── Instruction Text
├── Final Composite Display
└── "Take Another" / "Print" buttons
```

#### Control Flow
```
Touch/Button Input → State Change → Visual Feedback → 
Action Execution → Result Display
```

## Advanced Features

### 1. Dynamic Depth Adjustment
```python
# Auto-adjust depth ranges based on scene
def calibrate_depth_ranges():
    # Analyze initial depth frame
    depth_data = op('kinect_depth').numpyArray()
    
    # Find person's position
    person_depth = np.median(depth_data[depth_data > 0])
    
    # Set adaptive thresholds
    near_threshold = person_depth - 0.3
    far_threshold = person_depth + 0.3
    
    return near_threshold, far_threshold
```

### 2. Intelligent Background Removal
```
Player Index Stream → Body Detection → 
Smart Masking → Background Replacement
```

### 3. Motion Detection for Better Timing
```python
# Detect when person is ready for photo
def detect_motion_stability():
    # Compare consecutive depth frames
    motion_threshold = 0.05  # meters
    stable_frames_required = 30  # ~1 second at 30fps
    
    if motion_level < motion_threshold:
        stable_frame_count += 1
    else:
        stable_frame_count = 0
        
    return stable_frame_count >= stable_frames_required
```

### 4. Quality Enhancement

#### Depth Map Refinement
```
GLSL Shader for depth smoothing:
- Joint bilateral filtering
- Edge-preserving smoothing
- Hole filling algorithms
```

#### Color Correction
```
Auto White Balance → Exposure Correction → 
Color Grading → Final Enhancement
```

## Performance Optimization

### 1. Resolution Management
- Use lower resolution for real-time processing
- Full resolution only for final capture
- Efficient memory management

### 2. GPU Acceleration
```
CPU Tasks:
- State management
- User interface
- File I/O

GPU Tasks (TOPs):
- Image processing
- Depth filtering
- Compositing
- Real-time preview
```

### 3. Memory Optimization
```python
# Efficient texture handling
def manage_texture_memory():
    # Release unused textures
    # Use appropriate bit depths
    # Optimize texture formats
    pass
```

## Troubleshooting Common Issues

### 1. Depth Quality Problems
**Issue**: Noisy or incomplete depth data
**Solutions**:
- Improve lighting conditions
- Adjust Kinect positioning
- Use depth filtering
- Check for reflective surfaces

### 2. Edge Artifacts
**Issue**: Hard edges in composites
**Solutions**:
- Implement edge feathering
- Use bilateral filtering
- Apply morphological operations
- Adjust threshold values

### 3. Performance Issues
**Issue**: Low frame rate or lag
**Solutions**:
- Reduce processing resolution
- Optimize shader code
- Use efficient algorithms
- Monitor GPU usage

### 4. Calibration Problems
**Issue**: Misaligned color and depth
**Solutions**:
- Use Kinect's built-in alignment
- Implement manual calibration
- Check camera positioning
- Verify SDK installation

## Implementation Timeline

### Phase 1: Basic Setup (Week 1)
- [ ] Install and configure Kinect v2
- [ ] Set up basic TouchDesigner project
- [ ] Test color and depth streams
- [ ] Create simple UI framework

### Phase 2: Core Functionality (Week 2)
- [ ] Implement photo capture system
- [ ] Create depth-based masking
- [ ] Build state machine logic
- [ ] Test basic compositing

### Phase 3: Advanced Features (Week 3)
- [ ] Enhance depth processing
- [ ] Implement multi-layer compositing
- [ ] Add quality improvements
- [ ] Create polished UI

### Phase 4: Testing & Refinement (Week 4)
- [ ] User testing and feedback
- [ ] Performance optimization
- [ ] Bug fixes and improvements
- [ ] Final deployment setup

## File Structure

```
kinect-photobooth/
├── main.toe                 # Main TouchDesigner project
├── components/
│   ├── kinect_capture.tox   # Kinect interface component
│   ├── depth_processor.tox  # Depth processing logic
│   ├── photo_manager.tox    # Photo capture and storage
│   ├── compositor.tox       # Image compositing system
│   └── ui_controller.tox    # User interface management
├── shaders/
│   ├── depth_filter.glsl    # Custom depth filtering
│   ├── bilateral_filter.glsl # Edge-preserving smoothing
│   └── composite_blend.glsl  # Advanced blending modes
├── assets/
│   ├── backgrounds/         # Background images/videos
│   ├── ui_elements/         # Interface graphics
│   └── sounds/             # Audio feedback
└── output/
    └── photos/             # Captured photo storage
```

## Additional Resources

### Learning Materials
- [TouchDesigner Kinect Tutorials](https://derivative.ca/tags/kinect)
- [Depth Camera Processing Techniques](https://pterneas.com/)
- [Computer Vision Fundamentals](https://opencv.org/)

### Hardware Suppliers
- Microsoft Kinect v2 (check availability)
- Alternative: Azure Kinect or Orbbec cameras
- Professional lighting equipment
- Touch screen displays

### Community Support
- TouchDesigner Forum
- Kinect Developer Community
- Reddit r/TouchDesigner

---

This system creates a unique interactive experience where people can literally "meet themselves" in photos by using depth information to intelligently layer multiple captures. The key innovation is using the Kinect's depth sensing to determine which parts of each photo should be in front or behind, creating impossible but convincing composite images.