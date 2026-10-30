#include "ActionRunner.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

#include "ActionRegistry.h"
#include "config/Config.h"
#include "config/ConfigCategory.h"
#include "config/ConfigConstants.h"
#include "core/directoryConstants.h"
#include "data/PerformanceMetrics.h"
#include "visualiser/Line.h"
#include "visualiser/SettingParameter.h"
#include "visualiserProxy/ISceneWidgetVisualizer.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"

namespace viz::actions
{
namespace
{
namespace fs = std::filesystem;

/// The same rule as in the main window: data files lie next to Header.txt or in its `Output` subdirectory
/// (nothing is created here, unlike in the main window).
std::string resolveOutputFileName(const fs::path& headerPath, const std::string& outputFileNameFromHeader)
{
    const fs::path directory = headerPath.parent_path();

    bool dataNextToHeader = false;
    for (const auto& entry : fs::directory_iterator(directory))
    {
        if (! entry.is_regular_file())
            continue;
        const std::string name = entry.path().filename().string();
        const std::string extension = entry.path().extension().string();
        if (name.starts_with(outputFileNameFromHeader) && (extension == ".bin" || extension == ".txt"))
        {
            dataNextToHeader = true;
            break;
        }
    }

    const fs::path outputDirectory = dataNextToHeader ? directory : (directory / DirectoryConstants::OUTPUT_DIRECTORY);
    return (outputDirectory / outputFileNameFromHeader).string();
}

/// A mandatory parameter of Header.txt; a missing one is reported instead of dereferencing a null pointer.
ConfigParameter& need(ConfigCategory& category, const char* name)
{
    ConfigParameter* parameter = category.getConfigParameter(name);
    if (! parameter)
        throw std::runtime_error(std::string("Header.txt: parameter '") + name + "' is missing");
    return *parameter;
}

/// Reads the part of Header.txt that is needed to read the data files (the same fields as the main window reads).
SettingParameter readSettingParameter(const std::string& headerPath)
{
    Config config(headerPath, /*printWarnings=*/false);
    SettingParameter sp{};

    {
        ConfigCategory* general = config.getConfigCategory(ConfigConstants::CATEGORY_GENERAL);
        if (! general)
            throw std::runtime_error("Header.txt has no GENERAL section");

        sp.outputFileName = resolveOutputFileName(
            headerPath, need(*general, ConfigConstants::PARAM_OUTPUT_FILE_NAME).getValue<std::string>());
        sp.numberOfColumnX = need(*general, ConfigConstants::PARAM_NUMBER_OF_COLUMNS).getValue<int>();
        sp.numberOfRowsY = need(*general, ConfigConstants::PARAM_NUMBER_OF_ROWS).getValue<int>();
        const auto* slices = general->getConfigParameter(ConfigConstants::PARAM_NUMBER_OF_SLICES);
        sp.numberOfSlicesZ = slices ? slices->getValue<int>() : 1;
        sp.nsteps = static_cast<StepIndex>(need(*general, ConfigConstants::PARAM_NUMBER_STEPS).getValue<int>());
    }
    {
        ConfigCategory* distributed = config.getConfigCategory(ConfigConstants::CATEGORY_DISTRIBUTED);
        if (! distributed)
            throw std::runtime_error("Header.txt has no DISTRIBUTED section");

        sp.nNodeX = static_cast<NodeIndex>(need(*distributed, ConfigConstants::PARAM_NUMBER_NODE_X).getValue<int>());
        sp.nNodeY = static_cast<NodeIndex>(need(*distributed, ConfigConstants::PARAM_NUMBER_NODE_Y).getValue<int>());
        const auto* nodeZ = distributed->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_Z);
        sp.nNodeZ = nodeZ ? static_cast<NodeIndex>(nodeZ->getValue<int>()) : 1;
    }
    {
        ConfigCategory* visualization = config.getConfigCategory(ConfigConstants::CATEGORY_VISUALIZATION);
        const auto* mode = visualization ? visualization->getConfigParameter(ConfigConstants::PARAM_MODE) : nullptr;
        const auto* substates = visualization ? visualization->getConfigParameter(ConfigConstants::PARAM_SUBSTATES) : nullptr;
        const auto* reduction = visualization ? visualization->getConfigParameter(ConfigConstants::PARAM_REDUCTION) : nullptr;
        sp.readMode = mode ? mode->getValue<std::string>() : ConfigConstants::DEFAULT_MODE;
        sp.substates = substates ? substates->getValue<std::string>() : ConfigConstants::DEFAULT_SUBSTATES;
        sp.reduction = reduction ? reduction->getValue<std::string>() : ConfigConstants::DEFAULT_REDUCTION;
    }

    sp.numberOfLines = static_cast<int>(2 * sp.nNodeX * sp.nNodeY * sp.nNodeZ + sp.nNodeX * sp.nNodeZ + sp.nNodeY * sp.nNodeZ);
    sp.step = 0;
    sp.changed = false;
    return sp;
}

/// Disables the performance report of the readers for the time of the export (it prints a line per step).
class MetricsSilencer
{
public:
    MetricsSilencer()
        : wasEnabled_(PerformanceMetrics::isEnabled())
    {
        PerformanceMetrics::setEnabled(false);
    }
    ~MetricsSilencer()
    {
        PerformanceMetrics::setEnabled(wasEnabled_);
    }
    MetricsSilencer(const MetricsSilencer&) = delete;
    MetricsSilencer& operator=(const MetricsSilencer&) = delete;

private:
    bool wasEnabled_;
};

/// Reads the simulation through a visualizer of its own and serves it to an action.
class SimulationReader final : public IActionContext, public ICellReader
{
public:
    SimulationReader(std::unique_ptr<ISceneWidgetVisualizer> visualizer,
                     SettingParameter settings,
                     std::string headerPath,
                     const ActionJob& job,
                     std::vector<std::string>& warnings)
        : visualizer_(std::move(visualizer))
        , settings_(std::move(settings))
        , headerPath_(std::move(headerPath))
        , job_(job)
        , warnings_(warnings)
    {
        description_.modelName = job.modelName;
        description_.modelDirectory = fs::path(headerPath_).parent_path().string();
        description_.outputFileName = settings_.outputFileName;
        description_.columns = settings_.numberOfColumnX;
        description_.rows = settings_.numberOfRowsY;
        description_.layers = std::max(1, settings_.numberOfSlicesZ);
        description_.substates = settings_.getSubstateFields();

        // The same order of calls as in SceneWidget::setupSettingParameters/setupVtkScene/renderVtkScene
        visualizer_->initMatrix(settings_.numberOfColumnX, settings_.numberOfRowsY);
        visualizer_->prepareStage(settings_.nNodeX, settings_.nNodeY, settings_.nNodeZ);
        visualizer_->readStepsOffsetsForAllNodesFromFiles(settings_.nNodeX, settings_.nNodeY, settings_.nNodeZ, settings_.outputFileName);
        steps_ = visualizer_->availableSteps();
        lines_.resize(static_cast<std::size_t>(std::max(1, settings_.numberOfLines)));
    }

    ~SimulationReader() override
    {
        if (description_.layers > 1)
            visualizer_->clearNative3DSlice();
    }

    // --- IActionContext
    const ModelDescription& model() const override
    {
        return description_;
    }

    const std::vector<Step>& steps() const override
    {
        return steps_;
    }

    std::optional<std::string> headerValue(const std::string& category, const std::string& name) const override
    {
        try
        {
            if (! header_)
                header_ = std::make_unique<Config>(headerPath_, /*printWarnings=*/false);
            ConfigCategory* section = header_->getConfigCategory(category, /*ignoreCase=*/true);
            const ConfigParameter* parameter = section ? section->getConfigParameter(name) : nullptr;
            if (parameter)
                return parameter->getDefaultValue();
        }
        catch (const std::exception&)
        {
        }
        return std::nullopt;
    }

    void log(LogLevel level, const std::string& message) override
    {
        if (level == LogLevel::Warning)
            warnings_.push_back(message);

        if (job_.onLog)
        {
            job_.onLog(level, message);
            return;
        }
        (level == LogLevel::Info ? std::cout : std::cerr)
            << (level == LogLevel::Warning ? "Warning: " : level == LogLevel::Error ? "Error: " : "") << message << std::endl;
    }

    // --- reading
    void loadStep(Step step)
    {
        settings_.step = step;
        visualizer_->readStageStateFromFilesForStep(&settings_, lines_.data());
        sliceLayer_ = -1; // a slice belongs to the previous state of the grid
    }

    /// Selects the cell served by substate().
    void select(int layer, int row, int column)
    {
        layer_ = layer;
        row_ = row;
        column_ = column;
    }

    // --- ICellReader (the cell selected by select())
    std::string substate(const char* name) const override
    {
        if (description_.layers > 1 && layer_ != sliceLayer_)
        {
            // A native 3D model is read through a slice of constant z, the only way to reach other layers
            visualizer_->setNative3DSlice(GridSliceAxis::Z, layer_);
            sliceLayer_ = layer_;
        }
        return visualizer_->getCellStringEncoding(row_, column_, name);
    }

private:
    std::unique_ptr<ISceneWidgetVisualizer> visualizer_;
    SettingParameter settings_;
    std::string headerPath_;
    const ActionJob& job_;
    std::vector<std::string>& warnings_;
    ModelDescription description_;
    std::vector<Step> steps_;
    std::vector<Line> lines_;
    int layer_ = 0, row_ = 0, column_ = 0;
    mutable int sliceLayer_ = -1;
    mutable std::unique_ptr<Config> header_;
};

RunResult failure(std::string message, std::vector<std::string> warnings = {})
{
    RunResult result;
    result.ok = false;
    result.message = std::move(message);
    result.warnings = std::move(warnings);
    return result;
}

/// Sets all parameters of the action: the given values, or the defaults. Returns an error message, empty if all is fine.
std::string setParameters(IAction& action, const std::map<std::string, std::string>& values)
{
    const auto declared = action.parameters();

    for (const auto& [name, value] : values)
    {
        if (std::ranges::none_of(declared, [&](const ParameterInfo& p) { return p.name == name; }))
            return "The action has no parameter '" + name + "'";
    }

    for (const auto& parameter : declared)
    {
        const auto given = values.find(parameter.name);
        const std::string& value = given != values.end() ? given->second : parameter.defaultValue;

        if (parameter.required && trimmed(value).empty())
            return "Parameter '" + parameter.name + "' (" + parameter.label + ") is required";
        if (! parameter.choices.empty() && ! std::ranges::contains(parameter.choices, value))
            return "Parameter '" + parameter.name + "' has to be one of the listed choices, got '" + value + "'";

        const Result result = action.setParameter(parameter.name, value);
        if (! result)
            return "Parameter '" + parameter.name + "': " + result.message;
    }
    return {};
}

/// finish(false) after a failure; its own failure is of no interest then.
void abortAction(IAction& action) noexcept
{
    try
    {
        (void) action.finish(false);
    }
    catch (...)
    {
    }
}
} // namespace

RunResult runAction(const ActionJob& job)
{
    std::vector<std::string> warnings;
    IAction* action = nullptr;
    bool started = false;

    try
    {
        action = ActionRegistry::instance().find(job.actionId);
        if (! action)
            return failure("There is no action '" + job.actionId + "'. Is its plugin loaded?");

        if (! ActionRegistry::supportsModel(action->info(), job.modelName))
            return failure("Action '" + job.actionId + "' does not support the model '" + job.modelName + "'");

        if (! SceneWidgetVisualizerFactory::isModelRegistered(job.modelName))
            return failure("The model '" + job.modelName + "' is not loaded");

        if (! fs::exists(job.headerPath))
            return failure("Header.txt not found: " + job.headerPath);

        if (const std::string error = setParameters(*action, job.parameters); ! error.empty())
            return failure(error);

        const MetricsSilencer silencer;

        SimulationReader reader(SceneWidgetVisualizerFactory::create(job.modelName),
                                readSettingParameter(job.headerPath),
                                job.headerPath,
                                job,
                                warnings);
        const auto steps = reader.steps(); // a copy: the reader is also used by the action
        if (steps.empty())
            return failure("The simulation has no saved steps");

        if (const Result result = action->start(reader); ! result)
            return failure(result.message.empty() ? "The action refused to start" : result.message, warnings);
        started = true;

        const ModelDescription& model = reader.model();
        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            if (job.isCancelled && job.isCancelled())
            {
                abortAction(*action);
                RunResult result = failure("Cancelled", warnings);
                result.cancelled = true;
                return result;
            }

            const Step step = steps[i];
            reader.loadStep(step);

            Result result = action->beginStep(step);
            for (int layer = 0; result && layer < model.layers; ++layer)
                for (int row = 0; result && row < model.rows; ++row)
                    for (int column = 0; result && column < model.columns; ++column)
                    {
                        reader.select(layer, row, column);
                        result = action->processCell(CellRef{step, layer, row, column, reader});
                        if (! result)
                            result.message = "step " + std::to_string(step) + ", cell (row " + std::to_string(row) + ", column " +
                                             std::to_string(column) + "): " + result.message;
                    }
            if (result)
                result = action->endStep(step);

            if (! result)
            {
                abortAction(*action);
                return failure(result.message.empty() ? "The action failed at step " + std::to_string(step) : result.message, warnings);
            }

            if (job.onProgress)
                job.onProgress(i + 1, steps.size());
        }

        const Result finished = action->finish(true);
        RunResult result;
        result.ok = finished.ok;
        result.message = finished.message;
        result.warnings = std::move(warnings);
        return result;
    }
    catch (const std::exception& e)
    {
        if (started)
            abortAction(*action);
        return failure(std::string("The action failed: ") + e.what(), warnings);
    }
    catch (...)
    {
        if (started)
            abortAction(*action);
        return failure("The action failed: unknown error", warnings);
    }
}
} // namespace viz::actions
