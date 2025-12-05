# Model Header Example

This directory contains an example of the `Header.txt` file format used by the Qt-VTK-viewer's "Load Model from Directory" feature.

## File Structure

```
model_header_example/
├── Header.txt          # Configuration file (this example)
├── README.md           # This file
└── YourModelCell.h     # (Not included - add your own model)
```

## Header.txt Format

The `Header.txt` file is a configuration file that describes an OOpenCAL model. It uses an INI-like format with sections and key-value pairs.

### Sections

#### GENERAL (Required)

Contains basic model information:

```ini
GENERAL:
    number_of_columns=500
    number_of_rows=500
    number_of_steps=500
    output_file_name=ball
```

- **number_of_columns**: Width of the grid (required)
- **number_of_rows**: Height of the grid (required)
- **number_of_steps**: Total number of simulation steps (required)
- **output_file_name**: Base name for output files and compiled module (required)

#### DISTRIBUTED (Optional)

Specifies distributed computing parameters:

```ini
DISTRIBUTED:
    number_node_x=2
    number_node_y=1
```

- **number_node_x**: Number of nodes in X direction (default: 1)
- **number_node_y**: Number of nodes in Y direction (default: 1)

#### VISUALIZATION (Optional)

Defines visualization parameters:

```ini
VISUALIZATION:
    substates=h,z
    mode=binary
    reduction=sum,min,max
```

- **substates**: Comma-separated list of substates to visualize
- **mode**: Output format - `binary` or `text`
- **reduction**: Comma-separated list of reduction operations (sum, min, max, etc.)

## Usage

1. Create a directory for your model:
   ```bash
   mkdir MyModel
   cd MyModel
   ```

2. Create a `Header.txt` file with your model's configuration:
   ```bash
   cp examples/model_header_example/Header.txt MyModel/
   # Edit MyModel/Header.txt with your settings
   ```

3. Add your C++ model file (e.g., `MyModelCell.h`):
   ```bash
   cp path/to/MyModelCell.h MyModel/
   ```

4. Open Qt-VTK-viewer and select Model → Load Model from Directory...

5. Select your `MyModel` directory

6. The application will:
   - Parse Header.txt
   - Compile MyModelCell.h if needed
   - Load the compiled module
   - Add it to the Model menu

## Example: Ball Model

For a ball model with 500×500 grid and 500 steps:

```ini
GENERAL:
    number_of_columns=500
    number_of_rows=500
    number_of_steps=500
    output_file_name=ball

DISTRIBUTED:
    number_node_x=1
    number_node_y=1

VISUALIZATION:
    substates=h
    mode=binary
    reduction=sum
```

## Example: SciddicaT Model

For a SciddicaT model with distributed computing:

```ini
GENERAL:
    number_of_columns=1000
    number_of_rows=1000
    number_of_steps=1000
    output_file_name=sciddicat

DISTRIBUTED:
    number_node_x=4
    number_node_y=4

VISUALIZATION:
    substates=h,z,v
    mode=binary
    reduction=sum,min,max
```

## Notes

- Section names are case-insensitive (GENERAL, general, General all work)
- Whitespace around keys and values is trimmed
- Comments starting with # or ; are ignored
- All GENERAL section fields are required
- DISTRIBUTED and VISUALIZATION sections are optional
- The `output_file_name` is used as the base name for the compiled .so file

## Troubleshooting

### Header.txt not found
- Ensure the file is in the selected directory
- Check the filename is exactly "Header.txt" (case-sensitive on Linux)

### Invalid Header
- Verify all GENERAL section fields are present
- Check for typos in section names
- Ensure values are properly formatted

### No C++ header file found
- Add at least one .h file to the directory
- The first .h file found will be used for compilation

### Compilation fails
- Check that the C++ code is valid
- Ensure clang++ is installed
- Review error messages in the compilation log dialog

## See Also

- `doc/LOAD_MODEL_FROM_DIRECTORY.md` - User guide
- `doc/DEVELOPER_GUIDE_MODEL_LOADING.md` - Developer documentation
- `examples/custom_model_plugin/` - Example plugin structure
