# TouchDesigner Project Template: Kinect Photo Booth

## Project Network Structure

### Main Project Layout

```
/project
├── /kinect_input
│   ├── kinect_color (Kinect TOP)
│   ├── kinect_depth (Kinect TOP)
│   └── kinect_player_index (Kinect TOP)
├── /depth_processing
│   ├── depth_filter (GLSL TOP)
│   ├── depth_threshold (Math TOP)
│   └── mask_generator (Threshold TOP)
├── /photo_capture
│   ├── capture_trigger (Button COMP)
│   ├── photo_storage (Movie File Out TOP)
│   └── countdown_timer (Timer CHOP)
├── /compositing
│   ├── layer_compositor (Composite TOP)
│   ├── depth_masks (Multiple TOPs)
│   └── final_blend (Over TOP)
├── /ui_interface
│   ├── main_display (Container COMP)
│   ├── countdown_display (Text TOP)
│   └── control_buttons (Button COMPs)
└── /state_management
    ├── photo_booth_logic (DAT Execute)
    ├── session_state (Storage DAT)
    └── output_manager (Python Script)
```

## Detailed Operator Configurations

### 1. Kinect Input Setup

#### kinect_color (Kinect TOP)
```
Parameters:
- Hardware Version: Version 2
- Image: Color
- Camera Resolution: 1920x1080
- Mirror Image: On
- Active: On
```

#### kinect_depth (Kinect TOP)
```
Parameters:
- Hardware Version: Version 2
- Image: Depth
- Mirror Image: On
- Pixel Format: 32-bit float (RGBA)
- Active: On
```

#### kinect_player_index (Kinect TOP)
```
Parameters:
- Hardware Version: Version 2
- Image: Player Index
- Mirror Image: On
- Active: On
```

### 2. Depth Processing Network

#### depth_filter (GLSL TOP)
```glsl
// Bilateral Filter Shader
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D sDepthTex;
uniform float uSigmaSpace;
uniform float uSigmaColor;
uniform int uKernelSize;

void main() {
    vec2 texelSize = 1.0 / textureSize(sDepthTex, 0);
    float centerDepth = texture(sDepthTex, vUV).r;
    
    if (centerDepth == 0.0) {
        fragColor = vec4(0.0);
        return;
    }
    
    float totalWeight = 0.0;
    float filteredDepth = 0.0;
    
    int halfKernel = uKernelSize / 2;
    
    for (int x = -halfKernel; x <= halfKernel; x++) {
        for (int y = -halfKernel; y <= halfKernel; y++) {
            vec2 samplePos = vUV + vec2(x, y) * texelSize;
            float sampleDepth = texture(sDepthTex, samplePos).r;
            
            if (sampleDepth > 0.0) {
                float spatialWeight = exp(-0.5 * (x*x + y*y) / (uSigmaSpace * uSigmaSpace));
                float colorWeight = exp(-0.5 * pow(centerDepth - sampleDepth, 2.0) / (uSigmaColor * uSigmaColor));
                float weight = spatialWeight * colorWeight;
                
                filteredDepth += sampleDepth * weight;
                totalWeight += weight;
            }
        }
    }
    
    if (totalWeight > 0.0) {
        filteredDepth /= totalWeight;
    } else {
        filteredDepth = centerDepth;
    }
    
    fragColor = vec4(filteredDepth, filteredDepth, filteredDepth, 1.0);
}
```

Parameters:
```
- Input: kinect_depth
- Pixel Format: 32-bit float (RGBA)
- Custom uniforms:
  - uSigmaSpace: 2.0
  - uSigmaColor: 0.1
  - uKernelSize: 5
```

#### depth_threshold (Math TOP)
```
Parameters:
- Input A: depth_filter output
- Operation: Subtract
- Post-Op: Clamp
- Clamp Min: 0.0
- Clamp Max: 1.0
- Value B: Depth reference value (set dynamically)
```

#### mask_generator (Threshold TOP)
```
Parameters:
- Input: depth_threshold output
- Threshold Low: -0.3
- Threshold High: 0.3
- Output: Binary mask
```

### 3. Photo Capture System

#### State Management (Python DAT Execute)
```python
import os
import datetime

class PhotoBoothState:
    def __init__(self):
        self.state = 'IDLE'
        self.photo_count = 0
        self.max_photos = 4
        self.photos = []
        self.depth_references = []
        self.session_id = None
        
    def start_session(self):
        self.session_id = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.photo_count = 0
        self.photos = []
        self.depth_references = []
        self.state = 'COUNTDOWN'
        
        # Create session directory
        session_path = f"output/photos/{self.session_id}"
        os.makedirs(session_path, exist_ok=True)
        
        return session_path
        
    def capture_photo(self):
        if self.photo_count < self.max_photos:
            # Get current frame data
            color_frame = op('kinect_color')
            depth_frame = op('depth_filter')
            
            # Store photo and depth reference
            photo_filename = f"photo_{self.photo_count + 1}.jpg"
            depth_filename = f"depth_{self.photo_count + 1}.exr"
            
            # Save files (implement file saving logic)
            self.save_photo(color_frame, photo_filename)
            self.save_depth(depth_frame, depth_filename)
            
            # Store reference depth for masking
            person_depth = self.get_person_depth(depth_frame)
            self.depth_references.append(person_depth)
            
            self.photo_count += 1
            
            if self.photo_count >= self.max_photos:
                self.state = 'COMPOSITE'
            else:
                self.state = 'COUNTDOWN'
                
    def get_person_depth(self, depth_frame):
        # Extract typical person depth from player index
        player_mask = op('kinect_player_index').numpyArray()
        depth_data = depth_frame.numpyArray()
        
        # Find median depth of tracked player
        person_pixels = depth_data[player_mask > 0]
        if len(person_pixels) > 0:
            return float(np.median(person_pixels))
        else:
            return 2.0  # Default depth
            
    def save_photo(self, frame, filename):
        # Implement photo saving logic
        pass
        
    def save_depth(self, frame, filename):
        # Implement depth saving logic
        pass

# Global state instance
if not hasattr(parent(), 'photobooth_state'):
    parent().photobooth_state = PhotoBoothState()

state = parent().photobooth_state

def onFrameStart(frame):
    # Update UI based on current state
    op('state_display').par.text = f"State: {state.state}"
    op('photo_counter').par.text = f"Photo: {state.photo_count}/{state.max_photos}"

def onPulse(par):
    if par.name == 'start_button':
        state.start_session()
    elif par.name == 'capture_trigger':
        state.capture_photo()
```

### 4. Compositing System

#### Depth-Based Layer Creation
```python
# depth_layer_generator (Python Script)
def create_depth_layers():
    """Create multiple depth-based layers for compositing"""
    
    photos = parent().photobooth_state.photos
    depth_refs = parent().photobooth_state.depth_references
    
    if len(photos) < 4:
        return
        
    # Sort photos by depth (nearest to farthest)
    photo_depth_pairs = list(zip(photos, depth_refs))
    sorted_pairs = sorted(photo_depth_pairs, key=lambda x: x[1])
    
    # Create depth masks for each layer
    layers = []
    
    for i, (photo, depth_ref) in enumerate(sorted_pairs):
        # Create mask for this depth layer
        mask = create_depth_mask(depth_ref, tolerance=0.3)
        
        # Apply mask to photo
        masked_photo = apply_mask(photo, mask)
        layers.append(masked_photo)
        
    return layers

def create_depth_mask(target_depth, tolerance=0.3):
    """Create binary mask for specific depth range"""
    
    depth_data = op('depth_filter').numpyArray()
    
    # Create mask for depth range
    near_thresh = target_depth - tolerance
    far_thresh = target_depth + tolerance
    
    mask = np.logical_and(
        depth_data >= near_thresh,
        depth_data <= far_thresh
    )
    
    return mask.astype(np.float32)
```

#### layer_compositor (Composite TOP Chain)
```
Layer 1 (Background): Static background or farthest photo
│
├─ Over TOP ← Layer 2 (Far-Mid): Second farthest photo with mask
│
├─ Over TOP ← Layer 3 (Mid-Near): Second nearest photo with mask  
│
└─ Over TOP ← Layer 4 (Foreground): Nearest photo with mask
```

### 5. User Interface Components

#### main_display (Container COMP)
```
Layout:
┌─────────────────────────────────────────┐
│  Live Preview (Kinect Color Feed)       │
│                                         │
│  ┌─────────────┐  ┌─────────────────┐  │
│  │ Photo 1/4   │  │  Countdown: 3   │  │
│  │ [Captured]  │  │                 │  │
│  └─────────────┘  └─────────────────┘  │
│                                         │
│  ┌─────────────────────────────────────┐│
│  │        Instructions                 ││
│  │   "Stand in different positions"    ││
│  └─────────────────────────────────────┘│
│                                         │
│  [START] [RETAKE] [SAVE] [PRINT]       │
└─────────────────────────────────────────┘
```

#### Countdown System (Timer CHOP + Text TOP)
```python
# countdown_logic (DAT Execute)
def start_countdown():
    """Start 3-second countdown with visual feedback"""
    
    timer_chop = op('countdown_timer')
    timer_chop.par.start.pulse()
    
    # Visual countdown updates
    for i in range(3, 0, -1):
        op('countdown_text').par.text = str(i)
        # Schedule next update
        run(f"op('countdown_text').par.text = '{i-1}'", delayFrames=30)
        
    # Trigger capture after countdown
    run("op('capture_trigger').par.execute.pulse()", delayFrames=90)
```

### 6. Advanced Features Implementation

#### Motion Stability Detection
```python
# motion_detector (Python Script)
class MotionDetector:
    def __init__(self):
        self.previous_depth = None
        self.stable_frames = 0
        self.required_stability = 30  # frames
        self.motion_threshold = 0.05  # meters
        
    def check_stability(self):
        current_depth = op('depth_filter').numpyArray()
        
        if self.previous_depth is not None:
            # Calculate motion between frames
            diff = np.abs(current_depth - self.previous_depth)
            motion_level = np.mean(diff[diff > 0])
            
            if motion_level < self.motion_threshold:
                self.stable_frames += 1
            else:
                self.stable_frames = 0
                
            # Update stability indicator
            stability_ratio = self.stable_frames / self.required_stability
            op('stability_indicator').par.text = f"Stability: {stability_ratio:.1%}"
            
            # Auto-trigger if stable enough
            if self.stable_frames >= self.required_stability:
                if parent().photobooth_state.state == 'COUNTDOWN':
                    parent().photobooth_state.capture_photo()
                    self.stable_frames = 0
                    
        self.previous_depth = current_depth.copy()
        
# Global motion detector
if not hasattr(parent(), 'motion_detector'):
    parent().motion_detector = MotionDetector()

parent().motion_detector.check_stability()
```

#### Dynamic Background Replacement
```glsl
// background_replace.glsl
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D sColorTex;
uniform sampler2D sPlayerIndexTex;
uniform sampler2D sBackgroundTex;
uniform float uChromaKey;

void main() {
    vec4 color = texture(sColorTex, vUV);
    float playerIndex = texture(sPlayerIndexTex, vUV).r;
    vec4 background = texture(sBackgroundTex, vUV);
    
    // If no player detected, use background
    if (playerIndex < 0.01) {
        fragColor = background;
    } else {
        fragColor = color;
    }
}
```

## Project Execution Flow

### 1. Initialization
```python
# On project start
def initialize_project():
    # Check Kinect connection
    if not op('kinect_color').par.active:
        print("ERROR: Kinect not detected")
        return False
        
    # Initialize state
    parent().photobooth_state = PhotoBoothState()
    parent().motion_detector = MotionDetector()
    
    # Set up UI
    op('main_display').par.display = True
    
    return True
```

### 2. Session Flow
```
1. User presses START
2. System calibrates depth ranges
3. Countdown begins (3-2-1)
4. Photo 1 captured with depth reference
5. User moves to new position
6. Motion stability detection
7. Countdown and Photo 2 capture
8. Repeat for Photos 3 and 4
9. Automatic compositing begins
10. Final image displayed
11. Save/Print options presented
```

### 3. Cleanup and Reset
```python
def reset_session():
    """Clean up current session and prepare for next"""
    
    state = parent().photobooth_state
    state.state = 'IDLE'
    state.photo_count = 0
    state.photos.clear()
    state.depth_references.clear()
    
    # Clear UI displays
    op('countdown_text').par.text = ""
    op('photo_counter').par.text = "Ready"
    
    # Reset timers
    op('countdown_timer').par.initialize.pulse()
```

## Performance Optimization Settings

### TOP Common Parameters
```
All processing TOPs:
- Use Global Res Multiplier: On
- Resolution: Custom (reduce for real-time processing)
- Pixel Format: Appropriate bit depth
- Viewer Smoothness: Linear
```

### GPU Memory Management
```python
# memory_manager (Python Script)
def optimize_memory():
    """Manage GPU memory usage"""
    
    # Release unused textures
    for op_name in ['temp_tex1', 'temp_tex2', 'cache_tex']:
        if op(op_name) and not op(op_name).inputs:
            op(op_name).destroy()
            
    # Force garbage collection
    import gc
    gc.collect()
```

This template provides a complete starting point for your Kinect photo booth project in TouchDesigner. The modular structure allows you to build and test each component independently while maintaining clean data flow throughout the system.