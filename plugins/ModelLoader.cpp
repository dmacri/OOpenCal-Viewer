/** @file ModelLoader.cpp
 * @brief Implementation of ModelLoader for loading models from directories. */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <format>
#include <string>
#include <QString>
#include <QLibrary>

#include "ModelLoader.h"
#include "config/Config.h"
#include "config/ConfigConstants.h"
#include "CppModuleBuilder.h"
#include "core/directoryConstants.h"


namespace
{
namespace fs = std::filesystem;

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
}

ModelLoader::~ModelLoader() = default;


ModelLoader::LoadResult ModelLoader::loadModelFromDirectory(const std::string& modelDirectory, bool forceCompilation)
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

        try
        {
            result.outputFileName = readOutputFileName(result.config.get());
        }
        catch(const std::invalid_argument& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
            result.success = false;
            return result;
        }

        // Find C++ header file
        std::string sourceFile = findHeaderFile(modelDirectory);
        if (sourceFile.empty())
        {
            std::cerr << "Error: No C++ header file (.h) found in " << modelDirectory << std::endl;
            result.success = false;
            return result;
        }

        std::cout << "[DEBUG] Found header file: '" << sourceFile << "'\n";

        // Determine output file path
        const std::string moduleFileName = generateModuleNameForSourceFile(sourceFile);

        // Check if compilation is needed
        const bool compilationNecessarily = ! moduleExists(moduleFileName) || forceCompilation;
        if (compilationNecessarily)
        {
            const std::string wrapperSource = modelDirectory + "/" + result.outputFileName + std::string(DirectoryConstants::WRAPPER_FILE_SUFFIX);

            std::cout << "[DEBUG] Compiling module: " << sourceFile << std::endl;

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
            auto compilationResult = builder->compileModule(wrapperSource, moduleFileName);
            if (! compilationResult.success)
            {
                std::cerr << "Compilation failed with exit code: " << compilationResult.exitCode << std::endl;
                if (!compilationResult.stdErr.empty())
                {
                    std::cerr << "Error output:\n" << compilationResult.stdErr << std::endl;
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
        else
        {
            std::cout << "Module '" << moduleFileName << "' already exists" << std::endl;
            if (isFileNewer(sourceFile, moduleFileName))
            {
                std::cerr << "[WARNING] C++ source file '" << sourceFile << "' is never than module file '" << moduleFileName << "'" << std::endl;
            }
        }

        result.compiledModulePath = moduleFileName;
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

std::string ModelLoader::readOutputFileName(Config* config)
{
    // Get model name from output_file_name parameter
    ConfigCategory* generalCat = config->getConfigCategory(ConfigConstants::CATEGORY_GENERAL, /*ignoreCase=*/true);
    if (! generalCat)
    {
        throw std::invalid_argument(std::string("Error: GENERAL section not found in ") + DirectoryConstants::HEADER_FILE_NAME);
    }

    const ConfigParameter* outputParam = generalCat->getConfigParameter(ConfigConstants::PARAM_OUTPUT_FILE_NAME);
    if (! outputParam)
    {
        throw std::invalid_argument("Error: output_file_name not found in GENERAL section");
    }

    return outputParam->getValue<std::string>();
}

std::string ModelLoader::generateModuleNameForSourceFile(const std::string& cppHeaderFile)
{
    std::filesystem::path file(cppHeaderFile);
    const std::string baseName = file.stem().string();  // e.g. "BallCell"
    
#ifdef _WIN32
    // Windows: generate DLL name (e.g., "BallCellPlugin.dll")
    const auto sharedLibraryName = baseName + "Plugin.dll";
#else
    // Linux/macOS: generate shared library name (e.g., "libBallCellPlugin.so")
    const auto sharedLibraryName = "lib" + baseName + "Plugin.so";
#endif

    return (file.parent_path() / sharedLibraryName).string();
}
// TODO: GB: Use QLibrary to make this multiplatform
// std::string ModelLoader::generateModuleNameForSourceFile(const std::string& cppHeaderFile)
// {
//     std::filesystem::path file(cppHeaderFile);
//     const auto baseName = QString::fromStdString(file.stem().string()) + "Plugin";

//     // QLibrary handles platform-specific prefixes and suffixes
//     QLibrary lib(baseName);

//     // Force only filename generation, do not load
//     const auto libFileName = lib.fileName();

//     return file.parent_path() / libFileName.toStdString();
// }

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

        // Generate single wrapper with platform detection inside
        wrapper << "/** Auto-generated wrapper for " << modelName << " model */\n"
                << "#include <iostream>\n"
                << "#include <memory>\n"
                << "#include <string>\n"
                << "\n"
                << "#ifdef _WIN32\n"
                << "    #include <windows.h>\n"
                << "#endif\n"
                << "\n"
                << "#include \"visualiserProxy/SceneWidgetVisualizerProxy.h\"\n"
                << "#include \"visualiserProxy/SceneWidgetVisualizerFactory.h\"\n"
                << "#include \"" << className << ".h\"\n"
                << "\n"
                << "#define MODEL_NAME \"" << modelName << "\"\n"
                << "\n"
                << "// Platform-specific export declarations\n"
                << "#ifdef _WIN32\n"
                << "    #ifdef BUILDING_DLL\n"
                << "        #define DLL_EXPORT __declspec(dllexport)\n"
                << "    #else\n"
                << "        #define DLL_EXPORT __declspec(dllimport)\n"
                << "    #endif\n"
                << "#else\n"
                << "    #define DLL_EXPORT __attribute__((visibility(\"default\")))\n"
                << "#endif\n"
                << "\n"
                << "extern \"C\"\n"
                << "{\n"
                << "DLL_EXPORT void registerPlugin()\n"
                << "{\n"
                << "    std::cout << \"Registering \" MODEL_NAME \" plugin...\" << std::endl;\n"
                << "\n"
                << "    bool success = SceneWidgetVisualizerFactory::registerModel<" << className << ">(MODEL_NAME);\n"
                << "\n"
                << "    if (success)\n"
                << "    {\n"
                << "        std::cout << \"✓ \" MODEL_NAME \" plugin registered successfully!\" << std::endl;\n"
                << "        std::cout << \"  The model is now available in Model menu\" << std::endl;\n"
                << "    }\n"
                << "    else\n"
                << "    {\n"
                << "        std::cerr << \"✗ Failed to register \" MODEL_NAME \" - name may already exist\" << std::endl;\n"
                << "    }\n"
                << "}\n"
                << "\n"
                << "DLL_EXPORT const char* getPluginInfo()\n"
                << "{\n"
                << "#ifdef _WIN32\n"
                << "    return MODEL_NAME \" Plugin v1.0\\n\"\n"
                << "           \"Auto-generated from directory loader\\n\"\n"
                << "           \"Compatible with: Qt-VTK-viewer 2.x (Windows)\";\n"
                << "#else\n"
                << "    return MODEL_NAME \" Plugin v1.0\\n\"\n"
                << "           \"Auto-generated from directory loader\\n\"\n"
                << "           \"Compatible with: Qt-VTK-viewer 2.x\";\n"
                << "#endif\n"
                << "}\n"
                << "\n"
                << "DLL_EXPORT int getPluginVersion()\n"
                << "{\n"
                << "    return 100; // Version 1.00\n"
                << "}\n"
                << "\n"
                << "DLL_EXPORT const char* getModelName()\n"
                << "{\n"
                << "    return MODEL_NAME;\n"
                << "}\n"
                << "} // extern \"C\"\n";

        wrapper.close();
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << std::format("Error generating wrapper: {}\n", e.what());
        return false;
    }
}
