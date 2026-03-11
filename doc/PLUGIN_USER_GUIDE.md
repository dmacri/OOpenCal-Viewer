# Plugin User Guide - Adding Custom Models

Complete guide for users who want to add custom visualization models to OOpenCal-Viewer.

## Table of Contents

- [Quick Start (5 Minutes)](#quick-start-5-minutes)
- [Loading Plugins](#loading-plugins)
- [Creating Your Own Plugin](#creating-your-own-plugin)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)

---

## Quick Start (5 Minutes)

### Prerequisites

* OOpenCal-Viewer compiled and working
* C++ compiler (g++, clang++)
* CMake 3.16+
* VTK (same version as the application)

### Step 1: Copy the Plugin Template

```bash
cd /path/to/OOpenCal-Viewer
cp -r examples/custom_model_plugin my_plugin
cd my_plugin
```

### Step 2: Modify Your Model (CustomCell.h)

Open `CustomCell.h` and adapt the `CustomCell` class:

```cpp
class CustomCell : public Element
{
private:
    int value;  // Your data
    rgb outputColor{0, 0, 0};

public:
    CustomCell() : value(0) {}
    CustomCell(int val) : value(val) {}

    // Parse from string (read from file)
    void composeElement(char* str) override
    {
        this->value = atoi(str);
    }

    // Convert to string (write to file)
    char* stringEncoding() override
    {
        char* zstr = new char[16];
        sprintf(zstr, "%d", value);
        return zstr;
    }

    // MOST IMPORTANT: defines visualization color
    rgb* outputValue(char* str) override
    {
        // Your coloring logic
        if (value == 0)
            outputColor = rgb(255, 255, 255);  // White
        else
            outputColor = rgb(255, 0, 0);      // Red
        
        return &outputColor;
    }

    void startStep(int /*step*/) override {}
};
```

### Step 3: Change Model Name (CustomModelPlugin_Full.cpp)

```cpp
extern "C" {
__attribute__((visibility("default")))
void registerPlugin()
{
    SceneWidgetVisualizerFactory::registerModel("MyModel", []() {
        //                                        ^^^^^^^
        //                                     Change here!
        return std::make_unique<SceneWidgetVisualizerAdapter<CustomCell>>(
            "MyModel"  // And here!
        );
    });
    
    std::cout << "✓ MyModel plugin loaded!" << std::endl;
}
}
```

### Step 4: Compile

```bash
mkdir build && cd build
cmake ..
make
```

Result: `libCustomModelPlugin.so`

### Step 5: Install & Run

```bash
# Copy to plugins directory
mkdir -p ../../build/plugins
cp libCustomModelPlugin.so ../../build/plugins/

# Run application
cd ../../build
./QtVtkViewer
```

Your model **MyModel** will appear in the Model menu!

---

## Loading Plugins

### Method 1: GUI (Recommended)

1. Launch QtVtkViewer
2. Go to **Model → Load Plugin...** (or press **Ctrl+P**)
3. Select your `.so` file
4. Click Open
5. ✅ Success dialog appears
6. Your model is now available in the Model menu

### Method 2: Auto-load at Startup

Place your plugin in one of these directories:
- `./plugins/` (current directory)
- `../plugins/` (parent directory)
- `./build/plugins/` (build directory)

The application will automatically load all `.so` files on startup.

### Method 3: Command Line

```bash
./QtVtkViewer --plugin=/path/to/libMyPlugin.so
```

---

## Creating Your Own Plugin

### Full Example Structure

```
my_model_plugin/
├── CMakeLists.txt              # Build configuration
├── MyModelCell.h               # Your cell class
├── MyModelPlugin.cpp           # Registration code
└── README.md                   # Documentation
```

### 1. Define Your Cell Model

Your model must inherit from `Element`:

```cpp
// MyModelCell.h
#pragma once

#include "Element.h"

class MyModelCell : public Element
{
private:
    int value;
    rgb outputColor{128, 128, 128};

public:
    MyModelCell() : value(0) {}
    
    // Required methods:
    void composeElement(char* str) override;
    char* stringEncoding() override;
    rgb* outputValue(char* str) override;
    void startStep(int step) override;
};
```

### 2. Create Plugin Registration

```cpp
// MyModelPlugin.cpp
#include "MyModelCell.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
#include "visualiserProxy/SceneWidgetVisualizerAdapter.h"

extern "C" {

__attribute__((visibility("default")))
void registerPlugin()
{
    SceneWidgetVisualizerFactory::registerModel("MyModel", []() {
        return std::make_unique<SceneWidgetVisualizerAdapter<MyModelCell>>(
            0,           // ID (deprecated but required)
            "MyModel"    // Model name (appears in menu)
        );
    });
}

// Optional: Plugin metadata
__attribute__((visibility("default")))
const char* getPluginInfo()
{
    return "MyModel Plugin v1.0 - Custom visualization model";
}

__attribute__((visibility("default")))
int getPluginVersion()
{
    return 100; // Version 1.00
}
}
```

### 3. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyModelPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paths (adjust to your system)
set(OOPENCAL_DIR "/path/to/OOpenCAL")
set(OOPENCALVIEWER_DIR "/path/to/OOpenCal-Viewer")

# Find VTK
if(NOT VTK_FOUND)
    find_package(VTK REQUIRED)
endif()

# Include directories
include_directories(
    ${OOPENCALVIEWER_DIR}
    ${OOPENCALVIEWER_DIR}/visualiserProxy
    ${VTK_INCLUDE_DIRS}
    ${OOPENCAL_DIR}
)

# Create shared library
add_library(MyModelPlugin SHARED
    MyModelPlugin.cpp
)

# Link with VTK
target_link_libraries(MyModelPlugin
    ${VTK_LIBRARIES}
)

# Export symbols
set_target_properties(MyModelPlugin PROPERTIES
    CXX_VISIBILITY_PRESET default
)
```

### 4. Build

```bash
mkdir build && cd build
cmake .. -DOOPENCALVIEWER_DIR=/path/to/OOpenCal-Viewer \
         -DOOPENCAL_DIR=/path/to/OOpenCAL
make
```

---

## Examples

### Example 1: Temperature Gradient

```cpp
rgb* outputValue(char* str) override
{
    double temp = value / 100.0;  // Normalize 0-100 to 0.0-1.0
    
    if (temp < 0.5)
        outputColor = rgb(0, 0, static_cast<int>(255 * temp * 2));  // Black → Blue
    else
        outputColor = rgb(static_cast<int>(255 * (temp - 0.5) * 2), 0, 255);  // Blue → Purple
    
    return &outputColor;
}
```

### Example 2: Height Map

```cpp
rgb* outputValue(char* str) override
{
    int height = value;
    
    if (height < 10)
        outputColor = rgb(0, 0, 255);        // Water - blue
    else if (height < 50)
        outputColor = rgb(0, 255, 0);        // Plain - green
    else if (height < 100)
        outputColor = rgb(139, 69, 19);      // Mountains - brown
    else
        outputColor = rgb(255, 255, 255);    // Snow - white
    
    return &outputColor;
}
```

### Example 3: Cellular Automata States

```cpp
rgb* outputValue(char* str) override
{
    switch (value)
    {
    case 0:  // Dead
        outputColor = rgb(0, 0, 0);
        break;
    case 1:  // Alive
        outputColor = rgb(0, 255, 0);
        break;
    case 2:  // Infected
        outputColor = rgb(255, 0, 0);
        break;
    default: // Unknown
        outputColor = rgb(128, 128, 128);
    }
    return &outputColor;
}
```

---

## Troubleshooting

### Plugin Doesn't Load

**Problem:** No plugin loaded message in console

**Solutions:**

1. Check if file exists in `./plugins/`:
   ```bash
   ls -la ./plugins/
   ```

2. Check permissions:
   ```bash
   chmod +x ./plugins/libMyPlugin.so
   ```

3. Check dependencies:
   ```bash
   ldd ./plugins/libMyPlugin.so
   ```

4. Check exported symbols:
   ```bash
   nm -D libMyPlugin.so | grep registerPlugin
   ```
   Should show:
   ```
   0000000000XXXXX T registerPlugin
   ```

### Error: "does not export registerPlugin()"

**Cause:** Function not properly exported

**Solution:** Ensure your plugin has:

```cpp
extern "C" {
__attribute__((visibility("default")))
void registerPlugin() {
    // ...
}
}
```

### Model Doesn't Appear in Menu

**Cause:** Plugin compiled its own Factory copy (separate registry)

**Solution:** 
1. Do NOT compile `SceneWidgetVisualizerFactory.cpp` in your plugin
2. Main app must be linked with `-rdynamic`
3. Plugin loaded with `RTLD_GLOBAL` flag (already set)

### Application Crashes

**Possible causes:**

1. **Compiler mismatch** - use same compiler as main app
2. **Different VTK versions** - must be identical
3. **ABI incompatibility** - rebuild plugin with correct settings

### Symbols Not Found

**Problem:** Undefined symbol errors

**Solution:** Ensure main app has `-rdynamic` in CMakeLists.txt:

```cmake
target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
```

---

## Tips & Best Practices

### Development Tips

1. **Iterate quickly** - change color, recompile plugin only (seconds), reload
2. **Start simple** - use basic colors first (red, green, blue)
3. **Debug with prints** - `std::cout << "value=" << value << std::endl;`
4. **Test with small data** - verify logic before running full simulation
5. **Check an existing model header** - use any available model header as a reference

### Color Design

1. **Use meaningful colors** - choose colors that represent your data intuitively
2. **Consider colorblind users** - avoid red-green combinations
3. **Use gradients** - smooth transitions are easier to interpret
4. **High contrast** - ensure important states are visually distinct

### Performance

1. **Optimize `outputValue()`** - called for every cell, every frame
2. **Avoid allocations** - reuse `outputColor` member variable
3. **Cache calculations** - pre-compute lookup tables if needed

### Distribution

Before sharing your plugin:

- [ ] Plugin compiles without warnings
- [ ] `registerPlugin()` exports correctly
- [ ] Model has unique name
- [ ] Colors are meaningful and readable
- [ ] Code has comments
- [ ] Tested with various data
- [ ] Metadata added (getPluginInfo, getPluginVersion)
- [ ] README created

---

## Reference

### Required Element Methods

```cpp
class MyCell : public Element
{
    // Parse cell data from string (file input)
    void composeElement(char* str) override;
    
    // Encode cell data to string (file output)
    char* stringEncoding() override;
    
    // Return RGB color for visualization
    rgb* outputValue(char* str) override;
    
    // Called at start of each simulation step
    void startStep(int step) override;
};
```

### RGB Color Constructor

```cpp
rgb(int red, int green, int blue)
// Each component: 0-255
// Examples:
rgb(255, 0, 0)     // Red
rgb(0, 255, 0)     // Green
rgb(0, 0, 255)     // Blue
rgb(255, 255, 0)   // Yellow
rgb(255, 0, 255)   // Magenta
rgb(0, 255, 255)   // Cyan
rgb(255, 255, 255) // White
rgb(0, 0, 0)       // Black
```

---

## See Also

- **examples/custom_model_plugin/** - Complete working example
- **examples/crowdsimulation_model_plugin/** - Another example
- **doc/PLUGIN_ARCHITECTURE.md** - Technical implementation details
- **doc/VIEW_MODES.md** - 2D/3D visualization modes

---

**Last Updated:** 2025-10-20  
**Status:** ✅ Current and Complete
