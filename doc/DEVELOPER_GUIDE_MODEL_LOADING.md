# Developer Guide: Model Loading from Directory

## Architecture Overview

The model loading from directory feature consists of three main components:

### 1. ModelLoader (`utilities/ModelLoader.h/cpp`)

**Purpose**: Orchestrate model loading from directories

**Key Classes**:
- `ModelLoader`: Main loader class
- `ModelLoader::LoadResult`: Structure containing loading results
- `CompilationResult`: Structure containing compilation details

**Usage**:
```cpp
ModelLoader loader;
auto result = loader.loadModelFromDirectory("/path/to/model");
if (result.success) {
    std::string modulePath = result.compiledModulePath;
    std::string modelName = result.modelName;
}
```

**Responsibilities**:
- Validate directory structure
- Parse `Header.txt` using existing `Config` class
- Find C++ model source files
- Manage compilation via `CppModuleBuilder`
- Return detailed results

### 2. CppModuleBuilder (`utilities/CppModuleBuilder.h/cpp`)

**Purpose**: Compile C++ model files into shared libraries

**Key Classes**:
- `CompilationResult`: Structure containing compilation outcome
- `CppModuleBuilder`: Handles compilation process

**Key Methods**:
```cpp
// With OOpenCAL directory
CppModuleBuilder builder("clang++", "/path/to/OOpenCAL");

// Or use OOPENCAL_DIR environment variable
CppModuleBuilder builder("clang++");

auto result = builder.compileModule(
    "/path/to/wrapper.cpp",
    "/path/to/model.so",
    "c++17"
);
if (result.success) {
    // Module compiled successfully
}
```

**Features**:
- Captures compiler stdout and stderr
- Returns exit code and error messages
- Uses Tiny Process Library for safe execution
- Supports custom compiler paths
- Auto-detects C++ standard from `__cplusplus` macro
- Adds OOpenCAL include path (`-I$OOPENCAL_DIR/base`)

**Compilation Command Format**:
```
clang++ -shared -fPIC -std=c++17 -I"/path/to/OOpenCAL/base" "wrapper.cpp" -o "model.so"
```

**OOpenCAL Directory Resolution**:
1. If provided in constructor: use that path
2. Otherwise: check `OOPENCAL_DIR` environment variable
3. If neither available: compile without include path (may fail if model needs Cell.h)

### 3. Plugin Wrapper Generation

**Purpose**: Automatically generate plugin wrapper code for models

**Process**:
1. `ModelLoader::generateWrapper()` creates a C++ file with:
   - Plugin entry point: `registerPlugin()`
   - Metadata functions: `getPluginInfo()`, `getPluginVersion()`, `getModelName()`
   - Registration with `SceneWidgetVisualizerFactory`

2. Wrapper template:
```cpp
#include "ModelName.h"  // User's model header
#include "visualiserProxy/SceneWidgetVisualizerAdapter.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"

extern "C" {
    __attribute__((visibility("default")))
    void registerPlugin() {
        SceneWidgetVisualizerFactory::registerModel("ModelName", []() {
            return std::make_unique<SceneWidgetVisualizerAdapter<ModelName>>("ModelName");
        });
    }
    // ... other metadata functions
}
```

3. The wrapper is compiled together with the model header into a single .so file

**Benefits**:
- Automatic plugin generation from model headers
- No manual wrapper coding needed
- Consistent plugin interface
- Proper symbol visibility management

### 4. CompilationLogWidget (`widgets/CompilationLogWidget.h/cpp`)

**Purpose**: Display compilation results and errors to the user

**Key Methods**:
```cpp
CompilationLogWidget logWidget(parentWidget);
logWidget.displayCompilationResult(compilationResult);
logWidget.exec();  // Show as modal dialog
```

**Displayed Information**:
- Compilation status (success/failure)
- Exit code
- Source and output file paths
- Compilation command
- Compiler output with HTML formatting and syntax highlighting

### 4. MainWindow Integration

**New Methods**:
- `onLoadModelFromDirectoryRequested()`: Slot for menu action
- `loadModelFromDirectory(const QString& modelDirectory)`: Implementation

**Workflow**:
1. User selects "Load Model from Directory" from menu
2. File dialog opens to select directory
3. `loadModelFromDirectory()` is called with selected path
4. Process:
   - Parse Header.txt
   - Find C++ header file
   - Check if .so exists
   - Compile if needed
   - Load compiled module
   - Refresh model menu

## Data Flow

```
User Action (Menu)
    ↓
onLoadModelFromDirectoryRequested()
    ↓
QFileDialog (select directory)
    ↓
loadModelFromDirectory(path)
    ├─ HeaderParser::parseHeaderFile()
    ├─ Find *.h file in directory
    ├─ Check if .so exists
    ├─ If not:
    │  ├─ CppModuleBuilder::compileModule()
    │  ├─ If failed:
    │  │  └─ CompilationLogWidget::displayCompilationResult()
    │  └─ If success: continue
    ├─ PluginLoader::loadPlugin()
    ├─ SceneWidgetVisualizerFactory::registerModel()
    └─ MainWindow::recreateModelMenuActions()
```

## Integration Points

### With PluginLoader

The compiled module is loaded using the existing `PluginLoader`:

```cpp
PluginLoader& loader = PluginLoader::instance();
if (loader.loadPlugin(outputFile.toStdString())) {
    // Module loaded successfully
    recreateModelMenuActions();  // Update GUI
}
```

### With SceneWidgetVisualizerFactory

Models are registered through the factory (happens in plugin's `registerPlugin()` function):

```cpp
SceneWidgetVisualizerFactory::registerModel("ModelName", []() {
    return std::make_unique<SceneWidgetVisualizerAdapter<ModelCell>>("ModelName");
});
```

## Error Handling

### HeaderParser Errors

```cpp
try {
    auto config = HeaderParser::parseHeaderFile(path);
    if (!config.isValid()) {
        // Handle invalid configuration
    }
} catch (const std::runtime_error& e) {
    // Handle file read errors
}
```

### CppModuleBuilder Errors

```cpp
auto result = builder.compileModule(source, output, "c++17");
if (!result.success) {
    // result.exitCode: compiler exit code
    // result.stderr: error messages
    // result.stdout: compiler output
}
```

### PluginLoader Errors

```cpp
if (!loader.loadPlugin(modulePath)) {
    QString error = QString::fromStdString(loader.getLastError());
    // Handle load error
}
```

## Namespace Organization

All new classes are in the `viz::plugins` namespace:

```cpp
namespace viz::plugins {
    struct HeaderConfig { ... };
    class HeaderParser { ... };
    struct CompilationResult { ... };
    class CppModuleBuilder { ... };
}
```

## Dependencies

### External Libraries
- **Tiny Process Library**: Process execution and I/O capture
  - Included via FetchContent in CMakeLists.txt
  - Header: `process.hpp`
  - Usage: `TinyProcessLib::Process`

### Qt Libraries
- `QDialog`: Base for CompilationLogWidget
- `QFileDialog`: Directory selection
- `QProgressDialog`: Progress indication
- `QMessageBox`: User notifications

### Standard Libraries
- `<filesystem>`: File operations
- `<fstream>`: File I/O
- `<sstream>`: String parsing
- `<functional>`: Callbacks

## CMake Configuration

### FetchContent Setup

```cmake
FetchContent_Declare(
    tiny_process_library
    GIT_REPOSITORY https://gitlab.com/eidheim/tiny-process-library.git
    GIT_TAG master
)

FetchContent_MakeAvailable(tiny_process_library)

### Integration Tests

1. End-to-end model loading
2. Model switching after loading
3. Configuration loading with loaded model
4. Multiple models loaded sequentially

### Manual Testing

1. Load model from directory with pre-compiled .so
2. Load model from directory requiring compilation
3. Handle compilation errors gracefully
4. Verify model appears in menu
5. Switch between loaded models
6. Load configuration and visualize

## Performance Considerations

1. **Compilation**: Blocking operation, shown with progress dialog
2. **File I/O**: Synchronous, acceptable for directory operations
3. **Process Execution**: Uses Tiny Process Library for efficiency
4. **Memory**: Compilation results stored temporarily, freed after display

## Security Considerations

1. **File Paths**: Validated before use
2. **Compiler Execution**: Uses process library, not `std::system()`
3. **Output Capture**: Prevents buffer overflow
4. **User Input**: File dialog restricts to existing directories

## Future Enhancements

1. **Async Compilation**: Use QThread for non-blocking compilation
2. **Compiler Selection**: Allow user to choose compiler
3. **Build Options**: Configurable C++ standard and flags
4. **Caching**: Remember compilation results
5. **Validation**: Pre-compile check before attempting
6. **Logging**: Detailed compilation logs to file

## Debugging

### Enable Verbose Output

Compilation commands and results are logged to stdout/stderr:

```cpp
std::cout << "Compiling module: " << sourceFile << std::endl;
std::cout << "Command: " << command << std::endl;
```

### Inspect Compilation Results

The `CompilationResult` structure contains all details:

```cpp
auto result = builder.compileModule(...);
std::cout << "Exit code: " << result.exitCode << std::endl;
std::cout << "Stderr: " << result.stderr << std::endl;
std::cout << "Stdout: " << result.stdout << std::endl;
```

### Check Generated Files

Compiled modules are saved in the model directory:

```bash
ls -la /path/to/model/
# Should show: model.so (after compilation)
```
