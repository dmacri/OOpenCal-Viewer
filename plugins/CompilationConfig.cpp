/** @file CompilationConfig.cpp
 * @brief Implementation of CompilationConfig singleton for managing compilation settings. */

#include "CompilationConfig.h"
#include "CppModuleBuilder.h" // for isCompilerAvailable()

#include <sstream>
#include <cstdlib>
#include <unordered_set>
#include <filesystem>
#include <sstream>
#include <vector>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QRegularExpression>

namespace
{
/** @brief Extract compiler version from compiler executable using QProcess
 * @param compilerName Name or path to compiler executable
 * @return Version string (e.g., "15.2.0") or empty string if extraction failed */
std::string extractCompilerVersion(const std::string& compilerName)
{
    QProcess process;
    process.start(QString::fromStdString(compilerName), QStringList() << "--version");
    
    if (!process.waitForFinished(1000)) // 1 second timeout
        return "";
    
    QString output = process.readAllStandardOutput();
    if (output.isEmpty())
        return "";
    
    // Get first line of output
    QString firstLine = output.split('\n').first();
    
    // Use regex to extract version number (e.g., "15.2.0" or "11.4.0")
    // Pattern matches: one or more digits, followed by dot and digits, optionally repeated
    QRegularExpression versionRegex(R"((\d+\.\d+(?:\.\d+)?))");
    QRegularExpressionMatch match = versionRegex.match(firstLine);
    
    if (match.hasMatch())
    {
        return match.captured(1).toStdString();
    }
    
    return "";
}
} // anonymous namespace

namespace
{
std::string getEnvCompilerOverride();
} // anonymous namespace

namespace viz::plugins
{
std::pair<std::string, std::string> CompilationConfig::getBuildCompilerInfo()
{
#if defined(__clang__)
    std::ostringstream version;
    version << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
    return {"clang++", version.str()};
#elif defined(__GNUC__) || defined(__GNUG__)
    std::ostringstream version;
    version << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
    return {"g++", version.str()};
#elif defined(_MSC_VER)
    std::ostringstream version;
    version << _MSC_VER;
    // Decode version to readable format
    if (_MSC_VER >= 1940)
        version << " (VS 2022 17.10+)";
    else if (_MSC_VER >= 1930)
        version << " (VS 2022 17.0-17.9)";
    else if (_MSC_VER >= 1920)
        version << " (VS 2019)";
    else if (_MSC_VER >= 1910)
        version << " (VS 2017)";
    return {"cl", version.str()};
#else
    return {"", ""};
#endif
}

std::string CompilationConfig::getBuildCompilerInfoString()
{
    auto [name, version] = getBuildCompilerInfo();
    if (name.empty())
        return "Unknown compiler";
    return name + " " + version;
}

std::string CompilationConfig::getDefaultCompiler()
{
    auto [buildCompiler, buildVersion] = getBuildCompilerInfo();
    const std::string envCompiler = getEnvCompilerOverride();
    if (!envCompiler.empty() && isCompilerAvailable(envCompiler))
    {
        return envCompiler;
    }
    
#ifdef _WIN32
    // Windows: prioritize MSVC (cl.exe) then check for clang/gcc
    if (!buildCompiler.empty() && isCompilerAvailable(buildCompiler))
    {
        return buildCompiler;
    }
    
    // Try cl.exe first (Visual Studio compiler)
    if (isCompilerAvailable("cl"))
    {
        return "cl";
    }
    
    // Fallback to clang/gcc on Windows
    const std::vector<std::string> fallbacks = {"clang", "clang++", "gcc", "g++"};
    for (const auto& compiler : fallbacks)
    {
        if (isCompilerAvailable(compiler))
        {
            return compiler;
        }
    }
    
    return "cl"; // Default to cl.exe (will show as unavailable if not found)
#else
    // Linux/macOS: Priority 1: Try exact compiler with version (e.g., "clang++-15")
    if (!buildCompiler.empty() && !buildVersion.empty())
    {
        // Extract major version
        std::string majorVersion = buildVersion.substr(0, buildVersion.find('.'));
        std::string compilerWithVersion = buildCompiler + "-" + majorVersion;
        
        if (isCompilerAvailable(compilerWithVersion))
        {
            return compilerWithVersion;
        }
    }
    
    // Priority 2: Try same compiler family without version (e.g., "clang++" or "g++")
    if (!buildCompiler.empty() && isCompilerAvailable(buildCompiler))
    {
        return buildCompiler;
    }
    
    // Priority 3: Try other common compilers
    const std::vector<std::string> fallbacks = {"g++", "clang++", "c++"};
    for (const auto& compiler : fallbacks)
    {
        if (compiler != buildCompiler && isCompilerAvailable(compiler))
        {
            return compiler;
        }
    }
    
    // Fallback to clang++ if nothing found (will show as unavailable in UI)
    return "clang++";
#endif
}

namespace
{
/** @brief Get platform-specific compiler families to search
 * @return Vector of compiler family names for current platform */
std::vector<std::string> getPlatformCompilerFamilies()
{
#ifdef _WIN32
    return {"cl", "clang", "clang++", "gcc", "g++"};
#else
    return {"g++", "clang++", "c++"};
#endif
}

/** @brief Check if compiler should be considered versioned on this platform
 * @return true if platform supports versioned compiler variants (e.g., g++-11) */
bool supportsVersionedCompilers()
{
#ifdef _WIN32
    return false; // Windows typically doesn't use versioned compiler names
#else
    return true;  // Linux/macOS commonly use versioned compilers
#endif
}

std::string getEnvCompilerOverride()
{
    const char* envCompiler = std::getenv("OOPENCAL_COMPILER");
    if (!envCompiler || *envCompiler == '\0')
    {
        return {};
    }
    return std::string(envCompiler);
}

/** @brief Create compiler info structure for detected compiler
 * @param name Compiler name
 * @param buildCompiler Compiler used to build the application
 * @param buildVersion Version of build compiler
 * @return CompilerInfo structure */
CompilationConfig::CompilerInfo createCompilerInfo(
    const std::string& name,
    const std::string& buildCompiler,
    const std::string& buildVersion)
{
    CompilationConfig::CompilerInfo info;
    info.name = name;
    info.family = name;
    info.fullPath = name;
    info.isAvailable = true;
    
    // Determine if this is family match
    info.isFamilyMatch = (name == buildCompiler || 
                         (name == "cl" && buildCompiler == "cl"));
    
    // Extract version and determine exact match
    info.version = extractCompilerVersion(name);
    if (info.isFamilyMatch && !buildVersion.empty() && !info.version.empty())
    {
        std::string buildMajorVersion = buildVersion.substr(0, buildVersion.find('.'));
        std::string detectedMajor = info.version.substr(0, info.version.find('.'));
        info.isExactMatch = (detectedMajor == buildMajorVersion);
    }
    else
    {
        info.isExactMatch = info.isFamilyMatch;
    }
    
    return info;
}

CompilationConfig::CompilerInfo createCompilerInfoFromPath(
    const std::string& compilerPath,
    const std::string& buildCompiler,
    const std::string& buildVersion)
{
    CompilationConfig::CompilerInfo info;
    info.name = compilerPath;
    info.fullPath = compilerPath;
    info.family = std::filesystem::path(compilerPath).filename().string();
    info.isAvailable = true;

    info.isFamilyMatch = (info.family == buildCompiler ||
                         (info.family == "cl" && buildCompiler == "cl"));

    info.version = extractCompilerVersion(compilerPath);
    if (info.isFamilyMatch && !buildVersion.empty() && !info.version.empty())
    {
        std::string buildMajorVersion = buildVersion.substr(0, buildVersion.find('.'));
        std::string detectedMajor = info.version.substr(0, info.version.find('.'));
        info.isExactMatch = (detectedMajor == buildMajorVersion);
    }
    else
    {
        info.isExactMatch = info.isFamilyMatch;
    }

    return info;
}
} // anonymous namespace

std::vector<CompilationConfig::CompilerInfo> CompilationConfig::detectAvailableCompilers()
{
    std::vector<CompilerInfo> compilers;
    auto [buildCompiler, buildVersion] = getBuildCompilerInfo();
    const std::string envCompiler = getEnvCompilerOverride();
    
    const std::vector<std::string> families = getPlatformCompilerFamilies();
    const bool useVersionedVariants = supportsVersionedCompilers();

    if (!envCompiler.empty() && isCompilerAvailable(envCompiler))
    {
        compilers.push_back(createCompilerInfoFromPath(envCompiler, buildCompiler, buildVersion));
    }

    std::vector<CompilerInfo> detectedCompilers;
    for (const auto& family : families)
    {
        if (isCompilerAvailable(family))
        {
            detectedCompilers.push_back(createCompilerInfo(family, buildCompiler, buildVersion));
        }
        
        // Add versioned variants only on platforms that support them
        if (useVersionedVariants)
        {
            for (int version = 8; version <= 20; ++version)
            {
                std::string versionedCompiler = family + "-" + std::to_string(version);
                
                if (isCompilerAvailable(versionedCompiler))
                {
                    detectedCompilers.push_back(createCompilerInfo(versionedCompiler, buildCompiler, buildVersion));
                }
            }
        }
    }
    
    // Sort compilers by priority: exact match > family match > others (newest first)
    std::sort(detectedCompilers.begin(), detectedCompilers.end(), [](const CompilerInfo& a, const CompilerInfo& b) {
        if (a.isExactMatch != b.isExactMatch)
            return a.isExactMatch > b.isExactMatch;
        if (a.isFamilyMatch != b.isFamilyMatch)
            return a.isFamilyMatch > b.isFamilyMatch;
        return a.version > b.version;
    });

    if (!detectedCompilers.empty())
    {
        compilers.insert(compilers.end(), detectedCompilers.begin(), detectedCompilers.end());
    }

    return compilers;
}

CompilationConfig& CompilationConfig::getInstance()
{
    static CompilationConfig instance;
    return instance;
}

std::string CompilationConfig::getCompilationFlags() const
{
    if (m_state.compilationFlagsOverride.has_value())
    {
        return m_state.compilationFlagsOverride.value();
    }
    return getDefaultCompilationFlags();
}

std::string CompilationConfig::getDefaultCompilationFlags() const
{
#ifdef _WIN32
    // Windows: Use DLL export flags for Visual Studio
    return "/LD /DBUILDING_DLL /EHsc";
#else
    // Linux/macOS: Use shared library flags
    return "-shared -fPIC";
#endif
}

void CompilationConfig::setCompilationFlags(const std::string& flags)
{
    if (flags.empty())
    {
        m_state.compilationFlagsOverride.reset();
    } 
    else 
    {
        m_state.compilationFlagsOverride = flags;
    }
}

void CompilationConfig::resetCompilationFlags()
{
    m_state.compilationFlagsOverride.reset();
}

std::string CompilationConfig::getVtkFlags() const
{
    if (m_state.vtkFlagsOverride.has_value()) {
        return m_state.vtkFlagsOverride.value();
    }
    return getDefaultVtkFlags();
}

std::string CompilationConfig::getVtkIncludePaths() const
{
    if (m_state.vtkFlagsOverride.has_value())
    {
        // If override is set, we need to extract paths from the override
        std::string override = m_state.vtkFlagsOverride.value();
        std::stringstream ss(override);
        std::string item;
        std::vector<std::string> paths;
        
        while (ss >> item)
        {
            if (item.substr(0, 2) == "-I")
            {
                paths.push_back(item.substr(2));
            }
        }
        
        if (paths.empty())
            return override; // Return as-is if no -I flags found
        
        std::string result = paths[0];
        for (size_t i = 1; i < paths.size(); ++i) {
            result += " " + paths[i];
        }
        return result;
    }
    
    // Get default paths (without -I prefixes)
    return getDefaultVtkIncludePathsImpl();
}

std::string CompilationConfig::getDefaultVtkFlags() const
{
    return getDefaultVtkFlagsImpl();
}

void CompilationConfig::setVtkFlags(const std::string& flags)
{
    if (flags.empty())
    {
        m_state.vtkFlagsOverride.reset();
    } 
    else 
    {
        m_state.vtkFlagsOverride = flags;
    }
}

void CompilationConfig::resetVtkFlags()
{
    m_state.vtkFlagsOverride.reset();
}

std::string CompilationConfig::getOopencalDir() const
{
    if (m_state.oopencalDirOverride.has_value())
    {
        return m_state.oopencalDirOverride.value();
    }
    return getDefaultOopencalDir();
}

std::string CompilationConfig::getDefaultOopencalDir() const
{
    return getDefaultOopencalDirImpl();
}

void CompilationConfig::setOopencalDir(const std::string& path)
{
    if (path.empty())
    {
        m_state.oopencalDirOverride.reset();
    } 
    else 
    {
        m_state.oopencalDirOverride = path;
    }
}

void CompilationConfig::resetOopencalDir()
{
    m_state.oopencalDirOverride.reset();
}

std::string CompilationConfig::getViewerRootDir() const
{
    if (m_state.viewerRootDirOverride.has_value())
    {
        return m_state.viewerRootDirOverride.value();
    }
    return getDefaultViewerRootDir();
}

std::string CompilationConfig::getDefaultViewerRootDir() const
{
    return getDefaultViewerRootDirImpl();
}

void CompilationConfig::setViewerRootDir(const std::string& path)
{
    if (path.empty()) 
    {
        m_state.viewerRootDirOverride.reset();
    } 
    else 
    {
        m_state.viewerRootDirOverride = path;
    }
}

void CompilationConfig::resetViewerRootDir()
{
    m_state.viewerRootDirOverride.reset();
}

std::vector<std::string> CompilationConfig::getIncludePaths() const
{
    std::vector<std::string> paths;
    
    // Add OOpenCAL include paths
    auto oopencalPaths = getOopencalIncludePaths();
    paths.insert(paths.end(), oopencalPaths.begin(), oopencalPaths.end());
    
    // Add project include paths
    auto projectPaths = getProjectIncludePaths();
    paths.insert(paths.end(), projectPaths.begin(), projectPaths.end());
    
    return paths;
}

std::vector<std::string> CompilationConfig::getOopencalIncludePaths() const
{
    std::vector<std::string> paths;
    std::string oopencalDir = getOopencalDir();
    
    if (! oopencalDir.empty())
    {
        paths.push_back("-I\"" + oopencalDir + "/OOpenCAL/base\"");
        paths.push_back("-I\"" + oopencalDir + "\"");
    }
    
    return paths;
}

std::vector<std::string> CompilationConfig::getProjectIncludePaths() const
{
    std::vector<std::string> paths;
    std::string projectRoot = getViewerRootDir();
    
    if (! projectRoot.empty())
    {
        paths.push_back("-I\"" + projectRoot + "\"");
        paths.push_back("-I\"" + projectRoot + "/visualiserProxy\"");
        paths.push_back("-I\"" + projectRoot + "/config\"");
    }
    
    return paths;
}

bool CompilationConfig::hasOverrides() const
{
    return m_state.compilationFlagsOverride.has_value() ||
           m_state.vtkFlagsOverride.has_value() ||
           m_state.oopencalDirOverride.has_value() ||
           m_state.viewerRootDirOverride.has_value();
}

void CompilationConfig::resetToDefaults()
{
    m_state.compilationFlagsOverride.reset();
    m_state.vtkFlagsOverride.reset();
    m_state.oopencalDirOverride.reset();
    m_state.viewerRootDirOverride.reset();
}

std::string CompilationConfig::getConfigurationSummary() const
{
    std::string summary = "Compilation Configuration:\n";
    
    summary += "  Base flags: " + getCompilationFlags();
    if (m_state.compilationFlagsOverride.has_value())
    {
        summary += " (override: " + getDefaultCompilationFlags() + ")";
    }
    summary += "\n";
    
    summary += "  VTK flags: " + getVtkFlags();
    if (m_state.vtkFlagsOverride.has_value())
    {
        summary += " (override: " + getDefaultVtkFlags() + ")";
    }
    summary += "\n";
    
    summary += "  OOpenCAL dir: " + getOopencalDir();
    if (m_state.oopencalDirOverride.has_value())
    {
        summary += " (override: " + getDefaultOopencalDir() + ")";
    }
    summary += "\n";
    
    summary += "  Viewer root: " + getViewerRootDir();
    if (m_state.viewerRootDirOverride.has_value())
    {
        summary += " (override: " + getDefaultViewerRootDir() + ")";
    }
    summary += "\n";
    
    return summary;
}

std::string CompilationConfig::exportConfiguration() const
{
    std::string json = "{\n";
    json += "  \"compilationFlags\": \"" + getCompilationFlags() + "\",\n";
    json += "  \"vtkFlags\": \"" + getVtkFlags() + "\",\n";
    json += "  \"oopencalDir\": \"" + getOopencalDir() + "\",\n";
    json += "  \"viewerRootDir\": \"" + getViewerRootDir() + "\",\n";
    json += "  \"hasOverrides\": " + std::string(hasOverrides() ? "true" : "false") + "\n";
    json += "}";
    return json;
}

bool CompilationConfig::importConfiguration(const std::string& configJson)
{
    // Simple JSON parsing - in a real implementation, use a proper JSON library
    // This is a basic implementation for demonstration
    
    if (configJson.empty()) 
    {
        return false;
    }
    
    // Look for simple key-value pairs
    auto extractValue = [](const std::string& json, const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\": \"";
        size_t pos = json.find(searchKey);
        if (pos != std::string::npos)
        {
            pos += searchKey.length();
            size_t endPos = json.find("\"", pos);
            if (endPos != std::string::npos)
            {
                return json.substr(pos, endPos - pos);
            }
        }
        return "";
    };
    
    // Extract and set values
    std::string flags = extractValue(configJson, "compilationFlags");
    if (! flags.empty())
    {
        setCompilationFlags(flags);
    }
    
    std::string vtkFlags = extractValue(configJson, "vtkFlags");
    if (! vtkFlags.empty())
    {
        setVtkFlags(vtkFlags);
    }
    
    std::string oopencalDir = extractValue(configJson, "oopencalDir");
    if (! oopencalDir.empty())
    {
        setOopencalDir(oopencalDir);
    }
    
    std::string viewerRoot = extractValue(configJson, "viewerRootDir");
    if (! viewerRoot.empty())
    {
        setViewerRootDir(viewerRoot);
    }
    
    return true;
}

std::string CompilationConfig::getDefaultsSummary() const
{
    std::string summary = "Default Compilation Configuration:\n";
    summary += "  Base flags: " + getDefaultCompilationFlags() + "\n";
    summary += "  VTK flags: " + getDefaultVtkFlags() + "\n";
    summary += "  OOpenCAL dir: " + getDefaultOopencalDir() + "\n";
    summary += "  Viewer root: " + getDefaultViewerRootDir() + "\n";
    return summary;
}

std::string CompilationConfig::resolveDirectoryPath(const char* envVarName, const char* cmakeDefineValue) const
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

std::string CompilationConfig::getDefaultOopencalDirImpl() const
{
#ifdef OOPENCAL_DIR
    return resolveDirectoryPath("OOPENCAL_DIR", OOPENCAL_DIR);
#else
    return resolveDirectoryPath("OOPENCAL_DIR", nullptr);
#endif
}

std::string CompilationConfig::getDefaultViewerRootDirImpl() const
{
#ifdef OOPENCAL_VIEWER_ROOT
    return resolveDirectoryPath("OOPENCAL_VIEWER_ROOT", OOPENCAL_VIEWER_ROOT);
#else
    return resolveDirectoryPath("OOPENCAL_VIEWER_ROOT", nullptr);
#endif
}

std::string CompilationConfig::getDefaultVtkFlagsImpl() const
{
    std::string result = "";
    
    // First try environment variable
    const char* envVtk = std::getenv("VTK_INCLUDES");
    if (envVtk && std::string(envVtk) != "")
    {
        // Split by space and add -I flags
        std::string envStr(envVtk);
        std::stringstream ss(envStr);
        std::string path;
        
        while (ss >> path)
        {
            if (!path.empty())
            {
                result += "-I" + path + " ";
            }
        }
        return result;
    }
    
    // Fall back to CMake-provided VTK_INCLUDES
#ifdef VTK_INCLUDES
    std::string cmakeIncludes(VTK_INCLUDES);
    if (!cmakeIncludes.empty())
    {
        // Split by space and add -I flags
        std::stringstream ss(cmakeIncludes);
        std::string path;
        
        while (ss >> path)
        {
            if (!path.empty())
            {
                result += "-I" + path + " ";
            }
        }
        return result;
    }
#endif

    return {};
}

std::string CompilationConfig::getDefaultVtkIncludePathsImpl() const
{
    // First try environment variable
    const char* envVtk = std::getenv("VTK_INCLUDES");
    if (envVtk && std::string(envVtk) != "")
    {
        return deduplicatePaths(std::string(envVtk));
    }
    
    // Fall back to CMake-provided VTK_INCLUDES
#ifdef VTK_INCLUDES
    std::string cmakeIncludes(VTK_INCLUDES);
    if (! cmakeIncludes.empty())
    {
        return deduplicatePaths(cmakeIncludes);
    }
#endif
    
    return "";
}

std::string CompilationConfig::deduplicatePaths(const std::string& paths) const
{
    if (paths.empty())
        return "";
    
    std::stringstream ss(paths);
    std::string path;
    std::unordered_set<std::string> seen;
    std::vector<std::string> uniquePaths;
    
    while (ss >> path)
    {
        if (seen.find(path) == seen.end())
        {
            seen.insert(path);
            uniquePaths.push_back(path);
        }
    }
    
    if (uniquePaths.empty())
        return "";
    
    std::string result = uniquePaths[0];
    for (size_t i = 1; i < uniquePaths.size(); ++i)
    {
        result += " " + uniquePaths[i];
    }
    return result;
}
} // namespace viz::plugins
