/** Plugin_MinimalExample.cpp
 *
 * Minimal example plugin for OOpenCAL / Qt-VTK Viewer.
 * Demonstrates how to register a new model plugin without depending
 * on full library headers. This is a teaching / template version.
 *
 * Compile it as:
 *   g++ -std=c++23 -fPIC -shared Plugin_MinimalExample.cpp -o libMinimalExample.so */

#include <iostream>
#include <functional>
#include <memory>
#include <string>

// -----------------------------------------------------------------------------
//  Minimal stub definitions (for demonstration purposes)
//  In a real plugin, these would come from "visualiserProxy/..." headers
// -----------------------------------------------------------------------------

class ISceneWidgetVisualizer {};

template<typename CellType>
class SceneWidgetVisualizerTemplate : public ISceneWidgetVisualizer {
public:
    explicit SceneWidgetVisualizerTemplate(const std::string& modelName)
    {
        std::cout << "[Template] Creating visualizer for model: " << modelName << std::endl;
    }
};

// Mock factory that registers models by name (for illustration)
class SceneWidgetVisualizerFactory {
public:
    using ModelCreator = std::function<std::unique_ptr<ISceneWidgetVisualizer>()>;

    static bool registerModel(const std::string& name, ModelCreator creator)
    {
        std::cout << "[Factory] Registering model: " << name << std::endl;
        // In a real implementation this would store the lambda and return true/false
        (void)creator;
        return true;
    }
};

// -----------------------------------------------------------------------------
//  Plugin metadata (can be customized at build time via -DPLUGIN_* macros)
// -----------------------------------------------------------------------------

#ifndef PLUGIN_MODEL_NAME
#define PLUGIN_MODEL_NAME "MinimalModel"
#endif

#ifndef PLUGIN_CELL_CLASS
#define PLUGIN_CELL_CLASS DummyCell
#endif

// Stringify helpers for clean printing
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// -----------------------------------------------------------------------------
//  Plugin entry points (C linkage, default visibility)
// -----------------------------------------------------------------------------

extern "C" {

    /**
     * Called when the plugin is loaded.
     * Registers the model with the viewer's factory.
     */
    __attribute__((visibility("default")))
    void registerPlugin()
    {
        std::cout << "🧩 Loading plugin: " << PLUGIN_MODEL_NAME << std::endl;

        bool ok = SceneWidgetVisualizerFactory::registerModel<PLUGIN_CELL_CLASS>>(PLUGIN_MODEL_NAME);

        if (ok)
            std::cout << "✅ Plugin '" << PLUGIN_MODEL_NAME << "' registered successfully.\n";
        else
            std::cerr << "❌ Plugin registration failed (duplicate name?).\n";
    }

    /** Returns short info about the plugin */
    __attribute__((visibility("default")))
    const char* getPluginInfo()
    {
        return PLUGIN_MODEL_NAME " Plugin v1.0 — minimal example implementation.";
    }

    /** Returns plugin version */
    __attribute__((visibility("default")))
    int getPluginVersion()
    {
        return 100; // version 1.00
    }

    /** Returns model name provided by this plugin */
    __attribute__((visibility("default")))
    const char* getModelName()
    {
        return PLUGIN_MODEL_NAME;
    }

} // extern "C"
