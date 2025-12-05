/** @file CppModuleBuilder.cpp
 * @brief Implementation of CppModuleBuilder for compiling C++ modules. */

#include <filesystem>
#include <iostream>
#include <sstream>

#include "CppModuleBuilder.h"
#include "process.hpp" // tiny process library
#include "ModelLoader.h"

namespace fs = std::filesystem;


namespace
{
/** @brief Detect C++ standard from compiler
 * @param userStandard User-provided standard (if empty, auto-detect)
 * @return C++ standard string (e.g., "c++17", "c++23") */
std::string detectCppStandard(const std::string& userStandard)
{
    if (! userStandard.empty())
        return userStandard;

#ifdef __cplusplus
    if (__cplusplus >= 202302L)
        return "c++23";
    else if (__cplusplus >= 202002L)
        return "c++20";
    else if (__cplusplus >= 201703L)
        return "c++17";
#endif
    return "c++14";
}

/** @brief Check if a compiler is available in PATH
 * @param compiler Compiler name (e.g., "clang++", "g++")
 * @return true if compiler is available */
bool isCompilerAvailable(const std::string& compiler)
{
    try
    {
        // Try to run compiler with --version
        std::string command = compiler + " --version > /dev/null 2>&1";
        int exitCode = system(command.c_str());
        return exitCode == 0;
    }
    catch (...)
    {
        return false;
    }
}

/** @brief Find an available C++ compiler
 * @param preferredCompiler Preferred compiler (e.g., "clang++")
 * @return Path to available compiler, or empty string if none found */
std::string findAvailableCompiler(const std::string& preferredCompiler)
{
    // Try preferred compiler first
    if (isCompilerAvailable(preferredCompiler))
        return preferredCompiler;

    // Fallback compilers in order of preference
    const std::vector<std::string> fallbacks = {"g++", "clang++", "c++"};
    for (const auto& compiler : fallbacks)
    {
        if (compiler != preferredCompiler && isCompilerAvailable(compiler))
        {
            std::cout << "Preferred compiler '" << preferredCompiler << "' not found, using '"
                      << compiler << "' instead" << std::endl;
            return compiler;
        }
    }

    return ""; // No compiler found
}
} // namespace


namespace viz::plugins
{
CppModuleBuilder::CppModuleBuilder(const std::string& compilerPath,
                                   const std::string& oopencalDir)
    : compilerPath(compilerPath)
    , oopencalDir(oopencalDir)
    , progressCallback(nullptr)
{
    // If oopencalDir is empty, try to get it from environment variable
    if (this->oopencalDir.empty())
    {
        const char* envDir = std::getenv("OOPENCAL_DIR");
        if (envDir)
        {
            this->oopencalDir = envDir;
        }
        else
        {
            this->oopencalDir = OOPENCAL_DIR; // defined in CMake
        }
    }
}

bool CppModuleBuilder::moduleExists(const std::string& outputPath)
{
    return fs::exists(outputPath) && fs::is_regular_file(outputPath);
}

CompilationResult CppModuleBuilder::compileModule(const std::string& sourceFile,
                                                  const std::string& outputFile,
                                                  const std::string& cppStandard)
{
    lastResult = std::make_unique<CompilationResult>();
    lastResult->sourceFile = sourceFile;
    lastResult->outputFile = outputFile;

    // Check if source file exists
    if (! fs::exists(sourceFile))
    {
        lastResult->success = false;
        lastResult->stderr = "Source file does not exist: " + sourceFile;
        return *lastResult;
    }

    // Report progress: checking compiler
    if (progressCallback)
        progressCallback("Checking C++ compiler availability...");

    // Find an available compiler (with fallback support)
    std::string availableCompiler = findAvailableCompiler(compilerPath);
    if (availableCompiler.empty())
    {
        lastResult->success = false;
        lastResult->stderr = "No C++ compiler found. Please install clang++, g++, or c++.";
        lastResult->compileCommand = compilerPath + " (not found)";
        if (progressCallback)
            progressCallback("ERROR: No C++ compiler found");
        return *lastResult;
    }

    // Update compiler path if fallback was used
    if (availableCompiler != compilerPath)
    {
        std::cout << "Using fallback compiler: " << availableCompiler << std::endl;
        if (progressCallback)
            progressCallback("Using fallback compiler: " + availableCompiler);
        compilerPath = availableCompiler;
    }

    // Report progress: building command
    if (progressCallback)
        progressCallback("Preparing compilation command...");

    // Build the compilation command
    lastResult->compileCommand = buildCompileCommand(sourceFile, outputFile, cppStandard);

    std::cout << "Compiling module: " << sourceFile << std::endl;
    std::cout << "Command: " << lastResult->compileCommand << std::endl;

    // Report progress: starting compilation
    if (progressCallback)
        progressCallback("Compilation of module ...");

    // Execute the compilation command with progress reporting
    int lineCount = 0;
    lastResult->exitCode = executeCommand(
        lastResult->compileCommand,
        [this, &lineCount](const std::string& line) { 
            lastResult->stdout += line + "\n";
            // Report compilation progress every 5 lines to avoid too many updates
            if (progressCallback && !line.empty() && (++lineCount % 5 == 0))
                progressCallback("Compiling... (" + std::to_string(lineCount) + " lines)");
        },
        [this](const std::string& line) { 
            lastResult->stderr += line + "\n";
            // Report compilation errors immediately
            if (progressCallback && !line.empty())
                progressCallback("Error: " + line);
        });

    // Check if compilation succeeded
    if (lastResult->exitCode == 0 && moduleExists(outputFile))
    {
        lastResult->success = true;
        std::cout << "✓ Module compiled successfully: " << outputFile << std::endl;
    }
    else
    {
        lastResult->success = false;
        std::cerr << "✗ Compilation failed with exit code: " << lastResult->exitCode << std::endl;
        if (!lastResult->stderr.empty())
        {
            std::cerr << "Error output:\n" << lastResult->stderr << std::endl;
        }
    }

    return *lastResult;
}

std::string CppModuleBuilder::buildCompileCommand(const std::string& sourceFile,
                                                  const std::string& outputFile,
                                                  const std::string& cppStandard) const
{
    // Auto-detect C++ standard if not provided
    std::string standard = detectCppStandard(cppStandard);

    std::ostringstream cmd;
    cmd << compilerPath
        << " -shared"
        << " -fPIC"
        << " -std=" << standard;

    // Add OOpenCAL include path if available
    if (! oopencalDir.empty())
    {
        cmd << " -I\"" << oopencalDir << "/OOpenCAL/base\"";
        cmd << " -I\"" << oopencalDir << '"';
    }

    // Add Qt-VTK-viewer project include paths if available
    if (! projectRootPath.empty())
    {
        std::cout << "Project root path: " << projectRootPath << std::endl;
        cmd << " -I\"" << projectRootPath << "\"";
        cmd << " -I\"" << projectRootPath << "/visualiserProxy\"";
        cmd << " -I\"" << projectRootPath << "/config\"";
    }

    cmd << " " << VTK_COMPILE_FLAGS; // this is set from CMake, temporary solution (#61)

    cmd << " \"" << sourceFile << "\""
        << " -o \"" << outputFile << "\"";

    return cmd.str();
}

int CppModuleBuilder::executeCommand(const std::string& command,
                                      std::function<void(const std::string&)> stdout_callback,
                                      std::function<void(const std::string&)> stderr_callback)
{
    try
    {
        TinyProcessLib::Process process(
            command,
            "",
            [&stdout_callback](const char* bytes, size_t n) {
                if (stdout_callback)
                {
                    stdout_callback(std::string(bytes, n));
                }
            },
            [&stderr_callback](const char* bytes, size_t n) {
                if (stderr_callback)
                {
                    stderr_callback(std::string(bytes, n));
                }
            });

        return process.get_exit_status();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Process execution error: " << e.what() << std::endl;
        return -1;
    }
}
} // namespace viz::plugins
