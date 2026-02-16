/** @file CompilationConfig.h
 * @brief Singleton class for managing C++ module compilation configuration.
 *
 * This file contains the CompilationConfig singleton class which provides centralized
 * management of compilation flags, paths, and settings used throughout the application.
 * It allows for viewing default options and overriding them while maintaining visibility
 * of original values.
 *
 * The singleton manages:
 * - Base compilation flags (-shared, -fPIC)
 * - VTK compilation flags
 * - Environment variables (OOPENCAL_DIR, OOPENCAL_VIEWER_ROOT)
 * - Include paths that depend on environment variables
 * - Compiler settings and paths
 *
 * Both CppModuleBuilder and CompilationSettingsWidget use this singleton
 * to ensure consistent configuration across the application. */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <utility>

namespace viz::plugins
{
/** @class CompilationConfig
 * @brief Singleton for managing C++ module compilation configuration.
 *
 * This class provides a centralized way to manage compilation settings
 * used throughout the application. It maintains both default values
 * and user overrides, allowing for flexible configuration while
 * preserving the ability to view original defaults.
 *
 * Key features:
 * - Thread-safe singleton access
 * - Default values with override capability
 * - Automatic path resolution based on environment variables
 * - Integration with existing CppModuleBuilder functionality
 * - Support for VTK compilation flags
 *
 * Usage example:
 * @code
 * auto& config = CompilationConfig::getInstance();
 * 
 * // Get current compilation flags (with overrides applied)
 * std::string flags = config.getCompilationFlags();
 * 
 * // Override VTK flags
 * config.setVtkFlags("-I/usr/local/vtk/include");
 * 
 * // Get default OOpenCAL directory
 * std::string defaultDir = config.getDefaultOopencalDir();
 * 
 * // Override OOpenCAL directory
 * config.setOopencalDir("/custom/path");
 * @endcode */
class CompilationConfig
{
public:
    /** @brief Get the singleton instance
     * @return Reference to the singleton instance */
    static CompilationConfig& getInstance();
    
    // Delete copy constructor and assignment operator
    CompilationConfig(const CompilationConfig&) = delete;
    CompilationConfig& operator=(const CompilationConfig&) = delete;

    /** @brief Get base compilation flags (with overrides applied)
     * @return Compilation flags string (e.g., "-shared -fPIC") */
    std::string getCompilationFlags() const;
    
    /** @brief Get default base compilation flags
     * @return Default compilation flags string */
    std::string getDefaultCompilationFlags() const;
    
    /** @brief Set base compilation flags override
     * @param flags Custom compilation flags, or empty to use defaults */
    void setCompilationFlags(const std::string& flags);
    
    /** @brief Reset base compilation flags to defaults */
    void resetCompilationFlags();

    /** @brief Get VTK compilation flags (with overrides applied)
     * @return VTK flags string */
    std::string getVtkFlags() const;
    
    /** @brief Get VTK include paths without -I prefixes (with overrides applied)
     * @return VTK include paths string (semicolon-separated) */
    std::string getVtkIncludePaths() const;

    /** @brief Get default VTK compilation flags
     * @return Default VTK flags string */
    std::string getDefaultVtkFlags() const;

    /** @brief Set VTK compilation flags override
     * @param flags Custom VTK flags, or empty to use defaults */
    void setVtkFlags(const std::string& flags);

    /** @brief Reset VTK flags to defaults */
    void resetVtkFlags();

    /** @brief Get OOpenCAL directory path (with overrides applied)
     * @return OOpenCAL directory path */
    std::string getOopencalDir() const;

    /** @brief Get default OOpenCAL directory path
     * @return Default OOpenCAL directory path */
    std::string getDefaultOopencalDir() const;
    
    /** @brief Set OOpenCAL directory override
     * @param path Custom OOpenCAL directory path, or empty to use defaults */
    void setOopencalDir(const std::string& path);
    
    /** @brief Reset OOpenCAL directory to defaults */
    void resetOopencalDir();

    /** @brief Get OOpenCal-Viewer root directory path (with overrides applied)
     * @return OOpenCal-Viewer root directory path */
    std::string getViewerRootDir() const;
    
    /** @brief Get default OOpenCal-Viewer root directory path
     * @return Default OOpenCal-Viewer root directory path */
    std::string getDefaultViewerRootDir() const;
    
    /** @brief Set OOpenCal-Viewer root directory override
     * @param path Custom viewer root directory path, or empty to use defaults */
    void setViewerRootDir(const std::string& path);
    
    /** @brief Reset OOpenCal-Viewer root directory to defaults */
    void resetViewerRootDir();

    /** @brief Get all include paths based on current configuration
     * @return Vector of include path strings */
    std::vector<std::string> getIncludePaths() const;
    
    /** @brief Get OOpenCAL-specific include paths
     * @return Vector of OOpenCAL include paths */
    std::vector<std::string> getOopencalIncludePaths() const;
    
    /** @brief Get project-specific include paths
     * @return Vector of project include paths */
    std::vector<std::string> getProjectIncludePaths() const;

    /** @brief Check if any overrides are active
     * @return true if any configuration has been overridden */
    bool hasOverrides() const;
    
    /** @brief Reset all configuration to defaults */
    void resetToDefaults();

    /** @brief Get summary of current configuration state
     * @return String describing current configuration and overrides */
    std::string getConfigurationSummary() const;
    
    /** @brief Export current configuration to JSON-like string
     * @return String representation of current configuration */
    std::string exportConfiguration() const;
    
    /** @brief Import configuration from JSON-like string
     * @param configJson JSON string with configuration values
     * @return true if import was successful */
    bool importConfiguration(const std::string& configJson);
    
    /** @brief Get all default values for comparison
     * @return String with all default configuration values */
    std::string getDefaultsSummary() const;

    /** @brief Get the compiler name and version used to build this application
     * @return Pair of (compiler_name, version_string) e.g. ("clang++", "15.0.7") or ("g++", "13.2.0") */
    static std::pair<std::string, std::string> getBuildCompilerInfo();

    /** @brief Get compiler info as a formatted string
     * @return String like "g++ 13.2.0" or "clang++ 15.0.7" */
    static std::string getBuildCompilerInfoString();

    /** @brief Select the best available compiler with intelligent fallback
     * 
     * Selection priority:
     * 1. Same compiler and version as used to build the application
     * 2. Same compiler family (e.g., clang++ or g++) but different version
     * 3. Other available compilers (g++, clang++, c++)
     * 
     * @return Path to best available compiler, or "clang++" as fallback */
    static std::string getDefaultCompiler();

    /** @brief Compiler information structure */
    struct CompilerInfo
    {
        std::string name;           ///< Compiler executable name (e.g., "g++-15", "clang++")
        std::string family;         ///< Compiler family ("g++", "clang++", "cl")
        std::string version;        ///< Version string (e.g., "15.2.0")
        std::string fullPath;       ///< Full path to compiler executable
        bool isExactMatch;          ///< True if exact match with build compiler
        bool isFamilyMatch;         ///< True if same family as build compiler
        bool isAvailable;           ///< True if compiler is available in system
    };

    /** @brief Detect all available compilers in the system
     * 
     * This function searches for common C++ compilers (g++, clang++, c++) and their
     * versioned variants (e.g., g++-11, g++-12, clang++-15, etc.).
     * 
     * @return Vector of CompilerInfo structures, sorted by priority (exact match first,
     *         then family matches, then other compilers) */
    static std::vector<CompilerInfo> detectAvailableCompilers();

private:
    CompilationConfig() = default;
    ~CompilationConfig() = default;

    struct ConfigState
    {
        std::optional<std::string> compilationFlagsOverride;
        std::optional<std::string> vtkFlagsOverride;
        std::optional<std::string> oopencalDirOverride;
        std::optional<std::string> viewerRootDirOverride;
    };

    ConfigState m_state;

    /** @brief Resolve directory path using environment variable and CMake define
     * @param envVarName Environment variable name
     * @param cmakeDefineValue CMake define value
     * @return Resolved directory path */
    std::string resolveDirectoryPath(const char* envVarName, const char* cmakeDefineValue) const;

    /** @brief Get default OOpenCAL directory from environment/CMake
     * @return Default OOpenCAL directory path */
    std::string getDefaultOopencalDirImpl() const;

    /** @brief Get default viewer root directory from environment/CMake
     * @return Default viewer root directory path */
    std::string getDefaultViewerRootDirImpl() const;

    /** @brief Get default VTK compilation flags implementation */
    std::string getDefaultVtkFlagsImpl() const;

    /** @brief Get default VTK include paths without -I prefixes implementation */
    std::string getDefaultVtkIncludePathsImpl() const;

    /** @brief Remove duplicate paths from a space-separated string */
    std::string deduplicatePaths(const std::string& paths) const;
};
} // namespace viz::plugins
