/** @file ActionApi.h
 * @brief Public API of *action plugins*: model-independent mechanism for actions that work on the data of a simulation
 *        (exports, analyses, conversions...), e.g. exporting trajectories of pedestrians.
 *
 * Code that only makes sense for one kind of model (trajectories of pedestrians, a flow through a door, ...) does not
 * belong to the Viewer. It is delivered as a shared library (`.so`/`.dll`) loaded the same way as model plugins
 * (`Model -> Load Plugin...`, `./plugins/`, `--loadModel`). After loading, every action shows up in the `Actions` menu
 * of the application (for the models it supports).
 *
 * The Viewer drives the action, the plugin only reacts:
 *
 * 1. parameters()     - the plugin lists its parameters (name, description, type, default),
 * 2. setParameter()   - the Viewer sets every parameter (the plugin validates it and answers ok/error + message),
 * 3. start()          - the plugin gets the description of the simulation and prepares itself,
 * 4. for every step, in ascending order:  beginStep(), processCell() for **every** cell, endStep(),
 * 5. finish(true)     - everything was processed: the plugin writes its result,
 *    finish(false)    - the action was cancelled or failed: the plugin should drop what it has collected.
 *
 * Every function that can fail returns Result (ok + message), because C++ exceptions should not cross the library
 * boundary. (If a plugin throws anyway, the Viewer catches it and reports a failure.)
 *
 * The plugin does not need Qt, VTK nor the `Cell` class of the model. Values of a cell are read with
 * ICellReader::substate(), which gives `Cell::stringEncoding(substate)` - the same text the Viewer shows in its
 * tool tip. This header depends on the C++ standard library only. The Viewer and the plugin have to be built with
 * the same compiler and standard library (as for model plugins), because `std::string`, `std::vector`... cross the
 * library boundary.
 *
 * Minimal plugin:
 * @code
 * #include "plugins/ActionApi.h"
 * using namespace viz::actions;
 *
 * class CountCells : public IAction
 * {
 * public:
 *     ActionInfo info() const override { return {.id = "count-cells", .pluginName = "Demo", .title = "Count cells"}; }
 *     Result start(IActionContext&) override { count_ = 0; return Result::success(); }
 *     Result processCell(const CellRef&) override { ++count_; return Result::success(); }
 *     Result finish(bool completed) override
 *     {
 *         return completed ? Result::success(std::to_string(count_) + " cells") : Result::success();
 *     }
 * private:
 *     long long count_ = 0;
 * };
 *
 * VIZ_ACTION_PLUGIN(registry) { registry.registerAction(std::make_unique<CountCells>()); }
 * @endcode
 *
 * @see doc/ACTION_PLUGINS.md, examples/trajectory_export_plugin/ */

#pragma once

#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
    #define VIZ_ACTION_API_EXPORT extern "C" __declspec(dllexport)
#else
    #define VIZ_ACTION_API_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace viz::actions
{
/// Version of this API. A plugin built against a different version is refused by the Viewer.
/// Bump it whenever a virtual function of the interfaces below changes.
inline constexpr int API_VERSION = 2;

/// Index of a simulation step (the same as `StepIndex` in the Viewer).
using Step = unsigned;

/// Outcome of a call: success or failure with a message for the user.
struct Result
{
    bool ok = true;
    std::string message;

    static Result success(std::string message = {})
    {
        return {true, std::move(message)};
    }
    static Result failure(std::string message)
    {
        return {false, std::move(message)};
    }
    explicit operator bool() const
    {
        return ok;
    }
};

/// A parameter of an action. The Viewer builds a dialog from these and `--actionParam=name=value` sets them.
struct ParameterInfo
{
    enum class Type
    {
        String,        ///< any text
        Integer,       ///< whole number
        Real,          ///< floating point number (a dot is the decimal separator, regardless of the locale)
        Boolean,       ///< true/false (also 1/0, yes/no, on/off)
        SaveFilePath,  ///< path of a file to be written (file dialog "Save as")
        OpenFilePath,  ///< path of an existing file to be read
        Directory      ///< path of a directory
    };

    std::string name;                  ///< key, e.g. "output_file" (no spaces)
    std::string label;                 ///< text shown in the dialog, e.g. "Output file"
    std::string description;           ///< tool tip / help
    Type type = Type::String;
    std::string defaultValue;          ///< default, written as text; may be empty
    bool required = false;             ///< the Viewer refuses to start while the value is empty
    std::vector<std::string> choices;  ///< if not empty, the value has to be one of them (shown as a combo box)
    std::string fileFilter;            ///< for paths, e.g. "Text files (*.txt)"
};

/// Static description of an action.
struct ActionInfo
{
    std::string id;                            ///< unique and stable, used on the command line: "export-trajectories-julich"
    std::string pluginName;                    ///< name of the plugin (shown in the menu next to the action and in the log)
    std::string title;                         ///< entry of the menu, e.g. "Pedestrian trajectories (Jülich format)"
    std::string category = "Actions";          ///< submenu of the `Actions` menu, e.g. "Export", "Analysis"
    std::string description;                   ///< longer help
    std::vector<std::string> supportedModels;  ///< model names (as in the Model menu) the action works with; empty = any model
};

/// What the Viewer knows about the simulation being processed.
struct ModelDescription
{
    std::string modelName;                ///< name of the model (`Cell` class), as in the Model menu
    std::string modelDirectory;           ///< directory with `Header.txt`
    std::string outputFileName;           ///< base name (with directory) of the data files
    int columns = 0;                      ///< width of the whole grid (all nodes together)
    int rows = 0;                         ///< height of the whole grid
    int layers = 1;                       ///< depth of the whole grid (1 for 2D models)
    std::vector<std::string> substates;  ///< names of the substates listed in `Header.txt`
};

enum class LogLevel
{
    Info,
    Warning,
    Error
};

/// Given to IAction::start(); valid only during that call.
class IActionContext
{
public:
    virtual ~IActionContext() = default;

    virtual const ModelDescription& model() const = 0;

    /// Steps present in the output files, in ascending order (may be sparse, e.g. `visualization_step_rate=10`).
    /// They are the steps that will be passed to beginStep()/processCell()/endStep().
    virtual const std::vector<Step>& steps() const = 0;

    /// Value of a parameter of `Header.txt`, e.g. `headerValue("GENERAL", "number_steps")`.
    virtual std::optional<std::string> headerValue(const std::string& category, const std::string& name) const = 0;

    /// Message for the user (the console / the log of the application). Warnings are collected and shown at the end.
    virtual void log(LogLevel level, const std::string& message) = 0;
};

/// Access to the values of one cell.
class ICellReader
{
public:
    virtual ~ICellReader() = default;

    /** @brief Text encoding of the cell - `Cell::stringEncoding(@p substate)`.
     *  @param substate name of a substate understood by the model's Cell (e.g. "pedestriansIds");
     *                  nullptr or "" = the whole cell. */
    virtual std::string substate(const char* substate) const = 0;
};

/// A cell passed to IAction::processCell(). The reference is valid only during that call.
struct CellRef
{
    Step step;
    int layer;   ///< z, 0 for 2D models
    int row;     ///< 0 is the top row (as in the Viewer)
    int column;  ///< 0 is the left column
    const ICellReader& cell;
};

/// An action. One instance lives as long as the plugin is loaded and is used for many runs: start() has to reset
/// everything left by the previous run.
class IAction
{
public:
    virtual ~IAction() = default;

    virtual ActionInfo info() const = 0;

    /// Parameters the user can set. Called when the dialog is built (and before every run).
    virtual std::vector<ParameterInfo> parameters() const
    {
        return {};
    }

    /// Sets a parameter (text is always given, see ParameterInfo::Type). Validate it: the message of a failure is shown
    /// to the user. The Viewer calls it for every parameter before start() - with the default value if the user
    /// gave none.
    virtual Result setParameter(const std::string& name, const std::string& value)
    {
        (void) name;
        (void) value;
        return Result::failure("this action has no parameters");
    }

    /// Beginning of a run, all parameters are set. Return a failure to stop (finish() is not called then).
    virtual Result start(IActionContext& context) = 0;

    /// Before the cells of a step.
    virtual Result beginStep(Step step)
    {
        (void) step;
        return Result::success();
    }

    /// Called for every cell of every step (in order: layer, row, column). A failure stops the run.
    virtual Result processCell(const CellRef& cell) = 0;

    /// After the cells of a step.
    virtual Result endStep(Step step)
    {
        (void) step;
        return Result::success();
    }

    /// End of a run. @p completed is false when the run was cancelled or failed - drop what was collected.
    /// The message of a successful result is shown to the user (e.g. "Saved 75 trajectories to ...").
    virtual Result finish(bool completed) = 0;
};

/// Passed to a plugin when it is loaded.
class IActionRegistry
{
public:
    virtual ~IActionRegistry() = default;

    /// @return false (and the action is dropped) if an action with the same id is already registered
    virtual bool registerAction(std::unique_ptr<IAction> action) = 0;
};

/// @name Parsing of parameter values (locale independent; the whole text has to be a number)
/// @{
inline std::string trimmed(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

inline bool parseInteger(const std::string& text, long long& value)
{
    const std::string t = trimmed(text);
    const auto [end, error] = std::from_chars(t.data(), t.data() + t.size(), value);
    return ! t.empty() && error == std::errc{} && end == t.data() + t.size();
}

inline bool parseReal(const std::string& text, double& value)
{
    const std::string t = trimmed(text);
    const auto [end, error] = std::from_chars(t.data(), t.data() + t.size(), value);
    return ! t.empty() && error == std::errc{} && end == t.data() + t.size();
}

inline bool parseBoolean(const std::string& text, bool& value)
{
    std::string t = trimmed(text);
    for (auto& c : t)
        c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    if (t == "true" || t == "1" || t == "yes" || t == "on")
    {
        value = true;
        return true;
    }
    if (t == "false" || t == "0" || t == "no" || t == "off")
    {
        value = false;
        return true;
    }
    return false;
}
/// @}
} // namespace viz::actions

/** @def VIZ_ACTION_PLUGIN
 * Defines the entry points the Viewer looks for in a plugin: `getActionApiVersion()` and
 * `registerActionPlugin(IActionRegistry&)`. Use it once per library; the body registers the actions.
 * A library may additionally export the usual `registerPlugin()` of a model plugin. */
#define VIZ_ACTION_PLUGIN(registryParameter)                                                    \
    VIZ_ACTION_API_EXPORT int getActionApiVersion()                                             \
    {                                                                                           \
        return ::viz::actions::API_VERSION;                                                     \
    }                                                                                           \
    VIZ_ACTION_API_EXPORT void registerActionPlugin(::viz::actions::IActionRegistry& registryParameter)
