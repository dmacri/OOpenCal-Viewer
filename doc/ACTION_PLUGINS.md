# Action plugins

Some things only make sense for one kind of model: trajectories of pedestrians of a crowd simulation, a flow through a
door, a conversion of the output to another format... Such code does not belong to the Viewer. An **action plugin** is a
shared library (`.so` / `.dll`) with one or more *actions*. After the library is loaded, its actions appear in the
**Actions** menu of the Viewer (for the models they support) and can also be run from the command line without a display.

Action plugins need nothing but the C++ standard library and one header, [`plugins/ActionApi.h`](../plugins/ActionApi.h):
no Qt, no VTK, no OOpenCAL and not even the `Cell` class of the model. They are loaded like model plugins
(`QLibrary` + agreed functions), so there is nothing new to package into an AppImage.

> The Viewer and the plugin must be built with the same compiler and standard library (as for model plugins), because
> `std::string` and `std::vector` cross the library boundary.

## How an action works

The Viewer drives the action; the plugin only reacts:

| # | The Viewer calls | The plugin |
|---|---|---|
| 1 | `parameters()` | lists its parameters (name, label, description, type, default, required) |
| 2 | `setParameter(name, value)` for each of them | validates and stores the value; answers ok / error + message |
| 3 | `start(context)` | gets the description of the simulation (`context.model()`, `context.steps()`, `context.headerValue()`) and prepares itself |
| 4 | for every step: `beginStep(step)`, `processCell(cell)` for **every** cell, `endStep(step)` | collects what it needs; `cell.cell.substate("name")` gives `Cell::stringEncoding("name")` |
| 5 | `finish(true)` | writes the result and returns a message for the user |
|   | `finish(false)` (cancelled / failed) | drops what it has collected |

* Every function that can fail returns `Result` (ok + message). Nothing has to throw; if a plugin throws anyway,
  the Viewer catches it and reports a failure.
* The Viewer reads the data with a reader of its own, so running an action does not change what the window shows, and the
  plugin never gets the whole history at once (low memory use, progress and cancelling work).
* Steps are given in ascending order. They may be sparse (`visualization_step_rate`), the plugin gets the real step number.
* One action object lives as long as the plugin is loaded and is used for many runs, so `start()` has to reset the state.
* Rows are counted from the top of the picture, as in the Viewer; `layer` is 0 for 2D models.

Parameter types: text, integer, real (a dot is the decimal point regardless of the locale), boolean, path to a file to
write / to read, directory; a parameter with `choices` is shown as a combo box. `parseInteger`, `parseReal` and
`parseBoolean` in the API header help with validation.

## Minimal plugin

```cpp
#include "plugins/ActionApi.h"
using namespace viz::actions;

class CountCells : public IAction
{
public:
    ActionInfo info() const override
    {
        return {.id = "count-cells", .pluginName = "Demo", .title = "Count cells", .category = "Analysis"};
    }
    Result start(IActionContext&) override { count_ = 0; return Result::success(); }
    Result processCell(const CellRef&) override { ++count_; return Result::success(); }
    Result finish(bool completed) override
    {
        return completed ? Result::success(std::to_string(count_) + " cells") : Result::success();
    }

private:
    long long count_ = 0;
};

VIZ_ACTION_PLUGIN(registry)
{
    registry.registerAction(std::make_unique<CountCells>());
}
```

`VIZ_ACTION_PLUGIN` defines the two functions the Viewer looks for: `getActionApiVersion()` (a plugin built for another
version of the API is refused) and `registerActionPlugin(IActionRegistry&)`. A library may additionally export the usual
`registerPlugin()` of a model plugin, so one library can provide a model and its actions.

`ActionInfo::supportedModels` limits the action to the listed models (names as in the **Model** menu; empty = any model).
Do not export `getModelName()` from a library that has no model: the Viewer takes it for a model plugin.

Build (see [`examples/trajectory_export_plugin/CMakeLists.txt`](../examples/trajectory_export_plugin/CMakeLists.txt)):

```bash
cmake -S examples/trajectory_export_plugin -B build_trajectory_plugin
cmake --build build_trajectory_plugin
```

## Using actions

**GUI:** `Model -> Load Plugin...` (or put the library into `./plugins/`, or start with `--loadModel <library>`), open a
simulation, and choose the action in the **Actions** menu. A dialog asks for the parameters (the values are remembered),
then the action runs with a progress bar that can cancel it.

**Command line (no display needed, e.g. on a cluster):**

```bash
OOpenCal-Viewer --listActions --loadModel plugins/libTrajectoryExport.so

OOpenCal-Viewer /path/to/OutputCrowdSimulation --loadModel plugins/libTrajectoryExport.so \
    --runAction=export-trajectories-julich \
    --actionParam=output_file=trajectories.txt --actionParam=cell_size_m=0.4 --actionParam=step_duration_s=0.4
```

Exit code: 0 - done, 1 - the action failed, 2 - wrong command line.

## Example: pedestrian trajectories (Jülich format)

[`examples/trajectory_export_plugin`](../examples/trajectory_export_plugin) exports the trajectories of pedestrians in the
text format of Forschungszentrum Jülich (PeTrack, JuPedSim, PedPy): CRLF line ends, `#` comments, an empty line and
rows `id frame x/m y/m z/m` sorted by id and frame. It works with any model whose cells give the id of a pedestrian in
a substate (`pedestriansIds` of CrowdSimulation; `-1` means an empty cell).

| Parameter | Default | Meaning |
|---|---|---|
| `output_file` | - (required) | file to write |
| `cell_size_m` | 0.4 | side of a cell in metres; x, y = centre of the cell |
| `step_duration_s` | 0.4 | simulated time of a step; the frame rate in the header is 1/step_duration, frame = step number |
| `height_m` | 1.75 | the `z` column |
| `id_substate` | `pedestriansIds` | substate whose encoding is the pedestrian id |
| `id_offset` | 0 | added to every id (PeTrack counts from 1) |
| `flip_y` | true | y grows upwards, as the picture in the Viewer |
| `duplicate_ids` | `keep_first` | what to do when one id is in several cells of a step: keep the first and warn, or `fail` |

The grid has no scale, so `cell_size_m` and `step_duration_s` have to be set by the user to the values of the model.

## Design notes

* The registry (`plugins/ActionRegistry.h`) is handed to the plugin as an argument, so the plugin does not need symbols
  of the application (no `-rdynamic` dependence). Actions are destroyed before their library is unloaded.
* `plugins/ActionRunner.h` runs an action without any widget: it creates a visualizer of its own through
  `SceneWidgetVisualizerFactory`, reads each step and serves the cells. The same code is used by the GUI
  (`widgets/ActionsMenu`) and by `--runAction` (`core/ActionCommand`).
* Reading uses `ISceneWidgetVisualizer::getCellStringEncoding`, so the interface used by compiled model plugins is unchanged.
  For native 3D models other layers are reached through `setNative3DSlice`.
* Speed: every cell of every step costs one virtual call and one `std::string`. That is fine for grids like the crowd
  examples; for very large grids an action that needs only a few cells still has to look at all of them.
