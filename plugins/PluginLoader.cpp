/** @file PluginLoader.cpp
 * @brief Implementation of the plugin loading system. */

#include <filesystem>
#include <iostream>
#include <QLibrary>

#include "ActionRegistry.h"
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

    // Load the shared library using QLibrary for cross-platform compatibility
    auto library = std::make_unique<QLibrary>(QString::fromStdString(pluginPath));
    library->setLoadHints(QLibrary::LoadHint::ResolveAllSymbolsHint);
    
    if (! library->load())
    {
        lastError = std::string("Failed to load plugin: ") + library->errorString().toStdString();
        std::cerr << "Error: " << lastError << std::endl;
        return false;
    }

    // A plugin provides a model (registerPlugin), actions (registerActionPlugin, see ActionApi.h) or both
    typedef void (*RegisterFunc)();
    typedef void (*RegisterActionsFunc)(viz::actions::IActionRegistry&);
    typedef int (*ApiVersionFunc)();
    RegisterFunc registerPlugin = reinterpret_cast<RegisterFunc>(library->resolve("registerPlugin"));
    RegisterActionsFunc registerActions =
        reinterpret_cast<RegisterActionsFunc>(library->resolve("registerActionPlugin"));

    if (! registerPlugin && ! registerActions)
    {
        lastError = "Plugin does not export registerPlugin() nor registerActionPlugin() function";
        if (! library->errorString().isEmpty())
            lastError += std::string(": ") + library->errorString().toStdString();

        std::cerr << "Error: " << lastError << std::endl;
        library->unload();
        return false;
    }

    if (registerActions)
    {
        // The interfaces are C++ classes with virtual functions: refuse a plugin built for another version of them
        const auto apiVersion = reinterpret_cast<ApiVersionFunc>(library->resolve("getActionApiVersion"));
        const int pluginApiVersion = apiVersion ? apiVersion() : 0;
        if (pluginApiVersion != viz::actions::API_VERSION)
        {
            lastError = "Action plugin was built for action API version " + std::to_string(pluginApiVersion) +
                        ", the application provides version " + std::to_string(viz::actions::API_VERSION) +
                        " (rebuild the plugin)";
            std::cerr << "Error: " << lastError << std::endl;
            library->unload();
            return false;
        }
    }

    // Create plugin info
    PluginInfo info;
    info.path = pluginPath;
    info.handle = std::move(library);
    info.isLoaded = false;

    // Call the registration functions
    auto& actionRegistry = viz::actions::ActionRegistry::instance();
    try
    {
        if (registerPlugin)
            registerPlugin();
        if (registerActions)
        {
            actionRegistry.setCurrentOwner(pluginPath); // to destroy the actions before the library is unloaded
            registerActions(actionRegistry);
            actionRegistry.setCurrentOwner({});
        }
        info.isLoaded = true;
        std::cout << "✓ Loaded plugin: " << pluginPath << std::endl;
    }
    catch (const std::exception& e)
    {
        actionRegistry.setCurrentOwner({});
        actionRegistry.removeOwnedBy(pluginPath);
        lastError = std::string("Exception while registering plugin: ") + e.what();
        std::cerr << "Error: " << lastError << std::endl;
        info.handle->unload();
        return false;
    }

    // Extract metadata (optional functions)
    extractPluginMetadata(info);

    // Store plugin info
    loadedPlugins.push_back(std::move(info));

    clearError();
    return true;
}

void PluginLoader::extractPluginMetadata(PluginInfo& info)
{
    // Get plugin info string
    typedef const char* (*InfoFunc)();
    InfoFunc getPluginInfo = reinterpret_cast<InfoFunc>(info.handle->resolve("getPluginInfo"));
    if (getPluginInfo)
    {
        info.info = getPluginInfo();
        std::cout << "  Info: " << info.info << std::endl;
    }

    // Get plugin version
    typedef int (*VersionFunc)();
    VersionFunc getPluginVersion = reinterpret_cast<VersionFunc>(info.handle->resolve("getPluginVersion"));
    if (getPluginVersion)
    {
        info.version = getPluginVersion();
    }

    // Get model name
    InfoFunc getModelName = reinterpret_cast<InfoFunc>(info.handle->resolve("getModelName"));
    if (getModelName)
    {
        info.name = getModelName();
    }
}

void PluginLoader::removePlugin(const std::string &pluginPath)
{
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

    // The code of the actions is in the library, so they must be destroyed before it is unloaded
    if (const auto removed = viz::actions::ActionRegistry::instance().removeOwnedBy(pluginPath))
        std::cout << "✓ Unregistered " << removed << " action(s) of: " << pluginPath << std::endl;

    // Close the library using QLibrary::unload
    if (plugin.handle)
    {
        if (! plugin.handle->unload())
        {
            lastError = std::string("Failed to close plugin handle: ") + plugin.handle->errorString().toStdString();
            std::cerr << "Error: " << lastError << std::endl;
        }
        else
        {
            std::cout << "✓ Closed plugin handle: " << pluginPath << std::endl;
        }
        // No need for delete - unique_ptr handles cleanup automatically
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

            // Check if it's a shared library (cross-platform support)
            const auto& extension = path.extension().string();
            if (extension != ".so" && extension != ".dll")
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
