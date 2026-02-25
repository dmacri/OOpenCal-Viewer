/** Plugin_FullTemplate.cpp
 * 
 * FULL WORKING VERSION of the plugin with actual includes
 *
 * This version includes the real headers from OOpenCal-Viewer and will
 * compile into a working plugin that can be loaded at runtime.
 * 
 * Build instructions are in CMakeLists.txt */

// --- Plugin configuration macros ---
//
// These macros define which model and cell class this plugin will use.
// They can be overridden externally (e.g. via -DPLUGIN_MODEL_NAME="\"MyModel\"" or -DPLUGIN_CELL_CLASS=MyCell)
// so that this single plugin source file can be reused for different models.
//
// PLUGIN_MODEL_NAME        → Human-readable string used to identify the plugin/model name
// PLUGIN_CELL_CLASS → Actual C++ class implementing the cell (header will be auto-included below)
#ifndef PLUGIN_MODEL_NAME
#define PLUGIN_MODEL_NAME "CustomModel"
#endif

#ifndef PLUGIN_CELL_CLASS
#define PLUGIN_CELL_CLASS CustomCell
#endif

// --- Preprocessor helpers ---
// These macros are used to convert a macro value into a string literal.
// Direct stringification (#x) does not expand macros, so we need two levels:
// 1) STRINGIFY_LITERAL(x): turns token x into a string as-is
// 2) EXPAND_AND_STRINGIFY(x): expands x first, then stringifies the result
#define STRINGIFY_LITERAL(x) #x
#define EXPAND_AND_STRINGIFY(x) STRINGIFY_LITERAL(x)
// Helper macro: if the model name is quoted ("My Model"), this will print it cleanly.
#define CLEAN_MODEL_NAME EXPAND_AND_STRINGIFY(PLUGIN_MODEL_NAME)

#pragma message("[Plugin build info]")
#pragma message("  Model name: " CLEAN_MODEL_NAME)
#pragma message("  Cell class: " EXPAND_AND_STRINGIFY(PLUGIN_CELL_CLASS))

#include <iostream>
#include <memory> // std::unique_ptr
#include EXPAND_AND_STRINGIFY(PLUGIN_CELL_CLASS.h)
#include "visualiserProxy/SceneWidgetVisualizerProxy.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"


extern "C"
{
/// Plugin entry point - this gets called when the plugin is loaded
__attribute__((visibility("default")))
void registerPlugin()
{
    std::cout << "Registering " PLUGIN_MODEL_NAME " plugin..." << std::endl;
    
    // Register the custom model with the factory
    bool success = SceneWidgetVisualizerFactory::registerModel(PLUGIN_MODEL_NAME, []() {
        return std::make_unique<SceneWidgetVisualizerTemplate<PLUGIN_CELL_CLASS>>(
            PLUGIN_MODEL_NAME  // - this will appear in the menu
        );
    });
    
    if (success)
    {
        std::cout << "✓ " PLUGIN_MODEL_NAME " plugin registered successfully!" << std::endl;
        std::cout << "  The model is now available in Model menu" << std::endl;
    }
    else
    {
        std::cerr << "✗ Failed to register " PLUGIN_MODEL_NAME " - name may already exist" << std::endl;
    }
}

/// Optional: Get plugin metadata
__attribute__((visibility("default")))
const char* getPluginInfo()
{
    return PLUGIN_MODEL_NAME " Plugin v1.0\n"
           "Author: Developers of OOpenCal\n"
           "Description: Example custom cell model with color gradient visualization\n"
           "Compatible with: OOpenCal-Viewer 2.x";
}

/// Optional: Get plugin version
__attribute__((visibility("default")))
int getPluginVersion()
{
    return 100; // Version 1.00
}

/// Optional: Get model name provided by this plugin
__attribute__((visibility("default")))
const char* getModelName()
{
    return PLUGIN_MODEL_NAME;
}
} // extern "C"
