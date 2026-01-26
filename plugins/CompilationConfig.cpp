/** @file CompilationConfig.cpp
 * @brief Implementation of CompilationConfig singleton for managing compilation settings. */

#include "CompilationConfig.h"

#include <sstream>
#include <cstdlib>
#include <unordered_set>
#include <filesystem>
#include <sstream>

namespace viz::plugins
{
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
    return "-shared -fPIC";
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
