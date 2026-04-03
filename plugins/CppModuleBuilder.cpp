/** @file CppModuleBuilder.cpp
 * @brief Implementation of CppModuleBuilder for compiling C++ modules. */

#include <filesystem>
#include <iostream>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
#endif

#include "CppModuleBuilder.h"
#include "CompilationConfig.h"
#include "process.hpp" // tiny process library
#include "ModelLoader.h"
#include "data/PerformanceMetrics.h"

namespace fs = std::filesystem;

// Helper function to convert std::string to TinyProcessLib::Process::string_type
namespace
{
std::string getEnvCompilerOverride()
{
    const char* envCompiler = std::getenv("OOPENCAL_COMPILER");
    if (!envCompiler || *envCompiler == '\0')
    {
        return {};
    }
    return std::string(envCompiler);
}

bool looksLikePath(const std::string& value)
{
    return value.find('/') != std::string::npos || value.find('\\') != std::string::npos;
}

bool isExecutableFile(const std::string& path)
{
    std::error_code ec;
    auto st = fs::status(path, ec);
    if (ec || !fs::is_regular_file(st))
    {
        return false;
    }
#ifdef _WIN32
    return true;
#else
    const auto perms = st.permissions();
    return (perms & fs::perms::owner_exec) != fs::perms::none ||
           (perms & fs::perms::group_exec) != fs::perms::none ||
           (perms & fs::perms::others_exec) != fs::perms::none;
#endif
}

std::string quoteIfNeeded(const std::string& path)
{
    if (path.find(' ') == std::string::npos && path.find('"') == std::string::npos)
    {
        return path;
    }
    return "\"" + path + "\"";
}

std::vector<std::string> splitEnvPaths(const char* value)
{
    std::vector<std::string> result;
    if (!value || *value == '\0')
    {
        return result;
    }
    std::string current;
    for (const char* p = value; *p != '\0'; ++p)
    {
        if (*p == ':')
        {
            if (!current.empty())
            {
                result.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(*p);
        }
    }
    if (!current.empty())
    {
        result.push_back(current);
    }
    return result;
}

std::string detectBundledCxxVersionDir(const std::string& includeRoot)
{
    const fs::path cxxRoot = fs::path(includeRoot) / "c++";
    if (!fs::exists(cxxRoot) || !fs::is_directory(cxxRoot))
    {
        return {};
    }

    int bestVersion = -1;
    std::string bestName;
    for (const auto& entry : fs::directory_iterator(cxxRoot))
    {
        if (!entry.is_directory())
        {
            continue;
        }
        const std::string name = entry.path().filename().string();
        bool allDigits = !name.empty() && std::all_of(name.begin(), name.end(), ::isdigit);
        if (!allDigits)
        {
            continue;
        }
        int version = std::stoi(name);
        if (version > bestVersion)
        {
            bestVersion = version;
            bestName = name;
        }
    }

    if (bestVersion < 0)
    {
        return {};
    }
    return (cxxRoot / bestName).string();
}

inline TinyProcessLib::Process::string_type toProcessString(const std::string& str)
{
#ifdef _WIN32
    // On Windows, TinyProcessLib uses std::wstring
    if (str.empty())
        return TinyProcessLib::Process::string_type();
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    TinyProcessLib::Process::string_type wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
#else
    // On Unix-like systems, TinyProcessLib uses std::string
    return str;
#endif
}

/** @brief Find an available C++ compiler (legacy function for compatibility)
 * @param preferredCompiler Preferred compiler (e.g., "clang++")
 * @return Path to available compiler, or empty string if none found */
std::string findAvailableCompiler(const std::string& preferredCompiler)
{
    const std::string envCompiler = getEnvCompilerOverride();

    // If no preferred compiler specified, use intelligent selection from CompilationConfig
    if (preferredCompiler.empty())
    {
        if (!envCompiler.empty() && viz::plugins::isCompilerAvailable(envCompiler))
        {
            return envCompiler;
        }
        return viz::plugins::CompilationConfig::getDefaultCompiler();
    }
    
    // Try preferred compiler first
    if (viz::plugins::isCompilerAvailable(preferredCompiler))
        return preferredCompiler;

    // Try environment override next
    if (!envCompiler.empty() && viz::plugins::isCompilerAvailable(envCompiler))
        return envCompiler;

    // Fallback compilers in order of preference
    const std::vector<std::string> fallbacks = {"g++", "clang++", "c++"};
    for (const auto& compiler : fallbacks)
    {
        if (compiler != preferredCompiler && viz::plugins::isCompilerAvailable(compiler))
        {
            std::cout << "Preferred compiler '" << preferredCompiler << "' not found, using '"
                      << compiler << "' instead" << std::endl;
            return compiler;
        }
    }

    return ""; // No compiler found
}

/// Resolve directory path using environment variable and CMake define
static std::string resolveDirectoryPath(const char* envVarName, const char* cmakeDefineValue)
{
    // 1. Prefer environment variable
    if (envVarName)
    {
        if (const char* envPath = std::getenv(envVarName))
        {
            if (*envPath != '\0')
            {
                return envPath;
            }
        }
    }

    // 2. Use CMake-provided define if available and valid
    if (cmakeDefineValue && *cmakeDefineValue != '\0')
    {
        const std::string path = cmakeDefineValue;

        std::error_code ec;
        if (std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec))
        {
            return path;
        }
    }

    // 3. Nothing available
    return "";
}
} // namespace


namespace viz::plugins
{
CppModuleBuilder::CppModuleBuilder(const std::string& compilerPath,
                                   const std::string& oopencalDir)
    : compilerPath(compilerPath.empty() ? CompilationConfig::getDefaultCompiler() : compilerPath)
    , oopencalDir(oopencalDir.empty() ? CompilationConfig::getInstance().getOopencalDir() : oopencalDir)
    , projectRootPath(CompilationConfig::getInstance().getViewerRootDir())
    , progressCallback(nullptr)
{
    // Log the selected compiler
    if (!this->compilerPath.empty())
    {
        std::cout << "CppModuleBuilder initialized with compiler: " << this->compilerPath << std::endl;
    }
    else
    {
        std::cerr << "WARNING: No C++ compiler found during CppModuleBuilder initialization" << std::endl;
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
    // Create a performance session to track compilation time
    PerformanceSession perfSession("Module Compilation: " + fs::path(sourceFile).filename().string());
    perfSession.setCategory(PerformanceMetrics::MetricsCategory::Compilation);

    lastResult = std::make_unique<CompilationResult>();
    lastResult->sourceFile = sourceFile;
    lastResult->outputFile = outputFile;

    // Check if source file exists
    if (! fs::exists(sourceFile))
    {
        lastResult->success = false;
        lastResult->stdErr = "Source file does not exist: " + sourceFile;
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
        lastResult->stdErr = "No C++ compiler found. Please install clang++, g++, or c++.";
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
            lastResult->stdOut += line + "\n";
            // Report compilation progress every 5 lines to avoid too many updates
            if (progressCallback && !line.empty() && (++lineCount % 5 == 0))
                progressCallback("Compiling... (" + std::to_string(lineCount) + " lines)");
        },
        [this](const std::string& line) { 
            lastResult->stdErr += line + "\n";
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
        if (!lastResult->stdErr.empty())
        {
            std::cerr << "Error output:\n" << lastResult->stdErr << std::endl;
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

    // Get configuration from singleton
    auto& config = CompilationConfig::getInstance();
    
    std::ostringstream cmd;
    cmd << quoteIfNeeded(compilerPath)
        << " " << config.getCompilationFlags()
        << " -std=" << standard;

    const char* compilerInclude = std::getenv("COMPILER_INCLUDEDIR");
    const char* clangResource = std::getenv("CLANG_RESOURCE_INCLUDE");
    const char* compilerLibDir = std::getenv("COMPILER_LIBDIR");
    const char* gccToolchain = std::getenv("COMPILER_GCC_TOOLCHAIN");
    const char* sysroot = std::getenv("COMPILER_SYSROOT");

    if (gccToolchain && *gccToolchain != '\0')
    {
        cmd << " --gcc-toolchain=" << quoteIfNeeded(gccToolchain);
    }
    if (sysroot && *sysroot != '\0')
    {
        cmd << " --sysroot=" << quoteIfNeeded(sysroot);
    }
    if (compilerLibDir && *compilerLibDir != '\0')
    {
        cmd << " -L" << quoteIfNeeded(compilerLibDir);
    }
    if (compilerInclude && *compilerInclude != '\0')
    {
        // Prefer bundled headers over system headers when provided by AppImage
        cmd << " -nostdinc -nostdinc++";
        if (clangResource && *clangResource != '\0')
        {
            cmd << " -isystem " << quoteIfNeeded(clangResource);
        }

        const std::string includeRoot = compilerInclude;
        const std::string cxxDir = detectBundledCxxVersionDir(includeRoot);
        if (!cxxDir.empty())
        {
            cmd << " -isystem " << quoteIfNeeded(cxxDir);
            const std::string cxxArchDir = cxxDir + "/x86_64-linux-gnu";
            if (fs::exists(cxxArchDir))
            {
                cmd << " -isystem " << quoteIfNeeded(cxxArchDir);
            }
        }

        // Add C system headers after C++ dirs so include_next can find them
        cmd << " -isystem " << quoteIfNeeded(includeRoot);
        const std::string archInclude = includeRoot + "/x86_64-linux-gnu";
        if (fs::exists(archInclude))
        {
            cmd << " -isystem " << quoteIfNeeded(archInclude);
        }
    }

    // Add include paths from configuration
    auto includePaths = config.getIncludePaths();
    for (const auto& path : includePaths) {
        cmd << " " << path;
    }

    // Add VTK flags from configuration
    std::string vtkFlags = config.getVtkFlags();
    if (! vtkFlags.empty())
    {
        cmd << " " << vtkFlags;
    }

    cmd << " \"" << sourceFile << "\"" << " -o \"" << outputFile << "\"";

    return cmd.str();
}

int CppModuleBuilder::executeCommand(const std::string& command,
                                     std::function<void(const std::string&)> stdout_callback,
                                     std::function<void(const std::string&)> stderr_callback)
{
    try
    {
        TinyProcessLib::Process process(
            toProcessString(command),
            toProcessString(std::string()),
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

std::string detectCppStandard(const std::string &userStandard)
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
    else if (__cplusplus >= 201402L)
        return "c++14";
    else if (__cplusplus >= 201103L)
        return "c++11";
#endif
    return "c++14";
}

namespace
{
/** @brief Build platform-specific compiler version check command
 * @param compiler Name or path to compiler executable
 * @return Command string to check compiler availability */
std::string buildCompilerCheckCommand(const std::string& compiler)
{
    const auto quotedCompiler = (compiler.find(' ') == std::string::npos && compiler.find('"') == std::string::npos)
                                    ? compiler
                                    : "\"" + compiler + "\"";
#ifdef _WIN32
    if (compiler == "cl")
    {
        // MSVC: use a simple version check
        return "cl 2>nul >nul";
    }
    else
    {
        // Other compilers on Windows
        return quotedCompiler + " --version >nul 2>&1";
    }
#else
    // Linux/macOS: Try to run compiler with --version
    return quotedCompiler + " --version > /dev/null 2>&1";
#endif
}

/** @brief Execute compiler availability check command
 * @param command Command to execute
 * @return true if compiler is available, false otherwise */
bool executeCompilerCheck(const std::string& command)
{
    try
    {
        int exitCode = system(command.c_str());
        return exitCode == 0;
    }
    catch (...)
    {
        return false;
    }
}
} // anonymous namespace

bool isCompilerAvailable(const std::string &compiler)
{
    if (looksLikePath(compiler))
    {
        return isExecutableFile(compiler);
    }
    const std::string command = buildCompilerCheckCommand(compiler);
    return executeCompilerCheck(command);
}

std::string getOopencalDir()
{
#ifdef OOPENCAL_DIR
    return resolveDirectoryPath("OOPENCAL_DIR", OOPENCAL_DIR);
#else
    return resolveDirectoryPath("OOPENCAL_DIR", nullptr);
#endif
}

/// Get the directory containing this executable (project root)
std::string getProjectRootPath()
{
#ifdef OOPENCAL_VIEWER_ROOT
    return resolveDirectoryPath("OOPENCAL_VIEWER_ROOT", OOPENCAL_VIEWER_ROOT);
#else
    return resolveDirectoryPath("OOPENCAL_VIEWER_ROOT", nullptr);
#endif
}
} // namespace viz::plugins
