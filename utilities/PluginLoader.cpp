/** @file PluginLoader.cpp
 * @brief Implementation of the plugin loading system. */

#include <filesystem>
#include <iostream>
#include <dlfcn.h>

#include "PluginLoader.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h" // SceneWidgetVisualizerFactory::unregisterModel


namespace
{
namespace fs = std::filesystem;
}


PluginLoader& PluginLoader::instance()
{
    static PluginLoader loader;
    return loader;
}

bool PluginLoader::loadPlugin(const std::string& pluginPath, bool overridePlugin)
{
    if (isPluginLoaded(pluginPath))
    {
        lastError = "Plugin already loaded: " + pluginPath;
        std::cerr << "Warning: " << lastError << std::endl;
        if (overridePlugin)
        {
            removePlugin(pluginPath);
        }
        else
        {
            return false;
        }
    }

    if (! fs::exists(pluginPath))
    {
        lastError = "Plugin file does not exist: " + pluginPath;
        std::cerr << "Error: " << lastError << std::endl;
        return false;
    }

    // Load the shared library
    // RTLD_GLOBAL allows plugin to use symbols from main app
    void* handle = dlopen(pluginPath.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (! handle)
    {
        lastError = std::string("Failed to load plugin: ") + dlerror();
        std::cerr << "Error: " << lastError << std::endl;
        return false;
    }

    dlerror(); // Clear any existing errors

    // Find the registerPlugin function
    typedef void (*RegisterFunc)();
    RegisterFunc registerPlugin = (RegisterFunc) dlsym(handle, "registerPlugin");

    const char* dlsym_error = dlerror();
    if (dlsym_error || ! registerPlugin)
    {
        lastError = "Plugin does not export registerPlugin() function";
        if (dlsym_error)
            lastError += std::string(": ") + dlsym_error;

        std::cerr << "Error: " << lastError << std::endl;
        dlclose(handle);
        return false;
    }

    // Create plugin info
    PluginInfo info;
    info.path = pluginPath;
    info.handle = handle;
    info.isLoaded = false;

    // Call the registration function
    try
    {
        registerPlugin();
        info.isLoaded = true;
        std::cout << "✓ Loaded plugin: " << pluginPath << std::endl;
    }
    catch (const std::exception& e)
    {
        lastError = std::string("Exception while registering plugin: ") + e.what();
        std::cerr << "Error: " << lastError << std::endl;
        dlclose(handle);
        return false;
    }

    // Extract metadata (optional functions)
    extractPluginMetadata(info);

    // Store plugin info
    loadedPlugins.push_back(info);

    clearError();
    return true;
}

void PluginLoader::extractPluginMetadata(PluginInfo& info)
{
    // Get plugin info string
    typedef const char* (*InfoFunc)();
    InfoFunc getPluginInfo = (InfoFunc) dlsym(info.handle, "getPluginInfo");
    if (getPluginInfo)
    {
        info.info = getPluginInfo();
        std::cout << "  Info: " << info.info << std::endl;
    }

    // Get plugin version
    typedef int (*VersionFunc)();
    VersionFunc getPluginVersion = (VersionFunc) dlsym(info.handle, "getPluginVersion");
    if (getPluginVersion)
    {
        info.version = getPluginVersion();
    }

    // Get model name
    InfoFunc getModelName = (InfoFunc) dlsym(info.handle, "getModelName");
    if (getModelName)
    {
        info.name = getModelName();
    }
}

void PluginLoader::removePlugin(const std::string &pluginPath)
{
    namespace fs = std::filesystem;

    auto it = std::find_if(loadedPlugins.begin(), loadedPlugins.end(),
                           [&](const PluginInfo& p) { return p.path == pluginPath; });

    if (it == loadedPlugins.end())
    {
        lastError = "Plugin not found: " + pluginPath;
        std::cerr << "Warning: " << lastError << std::endl;
        return;
    }

    PluginInfo& plugin = *it;

    std::cout << "Unloading plugin: " << pluginPath << std::endl;

    if (!plugin.name.empty() && SceneWidgetVisualizerFactory::isModelRegistered(plugin.name))
    {
        if (SceneWidgetVisualizerFactory::unregisterModel(plugin.name))
            std::cout << "✓ Unregistered model: " << plugin.name << std::endl;
    }

    // Close the library .so
    if (plugin.handle)
    {
        if (dlclose(plugin.handle) != 0)
        {
            lastError = std::string("Failed to close plugin handle: ") + dlerror();
            std::cerr << "Error: " << lastError << std::endl;
        }
        else
        {
            std::cout << "✓ Closed plugin handle: " << pluginPath << std::endl;
        }
    }

    // Remove from list of loaded files
    loadedPlugins.erase(it);
    clearError();
}

int PluginLoader::loadPluginsFromDirectory(const std::string& directory)
{
    if (! fs::exists(directory) || ! fs::is_directory(directory))
    {
        return 0;
    }

    int loadedCount = 0;
    std::cout << "Scanning for plugins in: " << directory << std::endl;

    try
    {
        for (const auto& entry : fs::directory_iterator(directory))
        {
            if (! entry.is_regular_file())
                continue;

            const auto& path = entry.path();

            // Check if it's a shared library
            if (path.extension() != ".so")
                continue;

            if (loadPlugin(path.string()))
            {
                loadedCount++;
            }
        }

        if (loadedCount > 0)
        {
            std::cout << "Loaded " << loadedCount << " plugin(s) from " << directory << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error scanning directory " << directory << ": " << e.what() << std::endl;
    }

    return loadedCount;
}

int PluginLoader::loadFromStandardDirectories(const std::vector<std::string>& directories)
{
    int totalLoaded = 0;

    for (const auto& dir : directories)
    {
        totalLoaded += loadPluginsFromDirectory(dir);
    }

    return totalLoaded;
}

const std::vector<PluginInfo>& PluginLoader::getLoadedPlugins() const
{
    return loadedPlugins;
}

bool PluginLoader::isPluginLoaded(const std::string& pluginPath) const
{
    for (const auto& plugin : loadedPlugins)
    {
        if (plugin.path == pluginPath)
            return true;
    }
    return false;
}

std::string PluginLoader::getLastError() const
{
    return lastError;
}

void PluginLoader::clearError()
{
    lastError.clear();
}

PluginLoader::~PluginLoader()
{
    // Clean up all plugin handles
    for (auto& plugin : loadedPlugins)
    {
        if (plugin.handle)
        {
            // Note: We don't dlclose() because plugins need to remain loaded
            // for the lifetime of the application
        }
    }
}
