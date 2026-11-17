/** @file ActionRegistry.h
 * @brief Registry of actions provided by loaded plugins (see ActionApi.h). Qt-free. */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ActionApi.h"

namespace viz::actions
{
/** @class ActionRegistry
 *  @brief Keeps the actions of all loaded plugins.
 *
 *  Every action remembers the plugin (library path) it came from, so that it is destroyed before the library is
 *  unloaded: its code lives in the library.
 *
 *  The application uses the one registry returned by instance(); tests derive from this class to get a registry of their own. */
class ActionRegistry : public IActionRegistry
{
public:
    /** @return the registry shared by the whole application */
    static ActionRegistry& instance();

    /** @brief Registers @p action, remembering the current owner (see setCurrentOwner()).
     *  @return false if the action is null, has no id, or an action with this id already exists */
    bool registerAction(std::unique_ptr<IAction> action) override;

    /** @brief Actions registered from now on belong to @p owner (path of the plugin); pass "" to clear. */
    void setCurrentOwner(std::string owner);

    /** @brief Destroys the actions that came from the plugin @p owner.
     *  @return how many were removed */
    std::size_t removeOwnedBy(const std::string& owner);

    /** @return ids of all registered actions, in the order of registration */
    std::vector<std::string> ids() const;

    /** @return the action, or nullptr if there is no action with this id. The pointer stays valid until the plugin is unloaded. */
    IAction* find(const std::string& id) const;

    /** @return actions usable with the model @p modelName (those with an empty ActionInfo::supportedModels as well) */
    std::vector<IAction*> actionsFor(const std::string& modelName) const;

    /** @return true if the action can be used with the model (an empty list of supported models = any model) */
    static bool supportsModel(const ActionInfo& info, const std::string& modelName);

protected:
    ActionRegistry() = default; ///< only instance() (and a derived class of tests) creates a registry

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
