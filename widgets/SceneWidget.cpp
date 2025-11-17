/** @file SceneWidget.cpp
 * @brief Implementation of the SceneWidget class for 3D visualization. */

#include <iostream> // std::cout
#include <cmath> // std::isfinite
#include <filesystem>
#include <string>
#include <QApplication>
#include "utilities/directoryConstants.h"
#include <vtkCallbackCommand.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCamera.h>
#include <vtkNamedColors.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkAxesActor.h>
#include <vtkAxisActor2D.h>
#include <vtkProperty.h>
#include <vtkProperty2D.h>
#include <vtkCaptionActor2D.h>
#include <vtkTextProperty.h>
#include <vtkCoordinate.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkPropPicker.h>
#include "SceneWidget.h"
#include "config/Config.h"
#include "config/ConfigConstants.h"
#include "visualiser/Line.h"
#include "visualiser/Visualizer.hpp"
#include "visualiser/SettingParameter.h"
#include "widgets/ColorSettings.h"
#include "widgets/SubstatesDockWidget.h"
#include "widgets/CustomInteractorStyle.h"


namespace
{
/** @brief Checks if the given directory already contains data files matching the output name pattern
 *  @param configDir Directory to check
 *  @param outputFileNameFromCfg Base output filename to look for
 *  @return True if the directory contains data files, false otherwise */
bool isDataDirectory(const std::filesystem::path& configDir, const std::string& outputFileNameFromCfg)
{
    namespace fs = std::filesystem;

    for (const auto& entry : fs::directory_iterator(configDir))
    {
        if (! entry.is_regular_file())
            continue;

        std::string filename = entry.path().filename().string();

        // Check if file name starts with the output file name pattern
        if (filename.find(outputFileNameFromCfg) != 0)
            continue;

        // Check if it has a known data file extension
        std::string ext = entry.path().extension().string();
        if (ext == ".bin" || ext == ".txt")
            return true;
    }

    return false;
}

/** @brief Prepares the output file path for saving visualization data
 *  @param configFile Path to the configuration file
 *  @param outputFileNameFromCfg Output filename from configuration
 *  @return Full path to the output file */
std::string prepareOutputFileName(const std::string& configFile, const std::string& outputFileNameFromCfg)
{
    namespace fs = std::filesystem;

    // Step 1: determine output directory
    fs::path configPath(configFile);
    fs::path configDir = configPath.parent_path();

    // Step 2: check if we are already in a data directory
    const bool isInDataDirectory = isDataDirectory(configDir, outputFileNameFromCfg);

    // Step 3: build full output file path
    fs::path outputDir = isInDataDirectory ? configDir : (configDir / std::string(DirectoryConstants::OUTPUT_DIRECTORY));
    fs::create_directories(outputDir); // ensure that directory exists

    // Step 4: build and return the final output file path
    return (outputDir / outputFileNameFromCfg).string();
}

vtkColor3d toVtkColor(QColor color)
{
    return vtkColor3d{
        color.redF(),
        color.greenF(),
        color.blueF()
    };
}
} // namespace


SceneWidget::SceneWidget(QWidget* parent)
    : QVTKOpenGLNativeWidget(parent)
    , sceneWidgetVisualizerProxy{ SceneWidgetVisualizerFactory::defaultModel() }
    , settingParameter{ std::make_unique<SettingParameter>() }
    , currentModelName{ sceneWidgetVisualizerProxy->getModelName() }
    , gridActor{ vtkSmartPointer<vtkActor>::New() }
    , actorBuildLine{ vtkSmartPointer<vtkActor2D>::New() }
{
    enableToolTipWhenMouseAboveWidget();

    connect(&ColorSettings::instance(), &ColorSettings::colorsChanged, this, &SceneWidget::onColorsReloadRequested);
}

void SceneWidget::enableToolTipWhenMouseAboveWidget()
{
    setMouseTracking(/*enable=*/true);
    setAttribute(Qt::WA_AlwaysShowToolTips);
}

SceneWidget::~SceneWidget() = default;


void SceneWidget::triggerRenderUpdate()
{
    // Mark renderer as modified and request a render pass
    renderer->Modified();
    renderWindow()->Render();
}

void SceneWidget::applyCameraAngles()
{
    // Reset camera to default position
    auto camera = renderer->GetActiveCamera();
    if (! camera)
        return;

    camera->SetPosition(0, 0, 1);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);

    // Apply stored transformations in order: azimuth first, then elevation
    camera->Azimuth(cameraAzimuth);
    camera->Elevation(cameraElevation);

    // Reset camera bounds and render
    renderer->ResetCamera();
    triggerRenderUpdate();
}

void SceneWidget::loadAndUpdateVisualizationForCurrentStep()
{
    // Resize lines vector to match expected number of lines
    lines.resize(settingParameter->numberOfLines);

    // Read stage state from files for the current step
    sceneWidgetVisualizerProxy->readStageStateFromFilesForStep(settingParameter.get(), &lines[0]);

    // Refresh VTK visualization elements
    sceneWidgetVisualizerProxy->refreshWindowsVTK(settingParameter->numberOfRowsY, settingParameter->numberOfColumnX, gridActor);

    // Update load balancing lines if we have any
    if (settingParameter->numberOfLines > 0)
    {
        sceneWidgetVisualizerProxy->getVisualizer().refreshBuildLoadBalanceLine(lines,
                                                                                settingParameter->numberOfRowsY + 1,
                                                                                actorBuildLine);
    }

    // Update step number display
    sceneWidgetVisualizerProxy->getVisualizer().buildStepLine(settingParameter->step, singleLineTextStep);
}

void SceneWidget::prepareStageWithCurrentNodeConfiguration()
{
    // Initialize the visualizer stage with current node configuration
    sceneWidgetVisualizerProxy->prepareStage(settingParameter->nNodeX, settingParameter->nNodeY);
}

void SceneWidget::addVisualizer(const std::string& filename, StepIndex stepNumber)
{
    if (! std::filesystem::exists(filename))
    {
        throw std::invalid_argument(std::string("File '") + filename + "' does not exist!");
    }

    setupSettingParameters(filename, stepNumber);
    setupVtkScene();
    renderVtkScene();
}

void SceneWidget::setupSettingParameters(const std::string& configFilename, StepIndex stepNumber)
{
    readSettingsFromConfigFile(configFilename);

    // Each node has 2 lines (top and left edges)
    // Plus additional lines for bottom edge (nNodeX lines) and right edge (nNodeY lines)
    settingParameter->numberOfLines = 2 * (settingParameter->nNodeX * settingParameter->nNodeY) + settingParameter->nNodeX + settingParameter->nNodeY;
    settingParameter->step = stepNumber;
    settingParameter->changed = false;

    sceneWidgetVisualizerProxy->initMatrix(settingParameter->numberOfColumnX, settingParameter->numberOfRowsY);

    refreshBackgroundColorFromSettings();
}

void SceneWidget::refreshGridColorFromSettings()
{
    const auto color = ColorSettings::instance().gridColor();

    actorBuildLine->GetProperty()->SetColor(toVtkColor(color).GetData());
    actorBuildLine->GetProperty()->Modified();

    triggerRenderUpdate();
}

void SceneWidget::readSettingsFromConfigFile(const std::string& filename)
{
    Config config(filename);

    {
        ConfigCategory* generalContext = config.getConfigCategory(ConfigConstants::CATEGORY_GENERAL);
        const std::string outputFileNameFromCfg = generalContext->getConfigParameter(ConfigConstants::PARAM_OUTPUT_FILE_NAME)->getValue<std::string>();
        settingParameter->outputFileName = prepareOutputFileName(filename, outputFileNameFromCfg);
        settingParameter->numberOfColumnX = generalContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_OF_COLUMNS)->getValue<int>();
        settingParameter->numberOfRowsY = generalContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_OF_ROWS)->getValue<int>();
        settingParameter->nsteps = generalContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_STEPS)->getValue<int>();
        emit totalNumberOfStepsReadFromConfigFile(settingParameter->nsteps);
    }

    {
        ConfigCategory* execContext = config.getConfigCategory(ConfigConstants::CATEGORY_DISTRIBUTED);
        settingParameter->nNodeX = execContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_X)->getValue<int>();
        settingParameter->nNodeY = execContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_Y)->getValue<int>();
        /// Notice: there are much more params, which are not used: e.g. border_size_x, border_size_y
    }

    {
        ConfigCategory* visualizationContext = config.getConfigCategory(ConfigConstants::CATEGORY_VISUALIZATION);
        if (visualizationContext)
        {
            // Read visualization mode (text or binary)
            auto modeParam = visualizationContext->getConfigParameter(ConfigConstants::PARAM_MODE);
            settingParameter->readMode = modeParam ? modeParam->getValue<std::string>() : ConfigConstants::DEFAULT_MODE;

            // Read substates
            auto substatesParam = visualizationContext->getConfigParameter(ConfigConstants::PARAM_SUBSTATES);
            settingParameter->substates = substatesParam ? substatesParam->getValue<std::string>() : ConfigConstants::DEFAULT_SUBSTATES;

            // Read reduction operations
            auto reductionParam = visualizationContext->getConfigParameter(ConfigConstants::PARAM_REDUCTION);
            settingParameter->reduction = reductionParam ? reductionParam->getValue<std::string>() : ConfigConstants::DEFAULT_REDUCTION;
        }
        else
        {
            // Default values if ConfigConstants::CATEGORY_VISUALIZATION) section is not present
            settingParameter->readMode = ConfigConstants::DEFAULT_MODE;
            settingParameter->substates = ConfigConstants::DEFAULT_SUBSTATES;
            settingParameter->reduction = ConfigConstants::DEFAULT_REDUCTION;
        }
    }
}

void SceneWidget::setupVtkScene()
{
    sceneWidgetVisualizerProxy->prepareStage(settingParameter->nNodeX, settingParameter->nNodeY);

    renderWindow()->AddRenderer(renderer);
    interactor()->SetRenderWindow(renderWindow());

    renderWindow()->SetSize(settingParameter->numberOfColumnX, settingParameter->numberOfRowsY + 10);

    /// Use custom interactor style that zooms towards cursor position.
    /// This provides intuitive zoom behavior when using mouse wheel.
    vtkNew<CustomInteractorStyle> style;
    interactor()->SetInteractorStyle(style);

    renderWindow()->SetWindowName(QApplication::applicationName().toLocal8Bit().data());

    // Setup orientation axes widget
    setupAxesWidget();

    // Setup 2D ruler axes (bounds will be updated when data is loaded)
    setup2DRulerAxes();

    connectKeyboardCallback();
    connectMouseCallback();
    connectCameraCallback();
}

void SceneWidget::setupAxesWidget()
{
    // Configure axes actor
    axesActor->SetShaftTypeToCylinder();
    axesActor->SetXAxisLabelText("X");
    axesActor->SetYAxisLabelText("Y");
    axesActor->SetZAxisLabelText("Z");
    axesActor->SetTotalLength(1.0, 1.0, 1.0);
    axesActor->SetCylinderRadius(0.02);
    axesActor->SetConeRadius(0.05);
    axesActor->SetSphereRadius(0.03);

    // Make labels more readable
    axesActor->GetXAxisCaptionActor2D()->GetCaptionTextProperty()->SetFontSize(20);
    axesActor->GetYAxisCaptionActor2D()->GetCaptionTextProperty()->SetFontSize(20);
    axesActor->GetZAxisCaptionActor2D()->GetCaptionTextProperty()->SetFontSize(20);

    // Configure orientation marker widget
    axesWidget->SetOrientationMarker(axesActor);
    axesWidget->SetViewport(0.0, 0.0, 0.2, 0.2); // Bottom-left corner, 20% size
    axesWidget->SetInteractor(interactor());
    // Note: InteractiveOff() is not called here to avoid VTK warning
    // The widget is non-interactive by default when disabled
    axesWidget->SetEnabled(false); // Hidden by default (2D mode)
}

void SceneWidget::setup2DRulerAxes()
{
    // Configure X axis (horizontal, bottom)
    // Use World coordinates so the axis matches the actual data coordinates
    rulerAxisX->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    rulerAxisX->GetPosition2Coordinate()->SetCoordinateSystemToWorld();
    rulerAxisX->SetTitle("X");
    rulerAxisX->SetNumberOfLabels(5);
    rulerAxisX->SetLabelFormat("%.1f");
    rulerAxisX->GetTitleTextProperty()->SetColor(1.0, 1.0, 1.0);
    rulerAxisX->GetLabelTextProperty()->SetColor(1.0, 1.0, 1.0);
    rulerAxisX->GetProperty()->SetColor(0.8, 0.8, 0.8);

    // Configure Y axis (vertical, right side)
    // Use World coordinates so the axis matches the actual data coordinates
    rulerAxisY->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    rulerAxisY->GetPosition2Coordinate()->SetCoordinateSystemToWorld();
    rulerAxisY->SetTitle("Y");
    rulerAxisY->SetNumberOfLabels(5);
    rulerAxisY->SetLabelFormat("%.1f");
    rulerAxisY->GetTitleTextProperty()->SetColor(1.0, 1.0, 1.0);
    rulerAxisY->GetLabelTextProperty()->SetColor(1.0, 1.0, 1.0);
    rulerAxisY->GetProperty()->SetColor(0.8, 0.8, 0.8);

    // Adjust title position to move "Y" label to the right of the axis
    rulerAxisY->SetTitlePosition(1.2); // Move title further from axis (default is ~0.5)

    // Add to renderer but keep hidden initially
    renderer->AddViewProp(rulerAxisX);
    renderer->AddViewProp(rulerAxisY);
    rulerAxisX->SetVisibility(false);
    rulerAxisY->SetVisibility(false);
}

void SceneWidget::update2DRulerAxesBounds()
{
    if (! renderer || ! renderWindow())
        return;

    // Get grid actor bounds (actual data coordinates)
    double bounds[6];
    gridActor->GetBounds(bounds);

    // Check if bounds are valid
    if (bounds[0] >= bounds[1] || bounds[2] >= bounds[3] ||
        !std::isfinite(bounds[0]) || !std::isfinite(bounds[1]) ||
        !std::isfinite(bounds[2]) || !std::isfinite(bounds[3]))
    {
        return; // Invalid bounds
    }

    // Set range for axes (this determines the numeric labels)
    rulerAxisX->SetRange(bounds[0], bounds[1]);
    rulerAxisY->SetRange(bounds[2], bounds[3]);

    // Position X axis at the bottom of the data (horizontal line)
    rulerAxisX->GetPositionCoordinate()->SetValue(bounds[0], bounds[2], 0.0);
    rulerAxisX->GetPosition2Coordinate()->SetValue(bounds[1], bounds[2], 0.0);

    // Position Y axis at the RIGHT of the data (vertical line) - labels won't overlap scene
    rulerAxisY->GetPositionCoordinate()->SetValue(bounds[1], bounds[2], 0.0);
    rulerAxisY->GetPosition2Coordinate()->SetValue(bounds[1], bounds[3], 0.0);

    std::cout << "Ruler axes updated: X=[" << bounds[0] << ", " << bounds[1]
              << "], Y=[" << bounds[2] << ", " << bounds[3] << "]" << std::endl;
}

void SceneWidget::connectKeyboardCallback()
{
    vtkNew<vtkCallbackCommand> keypressCallback;
    keypressCallback->SetCallback(SceneWidget::keypressCallbackFunction);
    keypressCallback->SetClientData(this);
    interactor()->AddObserver(vtkCommand::KeyPressEvent, keypressCallback);
}

void SceneWidget::connectCameraCallback()
{
    if (! interactor())
        return;

    // Use EndInteractionEvent instead of camera ModifiedEvent
    // This is only called when user finishes rotating (releases mouse button)
    vtkNew<vtkCallbackCommand> cameraCallback;
    cameraCallback->SetCallback(SceneWidget::cameraCallbackFunction);
    cameraCallback->SetClientData(this);
    interactor()->AddObserver(vtkCommand::EndInteractionEvent, cameraCallback);
}

void SceneWidget::keypressCallbackFunction(vtkObject* caller, long unsigned int eventId, void* clientData, void* callData)
{
    vtkRenderWindowInteractor* interactor = static_cast<vtkRenderWindowInteractor*>(caller);

    const std::string keyPressed = interactor->GetKeySym();
    SceneWidget* sw = static_cast<SceneWidget*>(clientData);
    SettingParameter* sp = sw->settingParameter.get();

    if (keyPressed == "Up")
    {
        if (sp->step < sp->nsteps * 2)
            sp->step += 1;
        sp->changed = true;

        if (sw)
            sw->changedStepNumberWithKeyboardKeys(sp->step);
    }
    else if (keyPressed == "Down")
    {
        if (sp->step > 1)
            sp->step -= 1;
        sp->changed = true;

        if (sw)
            sw->changedStepNumberWithKeyboardKeys(sp->step);
    }

    if (sp->changed)
    {
        try
        {
            // Load and update visualization using helper method
            sw->loadAndUpdateVisualizationForCurrentStep();

            // Trigger render update
            sw->triggerRenderUpdate();
        }
        catch (const std::runtime_error& re)
        {
            std::cerr << "Runtime error di getElementMatrix: " << re.what() << std::endl;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error occurred: " << ex.what() << std::endl;
        }

        sp->changed = false;
    }
}

void SceneWidget::connectMouseCallback()
{
    vtkNew<vtkCallbackCommand> mouseMoveCallback;
    mouseMoveCallback->SetCallback(&SceneWidget::mouseCallbackFunction);
    mouseMoveCallback->SetClientData(this);
    interactor()->AddObserver(vtkCommand::MouseMoveEvent, mouseMoveCallback);
}

void SceneWidget::cameraCallbackFunction(vtkObject* caller, long unsigned int eventId, void* clientData, void* callData)
{
    Q_UNUSED(caller);
    Q_UNUSED(eventId);
    Q_UNUSED(callData);

    auto* self = static_cast<SceneWidget*>(clientData);
    if (! self)
        return;

    // Only emit signal in 3D mode (user finished rotating the camera)
    if (self->currentViewMode == ViewMode::Mode3D && self->renderer)
    {
        vtkCamera* camera = self->renderer->GetActiveCamera();
        if (camera)
        {
            // Get actual camera orientation from VTK
            double* position = camera->GetPosition();
            double* focalPoint = camera->GetFocalPoint();

            // Calculate azimuth and elevation from camera position
            double dx = position[0] - focalPoint[0];
            double dy = position[1] - focalPoint[1];
            double dz = position[2] - focalPoint[2];

            double azimuth = std::atan2(dy, dx) * 180.0 / vtkMath::Pi();
            double elevation = std::atan2(dz, std::sqrt(dx * dx + dy * dy)) * 180.0 / vtkMath::Pi();

            // Update internal state
            self->cameraAzimuth = azimuth;
            self->cameraElevation = elevation;

            // Emit signal with actual values
            emit self->cameraOrientationChanged(azimuth, elevation);
        }
    }
}

void SceneWidget::mouseCallbackFunction(vtkObject* caller, long unsigned int eventId, void* clientData, void* callData)
{
    auto interactor = static_cast<vtkRenderWindowInteractor*>(caller);
    auto* self = static_cast<SceneWidget*>(clientData);

    // 1) Get the event position from VTK (origin: bottom-left)
    int vtkX = interactor->GetEventPosition()[0];
    int vtkY = interactor->GetEventPosition()[1];

    // 2) Convert to Qt coordinates (origin: top-left) for tooltip/mapToGlobal
    int size[2];
    if (self->renderWindow())
    {
        size[0] = self->renderWindow()->GetSize()[0];
        size[1] = self->renderWindow()->GetSize()[1];
    }
    else
    {
        size[0] = 0;
        size[1] = 0;
    }
    int qtY = size[1] - vtkY;

    const auto lastMousePos = QPoint(vtkX, qtY);

    // 3) Use a picker to obtain an accurate world position (if something was "hit")
    vtkNew<vtkPropPicker> picker;
    bool picked = false;
    if (self->renderer)
    {
        // Pick returns 1 if something was hit (depending on the picker). Pass the renderer.
        if (picker->Pick(vtkX, vtkY, 0.0, self->renderer))
        {
            double pickPos[3];
            picker->GetPickPosition(pickPos);
            self->m_lastWorldPos = { pickPos[0], pickPos[1], pickPos[2] };
            picked = true;
        }
    }

    // 4) If the picker didn't hit anything, try DisplayToWorld as a fallback
    if (! picked && self->renderer)
    {
        double displayPt[3] = { static_cast<double>(vtkX), static_cast<double>(vtkY), 0.0 };
        self->renderer->SetDisplayPoint(displayPt);
        self->renderer->DisplayToWorld();
        double worldPt[4];
        self->renderer->GetWorldPoint(worldPt);
        if (worldPt[3] != 0.0)
        {
            self->m_lastWorldPos = { worldPt[0] / worldPt[3], worldPt[1] / worldPt[3], worldPt[2] / worldPt[3] };
        }
        else
        {
            // If w == 0, the result is invalid — keep previous or zero out
            self->m_lastWorldPos = { worldPt[0], worldPt[1], worldPt[2] };
        }
    }

    // 5) Update the tooltip (now using correct lastMousePos and m_lastWorldPos)
    self->updateToolTip(lastMousePos);
}

void SceneWidget::renderVtkScene()
{
    sceneWidgetVisualizerProxy->readStepsOffsetsForAllNodesFromFiles(settingParameter->nNodeX,
                                                                     settingParameter->nNodeY,
                                                                     settingParameter->outputFileName);

    emit availableStepsReadFromConfigFile(sceneWidgetVisualizerProxy->availableSteps());

    lines.resize(settingParameter->numberOfLines);
    sceneWidgetVisualizerProxy->readStageStateFromFilesForStep(settingParameter.get(), &lines[0]);

    sceneWidgetVisualizerProxy->drawWithVTK(settingParameter->numberOfRowsY, settingParameter->numberOfColumnX, renderer, gridActor);

    sceneWidgetVisualizerProxy->getVisualizer().buildLoadBalanceLine(lines,
                                                                     settingParameter->numberOfRowsY + 1,
                                                                     renderer,
                                                                     actorBuildLine);

    // Apply the remembered grid lines visibility state
    if (actorBuildLine)
    {
        actorBuildLine->SetVisibility(gridLinesVisible);
    }

    sceneWidgetVisualizerProxy->getVisualizer().buildStepText(settingParameter->step,
                                                              settingParameter->font_size,
                                                              singleLineTextStep,
                                                              renderer);

    // Reset camera to fit the new scene properly
    applyCameraAngles();

    // Update 2D ruler axes bounds now that data is loaded
    if (currentViewMode == ViewMode::Mode2D)
    {
        update2DRulerAxesBounds();
        rulerAxisX->SetVisibility(true);
        rulerAxisY->SetVisibility(true);
    }

    // Render
    renderWindow()->Render();
    interactor()->Initialize();
    interactor()->Enable();
}

std::array<double, 3> SceneWidget::screenToWorldCoordinates(const QPoint& pos) const
{
    std::array<double, 3> worldPos = { 0.0, 0.0, 0.0 };

    if (! renderer || ! renderWindow())
    {
        return worldPos;
    }

    // Convert screen coordinates to VTK display coordinates
    int* size = renderWindow()->GetSize();
    double displayPos[3] = {
        static_cast<double>(pos.x()),
        static_cast<double>(size[1] - pos.y()), // Flip Y coordinate
        0.0
    };

    // Convert display coordinates to world coordinates
    renderer->SetDisplayPoint(displayPos);
    renderer->DisplayToWorld();
    renderer->GetWorldPoint(worldPos.data());

    return worldPos;
}

QString SceneWidget::getNodeAtWorldPosition(const std::array<double, 3>& worldPos) const
{
    if (! settingParameter || ! sceneWidgetVisualizerProxy || ! renderer)
    {
        return {};
    }

    // Use unified bounds checking
    if (! isWorldPositionInGrid(worldPos.data()))
    {
        return {}; // Outside scene bounds
    }

    // Get the bounds of the entire scene
    double* bounds = renderer->ComputeVisiblePropBounds();
    if (! bounds)
    {
        return {};
    }

    // Calculate the width and height of each node's area in world coordinates
    const double sceneWidth = bounds[1] - bounds[0];
    const double sceneHeight = bounds[3] - bounds[2];

    const double nodeWidth = sceneWidth / settingParameter->nNodeX;
    const double nodeHeight = sceneHeight / settingParameter->nNodeY;

    // Calculate which node the position is in (0-based indices)
    const int nodeX = static_cast<int>((worldPos[0] - bounds[0]) / nodeWidth);
    const int nodeY = static_cast<int>((worldPos[1] - bounds[2]) / nodeHeight);

    // Check if the calculated node is within bounds
    if (nodeX >= 0 && nodeX < settingParameter->nNodeX &&
        nodeY >= 0 && nodeY < settingParameter->nNodeY)
    {
        return QString("Node [%1, %2]").arg(nodeX).arg(nodeY);
    }

    return {}; // Outside node grid
}

const Line* SceneWidget::findNearestLine(const std::array<double, 3>& worldPos, size_t& lineIndex, double& distanceSquared) const
{
    if (! settingParameter || ! sceneWidgetVisualizerProxy)
    {
        return nullptr;
    }

    // const auto& lines = sceneWidgetVisualizerProxy->getVisualizer().getLines();
    if (lines.empty())
    {
        return nullptr;
    }

    constexpr double threshold = 2; // Threshold for line selection (in world coordinates)
    constexpr double thresholdSq = threshold * threshold;

    const Line* nearestLine = nullptr;
    double minDistanceSq = std::numeric_limits<double>::max();
    size_t foundIndex = 0;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        const auto& line = lines[i];

        // Calculate squared distance from point to line segment
        const double lineLengthSq = (line.x2 - line.x1) * (line.x2 - line.x1) +
                                   (line.y2 - line.y1) * (line.y2 - line.y1);

        if (lineLengthSq < 1e-10) // Skip zero-length lines
            continue;

        const double t = std::max(0.0, std::min(1.0,
            ((worldPos[0] - line.x1) * (line.x2 - line.x1) +
             (worldPos[1] - line.y1) * (line.y2 - line.y1)) / lineLengthSq));

        const double projX = line.x1 + t * (line.x2 - line.x1);
        const double projY = line.y1 + t * (line.y2 - line.y1);

        const double dx = worldPos[0] - projX;
        const double dy = worldPos[1] - projY;
        const double distSq = dx * dx + dy * dy;

        if (distSq < minDistanceSq && distSq <= thresholdSq)
        {
            minDistanceSq = distSq;
            nearestLine = &line;
            foundIndex = i;
        }
    }

    if (nearestLine)
    {
        lineIndex = foundIndex;
        distanceSquared = minDistanceSq;
    }

    return nearestLine;
}

void SceneWidget::updateToolTip(const QPoint& lastMousePos)
{
    if (! renderer || ! renderWindow())
        return;

    // m_lastMousePos is already in Qt coordinates (origin: top-left)
    // m_lastWorldPos is set by the VTK callback (picker or DisplayToWorld fallback)

    // Check if we're over a line
    size_t lineIndex = 0;
    double distanceSq = 0.0;
    const Line* nearestLine = findNearestLine(m_lastWorldPos, lineIndex, distanceSq);

    // Prepare tooltip text
    QString tooltipText;

    if (nearestLine)
    {
        tooltipText += QString("Line %1/%2:").arg(lineIndex).arg(lines.size());
        tooltipText += QString("\n  From: (x1=%1, y1=%2)")
                           .arg(nearestLine->x1, 0, 'f', 2)
                           .arg(nearestLine->y1, 0, 'f', 2);
        tooltipText += QString("\n  To:   (x2=%1, y2=%2)")
                           .arg(nearestLine->x2, 0, 'f', 2)
                           .arg(nearestLine->y2, 0, 'f', 2);
        tooltipText += cellValueAtThisPositionAsText();
    }
    else if (QString nodeInfo = getNodeAtWorldPosition(m_lastWorldPos); ! nodeInfo.isEmpty())
    {
        tooltipText = QString("World Position: (x: %1, y: %2, z: %3)")
                          .arg(m_lastWorldPos[0], 0, 'f', 2)
                          .arg(m_lastWorldPos[1], 0, 'f', 2)
                          .arg(m_lastWorldPos[2], 0, 'f', 2);

        tooltipText += QString("\n%1").arg(nodeInfo);

        tooltipText += cellValueAtThisPositionAsText();
    }
    else
        tooltipText = "(Outside the grid)";

    // Use m_lastMousePos (Qt coordinates) to position the tooltip
    QPoint globalPos = mapToGlobal(lastMousePos);
    QToolTip::showText(globalPos, tooltipText, this, QRect(lastMousePos, QSize(1, 1)), 0);
}
QString SceneWidget::cellValueAtThisPositionAsText() const
{
    if (!sceneWidgetVisualizerProxy || !settingParameter)
        return {};

    int row = 0, col = 0;
    if (! convertWorldToGridCoordinates(m_lastWorldPos.data(), row, col))
        return {};

    QString tooltipText;
    // Get default cell value
    std::string cellValue = sceneWidgetVisualizerProxy->getCellStringEncoding(row, col);
    if (! cellValue.empty())
    {
        tooltipText += QString("\nCell Value: %1").arg(QString::fromStdString(cellValue));
    }

    // Get individual substate values if available
    auto substateFields = settingParameter->getSubstateFields();
    if (! substateFields.empty())
    {
        tooltipText += "\nSubstates:";
        for (const auto& field : substateFields)
        {
            std::string fieldValue = sceneWidgetVisualizerProxy->getCellStringEncoding(row, col, field.c_str());
            if (!fieldValue.empty())
            {
                tooltipText += QString("\n\t%1: %2").arg(QString::fromStdString(field)).arg(QString::fromStdString(fieldValue));
            }
        }
    }
    return tooltipText;
}

void SceneWidget::selectedStepParameter(StepIndex stepNumber)
{
    settingParameter->step = stepNumber;
    settingParameter->changed = true;
    upgradeModelInCentralPanel();
}

void SceneWidget::upgradeModelInCentralPanel()
{
    if (! settingParameter->changed)
        return;

    try
    {
        loadAndUpdateVisualizationForCurrentStep();

        triggerRenderUpdate();
        QApplication::processEvents();
    }
    catch (const std::runtime_error& re)
    {
        std::cerr << "Runtime error getElementMatrix: " << re.what() << std::endl;
        throw;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error occurred: " << ex.what() << std::endl;
        throw;
    }

    settingParameter->changed = false;
}

void SceneWidget::switchModel(const std::string& modelName)
{
    if (modelName == currentModelName)
    {
        return; // Already using this model
    }

    // Clean up old visualizer before creating new one
    if (sceneWidgetVisualizerProxy)
    {
        try
        {
            sceneWidgetVisualizerProxy->clearStage();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Warning: Error clearing old visualizer stage: " << e.what() << std::endl;
        }
    }

    // Create new visualizer with the selected model
    sceneWidgetVisualizerProxy = SceneWidgetVisualizerFactory::create(modelName);
    currentModelName = modelName;

    // Reinitialize the matrix with current dimensions
    sceneWidgetVisualizerProxy->initMatrix(settingParameter->numberOfColumnX, settingParameter->numberOfRowsY);

    std::cout << "Switched to model: " << sceneWidgetVisualizerProxy->getModelName() << std::endl;
}

void SceneWidget::reloadData()
{
    try
    {
        // Clear existing stage data to avoid duplicates
        sceneWidgetVisualizerProxy->clearStage();

        // Reinitialize stage with current node configuration using helper
        prepareStageWithCurrentNodeConfiguration();

        // Load step offsets from files
        sceneWidgetVisualizerProxy->readStepsOffsetsForAllNodesFromFiles(
            settingParameter->nNodeX,
            settingParameter->nNodeY,
            settingParameter->outputFileName
        );

        // Force a full refresh
        settingParameter->changed = true;
        upgradeModelInCentralPanel();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to reload data: " << e.what() << std::endl;
        throw;
    }
}

void SceneWidget::clearScene()
{
    // Clear the renderer
    renderer->RemoveAllViewProps();

    // Clear stage data
    sceneWidgetVisualizerProxy->clearStage();

    // Reset VTK actors
    gridActor = vtkSmartPointer<vtkActor>::New();
    actorBuildLine = vtkSmartPointer<vtkActor2D>::New();
}

void SceneWidget::loadNewConfiguration(const std::string& configFileName, int stepNumber)
{
    try
    {
        // Clear existing scene
        clearScene();

        // Setup new parameters from config file
        setupSettingParameters(configFileName, stepNumber);

        // Setup VTK scene using helper method
        prepareStageWithCurrentNodeConfiguration();

        // Render the scene with new data
        renderVtkScene();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load configuration: " << e.what() << std::endl;
        throw;
    }
}

void SceneWidget::onColorsReloadRequested()
{
    refreshBackgroundColorFromSettings();
    refreshStepNumberTextColorFromSettings();
    refreshGridColorFromSettings();
}
void SceneWidget::refreshBackgroundColorFromSettings()
{
    const auto color = ColorSettings::instance().backgroundColor();
    renderer->SetBackground(toVtkColor(color).GetData());
    triggerRenderUpdate();
}

void SceneWidget::refreshStepNumberTextColorFromSettings()
{
    const auto color = ColorSettings::instance().textColor();

    auto realTextProp = singleLineTextStep->GetTextProperty();
    realTextProp->SetColor(toVtkColor(color).GetData());
    realTextProp->Modified();

    triggerRenderUpdate();
}

void SceneWidget::setViewMode2D()
{
    if (! interactor())
        return;

    currentViewMode = ViewMode::Mode2D;

    // Use custom interactor style that zooms towards cursor position
    vtkNew<CustomInteractorStyle> style;
    interactor()->SetInteractorStyle(style);

    // Reset camera angles
    cameraAzimuth = {};
    cameraElevation = {};

    // Set camera to top-down view
    auto camera = renderer->GetActiveCamera();
    if (camera)
    {
        // Reset camera position and orientation
        camera->SetPosition(0, 0, 1);
        camera->SetFocalPoint(0, 0, 0);
        camera->SetViewUp(0, 1, 0);

        renderer->ResetCamera();
        renderWindow()->Render();
    }

    std::cout << "Switched to 2D view mode" << std::endl;

    // Hide orientation axes in 2D mode
    setAxesWidgetVisible(false);

    // Update and show 2D ruler axes only if we have valid data
    double bounds[6];
    renderer->ComputeVisiblePropBounds(bounds);
    if (bounds[0] < bounds[1] && bounds[2] < bounds[3] && std::isfinite(bounds[0]) && std::isfinite(bounds[1]))
    {
        update2DRulerAxesBounds();
        rulerAxisX->SetVisibility(true);
        rulerAxisY->SetVisibility(true);
    }
    else
    {
        // No data yet, keep ruler axes hidden
        rulerAxisX->SetVisibility(false);
        rulerAxisY->SetVisibility(false);
    }
}

void SceneWidget::setViewMode3D()
{
    if (! interactor())
        return;

    currentViewMode = ViewMode::Mode3D;

    // Use vtkInteractorStyleTrackballCamera which allows full 3D rotation
    vtkNew<vtkInteractorStyleTrackballCamera> style;
    interactor()->SetInteractorStyle(style);

    // Show orientation axes in 3D mode
    setAxesWidgetVisible(true);

    // Hide 2D ruler axes in 3D mode
    rulerAxisX->SetVisibility(false);
    rulerAxisY->SetVisibility(false);

    std::cout << "Switched to 3D view mode" << std::endl;
}

void SceneWidget::setAxesWidgetVisible(bool visible)
{
    if (axesWidget)
    {
        axesWidget->SetEnabled(visible);
        triggerRenderUpdate();
    }
}

void SceneWidget::setGridLinesVisible(bool visible)
{
    gridLinesVisible = visible;
    if (actorBuildLine)
    {
        actorBuildLine->SetVisibility(visible);
        triggerRenderUpdate();
    }
}

void SceneWidget::setCameraAzimuth(double angle)
{
    // Store the new azimuth value
    cameraAzimuth = angle;

    // Apply camera angles using helper method
    applyCameraAngles();
}

void SceneWidget::setCameraElevation(double angle)
{
    // Store the new elevation value
    cameraElevation = angle;

    // Apply camera angles using helper method
    applyCameraAngles();
}

void SceneWidget::setSubstatesDockWidget(SubstatesDockWidget* dockWidget)
{
    m_substatesDockWidget = dockWidget;
}

void SceneWidget::mousePressEvent(QMouseEvent* event)
{
    // Call parent implementation first
    QVTKOpenGLNativeWidget::mousePressEvent(event);

    // Update substate dock widget if available (for left clicks without Shift)
    if (m_substatesDockWidget && sceneWidgetVisualizerProxy && event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier))
    {
        // Check if click was inside the grid
        if (isWorldPositionInGrid(m_lastWorldPos.data()))
        {
            int row = 0, col = 0;
            if (convertWorldToGridCoordinates(m_lastWorldPos.data(), row, col))
            {
                // Update substate dock widget with cell values
                m_substatesDockWidget->updateCellValues(settingParameter.get(), row, col, sceneWidgetVisualizerProxy.get());
                // Show the dock widget when user clicks on a cell
                m_substatesDockWidget->show();
            }
        }
        else
        {
            // Click was outside grid (on background) - hide the dock widget
            m_substatesDockWidget->hide();
        }
    }
}

bool SceneWidget::convertWorldToGridCoordinates(const double worldPos[3], int& outRow, int& outCol) const
{
    if (!renderer || !settingParameter)
        return false;

    // Get the bounds of the entire scene
    double* bounds = renderer->ComputeVisiblePropBounds();
    if (!bounds)
        return false;

    // Calculate grid dimensions
    const double sceneWidth = bounds[1] - bounds[0];
    const double sceneHeight = bounds[3] - bounds[2];

    if (sceneWidth <= 0 || sceneHeight <= 0)
        return false;

    const double cellWidth = sceneWidth / settingParameter->numberOfColumnX;
    const double cellHeight = sceneHeight / settingParameter->numberOfRowsY;

    // Convert world position to grid indices
    // Matrix p[row][col] is indexed from top-left (row=0 at top, col=0 at left)
    // VTK Y increases upward, but we need to map to matrix row which increases downward
    int col = static_cast<int>((worldPos[0] - bounds[0]) / cellWidth);
    int row = static_cast<int>((worldPos[1] - bounds[2]) / cellHeight);

    // Clamp to valid range
    col = std::max(0, std::min(col, settingParameter->numberOfColumnX - 1));
    row = std::max(0, std::min(row, settingParameter->numberOfRowsY - 1));

    outRow = row;
    outCol = col;
    return true;
}

bool SceneWidget::isWorldPositionInGrid(const double worldPos[3]) const
{
    if (!renderer || !settingParameter)
        return false;

    // Get the bounds of the entire scene
    const double* bounds = renderer->ComputeVisiblePropBounds();
    if (! bounds)
        return false;

    // Check if position is within grid bounds
    if (worldPos[0] < bounds[0] || worldPos[0] > bounds[1] ||
        worldPos[1] < bounds[2] || worldPos[1] > bounds[3])
    {
        return false;  // Outside grid bounds
    }

    return true;  // Inside grid bounds
}

