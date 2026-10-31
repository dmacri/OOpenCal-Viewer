# Command-Line Arguments

The Visualizer application supports various command-line arguments for automated testing, batch processing, and customized startup behavior.

## Usage

```bash
./OOpenCal-Viewer [CONFIG_FILE] [OPTIONS]
```

## Positional Arguments

### `CONFIG_FILE` (optional)
Path to a configuration file to load at startup. If provided, the visualizer will load this configuration and display the VTK widget.

**Example:**
```bash
./OOpenCal-Viewer /path/to/config.txt
```

## Optional Arguments

### `--loadModel=<PATH>` (repeatable)
Load a custom model plugin from the specified path. This argument can be repeated multiple times to load multiple plugins.

**Example:**
```bash
./OOpenCal-Viewer --loadModel=/path/to/plugin1.so --loadModel=/path/to/plugin2.so
```

### `--startingModel=<NAME>`
Start the visualizer with a specific model. The model name must match a registered model (loaded via `--loadModel` or loaded from a model directory).

**Example:**
```bash
./OOpenCal-Viewer config.txt --startingModel=MyCustomModel
```

**Note:** When this option is used, the specified model will be selected in the GUI menu automatically.

### `--step=<NUMBER>`
Jump directly to a specific simulation step. The step number must be valid for the loaded configuration.

**Example:**
```bash
./OOpenCal-Viewer config.txt --step=100
```

### `--generateMoviePath=<PATH>`
Generate a video by running through all simulation steps. This is useful for automated testing and batch processing. The video will be saved to the specified path in OGG format.

**Example:**
```bash
./OOpenCal-Viewer config.txt --generateMoviePath=/tmp/output.ogv
```

**Note:** When this option is used, the GUI window is not displayed.

### `--generateImagePath=<PATH>`
Generate an image of the current step and save it to the specified path. The image format is determined by the file extension (PNG, JPG, BMP, etc.).

**Example:**
```bash
./OOpenCal-Viewer config.txt --step=50 --generateImagePath=/tmp/step50.png
```

**Note:** When this option is used, the GUI window is not displayed.

### `--exitAfterLastStep`
Exit the application after processing the last step. This is particularly useful when combined with `--generateMoviePath` or `--generateImagePath` for automated batch processing.

**Example:**
```bash
./OOpenCal-Viewer config.txt --generateMoviePath=/tmp/movie.ogv --exitAfterLastStep
```

### `--autoPlay`
Start the playback automatically as soon as the simulation data is loaded, as if the Play button was pressed. Together with
`--exitAfterLastStep` the application exits when the last step has been played, which makes it usable for performance tests
without any interaction. Use `--step=<NUMBER>` to start the playback from a chosen step.

**Example:**
```bash
./OOpenCal-Viewer /path/to/model/directory --step=100 --autoPlay --exitAfterLastStep
```

**Note:** The playback is driven by a timer, so the delay between the steps set in the GUI is part of the total time.
For measuring the speed of reading and rendering use `--benchmark` instead.

### `--renderingMode=<points|cells|texture>`
Chooses how the grid of the flat (2D) view is drawn. The same choice is available in the **Settings → Rendering** menu, and the menu
shows the mode given here. Without this option `points` is used.

| Mode | Description |
|------|-------------|
| `points` | One vertex per cell. The colours of neighbouring cells are blended, so the picture looks blurred. |
| `cells` | One quad per cell. Sharp cells, but slower and it needs more memory for large grids. |
| `texture` | One pixel per cell in a texture drawn on a single quad. Sharp cells and the fastest way to draw large grids. A grid with more than 8192 rows or columns does not fit in a texture and is drawn as `cells`. |

**Example:**
```bash
./OOpenCal-Viewer /path/to/model/directory --renderingMode=texture
```

### `--benchmark`
Measures how fast the steps are shown, prints the result and exits with code 0 (1 when there is nothing to measure). The steps
from `--step=<NUMBER>` (the first step by default) to the last one are shown one after another, **synchronously and without
any delay**: no playback timer, no waiting for the vertical blank of the monitor (swap interval 0) and the time of drawing
includes waiting until the GPU has finished. So to measure e.g. the last 100 steps, give `--step` a number 100 steps before
the end. `--benchmark` replaces `--autoPlay`, `--generateImagePath` and `--generateMoviePath`.

The time of every step is split into phases, because a different `--renderingMode` changes only some of them:

| Phase | What is measured |
|-------|------------------|
| `read` | Reading the files of the step and parsing them into the cells. |
| `coloring (CPU)` | Calculating the colour of every cell and writing it into the lookup table (`points`, `cells`) or into the pixels of the texture (`texture`). |
| `VTK update` | The rest of updating the VTK scene after the colours changed. |
| `render (GPU finished)` | Drawing the scene, including waiting for the GPU. |

The first step is a warm-up (the OpenGL objects are created, the texture is uploaded) and is not counted in the summary.
Besides the lines of the steps, the output has a table with the median, mean, minimum and maximum of every phase, the grid size,
the rendering mode that was really used, the name of the OpenGL renderer and, on Linux, the peak memory of the process.

**Example** (compare the three modes; run each one in a separate process, so that the memory used by the previous one does not matter):
```bash
for mode in points cells texture; do
    ./OOpenCal-Viewer /path/to/model/directory --step=900 --renderingMode=$mode --benchmark > benchmark_$mode.txt
done
```

**Notes:**
- The window is shown during the measurement and has to stay uncovered and visible.
- The drivers can force vsync regardless of the application. Then switch it off in the driver, e.g. `vblank_mode=0` for Mesa
  or `__GL_SYNC_TO_VBLANK=0` for NVIDIA, and check that `total` of a step is not about 16.7 ms (the 60 Hz period) for small grids.
- `coloring` is measured only in the flat (2D) view; in the other views it is 0 and its time is included in `VTK update`.
- Software rendering (e.g. `llvmpipe` in the name of the OpenGL renderer) says a lot about the CPU work and nothing about the GPU.

### `--buildPrecompiledHeader[=<DIR>]`

Builds the precompiled header which speeds up on-the-fly compilation of models and exits. It does not start the GUI and needs
no display. `<DIR>` defaults to the `OOPENCAL_PRECOMPILED_HEADER_DIR` setting. Exit code: 0 on success, 1 if building failed,
2 if no directory is known. See [PRECOMPILED_HEADER.md](PRECOMPILED_HEADER.md).

## Examples

### Example 1: Load configuration and start with specific model
```bash
./OOpenCal-Viewer config.txt --startingModel=MyCustomModel
```
This loads the configuration file and automatically selects the "MyCustomModel" model.

### Example 2: Generate a movie for testing
```bash
./OOpenCal-Viewer config.txt --generateMoviePath=/tmp/test_output.ogv --exitAfterLastStep
```
This runs through all steps, generates a video, and exits automatically.

### Example 3: Load custom plugins and start with one
```bash
./OOpenCal-Viewer config.txt \
  --loadModel=/path/to/custom_plugin1.so \
  --loadModel=/path/to/custom_plugin2.so \
  --startingModel=CustomPlugin1
```
This loads two custom plugins and starts with the first one.

### Example 4: Jump to specific step and generate image
```bash
./OOpenCal-Viewer config.txt --step=75 --generateImagePath=/tmp/step75.png --exitAfterLastStep
```
This loads the configuration, jumps to step 75, generates an image, and exits.

## Current Behavior

- **Without arguments:** The application starts with an empty GUI (no VTK widget displayed) until a configuration file is loaded via the File menu.
- **With config file only:** The application loads the configuration and displays the VTK widget with the first step.
- **With additional options:** The application applies the specified options after loading the configuration.

## Implementation Details

Command-line arguments are parsed using the **argparse** library (https://github.com/p-ranav/argparse), which is automatically downloaded via CMake's FetchContent mechanism.

The parsing is handled by the `CommandLineParser` class located in:
- Header: `core/CommandLineParser.h`
- Implementation: `core/CommandLineParser.cpp`

The parsed arguments are applied in `MainWindow::applyCommandLineOptions()` method in `mainwindow.cpp`.

## Notes

- All paths should be absolute or relative to the current working directory.
- Model names are case-sensitive and must match exactly as registered in the application.
- When using `--startingModel`, the model must be loaded before the configuration file is processed. The data will be reloaded with the new model.
- Image format is automatically determined by the file extension (e.g., `.png`, `.jpg`, `.bmp`).
- Video generation uses 1 FPS by default for testing purposes.
