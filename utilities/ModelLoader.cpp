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
#include "config/ConfigConstants.h"
#include "CppModuleBuilder.h"
#include "directoryConstants.h"


namespace
{
namespace fs = std::filesystem;

/// Get the directory containing this executable (project root)
static std::string getProjectRootPath()
{
    // 1. Prefer environment variable first
    if (const char* envPath = std::getenv("OOPENCAL_VIEWER_ROOT"))
    {
        if (! std::string(envPath).empty())
        {
            return envPath;
        }
    }

// 2. Use compile-time define provided by CMake (if exists and directory is valid)
#ifdef OOPENCAL_VIEWER_ROOT
    {
        const std::string viewerPath = OOPENCAL_VIEWER_ROOT;

        // Only use it if it points to an existing directory
        std::error_code ec; // this is used to use version which does not throw
        if (std::filesystem::exists(viewerPath, ec) && std::filesystem::is_directory(viewerPath, ec))
        {
            return viewerPath;
        }
    }
#endif

    // 3. Fallback: nothing available → return empty string
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

std::string generateClassNameFromCppHeaderFileName(const std::string& cppHeaderFile)
{
    std::filesystem::path file(cppHeaderFile);
    const std::string base = file.stem().string();  // e.g. "BallCell"
    return base;
}
} // namespace


ModelLoader::ModelLoader()
    : builder(std::make_unique<viz::plugins::CppModuleBuilder>())
{
    std::string projectRoot = getProjectRootPath();
    if (! projectRoot.empty())
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
        ConfigCategory* generalCat = result.config->getConfigCategory(ConfigConstants::CATEGORY_GENERAL, true);
        if (!generalCat)
        {
            std::cerr << "Error: GENERAL section not found in Header.txt" << std::endl;
            result.success = false;
            return result;
        }

        ConfigParameter* outputParam = generalCat->getConfigParameter(ConfigConstants::PARAM_OUTPUT_FILE_NAME);
        if (!outputParam)
        {
            std::cerr << "Error: output_file_name not found in GENERAL section" << std::endl;
            result.success = false;
            return result;
        }

        result.outputFileName = outputParam->getValue<std::string>();

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
        std::cout << "Trying to open: " << outputFile << "'\t, source never than compiled?: " << std::boolalpha << isFileNewer(sourceFile, outputFile) << std::endl;

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
            const std::string wrapperSource = modelDirectory + "/" + result.outputFileName + std::string(DirectoryConstants::WRAPPER_FILE_SUFFIX);

            std::cout << "Compiling module: " << sourceFile << std::endl;

            // Generate wrapper code
            const auto className = generateClassNameFromCppHeaderFileName(sourceFile);
            if (! generateWrapper(wrapperSource, result.outputFileName, className))
            {
                std::cerr << "Error: Failed to generate wrapper code" << std::endl;
                result.success = false;
                return result;
            }

            // Compile the wrapper to .so (which includes the model header)
            // Empty string for cppStandard triggers auto-detection in CppModuleBuilder
            auto compilationResult = builder->compileModule(wrapperSource, outputFile);
            if (! compilationResult.success)
            {
                std::cerr << "Compilation failed with exit code: " << compilationResult.exitCode << std::endl;
                if (!compilationResult.stderr.empty())
                {
                    std::cerr << "Error output:\n" << compilationResult.stderr << std::endl;
                }
                result.success = false;
                result.compilationResult = compilationResult;
                return result;
            }
            
            constexpr bool removeWrapperAfterSuccessfullCompilation = true;
            if constexpr(removeWrapperAfterSuccessfullCompilation)
            {
                try
                {
                    if (fs::exists(wrapperSource))
                    {
                        fs::remove(wrapperSource);
                        std::cout << "Removed wrapper file: " << wrapperSource << std::endl;
                    }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Warning: Failed to remove wrapper file: " << e.what() << std::endl;
                    // Don't fail the build if wrapper removal fails
                }
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
    if (! fs::exists(modelDirectory) || ! fs::is_directory(modelDirectory))
    {
        std::cerr << "Error: Directory does not exist: " << modelDirectory << std::endl;
        return false;
    }

    const fs::path headerPath = fs::path(modelDirectory) / DirectoryConstants::HEADER_FILE_NAME;
    if (! fs::exists(headerPath))
    {
        std::cerr << "Error: Header.txt not found in " << modelDirectory << std::endl;
        return false;
    }

    const std::string headerFile = findHeaderFile(modelDirectory);
    if (headerFile.empty())
    {
        std::cerr << "Error: No C++ header file (.h) found in " << modelDirectory << std::endl;
        return false;
    }

    return true;
}

std::string ModelLoader::findHeaderFile(const std::string& modelDirectory)
{
    auto it = std::ranges::find_if(fs::directory_iterator(modelDirectory), fs::directory_iterator{}, [](const fs::directory_entry& entry)
        {
            return entry.is_regular_file() && entry.path().extension() == ".h";
        });

    if (it != fs::directory_iterator{})
    {
        return it->path().string();
    }
    return {};
}

bool ModelLoader::moduleExists(const std::string& outputPath)
{
    return fs::exists(outputPath) && fs::is_regular_file(outputPath);
}

bool ModelLoader::generateWrapper(const std::string& wrapperPath, const std::string& modelName, const std::string& className)
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
#include "{1}.h"

#define MODEL_NAME "{0}"

extern "C"
{{
__attribute__((visibility("default")))
void registerPlugin()
{{
    std::cout << "Registering " MODEL_NAME " plugin..." << std::endl;

    bool success = SceneWidgetVisualizerFactory::registerModel(MODEL_NAME, []() {{
        return std::make_unique<SceneWidgetVisualizerAdapter<{1}>>(
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
)", modelName, className);

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
