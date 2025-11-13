/** @file ModelLoader.h
 * @brief Loader for OOpenCAL models from directories with automatic compilation.
 *
 * This class handles loading OOpenCAL models from directories containing
 * Header.txt configuration files and C++ source code. */

#pragma once

#include <string>
#include <memory>
#include <optional>

class Config;

namespace viz::plugins
{
class CppModuleBuilder;

/** @struct CompilationResult
 * @brief Result of a compilation attempt. */
struct CompilationResult
{
    bool success = false;           ///< Whether compilation succeeded
    int exitCode = -1;              ///< Compiler exit code
    std::string stdout;             ///< Compiler standard output
    std::string stderr;             ///< Compiler error output
    std::string sourceFile;         ///< Source file that was compiled
    std::string outputFile;         ///< Output .so file path
    std::string compileCommand;     ///< The actual compile command used
};
}

/** @class ModelLoader
 * @brief Loads OOpenCAL models from directories with automatic compilation.
 *
 * This class provides functionality to:
 * - Validate model directory structure
 * - Parse Header.txt configuration using existing Config class
 * - Find C++ model source files
 * - Compile models if needed using CppModuleBuilder
 * - Return compilation results for error handling
 *
 * Example usage:
 * @code
 * ModelLoader loader;
 * auto result = loader.loadModelFromDirectory("/path/to/model");
 * if (result.success) {
 *     std::string modulePath = result.compiledModulePath;
 * } else {
 *     std::cerr << "Compilation failed: " << result.stderr << std::endl;
 * }
 * @endcode */
class ModelLoader
{
public:
    /** @struct LoadResult
     * @brief Result of model loading attempt. */
    struct LoadResult
    {
        bool success = false;                    ///< Whether loading succeeded
        std::string compiledModulePath;          ///< Path to compiled .so file
        std::string outputFileName;              ///< Output file name from Header.txt (e.g., "ball")
        std::string pluginModelName;             ///< Model name from plugin's getModelName() (e.g., "Ball Model")
        std::shared_ptr<Config> config;          ///< Parsed configuration
        std::optional<viz::plugins::CompilationResult> compilationResult; ///< Compilation details if compilation was attempted
    };

    /** @brief Create a new ModelLoader instance */
    ModelLoader();
    ~ModelLoader();

    /** @brief Load a model from a directory
     * @param modelDirectory Path to directory containing Header.txt and model source
     * @return LoadResult with success status and details */
    LoadResult loadModelFromDirectory(const std::string& modelDirectory);

    /** @brief Get the C++ module builder
     * @return Pointer to the CppModuleBuilder instance */
    viz::plugins::CppModuleBuilder* getBuilder() { return builder.get(); }

private:
    std::unique_ptr<viz::plugins::CppModuleBuilder> builder;

    /** @brief Validate directory structure
     * @param modelDirectory Directory to validate
     * @return true if directory contains Header.txt and at least one .h file */
    bool validateDirectory(const std::string& modelDirectory);

    /** @brief Find C++ header file in directory
     * @param modelDirectory Directory to search
     * @return Path to first .h file found, or empty string if none found */
    std::string findHeaderFile(const std::string& modelDirectory);

    /** @brief Check if compiled module exists and is up-to-date
     * @param outputPath Path to the .so file
     * @return true if file exists and is readable */
    bool moduleExists(const std::string& outputPath);

    /** @brief Generate wrapper code for the model
     * @param modelName Name of the model
     * @param wrapperPath Path where wrapper file should be created
     * @return true if wrapper was generated successfully */
    bool generateWrapper(const std::string& wrapperPath, const std::string& modelName, const std::string& className);
};
