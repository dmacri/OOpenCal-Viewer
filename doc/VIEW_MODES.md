# View Modes: 2D and 3D

## Overview

The OOpenCal-Viewer now supports switching between **2D** and **3D** view modes, allowing you to visualize your OpenCAL simulation data from different perspectives.

## Features

### 2D Mode (Default)
- **Top-down orthographic view** - Camera positioned directly above the scene
- **Rotation disabled** - Prevents accidental camera rotation
- **Zoom enabled** - You can still zoom in/out using mouse wheel
- **Pan enabled** - Click and drag to pan the view
- **2D Ruler axes** - Frame around the scene showing:
  - **X axis ruler** - Horizontal scale with tick marks and labels
  - **Y axis ruler** - Vertical scale with tick marks and labels
  - **Grid lines** - Optional grid overlay for precise measurements
  - Positioned on outer edges, doesn't obscure the scene
- Best for analyzing 2D cellular automata simulations

### 3D Mode
- **Full 3D perspective view** - Camera can be positioned anywhere in 3D space
- **Mouse rotation** - Click and drag to rotate the camera around the scene
- **Camera controls** - Precise control via sliders:
  - **Azimuth** (-180° to +180°) - Rotation around the Z axis (horizontal rotation)
  - **Elevation** (-90° to +90°) - Rotation around the X axis (vertical tilt)
- **Interactive manipulation** - Use mouse to freely explore the 3D scene
- **Orientation axes widget** - Small 3D axes indicator in the bottom-left corner showing:
  - **Red axis** = X direction
  - **Green axis** = Y direction
  - **Blue axis** = Z direction
  - Rotates with the camera to show current viewing orientation
- Useful for 3D models and examining data from different angles

## How to Use

### Switching Between Modes

1. **Via Menu**:
   - Go to **View** → **2D Mode** or **3D Mode**
   - The current mode is indicated by a checkmark

2. **Camera Controls** (3D Mode only):
   - When in 3D mode, camera control sliders appear above the playback controls
   - Adjust **Azimuth** to rotate horizontally around the scene
   - Adjust **Elevation** to tilt the camera up or down
   - Use the spinboxes for precise angle input

### Tips

- **2D Mode**: Best for most OpenCAL 2D simulations (default behavior)
- **3D Mode**: Useful when you want to:
  - View the simulation from an angle
  - Examine height/depth information (if available in 3D models)
  - Create presentation screenshots from different perspectives
  
- **Mouse Controls in 3D Mode**:
  - **Left-click + drag**: Rotate camera
  - **Middle-click + drag**: Pan view
  - **Right-click + drag**: Zoom
  - **Scroll wheel**: Zoom in/out

## Technical Details

### Camera Behavior

- **2D Mode**: Uses `vtkInteractorStyleImage` which restricts camera movement to 2D plane
- **3D Mode**: Uses `vtkInteractorStyleTrackballCamera` which allows full 3D manipulation

### Compatibility

- View modes work with all model types loaded via plugins or model directories.
- Switching modes does **not** reload data files
- View mode preference is **not** saved between sessions (always starts in 2D)

## Future Enhancements

When 3D model data becomes available, the following features could be added:
- Automatic detection of 2D vs 3D data
- Depth/height visualization
- Isosurface rendering
- Volume rendering for 3D cellular automata

## Troubleshooting

**Q: The 3D controls don't appear**  
A: Make sure you've selected **View → 3D Mode** from the menu

**Q: The camera is in a strange position**  
A: Switch back to 2D mode, then to 3D mode again to reset the camera

**Q: Can I save my camera position?**  
A: Currently, camera positions are not saved. This feature may be added in the future.
