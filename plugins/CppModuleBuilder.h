/** @file CppModuleBuilder.h
 * @brief Builder for compiling C++ model modules into shared libraries.
 *
 * This class handles automatic compilation of C++ model files into shared libraries
 * (.so files) using the Tiny Process Library to execute the compiler. */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "PrecompiledHeader.h"

namespace viz::plugins
{
// Forward declaration - CompilationResult is defined in ModelLoader.h
struct CompilationResult;

/** @class CppModuleBuilder
 * @brief Compiles C++ model files into shared libraries.
 *
 * This class provides functionality to:
 * - Check if a compiled module already exists
 * - Compile C++ header files into shared libraries
 * - Capture compiler output and errors
 * - Report compilation results
 * - Report progress via callback
 *
 * Example usage:
 * @code
 * CppModuleBuilder builder;
 * builder.setProgressCallback([](const std::string& msg) {
 *     std::cout << "Progress: " << msg << std::endl;
 * });
 * auto result = builder.compileModule("/path/to/XCell.h", "/path/to/output.so");
 * if (!result.success) {
 *     std::cerr << "Compilation failed:\n" << result.stderr << std::endl;
 * }
 * @endcode */
class CppModuleBuilder
{
public:
    /// Callback type for progress reporting
    using ProgressCallback = std::function<void(const std::string&)>;
    /** @brief Create a new CppModuleBuilder instance
     * @param compilerPath Path to the compiler (e.g., "clang++"). If empty, intelligently selects the best available compiler (preferring the one used to build the application).
     * @param oopencalDir Path to OOpenCAL base directory for includes. If empty, uses OOPENCAL_DIR env var. */
    explicit CppModuleBuilder(const std::string& compilerPath = "",
                              const std::string& oopencalDir = "");

    /** @brief Check if a compiled module already exists
     * @param outputPath Path to the .so file
     * @return true if the file exists and is readable */
    static bool moduleExists(const std::string& outputPath);

    /** @brief Compile a C++ module
     * @param sourceFile Path to the C++ header file (.h)
     * @param outputFile Path where the .so file should be created
     * @param cppStandard C++ standard to use (e.g., "c++17", "c++23"). If empty, auto-detects from __cplusplus
     * @return CompilationResult with success status and output */
    CompilationResult compileModule(const std::string& sourceFile,
                                    const std::string& outputFile,
                                    const std::string& cppStandard = "");

    /** @brief Build the precompiled header which speeds up compilation of models
     *
     * It precompiles the headers which every generated wrapper includes (standard library, VTK, Viewer templates,
     * OOpenCAL Cell.h) with the same compiler and flags which compileModule() uses. compileModule() uses the result
     * automatically as long as it is valid for the current settings and falls back to a normal compilation otherwise.
     * Existing results in the directory are replaced; a failed build leaves nothing usable behind.
     * @param outputDirectory Directory for OOpenCalViewerPrecompiled.h, its .pch/.gch and the manifest (created if missing)
     * @param cppStandard C++ standard to use. If empty, auto-detects from __cplusplus
     * @return CompilationResult with success status, the command and the compiler output */
    CompilationResult buildPrecompiledHeader(const std::string& outputDirectory,
                                             const std::string& cppStandard = "");

    /** @brief Get the last compilation result
     * @return Pointer to the last result, or nullptr if no compilation has been done */
    const CompilationResult* getLastResult() const
    {
        return lastResult.get();
    }

    /** @brief Set the compiler path
     * @param path Path to the compiler executable */
    void setCompilerPath(const std::string& path)
    {
        compilerPath = path;
    }

    /** @brief Get the current compiler path */
    const std::string& getCompilerPath() const
    {
        return compilerPath;
    }

    /** @brief Set the project root directory (for include paths)
     * @param path Path to the OOpenCal-Viewer project root */
    void setProjectRootPath(const std::string& path)
    {
        projectRootPath = path;
    }

    /** @brief Get the project root path */
    const std::string& getProjectRootPath() const
    {
        return projectRootPath;
    }

    /** @brief Set progress callback for compilation updates
     * @param callback Function to call with progress messages */
    void setProgressCallback(ProgressCallback callback)
    {
        progressCallback = callback;
    }

private:
    std::string compilerPath;
    std::string oopencalDir;
    std::string projectRootPath;
    ProgressCallback progressCallback;
    std::unique_ptr<CompilationResult> lastResult;

    /// Describes how the precompiled header is used by one compilation
    struct PrecompiledHeaderUse
    {
        pch::CompilerFamily family = pch::CompilerFamily::Unknown;
        std::string directory;      ///< Directory with the precompiled header
        std::string relocationRoot; ///< Root of the bundled toolchain (empty for a system compiler)
    };

    /** @brief Decide whether the configured precompiled header can be used by the current compiler and settings
     * @param standard C++ standard which is going to be used
     * @return How to use it, or nothing (the reason is logged unless no precompiled header was built at all) */
    std::optional<PrecompiledHeaderUse> selectPrecompiledHeader(const std::string& standard) const;

    /** @brief Build the part of the command shared by compiling a module and building the precompiled header:
     *         compiler, flags, standard, toolchain options, include paths and VTK flags
     * @param standard C++ standard
     * @param forPrecompiledHeader If true, flags which matter only for linking (-shared) are left out
     * @return The command without the source file, the output file and the precompiled header options */
    std::string buildBaseCommand(const std::string& standard, bool forPrecompiledHeader) const;

    /** @brief Build the compilation command
     * @param sourceFile Source file path
     * @param outputFile Output file path
     * @param cppStandard C++ standard
     * @param precompiledHeader Precompiled header to use, or nullptr to compile without it
     * @return The complete compilation command */
    std::string buildCompileCommand(const std::string& sourceFile,
                                    const std::string& outputFile,
                                    const std::string& cppStandard,
                                    const PrecompiledHeaderUse* precompiledHeader = nullptr) const;

    /** @brief Execute a system command and capture output
     * @param command The command to execute
     * @param stdout_callback Callback for stdout data
     * @param stderr_callback Callback for stderr data
     * @return Exit code of the process */
    int executeCommand(const std::string& command,
                       std::function<void(const std::string&)> stdout_callback,
                       std::function<void(const std::string&)> stderr_callback);
};

/** @brief Detect C++ standard from compiler
 * @param userStandard User-provided standard (if empty, auto-detect)
 * @return C++ standard string (e.g., "c++17", "c++23") */
std::string detectCppStandard(const std::string& userStandard={});

/** @brief Check if a compiler is available in PATH
 * @param compiler Compiler name (e.g., "clang++", "g++")
 * @return true if compiler is available */
bool isCompilerAvailable(const std::string& compiler);

std::string getOopencalDir();

std::string getProjectRootPath();
} // namespace viz::plugins
