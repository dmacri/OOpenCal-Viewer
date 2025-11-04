/** @file ModelLoader.cpp
 * @brief Implementation of ModelLoader for loading models from directories. */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <format>
#include <string>

#include "ModelLoader.h"
#include "config/Config.h"
#include "CppModuleBuilder.h"

namespace
{
namespace fs = std::filesystem;

/// Get the directory containing this executable (project root)
static std::string getProjectRootPath()
{
    // Try to get from environment variable first
    const char* envPath = std::getenv("QT_VTK_VIEWER_ROOT");
    if (envPath)
        return envPath;

    // Otherwise, return empty - caller should set it
    return "";
}

/** Check if file `a` is newer than file `b`.
 *  Returns:
 *   true  - if `a` exists and its last modification time is more recent than `b`
 *   false - otherwise (including cases when either file does not exist) **/
bool isFileNewer(const std::filesystem::path& a, const std::filesystem::path& b)
{
    namespace fs = std::filesystem;

    // Check if both files exist
    if (! fs::exists(a) || ! fs::exists(b))
    {
        std::cerr << "Warning: One or both files do not exist.\n";
        return false;
    }

    // Compare modification times
    return fs::last_write_time(a) > fs::last_write_time(b);
}

std::string generateModuleNameForSourceFile(const std::string& cppHeaderFile)
{
    std::filesystem::path file(cppHeaderFile);
    const std::string base = file.stem().string();  // e.g. "BallCell"
    const auto sharedLibraryName = "lib" + base + "Plugin.so";
    return file.parent_path() / sharedLibraryName;
}
} // namespace


ModelLoader::ModelLoader()
    : builder(std::make_unique<viz::plugins::CppModuleBuilder>())
{
    std::string projectRoot = getProjectRootPath();
    if (!projectRoot.empty())
    {
        builder->setProjectRootPath(projectRoot);
    }
}

ModelLoader::~ModelLoader() = default;


ModelLoader::LoadResult ModelLoader::loadModelFromDirectory(const std::string& modelDirectory)
{
    LoadResult result;

    // Validate directory structure
    if (! validateDirectory(modelDirectory))
    {
        result.success = false;
        return result;
    }

    try
    {
        // Parse Header.txt using Config class
        std::string headerPath = modelDirectory + "/Header.txt";
        result.config = std::make_shared<Config>(headerPath);
        result.config->readConfigFile();

        // Get model name from output_file_name parameter
        ConfigCategory* generalCat = result.config->getConfigCategory("GENERAL", true);
        if (!generalCat)
        {
            std::cerr << "Error: GENERAL section not found in Header.txt" << std::endl;
            result.success = false;
            return result;
        }

        ConfigParameter* outputParam = generalCat->getConfigParameter("output_file_name");
        if (!outputParam)
        {
            std::cerr << "Error: output_file_name not found in GENERAL section" << std::endl;
            result.success = false;
            return result;
        }

        result.modelName = outputParam->getValue<std::string>();

        // Find C++ header file
        std::string sourceFile = findHeaderFile(modelDirectory);
        if (sourceFile.empty())
        {
            std::cerr << "Error: No C++ header file (.h) found in " << modelDirectory << std::endl;
            result.success = false;
            return result;
        }

        std::cout << "Found header file: '" << sourceFile << "'\n";

        // Determine output file path
        const std::string outputFile = generateModuleNameForSourceFile(sourceFile);
        std::cout << "Trying to open: " << outputFile << "'\t" << isFileNewer(sourceFile, outputFile) << std::endl;

        // Check if compilation is needed
        if (moduleExists(outputFile))
        {
            std::cout << "Module '" << outputFile << "' already exists" << std::endl;
            if (isFileNewer(sourceFile, outputFile))
            {
                std::cerr << "[WARNING] C++ source file '" << sourceFile << "' is never than module file '" << outputFile << "'" << std::endl;
            }
        }
        else // if module does not exist
        {
            const std::string wrapperSource = modelDirectory + "/" + result.modelName + "_wrapper.cpp";

            std::cout << "Compiling module: " << sourceFile << std::endl;

            // Generate wrapper code
            if (!generateWrapper(result.modelName, wrapperSource))
            {
                std::cerr << "Error: Failed to generate wrapper code" << std::endl;
                result.success = false;
                return result;
            }

            // Compile the wrapper to .so (which includes the model header)
            // Empty string for cppStandard triggers auto-detection in CppModuleBuilder
            auto compilationResult = builder->compileModule(wrapperSource, outputFile, "");

            if (!compilationResult.success)
            {
                std::cerr << "Compilation failed with exit code: " << compilationResult.exitCode << std::endl;
                if (!compilationResult.stderr.empty())
                {
                    std::cerr << "Error output:\n" << compilationResult.stderr << std::endl;
                }
                result.success = false;
                result.compilationResult = new viz::plugins::CompilationResult(compilationResult);
                return result;
            }
        }

        result.compiledModulePath = outputFile;
        result.success = true;
        return result;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error loading model: " << e.what() << std::endl;
        result.success = false;
        return result;
    }
}

bool ModelLoader::validateDirectory(const std::string& modelDirectory)
{
    if (!fs::exists(modelDirectory) || !fs::is_directory(modelDirectory))
    {
        std::cerr << "Error: Directory does not exist: " << modelDirectory << std::endl;
        return false;
    }

    std::string headerPath = modelDirectory + "/Header.txt";
    if (!fs::exists(headerPath))
    {
        std::cerr << "Error: Header.txt not found in " << modelDirectory << std::endl;
        return false;
    }

    // Check if at least one .h file exists
    bool hasHeaderFile = false;
    for (const auto& entry : fs::directory_iterator(modelDirectory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".h")
        {
            hasHeaderFile = true;
            break;
        }
    }

    if (!hasHeaderFile)
    {
        std::cerr << "Error: No C++ header file (.h) found in " << modelDirectory << std::endl;
        return false;
    }

    return true;
}

std::string ModelLoader::findHeaderFile(const std::string& modelDirectory)
{
    for (const auto& entry : fs::directory_iterator(modelDirectory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".h")
        {
            return entry.path().string();
        }
    }
    return "";
}

bool ModelLoader::moduleExists(const std::string& outputPath)
{
    return fs::exists(outputPath) && fs::is_regular_file(outputPath);
}

bool ModelLoader::generateWrapper(const std::string& modelName, const std::string& wrapperPath)
{
    try
    {
        std::ofstream wrapper(wrapperPath);
        if (!wrapper.is_open())
        {
            std::cerr << std::format("Error: Cannot create wrapper file: {}\n", wrapperPath);
            return false;
        }

        const std::string code = std::format(R"(/** Auto-generated wrapper for {0} model */
#include <iostream>
#include <memory>
#include <string>
#include "visualiserProxy/SceneWidgetVisualizerAdapter.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"

// The actual model class is defined in the compiled model header
#include "{0}.h"

#define MODEL_NAME "{0}"

extern "C"
{{
__attribute__((visibility("default")))
void registerPlugin()
{{
    std::cout << "Registering " MODEL_NAME " plugin..." << std::endl;

    bool success = SceneWidgetVisualizerFactory::registerModel(MODEL_NAME, []() {{
        return std::make_unique<SceneWidgetVisualizerAdapter<{0}>>(
            MODEL_NAME
        );
    }});

    if (success)
    {{
        std::cout << "✓ " MODEL_NAME " plugin registered successfully!" << std::endl;
        std::cout << "  The model is now available in Model menu" << std::endl;
    }}
    else
    {{
        std::cerr << "✗ Failed to register " MODEL_NAME " - name may already exist" << std::endl;
    }}
}}

__attribute__((visibility("default")))
const char* getPluginInfo()
{{
    return MODEL_NAME " Plugin v1.0\n"
           "Auto-generated from directory loader\n"
           "Compatible with: Qt-VTK-viewer 2.x";
}}

__attribute__((visibility("default")))
int getPluginVersion()
{{
    return 100; // Version 1.00
}}

__attribute__((visibility("default")))
const char* getModelName()
{{
    return MODEL_NAME;
}}
}} // extern "C"
)", modelName);

        wrapper << code;
        wrapper.close();

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << std::format("Error generating wrapper: {}\n", e.what());
        return false;
    }
}
