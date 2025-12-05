/** @file CppModuleBuilder.h
 * @brief Builder for compiling C++ model modules into shared libraries.
 *
 * This class handles automatic compilation of C++ model files into shared libraries
 * (.so files) using the Tiny Process Library to execute the compiler. */

#pragma once

#include <functional>
#include <memory>
#include <string>


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
     * @param compilerPath Path to the compiler (e.g., "clang++"). If empty, uses system PATH.
     * @param oopencalDir Path to OOpenCAL base directory for includes. If empty, uses OOPENCAL_DIR env var. */
    explicit CppModuleBuilder(const std::string& compilerPath = "clang++",
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

    /** @brief Build the compilation command
     * @param sourceFile Source file path
     * @param outputFile Output file path
     * @param cppStandard C++ standard
     * @return The complete compilation command */
    std::string buildCompileCommand(const std::string& sourceFile,
                                    const std::string& outputFile,
                                    const std::string& cppStandard) const;

    /** @brief Execute a system command and capture output
     * @param command The command to execute
     * @param stdout_callback Callback for stdout data
     * @param stderr_callback Callback for stderr data
     * @return Exit code of the process */
    int executeCommand(const std::string& command,
                       std::function<void(const std::string&)> stdout_callback,
                       std::function<void(const std::string&)> stderr_callback);
};
} // namespace viz::plugins
