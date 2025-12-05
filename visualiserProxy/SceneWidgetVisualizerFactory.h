/** @file SceneWidgetVisualizerFactory.h
 * @brief Factory for creating scene widget visualizers.
 * 
 * This file contains the SceneWidgetVisualizerFactory class, which is responsible for
 * creating appropriate visualizer instances based on the model name. The factory
 * enables dynamic model selection at runtime and provides a unified way to create
 * visualizers without exposing the underlying implementation details.
 * 
 * The factory uses a registry-based approach where models can be registered at startup.
 * This design enables a plugin system where new models can be added dynamically without
 * recompiling the application. Models are identified by their string names.
 * 
 * @see ISceneWidgetVisualizer
 * @see SceneWidgetVisualizerAdapter
 * @see doc/CHANGELOG_RUNTIME_MODELS.md for architectural details */

#pragma once

#include <functional>
#include <map>
#include <memory> // std::unique_ptr<>
#include <string>
#include <vector>

class ISceneWidgetVisualizer;

/** @class SceneWidgetVisualizerFactory
 * @brief Factory class for creating visualizer instances based on model name.
 * 
 * This factory is responsible for creating the appropriate visualizer instances
 * for different model types identified by their string names. It provides a simple
 * interface for creating visualizers without requiring knowledge of the specific
 * implementation details of each model.
 * 
 * The factory uses a registry pattern where models are registered with factory functions.
 * This allows for a plugin system where new models can be added at runtime by registering
 * their creation functions, enabling dynamic model loading without recompilation.
 * 
 * Built-in models are registered automatically at startup via the static initialization
 * mechanism. Plugin models can be registered by calling registerModel() with appropriate
 * creation functions.
 * 
 * @note This class uses static methods and a static registry for simplicity. */
class SceneWidgetVisualizerFactory
{
public:
    /// Type for model creation functions
    using ModelCreator = std::function<std::unique_ptr<ISceneWidgetVisualizer>()>;

    /** @brief Create a visualizer from a string name.
     * 
     * @param modelName The name of the model (e.g., "Ball", "SciddicaT")
     * @return std::unique_ptr<ISceneWidgetVisualizer> Pointer to the created visualizer
     * @throws std::invalid_argument if the model name is not recognized */
    static std::unique_ptr<ISceneWidgetVisualizer> create(const std::string& modelName);

    /// @brief Create a visualizer for default model
    static std::unique_ptr<ISceneWidgetVisualizer> defaultModel();

    /** @brief Register a new model with the factory.
     * 
     * This method allows adding new models at runtime. The creator function will be
     * called whenever a visualizer for this model is requested.
     * 
     * @param modelName The unique name of the model
     * @param creator Function that creates a new instance of the visualizer
     * @return true if registration succeeded, false if model already exists */
    static bool registerModel(const std::string& modelName, ModelCreator creator);

    /// @brief Get all available model names
    static std::vector<std::string> getAvailableModels();

    /** @brief Check if a model is registered.
     * 
     * @param modelName The name of the model to check
     * @return true if the model is registered, false otherwise */
    static bool isModelRegistered(const std::string& modelName);

    static bool unregisterModel(const std::string& modelName)
    {
        auto& registry = getRegistry();
        return registry.erase(modelName) > 0;
    }

private:
    /// Registry of model creation functions
    static std::map<std::string, ModelCreator>& getRegistry();

    /// Initialize built-in models (called automatically)
    static void initializeBuiltInModels();

    /// Flag to track if built-in models are initialized
    static inline bool isInitializedWithBuildInModels = false;
};
