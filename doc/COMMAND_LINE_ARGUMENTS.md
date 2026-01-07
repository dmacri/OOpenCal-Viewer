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
Start the visualizer with a specific model. The model name must match a registered model (either built-in or loaded via `--loadModel`).

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

## Examples

### Example 1: Load configuration and start with specific model
```bash
./OOpenCal-Viewer config.txt --startingModel=Ball
```
This loads the configuration file and automatically selects the "Ball" model.

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
- Header: `utilities/CommandLineParser.h`
- Implementation: `utilities/CommandLineParser.cpp`

The parsed arguments are applied in `MainWindow::applyCommandLineOptions()` method in `mainwindow.cpp`.

## Notes

- All paths should be absolute or relative to the current working directory.
- Model names are case-sensitive and must match exactly as registered in the application.
- When using `--startingModel`, the model must be loaded before the configuration file is processed. The data will be reloaded with the new model.
- Image format is automatically determined by the file extension (e.g., `.png`, `.jpg`, `.bmp`).
- Video generation uses 1 FPS by default for testing purposes.
