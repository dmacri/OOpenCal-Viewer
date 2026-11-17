#include "ActionRegistry.h"

#include <algorithm>
#include <iostream>

namespace viz::actions
{
ActionRegistry& ActionRegistry::instance()
{
    static ActionRegistry registry;
    return registry;
}

bool ActionRegistry::registerAction(std::unique_ptr<IAction> action)
{
    if (! action)
        return false;

    const std::string id = action->info().id;
    if (id.empty())
    {
        std::cerr << "Warning: an action without an id was refused" << std::endl;
        return false;
    }
    if (find(id))
    {
        std::cerr << "Warning: action '" << id << "' is already registered" << std::endl;
        return false;
    }

    entries_.push_back(Entry{id, currentOwner_, std::move(action)});
    return true;
}

void ActionRegistry::setCurrentOwner(std::string owner)
{
    currentOwner_ = std::move(owner);
}

std::size_t ActionRegistry::removeOwnedBy(const std::string& owner)
{
    if (owner.empty())
        return 0;

    const auto removed = std::erase_if(entries_, [&](const Entry& entry) { return entry.owner == owner; });
    return removed;
}

std::vector<std::string> ActionRegistry::ids() const
{
    std::vector<std::string> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_)
        result.push_back(entry.id);
    return result;
}

IAction* ActionRegistry::find(const std::string& id) const
{
    const auto it = std::ranges::find(entries_, id, &Entry::id);
    return it == entries_.end() ? nullptr : it->action.get();
}

bool ActionRegistry::supportsModel(const ActionInfo& info, const std::string& modelName)
{
    return info.supportedModels.empty() || std::ranges::contains(info.supportedModels, modelName);
}

std::vector<IAction*> ActionRegistry::actionsFor(const std::string& modelName) const
{
    std::vector<IAction*> result;
    for (const auto& entry : entries_)
    {
        if (supportsModel(entry.action->info(), modelName))
            result.push_back(entry.action.get());
    }
    return result;
}
} // namespace viz::actions
