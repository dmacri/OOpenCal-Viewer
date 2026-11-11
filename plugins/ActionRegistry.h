/** @file ActionRegistry.h
 * @brief Registry of actions provided by loaded plugins (see ActionApi.h). Qt-free. */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ActionApi.h"

namespace viz::actions
{
/** @brief Keeps the actions of all loaded plugins.
 *
 * Every action remembers the plugin (library path) it came from, so that it is destroyed before the library is
 * unloaded: its code lives in the library. */
class ActionRegistry : public IActionRegistry
{
public:
    /// The registry shared by the whole application.
    static ActionRegistry& instance();

    /// Registry of its own (used by tests).
    ActionRegistry() = default;

    bool registerAction(std::unique_ptr<IAction> action) override;

    /// Actions registered from now on belong to @p owner (path of the plugin); pass "" to clear.
    void setCurrentOwner(std::string owner);

    /// Destroys the actions that came from the plugin @p owner. @return how many were removed
    std::size_t removeOwnedBy(const std::string& owner);

    /// Ids of all registered actions.
    std::vector<std::string> ids() const;

    /// @return nullptr if there is no action with this id. The pointer stays valid until the plugin is unloaded.
    IAction* find(const std::string& id) const;

    /// Actions usable with the model @p modelName (those with an empty ActionInfo::supportedModels as well).
    std::vector<IAction*> actionsFor(const std::string& modelName) const;

    /// @return true if the action can be used with the model (an empty list of supported models = any model)
    static bool supportsModel(const ActionInfo& info, const std::string& modelName);

private:
    struct Entry
    {
        std::string id;
        std::string owner;
        std::unique_ptr<IAction> action;
    };

    std::vector<Entry> entries_;
    std::string currentOwner_;
};
} // namespace viz::actions
