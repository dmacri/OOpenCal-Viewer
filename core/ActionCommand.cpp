#include "ActionCommand.h"

#include <QCoreApplication>
#include <clocale>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "CommandLineParser.h"
#include "plugins/ActionRegistry.h"
#include "plugins/ActionRunner.h"
#include "plugins/CppModuleBuilder.h"
#include "plugins/ModelLoader.h"
#include "plugins/PluginLoader.h"

// The options are also registered in CommandLineParser (only to be listed in --help)
static_assert(std::string_view(CommandLineParser::ARG_RUN_ACTION) == viz::cli::RUN_ACTION_OPTION, "spell the option the same way");
static_assert(std::string_view(CommandLineParser::ARG_ACTION_PARAM) == viz::cli::ACTION_PARAM_OPTION, "spell the option the same way");
static_assert(std::string_view(CommandLineParser::ARG_LIST_ACTIONS) == viz::cli::LIST_ACTIONS_OPTION, "spell the option the same way");

namespace viz::cli
{
namespace
{
namespace fs = std::filesystem;
using viz::actions::ActionRegistry;

struct Request
{
    bool list = false;
    std::string actionId;
    std::string directory;
    std::vector<std::string> pluginPaths;
    std::map<std::string, std::string> parameters;
    std::string error;
};

/// Value of `--option=value` or `--option value` (the latter consumes the next argument).
std::optional<std::string> optionValue(std::string_view option, int& i, int argc, const char* const argv[])
{
    const std::string_view arg = argv[i];
    if (arg == option)
        return i + 1 < argc ? std::optional<std::string>(argv[++i]) : std::nullopt;
    if (arg.size() > option.size() && arg.substr(0, option.size()) == option && arg[option.size()] == '=')
        return std::string(arg.substr(option.size() + 1));
    return std::nullopt;
}

bool isOption(std::string_view option, std::string_view arg)
{
    return arg == option || (arg.size() > option.size() && arg.substr(0, option.size()) == option && arg[option.size()] == '=');
}

bool isActionCommandLine(int argc, const char* const argv[])
{
    for (int i = 1; i < argc; ++i)
        if (isOption(RUN_ACTION_OPTION, argv[i]) || std::string_view(argv[i]) == LIST_ACTIONS_OPTION)
            return true;
    return false;
}

Request parse(int argc, const char* const argv[])
{
    Request request;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == LIST_ACTIONS_OPTION)
            request.list = true;
        else if (isOption(RUN_ACTION_OPTION, arg))
        {
            const auto value = optionValue(RUN_ACTION_OPTION, i, argc, argv);
            if (! value)
                request.error = std::string(RUN_ACTION_OPTION) + " needs an action id";
            else
                request.actionId = *value;
        }
        else if (isOption(ACTION_PARAM_OPTION, arg))
        {
            const auto value = optionValue(ACTION_PARAM_OPTION, i, argc, argv);
            const auto equals = value ? value->find('=') : std::string::npos;
            if (equals == std::string::npos || equals == 0)
                request.error = std::string(ACTION_PARAM_OPTION) + " has to be given as name=value";
            else
                request.parameters[value->substr(0, equals)] = value->substr(equals + 1);
        }
        else if (isOption("--loadModel", arg))
        {
            if (const auto value = optionValue("--loadModel", i, argc, argv))
                request.pluginPaths.push_back(*value);
            else
                request.error = "--loadModel needs a path";
        }
        else if (! arg.starts_with("-") && request.directory.empty())
            request.directory = arg;
    }
    return request;
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

int listActions()
{
    const auto ids = ActionRegistry::instance().ids();
    if (ids.empty())
    {
        std::cout << "No actions are available. Load a plugin with --loadModel <library>." << std::endl;
        return 0;
    }

    for (const auto& id : ids)
    {
        const auto* action = ActionRegistry::instance().find(id);
        const auto info = action->info();
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
        for (const auto& parameter : action->parameters())
        {
            std::cout << "    --actionParam=" << parameter.name << "=<" << typeName(parameter.type) << ">"
                      << (parameter.required ? "  (required)" : "") << "  " << parameter.label;
            if (! parameter.defaultValue.empty())
                std::cout << "  [default: " << parameter.defaultValue << "]";
            std::cout << '\n';
            if (! parameter.description.empty())
                std::cout << "        " << parameter.description << '\n';
        }
    }
    return 0;
}

/// Compiles (if needed) and loads the model of a simulation directory; returns its name or an empty string.
std::string loadModel(const fs::path& headerPath)
{
    ModelLoader loader;
#ifdef OOPENCAL_VIEWER_ROOT
    loader.getBuilder()->setProjectRootPath(OOPENCAL_VIEWER_ROOT);
#endif
    const auto result = loader.loadModelFromDirectory(headerPath.parent_path().string());
    if (! result.success)
    {
        std::cerr << "✗ Cannot load the model from '" << headerPath.parent_path().string() << "'";
        if (result.compilationResult)
            std::cerr << ":\n" << result.compilationResult->stdErr;
        std::cerr << std::endl;
        return {};
    }

    PluginLoader& pluginLoader = PluginLoader::instance();
    if (! pluginLoader.loadPlugin(result.compiledModulePath, /*overridePlugin=*/true))
    {
        std::cerr << "✗ Cannot load the compiled model: " << pluginLoader.getLastError() << std::endl;
        return {};
    }
    for (const auto& plugin : pluginLoader.getLoadedPlugins())
        if (plugin.path == result.compiledModulePath && ! plugin.name.empty())
            return plugin.name;
    return result.outputFileName;
}

int run(const Request& request)
{
    if (request.actionId.empty())
    {
        std::cerr << "Error: give the action with " << RUN_ACTION_OPTION << "=<id> (see " << LIST_ACTIONS_OPTION << ")" << std::endl;
        return 2;
    }
    if (! ActionRegistry::instance().find(request.actionId))
    {
        std::cerr << "Error: there is no action '" << request.actionId << "'. Is its plugin loaded (--loadModel <library>)? See "
                  << LIST_ACTIONS_OPTION << "." << std::endl;
        return 2;
    }
    if (request.directory.empty())
    {
        std::cerr << "Error: give the directory of the simulation (the one with Header.txt)" << std::endl;
        return 2;
    }

    fs::path headerPath = request.directory;
    if (fs::is_directory(headerPath))
        headerPath /= "Header.txt";
    if (! fs::exists(headerPath))
    {
        std::cerr << "Error: " << headerPath.string() << " does not exist" << std::endl;
        return 2;
    }

    const std::string modelName = loadModel(headerPath);
    if (modelName.empty())
        return 1;

    viz::actions::ActionJob job;
    job.actionId = request.actionId;
    job.modelName = modelName;
    job.headerPath = headerPath.string();
    job.parameters = request.parameters;
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
        return 1;
    }
    std::cout << "✓ " << result.message << std::endl;
    if (! result.warnings.empty())
        std::cout << "  (" << result.warnings.size() << " warning(s) above)" << std::endl;
    return 0;
}
} // namespace

std::optional<int> runActionCommand(int& argc, char* argv[])
{
    if (! isActionCommandLine(argc, argv))
        return std::nullopt;

    QCoreApplication coreApplication(argc, argv); // no GUI and no display needed
    std::setlocale(LC_NUMERIC, "C");              // the same decimal point in the files regardless of the system language

    const Request request = parse(argc, argv);
    if (! request.error.empty())
    {
        std::cerr << "Error: " << request.error << std::endl;
        return 2;
    }

    PluginLoader& pluginLoader = PluginLoader::instance();
    pluginLoader.loadFromStandardDirectories({"./plugins", "../plugins", "./build/plugins"});
    for (const auto& path : request.pluginPaths)
        if (! pluginLoader.isPluginLoaded(path) && ! pluginLoader.loadPlugin(path)) // may be already loaded from ./plugins
        {
            std::cerr << "✗ Cannot load the plugin " << path << ": " << pluginLoader.getLastError() << std::endl;
            return 1;
        }

    return request.list ? listActions() : run(request);
}
} // namespace viz::cli
