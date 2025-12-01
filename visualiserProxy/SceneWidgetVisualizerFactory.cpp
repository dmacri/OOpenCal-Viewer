#include "visualiserProxy/SceneWidgetVisualizer.pch.h" // precompiled header ... hopefully

#include <stdexcept> // std::invalid_argument

#include <OOpenCAL/models/Ball/BallCell.h>
#include <OOpenCAL/models/SciddicaT/SciddicaTCell.h>


std::map<std::string, SceneWidgetVisualizerFactory::ModelCreator>& SceneWidgetVisualizerFactory::getRegistry()
{
    static std::map<std::string, ModelCreator> registry;
    return registry;
}

void SceneWidgetVisualizerFactory::initializeBuiltInModels()
{
    if (isInitializedWithBuildInModels)
    {
        return;
    }

    // Register built-in models
    registerModel("Ball",
                  []()
                  {
                      return std::make_unique<SceneWidgetVisualizerAdapter<BallCell>>("Ball");
                  });

    registerModel("SciddicaT",
                  []()
                  {
                      return std::make_unique<SceneWidgetVisualizerAdapter<SciddicaTCell>>("SciddicaT");
                  });

    isInitializedWithBuildInModels = true;
}

std::unique_ptr<ISceneWidgetVisualizer> SceneWidgetVisualizerFactory::create(const std::string& modelName)
{
    // Ensure built-in models are registered
    initializeBuiltInModels();

    auto& registry = getRegistry();
    auto it = registry.find(modelName);

    if (it == registry.end())
    {
        throw std::invalid_argument("Unknown model name: " + modelName);
    }

    return it->second();
}

std::unique_ptr<ISceneWidgetVisualizer> SceneWidgetVisualizerFactory::defaultModel()
{
    // Ensure built-in models are registered
    initializeBuiltInModels();

    auto registry = getRegistry();
    if (registry.empty())
    {
        throw std::runtime_error("No models!");
    }

    auto firstModel = registry.begin();
    std::cout << "Using default model '" << firstModel->first << "'" << endl;
    return firstModel->second();
}

bool SceneWidgetVisualizerFactory::registerModel(const std::string& modelName, ModelCreator creator)
{
    auto& registry = getRegistry();

    // Check if model already exists
    if (registry.find(modelName) != registry.end())
    {
        return false;
    }

    registry[modelName] = std::move(creator);
    return true;
}

std::vector<std::string> SceneWidgetVisualizerFactory::getAvailableModels()
{
    // Ensure built-in models are registered
    initializeBuiltInModels();

    std::vector<std::string> models;
    auto& registry = getRegistry();

    for (const auto& [name, creator] : registry)
    {
        models.push_back(name);
    }

    return models;
}

bool SceneWidgetVisualizerFactory::isModelRegistered(const std::string& modelName)
{
    // Ensure built-in models are registered
    initializeBuiltInModels();

    auto& registry = getRegistry();
    return registry.find(modelName) != registry.end();
}
