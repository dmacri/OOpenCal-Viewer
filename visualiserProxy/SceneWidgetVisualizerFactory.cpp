#include "SceneWidgetVisualizerFactory.h"

#include <stdexcept> // std::invalid_argument

#include "ISceneWidgetVisualizer.h"


std::map<std::string, SceneWidgetVisualizerFactory::ModelCreator>& SceneWidgetVisualizerFactory::getRegistry()
{
    static std::map<std::string, ModelCreator> registry;
    return registry;
}

std::unique_ptr<ISceneWidgetVisualizer> SceneWidgetVisualizerFactory::create(const std::string& modelName)
{
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
    auto registry = getRegistry();
    if (registry.empty())
    {
        throw std::runtime_error("No models registered.");
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
    auto& registry = getRegistry();
    return registry.find(modelName) != registry.end();
}
