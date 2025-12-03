/** @file CppModuleBuilder.cpp
 * @brief Implementation of CppModuleBuilder for compiling C++ modules. */

#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>
#include <dirent.h>

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

/** @brief Get clang toolchain path from various sources
 * @return Path to clang++ in AppImage or system, or empty string if not found */
std::string getClangToolchainPath()
{
    // 1. Check environment variable first (set by AppRun script)
    if (const char* envPath = std::getenv("CLANG_TOOLCHAIN_PATH"))
    {
        std::string path = envPath;
        if (! path.empty())
        {
            std::string clangPath = path + "/clang++";
            if (fs::exists(clangPath))
            {
                return clangPath;
            }
        }
    }

    // 2. Check CMake-provided compile definition (for local builds)
#ifdef CLANG_TOOLCHAIN_PATH
    {
        std::string path = CLANG_TOOLCHAIN_PATH;
        if (! path.empty())
        {
            std::string clangPath = path + "/clang++";
            if (fs::exists(clangPath))
            {
                return clangPath;
            }
        }
    }
#endif

    // 3. Check common AppImage paths
    const std::vector<std::string> appImagePaths = {
        "../usr/bin/clang++",
        "../../usr/bin/clang++",
        "/usr/local/bin/clang++",
        "/opt/clang/bin/clang++"
    };
    
    for (const auto& path : appImagePaths)
    {
        if (fs::exists(path))
        {
            return path;
        }
    }

    // 4. Return empty - will use system clang++ or fallback
    return "";
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

    // Try to use clang from AppImage first
    std::string clangFromAppImage = getClangToolchainPath();
    std::string availableCompiler;
    
    if (! clangFromAppImage.empty())
    {
        // If clang from AppImage is found, use it directly without testing
        // (testing may fail due to missing libraries in AppImage mount point)
        std::cout << "Using clang from AppImage: " << clangFromAppImage << std::endl;
        if (progressCallback)
            progressCallback("Using clang from AppImage: " + clangFromAppImage);
        
        // Resolve relative path to absolute path
        try
        {
            std::string absolutePath = fs::absolute(clangFromAppImage).string();
            std::cout << "Resolved to absolute path: " << absolutePath << std::endl;
            availableCompiler = absolutePath;
        }
        catch (const std::exception& e)
        {
            std::cout << "Warning: Could not resolve path to absolute: " << e.what() << std::endl;
            availableCompiler = clangFromAppImage;
        }
    }
    else
    {
        // No clang from AppImage, find an available compiler (with fallback support)
        availableCompiler = findAvailableCompiler(compilerPath);
    }
    
    if (availableCompiler.empty())
    {
        lastResult->success = false;
        lastResult->stderr = "No C++ compiler found. Please install clang++, g++, or c++.";
        lastResult->compileCommand = compilerPath + " (not found)";
        if (progressCallback)
            progressCallback("ERROR: No C++ compiler found");
        return *lastResult;
    }
    
    // Update compiler path to the one we're actually using
    compilerPath = availableCompiler;

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
    
    // If using clang from AppImage, we need to set LD_LIBRARY_PATH for it to find its libraries
    // RPATH doesn't work for subprocess, so we need to set it explicitly
    if (compilerPath.find(".mount_") != std::string::npos ||
        compilerPath.find("/tmp/") != std::string::npos)
    {
        // Extract the base path (e.g., /tmp/.mount_OOpenCxxx)
        std::string basePath = compilerPath.substr(0, compilerPath.find_last_of('/'));
        std::string libPath = basePath + "/../lib/clang-libs:" + basePath + "/../lib";
        
        // Wrap command to set LD_LIBRARY_PATH
        cmd << "LD_LIBRARY_PATH=\"" << libPath << ":$LD_LIBRARY_PATH\" ";
    }
    
    cmd << compilerPath
        << " -shared"
        << " -fPIC"
        << " -std=" << standard;

    // If using clang from AppImage, disable system include paths and use only bundled headers
    // This prevents clang from finding conflicting system headers
    if (compilerPath.find(".mount_") != std::string::npos ||
        compilerPath.find("/tmp/") != std::string::npos)
    {
        cmd << " -nostdinc"      // Disable standard C include paths
             << " -nostdinc++";   // Disable standard C++ include paths
    }

    // If using clang from AppImage, add system include paths from AppImage
    // Clang from AppImage needs to find headers bundled in the AppImage
    if (compilerPath.find(".mount_") != std::string::npos ||
        compilerPath.find("/tmp/") != std::string::npos)
    {
        // Extract the base path (e.g., /tmp/.mount_OOpenCxxx)
        std::string basePath = compilerPath.substr(0, compilerPath.find_last_of('/'));
        
        // IMPORTANT: Resolve relative paths to absolute paths so clang finds them correctly
        // Extract mount point from basePath (e.g., /tmp/.mount_OOpenCeboWkw from /tmp/.mount_OOpenCeboWkw/usr/bin)
        try
        {
            // basePath is like: /tmp/.mount_OOpenCeboWkw/usr/bin
            // We need to go up 2 levels to get: /tmp/.mount_OOpenCeboWkw
            std::string mountPoint = basePath;
            size_t lastSlash = mountPoint.find_last_of('/');  // Remove /bin
            if (lastSlash != std::string::npos)
                mountPoint = mountPoint.substr(0, lastSlash);
            lastSlash = mountPoint.find_last_of('/');  // Remove /usr
            if (lastSlash != std::string::npos)
                mountPoint = mountPoint.substr(0, lastSlash);
            
            std::cout << "DEBUG: Mount point = " << mountPoint << std::endl;
            
            // Now construct absolute paths directly
            std::string appIncludePath = mountPoint + "/usr/include";
            std::string clangIncludePath = mountPoint + "/usr/lib/clang/21/include";
            std::string gccIncludePath14 = mountPoint + "/usr/lib/gcc/x86_64-linux-gnu/14/include";
            std::string gccIncludeFixedPath14 = mountPoint + "/usr/lib/gcc/x86_64-linux-gnu/14/include-fixed";
            std::string gccIncludePath15 = mountPoint + "/usr/lib64/gcc/x86_64-pc-linux-gnu/15.2.1/include";
            std::string gccIncludeFixedPath15 = mountPoint + "/usr/lib64/gcc/x86_64-pc-linux-gnu/15.2.1/include-fixed";
            
            std::cout << "DEBUG: Resolved AppImage include paths:" << std::endl;
            std::cout << "  appIncludePath = " << appIncludePath << std::endl;
            std::cout << "  clangIncludePath = " << clangIncludePath << std::endl;
            
            // Add AppImage bundled system include paths
            // CRITICAL ORDER: musl C headers MUST come before libc++ headers so libc++ can find them
            cmd << " -isystem \"" << appIncludePath << "\""  // musl C headers (stdio.h, stdlib.h, etc.)
                << " -isystem \"" << appIncludePath << "/x86_64-linux-gnu\""
                << " -isystem \"" << appIncludePath << "/c++/v1\""  // LLVM libc++ C++ headers (uses musl C headers above)
                << " -isystem \"" << appIncludePath << "/c++\""
                << " -isystem \"" << appIncludePath << "/x86_64-unknown-linux-gnu/c++/v1\"";  // Platform-specific C++ config (__config_site)
            
            // Add LLVM clang intrinsics headers (from LLVM, not system GCC)
            cmd << " -isystem \"" << clangIncludePath << "\"";
            
            // Also add GCC include paths from AppImage as fallback (for intrinsics, etc.)
            // These should be AFTER clang intrinsics to avoid conflicts
            cmd << " -isystem \"" << gccIncludePath14 << "\""
                << " -isystem \"" << gccIncludeFixedPath14 << "\""
                << " -isystem \"" << gccIncludePath15 << "\""
                << " -isystem \"" << gccIncludeFixedPath15 << "\"";
        }
        catch (const std::exception& e)
        {
            std::cout << "ERROR: Failed to resolve AppImage include paths: " << e.what() << std::endl;
        }
    }

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
    else
    {
        // Try to get project root from environment variable (set by AppRun script)
        if (const char* viewerRoot = std::getenv("OOPENCAL_VIEWER_ROOT"))
        {
            std::string viewerRootPath = viewerRoot;
            if (! viewerRootPath.empty())
            {
                std::cout << "Project root path from OOPENCAL_VIEWER_ROOT: " << viewerRootPath << std::endl;
                cmd << " -I\"" << viewerRootPath << "\"";
                cmd << " -I\"" << viewerRootPath << "/visualiserProxy\"";
                cmd << " -I\"" << viewerRootPath << "/config\"";
                cmd << " -I\"" << viewerRootPath << "/utilities\"";
                cmd << " -I\"" << viewerRootPath << "/visualiser\"";
                cmd << " -I\"" << viewerRootPath << "/widgets\"";
            }
        }
    }
    
    // Add VTK include path
    // First check if we're in AppImage, then check system
    if (compilerPath.find(".mount_") != std::string::npos ||
        compilerPath.find("/tmp/") != std::string::npos)
    {
        // Try to find VTK in AppImage
        std::string basePath = compilerPath.substr(0, compilerPath.find_last_of('/'));
        std::string mountPoint = basePath;
        size_t lastSlash = mountPoint.find_last_of('/');
        if (lastSlash != std::string::npos)
            mountPoint = mountPoint.substr(0, lastSlash);
        lastSlash = mountPoint.find_last_of('/');
        if (lastSlash != std::string::npos)
            mountPoint = mountPoint.substr(0, lastSlash);
        
        // Look for VTK in AppImage include directory
        std::string vtkIncludePath = mountPoint + "/usr/include";
        // We'll add this as a regular -I flag since VTK headers are in AppImage
        cmd << " -I\"" << vtkIncludePath << "/vtk-9.1\"";
    }
    else
    {
        // For system clang, add system VTK path
        cmd << " -I/usr/include/vtk-9.1";
    }
    
    // Add static-clang intrinsics headers (if using AppImage clang)
    if (compilerPath.find(".mount_") != std::string::npos ||
        compilerPath.find("/tmp/") != std::string::npos)
    {
        // Extract mount point
        std::string basePath = compilerPath.substr(0, compilerPath.find_last_of('/'));
        std::string mountPoint = basePath;
        size_t lastSlash = mountPoint.find_last_of('/');
        if (lastSlash != std::string::npos)
            mountPoint = mountPoint.substr(0, lastSlash);
        lastSlash = mountPoint.find_last_of('/');
        if (lastSlash != std::string::npos)
            mountPoint = mountPoint.substr(0, lastSlash);
        
        // Try to find clang intrinsics - try v17 first (static-clang), then any version
        std::string clangIntrinsicsPath = mountPoint + "/usr/lib/clang/17/include";
        
        // Check if v17 exists, otherwise try to find any version
        std::ifstream checkV17(clangIntrinsicsPath + "/stddef.h");
        if (!checkV17.good())
        {
            // Try to find any clang version in lib/clang/
            std::string clangLibPath = mountPoint + "/usr/lib/clang";
            DIR* dir = opendir(clangLibPath.c_str());
            if (dir)
            {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr)
                {
                    if (entry->d_type == DT_DIR && entry->d_name[0] != '.')
                    {
                        clangIntrinsicsPath = clangLibPath + "/" + entry->d_name + "/include";
                        break;
                    }
                }
                closedir(dir);
            }
        }
        
        cmd << " -isystem \"" << clangIntrinsicsPath << "\"";
    }
    
    // If using clang from AppImage, also add project headers bundled in AppImage
    std::cout << "DEBUG: compilerPath = " << compilerPath << std::endl;
    if (compilerPath.find(".mount_") != std::string::npos ||
        compilerPath.find("/tmp/") != std::string::npos)
    {
        std::cout << "DEBUG: Detected AppImage clang, adding bundled headers" << std::endl;
        // Extract the base path (e.g., /tmp/.mount_OOpenCxxx/usr/bin)
        std::string binPath = compilerPath.substr(0, compilerPath.find_last_of('/'));
        std::cout << "DEBUG: binPath = " << binPath << std::endl;
        
        // Extract mount point from binPath (e.g., /tmp/.mount_OOpenCeboWkw from /tmp/.mount_OOpenCeboWkw/usr/bin)
        try
        {
            std::string mountPoint = binPath;
            size_t lastSlash = mountPoint.find_last_of('/');  // Remove /bin
            if (lastSlash != std::string::npos)
                mountPoint = mountPoint.substr(0, lastSlash);
            lastSlash = mountPoint.find_last_of('/');  // Remove /usr
            if (lastSlash != std::string::npos)
                mountPoint = mountPoint.substr(0, lastSlash);
            
            std::string appIncludePath = mountPoint + "/usr/include";
            std::cout << "DEBUG: appIncludePath = " << appIncludePath << std::endl;
            
            // Add bundled project headers from AppImage
            cmd << " -I\"" << appIncludePath << "\"";
            cmd << " -I\"" << appIncludePath << "/visualiserProxy\"";
            cmd << " -I\"" << appIncludePath << "/config\"";
            cmd << " -I\"" << appIncludePath << "/utilities\"";
            cmd << " -I\"" << appIncludePath << "/visualiser\"";
            cmd << " -I\"" << appIncludePath << "/widgets\"";
        }
        catch (const std::exception& e)
        {
            std::cout << "DEBUG: Error resolving AppImage include path: " << e.what() << std::endl;
        }
    }
    else
    {
        std::cout << "DEBUG: Not AppImage clang, skipping bundled headers" << std::endl;
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
