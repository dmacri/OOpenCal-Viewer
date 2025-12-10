/** @file PluginLoader.h
 * @brief Plugin management system for dynamic model loading.
 *
 * This class handles loading, managing, and querying plugins for the Qt-VTK Viewer.
 * Plugins are shared libraries (.so) that register custom cell models at runtime. */

#pragma once

#include <string>
#include <vector>

/** @brief Information about a loaded plugin */
struct PluginInfo
{
    std::string path; ///< Full path to the plugin file
    std::string name; ///< Plugin name (from getModelName)
    std::string info; ///< Plugin description (from getPluginInfo)
    int version{};    ///< Plugin version (from getPluginVersion)
    void* handle{};   ///< dlopen handle
    bool isLoaded{};  ///< Load status
};

/** @brief Manages plugin loading and lifecycle
 * 
 * This class provides a centralized way to:
 * - Load plugins from files or directories
 * - Track loaded plugins
 * - Query plugin information
 * - Handle errors gracefully
 * 
 * Example usage:
 * @code
 * PluginLoader& loader = PluginLoader::instance();
 * if (loader.loadPlugin("/path/to/plugin.so")) {
 *     std::cout << "Plugin loaded successfully\n";
 * }
 * @endcode */
class PluginLoader
{
public:
    /** @brief Get singleton instance */
    static PluginLoader& instance();

    /** @brief Load a single plugin from path
     * @param pluginPath Full path to .so file
     * @return true if loaded successfully, false otherwise */
    bool loadPlugin(const std::string& pluginPath, bool overridePlugin=false);

    /** @brief Load all plugins from a directory
     * @param directory Path to directory containing .so files
     * @return Number of successfully loaded plugins */
    int loadPluginsFromDirectory(const std::string& directory);

    /** @brief Load plugins from multiple standard directories
     * @param directories List of directory paths to scan
     * @return Total number of loaded plugins */
    int loadFromStandardDirectories(const std::vector<std::string>& directories);

    /** @brief Get list of all loaded plugins */
    const std::vector<PluginInfo>& getLoadedPlugins() const;

    /** @brief Check if a plugin is already loaded
     * @param pluginPath Path to check
     * @return true if plugin with this path is loaded */
    bool isPluginLoaded(const std::string& pluginPath) const;

    /** @brief Get the last error message */
    std::string getLastError() const;

    /** @brief Clear the last error */
    void clearError();

    ~PluginLoader();

private:
    PluginLoader() = default;

    // Prevent copying
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    /// @brief Extract plugin metadata after loading
    void extractPluginMetadata(PluginInfo& info);

    /// @brief Remove plugin from application, if loaded
    void removePlugin(const std::string& pluginPath);

    std::vector<PluginInfo> loadedPlugins;
    std::string lastError;
};
