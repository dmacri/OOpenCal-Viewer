# Plot of a cell over the whole simulation

Clicking a cell shows its substate values for the **current** step in the *Substate Parameters* panel.
The **Plot over time...** button under the cell coordinates shows how the substates of that cell change
over **all** simulation steps.

## How to use it

1. Open a model and click a cell (in the 2D view, in the 3D view of a 2D model, in a cross-section of
   a 3D model, or in the complete volume of a native 3D model).
2. In the *Substate Parameters* panel press **Plot over time...**.
3. A window opens with the plot of one substate versus the step number:
   - choose the substate in the **Substate** combo box (the one highlighted in the panel is preselected),
   - the red dashed line marks the step currently shown in the scene and follows it,
   - hovering the mouse over a point shows its step and value,
   - the mouse wheel zooms the plot and dragging with the left button moves it (the usual VTK chart
     interaction),
   - **Export CSV...** saves the values of *all* substates of the cell (`step,<substate1>,<substate2>,...`).
4. While the window is open it follows the clicked cell: click another cell and the plot is refreshed.

The window shows the coordinates of the cell in the whole grid, e.g. `Cell (x=50, y=49, z=48)`. For a 3D
model they differ from the coordinates in the panel when a cross-section is displayed, because the panel
shows the position in the displayed plane.

## What is plotted

* One point per **available step**, i.e. per step written to the output files. If the model writes the
  output every N steps, the plot has a point every N steps.
* Only substates whose text value is a number can be plotted. A substate which is not a number, or a step
  whose files cannot be read, leaves a gap in the line. The summary under the plot says how many steps
  have a value (`9 of 9 steps, min 0, max 0.92`).
* Both `.` and `,` are understood as the decimal separator, whatever the locale of the process.

## How the data are read

Only the record of the clicked cell is read from every step, not the whole grid:

* **binary** output: a single `sizeof(Cell)` record is read from the file with a seek;
* **text** output: the lines before the cell's row are skipped and one line is parsed.

The node which contains the cell is found separately for each step, because the size of the nodes may
change between steps. A progress dialog with a **Cancel** button is shown when the reading takes more
than a moment (for text output of a big model with many steps it can take a while). A cancelled reading
shows the steps read so far.

## For developers

| Part | File |
|------|------|
| Reading one cell at many steps | `ModelReader::readCellAcrossSteps()` in `data/ModelReader.hpp` |
| Result type, number parsing, CSV | `visualiserProxy/CellHistory.h` |
| Cell of a cross-section -> cell of the grid | `gridIndexFromSliceView()` in `visualiserProxy/ContiguousGrid.h` |
| Entry point for the UI | `ISceneWidgetVisualizer::cellHistory()` (displayed plane), `cellHistoryOfVolumeCell()` (cell clicked in the 3D volume) |
| Cell of the 3D volume under the cursor | `visualiser/VolumePicking.h`, `ISceneWidgetVisualizer::pickNative3DVolumeCell()` |
| Plot widget (`vtkChartXY`, VTK modules `ChartsCore` and `ViewsContext2D`) | `widgets/TimeSeriesPlot.*` |
| Window | `widgets/CellHistoryDialog.*` |
| Button and wiring | `widgets/SubstatesDockWidget.*` |
| Tests | `tests/CellHistoryTests.cpp`, `tests/VolumePickingTests.cpp` |
