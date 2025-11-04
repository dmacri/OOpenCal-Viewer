# Loading Models from Directory

## Overview

The Qt-VTK-viewer now supports loading OOpenCAL models directly from a directory. This feature automatically:

1. Reads the model configuration from `Header.txt`
2. Finds the C++ model source file (`.h`)
3. Checks if a compiled module (`.so`) already exists
4. Automatically compiles the module if needed (using Clang)
5. Loads the compiled module into the application
6. Displays any compilation errors in a user-friendly dialog

## Requirements

### For Using Pre-compiled Models
- A directory containing:
  - `Header.txt` - Configuration file with model metadata
  - `*.h` - C++ header file with the model class

### For Auto-compilation
- `clang++` compiler installed on the system
- The model source file must be compilable as a shared library
- `OOPENCAL_DIR` environment variable set to the OOpenCAL base directory

## Configuration and Setup

### Environment Variables

Set the OOpenCAL base directory for compilation:
```bash
export OOPENCAL_DIR=/path/to/OOpenCAL
```

This allows the compiler to find `Cell.h` and other base headers in `$OOPENCAL_DIR/base/`.

### Programmatic Configuration

You can also pass the OOpenCAL directory when creating a `CppModuleBuilder`:
```cpp
CppModuleBuilder builder("clang++", "/path/to/OOpenCAL");
```

## Header.txt Format

The `Header.txt` file uses a simple key-value format organized by sections:

```ini
GENERAL:
    number_of_columns=500
    number_of_rows=500
    output_file_name=ball

DISTRIBUTED:
    number_node_x=2
    number_node_y=1

VISUALIZATION:
    substates=h,z
    mode=binary
    reduction=sum,min,max
```

### GENERAL Section
- **number_of_columns** (required): Grid width
- **number_of_rows** (required): Grid height
- **number_of_steps** (required): Total simulation steps
- **output_file_name** (required): Base name for output files and compiled module

### DISTRIBUTED Section
- **number_node_x**: Number of nodes in X direction (default: 1)
- **number_node_y**: Number of nodes in Y direction (default: 1)

### VISUALIZATION Section
- **substates**: Comma-separated list of substates to visualize
- **mode**: Output format - `binary` or `text`
- **reduction**: Comma-separated list of reduction operations

## Error Handling and Robustness

### Missing Categories or Parameters

The application gracefully handles missing categories and parameters:
- Missing categories are skipped with a `WARNING` message to stderr
- Missing parameters in a category are reported but don't cause failure
- The application continues loading with default values for missing settings

Example:
```
WARNING: Unknown config category 'UNKNOWN_CATEGORY' - skipping
WARNING: Unexpected end of file while reading parameters for category 'GENERAL'
```

### Compilation Errors

If compilation fails:
1. A detailed error dialog appears showing:
   - Compilation status (success/failure)
   - Exit code
   - Source and output file paths
   - Complete compilation command
   - Compiler output with syntax highlighting
   - Error messages with color coding

2. The error output is properly formatted as HTML for readability

## Usage

### Via GUI

1. Open the Qt-VTK-viewer application
2. Go to **File** menu → **Load Model from Directory...**
3. Select the directory containing your model
4. The application will:
   - Parse `Header.txt`
   - Compile the model if needed (showing progress)
   - Display any compilation errors if they occur
   - Load the model and add it to the Model menu

### Via Command Line

Currently, the directory loading feature is available through the GUI. For command-line model loading, use the existing plugin loading mechanism:

```bash
./QtVtkViewer --loadModel=/path/to/model.so
```

## Compilation Process

When a model needs compilation:

1. The application searches for a C++ header file (`.h`) in the directory
2. It uses `clang++` to compile the file with:
   - `-shared` flag for shared library
   - `-fPIC` flag for position-independent code
   - `-std=c++17` standard (can be adjusted)
3. The compiled `.so` file is saved in the same directory
4. The module is then loaded using the standard plugin loader

### Compilation Command

The actual command used is similar to:

```bash
clang++ -shared -fPIC -std=c++17 "ModelCell.h" -o "model.so"
```

## Error Handling

### Compilation Errors

If compilation fails, a dialog window displays:
- **Compilation Status**: Success or failure with exit code
- **File Information**: Source file, output file, and command used
- **Compiler Output**: Standard output and error messages from the compiler

### Common Issues

1. **Header.txt not found**
   - Ensure the file exists in the selected directory
   - Check the filename is exactly "Header.txt" (case-sensitive)

2. **No C++ header file found**
   - Ensure there's at least one `.h` file in the directory
   - The first `.h` file found will be used

3. **Compilation fails**
   - Check that the C++ code is valid and compilable
   - Ensure `clang++` is installed: `which clang++`
   - Review the error messages in the compilation log dialog

4. **Module already loaded**
   - If a module with the same name is already loaded, the application will ask if you want to replace it
   - Currently, modules cannot be unloaded (they remain for the application lifetime)

## Example Directory Structure

```
MyModel/
├── Header.txt
├── MyModelCell.h
├── Output/
│   ├── MyModel_0.txt
│   ├── MyModel_1.txt
│   └── ...
└── MyModel.so (created after first compilation)
```

## Integration with Existing Features

Once a model is loaded from a directory:

1. It appears in the **Model** menu like any other model
2. You can switch between models using the menu
3. You can load a configuration file to visualize the model's output data
4. All standard visualization features work normally

## Technical Details

### Classes Involved

- **HeaderParser**: Parses `Header.txt` configuration files
- **CppModuleBuilder**: Handles C++ compilation using Tiny Process Library
- **CompilationLogWidget**: Displays compilation results and errors
- **PluginLoader**: Loads compiled `.so` files

### Dependencies

- **Tiny Process Library**: Used for safe process execution and output capture
- **Qt**: For GUI dialogs and file operations
- **Clang**: For C++ compilation

## Limitations

1. Only one C++ header file per directory is supported (the first `.h` found)
2. Models cannot be unloaded once loaded (application lifetime)
3. Compilation uses a fixed C++ standard (C++17)
4. The compiler path is hardcoded to `clang++` (must be in system PATH)

## Future Enhancements

Potential improvements for future versions:

1. Support for multiple header files in a directory
2. Configurable C++ standard and compiler options
3. Model unloading capability
4. Caching of compilation results
5. Support for different compilers (GCC, MSVC)
6. Parallel compilation for multiple models
