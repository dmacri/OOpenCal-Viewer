#include "ActionRunner.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include "ActionRegistry.h"
#include "config/Config.h"
#include "config/ConfigCategory.h"
#include "core/types.h"
#include "data/PerformanceMetrics.h"
#include "data/SimulationHeader.h"
#include "visualiser/Line.h"
#include "visualiser/SettingParameter.h"
#include "visualiserProxy/ISceneWidgetVisualizer.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"

namespace viz::actions
{
namespace
{
namespace fs = std::filesystem;

// The step crosses the library boundary as a fixed-width type, the Viewer uses StepIndex for the same thing
static_assert(std::is_same_v<Step, StepIndex>, "viz::actions::Step has to be the same type as StepIndex");

/** Disables the performance report of the readers for the time of the run (it prints a line per step). */
class MetricsSilencer
{
public:
    MetricsSilencer()
        : wasEnabled(PerformanceMetrics::isEnabled())
    {
        PerformanceMetrics::setEnabled(false);
    }
    ~MetricsSilencer()
    {
        PerformanceMetrics::setEnabled(wasEnabled);
    }
    MetricsSilencer(const MetricsSilencer&) = delete;
    MetricsSilencer& operator=(const MetricsSilencer&) = delete;

private:
    bool wasEnabled;
};

/** Reads the simulation through a visualizer of its own and serves it to an action. */
class SimulationReader final : public IActionContext, public ICellReader
{
public:
    SimulationReader(std::unique_ptr<ISceneWidgetVisualizer> visualizer, const ActionJob& job, std::vector<std::string>& warnings);
    ~SimulationReader() override;

    // IActionContext
    const ModelDescription& model() const override;
    const std::vector<Step>& steps() const override;
    std::optional<std::string> headerValue(const std::string& category, const std::string& name) const override;
    void log(LogLevel level, const std::string& message) override;

    /** Reads the whole grid of @p step. */
    void loadStep(Step step);

    /** Selects the cell served by substate(). */
    void select(int layer, int row, int column);

    // ICellReader: the cell selected by select()
    std::string substate(const char* name) const override;

private:
    void readSettings();
    void describeModel();
    void openDataFiles();

    std::unique_ptr<ISceneWidgetVisualizer> visualizer;
    const ActionJob& job;
    std::vector<std::string>& warnings;
    SettingParameter settings{};
    ModelDescription description;
    std::vector<Step> availableSteps;
    std::vector<Line> lines;
    int layer = 0, row = 0, column = 0;
    mutable int sliceLayer = -1;
    mutable std::unique_ptr<Config> header;
};

RunResult failedRun(std::string message, std::vector<std::string> warnings = {});
std::string setParameters(IAction& action, const std::map<std::string, std::string>& values);
Status processStep(IAction& action, SimulationReader& reader, Step step);
void abortAction(IAction& action) noexcept;
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
            return failedRun("There is no action '" + job.actionId + "'. Is its plugin loaded?");

        if (! ActionRegistry::supportsModel(action->info(), job.modelName))
            return failedRun("Action '" + job.actionId + "' does not support the model '" + job.modelName + "'");

        if (! SceneWidgetVisualizerFactory::isModelRegistered(job.modelName))
            return failedRun("The model '" + job.modelName + "' is not loaded");

        if (! fs::exists(job.headerPath))
            return failedRun("Header.txt not found: " + job.headerPath);

        if (const std::string error = setParameters(*action, job.parameters); ! error.empty())
            return failedRun(error);

        const MetricsSilencer silencer;

        SimulationReader reader(SceneWidgetVisualizerFactory::create(job.modelName), job, warnings);
        const std::vector<Step> steps = reader.steps(); // a copy: the reader is also used by the action
        if (steps.empty())
            return failedRun("The simulation has no saved steps");

        if (const Status status = action->start(reader); ! status)
            return failedRun(status.error().empty() ? "The action refused to start" : status.error(), warnings);
        started = true;

        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            if (job.isCancelled && job.isCancelled())
            {
                abortAction(*action);
                RunResult result = failedRun("Cancelled", warnings);
                result.cancelled = true;
                return result;
            }

            if (const Status status = processStep(*action, reader, steps[i]); ! status)
            {
                abortAction(*action);
                return failedRun(status.error().empty() ? "The action failed at step " + std::to_string(steps[i]) : status.error(), warnings);
            }

            if (job.onProgress)
                job.onProgress(i + 1, steps.size());
        }

        const Summary summary = action->finish(true);
        RunResult result;
        result.ok = summary.has_value();
        result.message = summary ? *summary : summary.error();
        result.warnings = std::move(warnings);
        return result;
    }
    catch (const std::exception& e)
    {
        if (started)
            abortAction(*action);
        return failedRun(std::string("The action failed: ") + e.what(), warnings);
    }
    catch (...)
    {
        if (started)
            abortAction(*action);
        return failedRun("The action failed: unknown error", warnings);
    }
}

namespace
{
SimulationReader::SimulationReader(std::unique_ptr<ISceneWidgetVisualizer> visualizer, const ActionJob& job, std::vector<std::string>& warnings)
    : visualizer(std::move(visualizer))
    , job(job)
    , warnings(warnings)
{
    readSettings();
    describeModel();
    openDataFiles();
}

SimulationReader::~SimulationReader()
{
    if (description.layers > LAYERS_OF_2D_MODEL)
        visualizer->clearNative3DSlice();
}

void SimulationReader::readSettings()
{
    SimulationHeader::readSettingParameter(job.headerPath, settings, /*printWarnings=*/false);
    settings.numberOfLines = SimulationHeader::numberOfBoundaryLines(settings);
    settings.step = 0;
    settings.changed = false;
}

void SimulationReader::describeModel()
{
    description.modelName = job.modelName;
    description.modelDirectory = fs::path(job.headerPath).parent_path().string();
    description.outputFileName = settings.outputFileName;
    description.columns = settings.numberOfColumnX;
    description.rows = settings.numberOfRowsY;
    description.layers = std::max(LAYERS_OF_2D_MODEL, settings.numberOfSlicesZ);
    description.substates = settings.getSubstateFields();
}

void SimulationReader::openDataFiles()
{
    // The same order of calls as in SceneWidget::setupSettingParameters/setupVtkScene/renderVtkScene
    visualizer->initMatrix(settings.numberOfColumnX, settings.numberOfRowsY);
    visualizer->prepareStage(settings.nNodeX, settings.nNodeY, settings.nNodeZ);
    visualizer->readStepsOffsetsForAllNodesFromFiles(settings.nNodeX, settings.nNodeY, settings.nNodeZ, settings.outputFileName);
    availableSteps = visualizer->availableSteps();
    lines.resize(static_cast<std::size_t>(std::max(1, settings.numberOfLines)));
}

const ModelDescription& SimulationReader::model() const
{
    return description;
}

const std::vector<Step>& SimulationReader::steps() const
{
    return availableSteps;
}

std::optional<std::string> SimulationReader::headerValue(const std::string& category, const std::string& name) const
{
    try
    {
        if (! header)
            header = std::make_unique<Config>(job.headerPath, /*printWarnings=*/false);
        ConfigCategory* section = header->getConfigCategory(category, /*ignoreCase=*/true);
        const ConfigParameter* parameter = section ? section->getConfigParameter(name) : nullptr;
        if (parameter)
            return parameter->getDefaultValue();
    }
    catch (const std::exception&)
    {
    }
    return std::nullopt;
}

void SimulationReader::log(LogLevel level, const std::string& message)
{
    if (level == LogLevel::Warning)
        warnings.push_back(message);

    if (job.onLog)
    {
        job.onLog(level, message);
        return;
    }
    (level == LogLevel::Info ? std::cout : std::cerr)
        << (level == LogLevel::Warning ? "Warning: " : level == LogLevel::Error ? "Error: " : "") << message << std::endl;
}

void SimulationReader::loadStep(Step step)
{
    settings.step = step;
    visualizer->readStageStateFromFilesForStep(&settings, lines.data());
    sliceLayer = -1; // a slice belongs to the previous state of the grid
}

void SimulationReader::select(int newLayer, int newRow, int newColumn)
{
    layer = newLayer;
    row = newRow;
    column = newColumn;
}

std::string SimulationReader::substate(const char* name) const
{
    if (description.layers > LAYERS_OF_2D_MODEL && layer != sliceLayer)
    {
        // A native 3D model is read through a slice of constant z, the only way to reach other layers
        visualizer->setNative3DSlice(GridSliceAxis::Z, layer);
        sliceLayer = layer;
    }
    return visualizer->getCellStringEncoding(row, column, name);
}

RunResult failedRun(std::string message, std::vector<std::string> warnings)
{
    RunResult result;
    result.ok = false;
    result.message = std::move(message);
    result.warnings = std::move(warnings);
    return result;
}

/** Sets all parameters of the action: the given values, or the defaults. Returns an error message, empty if all is fine. */
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

        if (const Status status = action.setParameter(parameter.name, value); ! status)
            return "Parameter '" + parameter.name + "': " + status.error();
    }
    return {};
}

/** Reads @p step and gives it to the action: beginStep(), every cell, endStep(). */
Status processStep(IAction& action, SimulationReader& reader, Step step)
{
    reader.loadStep(step);

    if (Status status = action.beginStep(step); ! status)
        return status;

    const ModelDescription& model = reader.model();
    for (int layer = 0; layer < model.layers; ++layer)
        for (int row = 0; row < model.rows; ++row)
            for (int column = 0; column < model.columns; ++column)
            {
                reader.select(layer, row, column);
                if (Status status = action.processCell(CellRef{step, layer, row, column, reader}); ! status)
                    return failure("step " + std::to_string(step) + ", cell (row " + std::to_string(row) + ", column " + std::to_string(column) +
                                   "): " + status.error());
            }

    return action.endStep(step);
}

/** finish(false) after a failure; its own failure is of no interest then. */
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
} // namespace viz::actions
