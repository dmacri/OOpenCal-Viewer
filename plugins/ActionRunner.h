/** @file ActionRunner.h
 * @brief Runs an action plugin (see ActionApi.h) on a simulation directory. Free of Qt widgets, usable without a display.
 *
 * The data is read with a visualizer of its own (created by SceneWidgetVisualizerFactory, like the one of the main
 * window), so running an action neither changes nor depends on what is shown in the Viewer. */

#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ActionApi.h"

namespace viz::actions
{
/// Everything needed to run an action.
struct ActionJob
{
    std::string actionId;                           ///< id of a registered action (ActionRegistry)
    std::string modelName;                          ///< model registered in SceneWidgetVisualizerFactory, as in the Model menu
    std::string headerPath;                         ///< path to `Header.txt` of the simulation
    std::map<std::string, std::string> parameters;  ///< values of parameters; missing ones get the default of the action

    std::function<void(std::size_t done, std::size_t total)> onProgress; ///< optional, called after every step
    std::function<bool()> isCancelled;                                   ///< optional, checked before every step
    std::function<void(LogLevel, const std::string&)> onLog;             ///< optional; by default the messages go to std::cout/std::cerr
};

/// Outcome of a run.
struct RunResult
{
    bool ok = false;
    bool cancelled = false;
    std::string message;                    ///< summary or the reason of the failure
    std::vector<std::string> warnings;      ///< everything the action logged as a warning
};

/** @brief Runs the action: sets the parameters, start(), the steps and cells, finish(). Never throws: every failure
 *  (unknown action, missing data, a failure or an exception of the plugin) is returned in RunResult. */
RunResult runAction(const ActionJob& job);
} // namespace viz::actions
