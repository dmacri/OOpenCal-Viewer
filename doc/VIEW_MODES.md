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
  - **Labels stay readable at any zoom** - When the scene is zoomed out the numbers never overlap:
    their font shrinks (down to about half of its normal size), and if that is not enough only
    every 2nd, 3rd, ... label is shown (the tick marks stay). Zooming in restores the full labels.
    Both rulers always use the same label size
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

### Clicking a cell of a native 3D model

A model with more than one Z plane (`number_of_slices` > 1) is drawn as a **volume**. Like in the 2D view,
the *Substate Parameters* panel opens when you **click a cell** of the volume:

- The cell is the **first visible cell along the line of sight** under the cursor, i.e. the one you see
  there. Cells with a transparent value (the lowest value of the colour range) cannot be clicked, and
  neither can the empty planes inserted by the **Z spacing** option.
- The panel header shows all three coordinates, e.g. `Cell: (x=47, y=53, z=59)`, because in the volume
  there is no displayed plane they could refer to. **Plot over time...** plots exactly this cell.
- Hovering the mouse over the volume shows a ToolTip with the coordinates and the values of the cell.
  No ToolTip is shown while a mouse button is held (rotating, panning, zooming).
- Because the left button rotates the volume, the cell is taken when the button is **released**, and only
  if the mouse has not been dragged (more than a few pixels) in between. A click on the background hides
  the panel, unless it was opened from the menu (see below).
- In a cross-section of a native 3D model the click works as before (the panel shows the position in the
  displayed plane).

### Substates in the volume of a native 3D model

Every cell of a native 3D model already has its own place in 3D and its own value of every substate, so
"Use as 3D altitude" (a height, as in a 2D model) makes no sense there. In a native 3D model the checkbox of
a substate in the *Substate Parameters* panel is **Show in 3D volume** instead (it is enabled once both Min
and Max are set):

- A checked substate is drawn **in the volume**: every cell with a value is a voxel, coloured by the
  **intensity** of the value, i.e. by where it lies between Min and Max, from the Min colour to the Max
  colour of the substate (the *Colors* buttons). Without colours of its own a substate is coloured like the
  model colours its lowest and highest value (including the transparency of those colours), or - when the
  model does not tell them apart - by a default gradient which differs from substate to substate.
- A cell is drawn when **Min < value <= Max** (and the value is not the *No value* of the substate); otherwise
  it has no value and is transparent. Min is the background, Max itself is still drawn.
- **Several substates** can be checked, all of them are drawn. Where more than one has a value in the same
  cell, only the **highest one in the panel is shown** - drag the substates to change their order. A cell
  where the highest substate has no value shows the next one.
- Without any substate checked the volume is drawn as before (by the first substate used for coloring, or by
  the default cell value). **Deactivate Substate** unchecks them all.
- Changing Min, Max, the colours or the order redraws the volume. The substates shown in the volume do not
  affect a cross-section, which is coloured with *Use substate for coloring*.
- Clicking or hovering a voxel shows the values of all substates of that cell (see above), also of those
  which are not shown.
- **Opening the panel without a cell to click:** until a substate is chosen the volume can be completely
  transparent, so there may be nothing to click. Use **View -> Substate parameters**: it shows the panel
  (with the substates and their Min/Max/colours) without selecting a cell. The panel opened this way stays
  open when the background of the scene is clicked and is closed from the same menu item or with its close
  button.

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
