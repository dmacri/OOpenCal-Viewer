#include "ActionCommand.h"

#include <QCoreApplication>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>

#include "ApplicationName.h"
#include "CommandLineParser.h"
#include "plugins/ActionRegistry.h"
#include "plugins/ActionRunner.h"
#include "plugins/CppModuleBuilder.h"
#include "plugins/ModelLoader.h"
#include "plugins/PluginLoader.h"
#include "widgets/LocaleSettingsDialog.h"

namespace viz::cli
{
namespace
{
namespace fs = std::filesystem;
using viz::actions::ActionRegistry;

enum ExitCode
{
    EXIT_DONE = 0,
    EXIT_ACTION_FAILED = 1,
    EXIT_WRONG_COMMAND_LINE = 2
};

bool asksForActionCommand(int argc, char* argv[]);
bool loadPlugins(const CommandLineParser& commandLine);
int listActions();
int runAction(const CommandLineParser& commandLine);
std::optional<fs::path> findHeader(const CommandLineParser& commandLine);
std::optional<std::string> loadModelOfSimulation(const fs::path& headerPath);
std::optional<std::map<std::string, std::string>> parseActionParameters(const std::vector<std::string>& assignments);
void printAction(const viz::actions::IAction& action);
const char* typeName(viz::actions::ParameterInfo::Type type);
} // namespace

std::optional<int> runActionCommand(int& argc, char* argv[])
{
    if (! asksForActionCommand(argc, argv))
        return std::nullopt;

    QCoreApplication coreApplication(argc, argv); // no GUI and no display needed

    // The same as the GUI does: numbers are read and written in the locale saved in Settings -> Number Locale
    QCoreApplication::setApplicationName(APPLICATION_NAME);
    LocaleSettingsDialog::applySavedLocale();

    CommandLineParser commandLine;
    if (! commandLine.parse(argc, argv))
        return EXIT_WRONG_COMMAND_LINE;

    if (! loadPlugins(commandLine))
        return EXIT_ACTION_FAILED;

    return commandLine.shouldListActions() ? listActions() : runAction(commandLine);
}

namespace
{
bool asksForActionCommand(int argc, char* argv[])
{
    // The command line is parsed properly later; here it is only looked through, as the GUI must not be created for it
    return std::any_of(argv + 1, argv + argc,
                       [](std::string_view argument)
                       {
                           return argument == CommandLineParser::ARG_LIST_ACTIONS || argument.starts_with(CommandLineParser::ARG_RUN_ACTION);
                       });
}

bool loadPlugins(const CommandLineParser& commandLine)
{
    PluginLoader& pluginLoader = PluginLoader::instance();
    pluginLoader.loadFromStandardDirectories();

    auto failed = pluginLoader.loadPlugins(commandLine.getLoadModelPaths(), PluginKind::Model);
    const auto failedActions = pluginLoader.loadPlugins(commandLine.getLoadPluginPaths(), PluginKind::Actions);
    failed.insert(failed.end(), failedActions.begin(), failedActions.end());

    for (const auto& path : failed)
        std::cerr << "✗ Cannot load the plugin " << path << ": " << pluginLoader.getLastError() << std::endl;
    return failed.empty();
}

int listActions()
{
    const auto ids = ActionRegistry::instance().ids();
    if (ids.empty())
        std::cout << "No actions are available. Load a plugin with --loadPlugin <library>." << std::endl;

    for (const auto& id : ids)
        printAction(*ActionRegistry::instance().find(id));
    return EXIT_DONE;
}

int runAction(const CommandLineParser& commandLine)
{
    if (! commandLine.getRunActionId())
    {
        std::cerr << "Error: give the action with " << CommandLineParser::ARG_RUN_ACTION << "=<id> (see "
                  << CommandLineParser::ARG_LIST_ACTIONS << ")" << std::endl;
        return EXIT_WRONG_COMMAND_LINE;
    }
    const std::string& actionId = *commandLine.getRunActionId();
    if (! ActionRegistry::instance().find(actionId))
    {
        std::cerr << "Error: there is no action '" << actionId << "'. Is its plugin loaded (" << CommandLineParser::ARG_LOAD_PLUGIN
                  << " <library>)? See " << CommandLineParser::ARG_LIST_ACTIONS << "." << std::endl;
        return EXIT_WRONG_COMMAND_LINE;
    }

    const auto parameters = parseActionParameters(commandLine.getActionParameters());
    const auto headerPath = findHeader(commandLine);
    if (! parameters || ! headerPath)
        return EXIT_WRONG_COMMAND_LINE;

    const auto modelName = loadModelOfSimulation(*headerPath);
    if (! modelName)
        return EXIT_ACTION_FAILED;

    viz::actions::ActionJob job;
    job.actionId = actionId;
    job.modelName = *modelName;
    job.headerPath = headerPath->string();
    job.parameters = *parameters;
    job.onProgress = [lastPercent = -1](std::size_t done, std::size_t total) mutable
    {
        const int percent = static_cast<int>(100 * done / std::max<std::size_t>(total, 1));
        if (percent / 10 != lastPercent / 10 || done == total)
            std::cerr << "\r" << percent << "% (" << done << "/" << total << " steps)" << (done == total ? "\n" : "") << std::flush;
        lastPercent = percent;
    };

    const auto result = viz::actions::runAction(job);
    if (! result.ok)
    {
        std::cerr << "✗ " << result.message << std::endl;
        return EXIT_ACTION_FAILED;
    }
    std::cout << "✓ " << result.message << std::endl;
    if (! result.warnings.empty())
        std::cout << "  (" << result.warnings.size() << " warning(s) above)" << std::endl;
    return EXIT_DONE;
}

/// @return path to Header.txt of the simulation given on the command line (a directory), or nothing after printing why not
std::optional<fs::path> findHeader(const CommandLineParser& commandLine)
{
    if (! commandLine.getConfigFile() || ! commandLine.isModelDirectory())
    {
        std::cerr << "Error: give the directory of the simulation (the one with Header.txt)" << std::endl;
        return std::nullopt;
    }

    const fs::path headerPath = fs::path(*commandLine.getConfigFile()) / "Header.txt";
    if (! fs::exists(headerPath))
    {
        std::cerr << "Error: " << headerPath.string() << " does not exist" << std::endl;
        return std::nullopt;
    }
    return headerPath;
}

/// Compiles (if needed) and loads the model of the simulation, like the main window does; returns its name.
std::optional<std::string> loadModelOfSimulation(const fs::path& headerPath)
{
    ModelLoader modelLoader;
#ifdef OOPENCAL_VIEWER_ROOT
    modelLoader.getBuilder()->setProjectRootPath(OOPENCAL_VIEWER_ROOT);
#endif
    const auto result = modelLoader.loadModelFromDirectory(headerPath.parent_path().string());
    if (! result.success)
    {
        std::cerr << "✗ Cannot load the model from '" << headerPath.parent_path().string() << "'";
        if (result.compilationResult)
            std::cerr << ":\n" << result.compilationResult->stdErr;
        std::cerr << std::endl;
        return std::nullopt;
    }

    PluginLoader& pluginLoader = PluginLoader::instance();
    if (! pluginLoader.loadPlugin(result.compiledModulePath, /*overridePlugin=*/true))
    {
        std::cerr << "✗ Cannot load the compiled model: " << pluginLoader.getLastError() << std::endl;
        return std::nullopt;
    }

    const PluginInfo* plugin = pluginLoader.findLoadedPlugin(result.compiledModulePath);
    return plugin && ! plugin->name.empty() ? plugin->name : result.outputFileName;
}

/// Splits the values of `--actionParam` (name=value); prints why and returns nothing if one is malformed.
std::optional<std::map<std::string, std::string>> parseActionParameters(const std::vector<std::string>& assignments)
{
    std::map<std::string, std::string> parameters;
    for (const auto& assignment : assignments)
    {
        const auto equals = assignment.find('=');
        if (equals == std::string::npos || equals == 0)
        {
            std::cerr << "Error: " << CommandLineParser::ARG_ACTION_PARAM << " has to be given as name=value, got '" << assignment << "'" << std::endl;
            return std::nullopt;
        }
        parameters[assignment.substr(0, equals)] = assignment.substr(equals + 1);
    }
    return parameters;
}

void printAction(const viz::actions::IAction& action)
{
    const auto info = action.info();
    std::cout << info.id << "  [" << info.category << ", plugin: " << info.pluginName << "]\n  " << info.title << '\n';
    if (! info.description.empty())
        std::cout << "  " << info.description << '\n';
    if (! info.supportedModels.empty())
    {
        std::cout << "  models:";
        for (const auto& model : info.supportedModels)
            std::cout << " \"" << model << '"';
        std::cout << '\n';
    }
    for (const auto& parameter : action.parameters())
    {
        std::cout << "    " << CommandLineParser::ARG_ACTION_PARAM << "=" << parameter.name << "=<" << typeName(parameter.type) << ">"
                  << (parameter.required ? "  (required)" : "") << "  " << parameter.label;
        if (! parameter.defaultValue.empty())
            std::cout << "  [default: " << parameter.defaultValue << "]";
        std::cout << '\n';
        if (! parameter.description.empty())
            std::cout << "        " << parameter.description << '\n';
    }
}

const char* typeName(viz::actions::ParameterInfo::Type type)
{
    using Type = viz::actions::ParameterInfo::Type;
    switch (type)
    {
        case Type::String: return "text";
        case Type::Integer: return "integer";
        case Type::Real: return "number";
        case Type::Boolean: return "true/false";
        case Type::SaveFilePath: return "file to write";
        case Type::OpenFilePath: return "file to read";
        case Type::Directory: return "directory";
    }
    return "";
}
} // namespace
} // namespace viz::cli
