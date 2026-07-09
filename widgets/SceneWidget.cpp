/** @file SceneWidget.cpp
 * @brief Implementation of the SceneWidget class for 3D visualization. */

#include <iostream> // std::cout
#include <algorithm>
#include <cmath> // std::isfinite
#include <filesystem>
#include <string>
#include <QApplication>
#include "core/directoryConstants.h"
#include "widgets/WaitCursorGuard.h"
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
#include <vtkMath.h>
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
#include "data/PerformanceMetrics.h"


namespace
{
class NullSceneWidgetVisualizer final : public ISceneWidgetVisualizer
{
public:
    void initMatrix(int, int) override {}
    void prepareStage(int, int, int) override {}
    void clearStage() override {}
    void readStepsOffsetsForAllNodesFromFiles(int, int, int, const std::string&) override {}
    void readStageStateFromFilesForStep(SettingParameter*, Line*) override {}
    void drawWithVTK(int, int, vtkSmartPointer<vtkRenderer>, vtkSmartPointer<vtkActor>, const std::vector<const SubstateInfo*>&, bool) override {}
    void refreshWindowsVTK(int, int, vtkSmartPointer<vtkActor>, const std::vector<const SubstateInfo*>&) override {}
    void setNative3DSlice(GridSliceAxis, int) override {}
    void clearNative3DSlice() override {}
    bool isNative3DSliceEnabled() const override { return false; }
    GridSliceAxis native3DSliceAxis() const override { return GridSliceAxis::Z; }
    int native3DSliceIndex() const override { return 0; }
    void drawWithVTK3DSubstate(int, int, vtkSmartPointer<vtkRenderer>, vtkSmartPointer<vtkActor>, const std::string&, double, double, const std::vector<const SubstateInfo*>&) override {}
    void drawWithVTK3DSubstates(int, int, vtkSmartPointer<vtkRenderer>, vtkSmartPointer<vtkActor>, const std::vector<const SubstateInfo*>&, const std::vector<const SubstateInfo*>&) override {}
    void refreshWindowsVTK3DSubstate(int, int, vtkSmartPointer<vtkActor>, const std::string&, double, double, const std::vector<const SubstateInfo*>&) override {}
    void refreshWindowsVTK3DSubstates(int, int, vtkSmartPointer<vtkActor>, const std::vector<const SubstateInfo*>&, const std::vector<const SubstateInfo*>&) override {}
    void drawWithVTK3DSubstateSlice(int, int, vtkSmartPointer<vtkRenderer>, vtkSmartPointer<vtkActor>, const std::string&, double, double, const std::vector<const SubstateInfo*>&, GridSliceAxis, int) override {}
    void drawWithVTK3DSubstatesSlice(int, int, vtkSmartPointer<vtkRenderer>, vtkSmartPointer<vtkActor>, const std::vector<const SubstateInfo*>&, const std::vector<const SubstateInfo*>&, GridSliceAxis, int) override {}
    void drawFlatSceneBackground(int, int, vtkSmartPointer<vtkRenderer>, vtkSmartPointer<vtkActor>) override {}
    void refreshFlatSceneBackground(int, int, vtkSmartPointer<vtkActor>) override {}
    void drawGridLinesOn3DSurface(int, int, const std::vector<Line>&, vtkSmartPointer<vtkRenderer>, vtkSmartPointer<vtkActor>, const std::string&, double, double) override {}
    void drawGridLinesOn3DSubstateStack(int, int, const std::vector<Line>&, vtkSmartPointer<vtkRenderer>, vtkSmartPointer<vtkActor>, const std::vector<const SubstateInfo*>&) override {}
    void refreshGridLinesOn3DSurface(int, int, const std::vector<Line>&, vtkSmartPointer<vtkActor>, const std::string&, double, double) override {}
    void refreshGridLinesOn3DSubstateStack(int, int, const std::vector<Line>&, vtkSmartPointer<vtkActor>, const std::vector<const SubstateInfo*>&) override {}

    Visualizer& getVisualizer() override
    {
        static Visualizer visualizer;
        return visualizer;
    }

    std::string getModelName() const override
    {
        return {};
    }

    std::vector<StepIndex> availableSteps() const override
    {
        return {};
    }

    std::string getCellStringEncoding(int, int, const char*) const override
    {
        return {};
    }
};

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

struct CameraEulerAngles
{
    double roll = 0.0;  // X
    double pitch = 0.0; // Y
    double yaw = 0.0;   // Z
};

struct CameraBasis
{
    std::array<double, 3> right;
    std::array<double, 3> up;
    std::array<double, 3> backward;
};

constexpr double radiansToDegrees(double radians)
{
    return radians * 180.0 / vtkMath::Pi();
}

constexpr double degreesToRadians(double degrees)
{
    return degrees * vtkMath::Pi() / 180.0;
}

/** Build R = Rz(yaw) * Ry(pitch) * Rx(roll).
 *
 * Its columns are the camera's right, up and backward vectors, matching the
 * baseline VTK camera (right=+X, up=+Y, position direction=+Z). */
CameraBasis cameraBasisFromEuler(const CameraEulerAngles& angles)
{
    const double x = degreesToRadians(angles.roll);
    const double y = degreesToRadians(angles.pitch);
    const double z = degreesToRadians(angles.yaw);

    const double cx = std::cos(x);
    const double sx = std::sin(x);
    const double cy = std::cos(y);
    const double sy = std::sin(y);
    const double cz = std::cos(z);
    const double sz = std::sin(z);

    return CameraBasis{
        .right = {
            cz * cy,
            sz * cy,
            -sy
        },
        .up = {
            cz * sy * sx - sz * cx,
            sz * sy * sx + cz * cx,
            cy * sx
        },
        .backward = {
            cz * sy * cx + sz * sx,
            sz * sy * cx - cz * sx,
            cy * cx
        }
    };
}

CameraEulerAngles cameraEulerFromVtk(vtkCamera& camera)
{
    double position[3];
    double focalPoint[3];
    double viewUp[3];
    camera.GetPosition(position);
    camera.GetFocalPoint(focalPoint);
    camera.GetViewUp(viewUp);

    double backward[3] = {
        position[0] - focalPoint[0],
        position[1] - focalPoint[1],
        position[2] - focalPoint[2]
    };
    if (vtkMath::Normalize(backward) == 0.0)
        return {};

    // Gram-Schmidt removes numerical drift accumulated by the trackball.
    const double upProjection = vtkMath::Dot(viewUp, backward);
    double up[3] = {
        viewUp[0] - upProjection * backward[0],
        viewUp[1] - upProjection * backward[1],
        viewUp[2] - upProjection * backward[2]
    };
    if (vtkMath::Normalize(up) == 0.0)
        return {};

    double right[3];
    vtkMath::Cross(up, backward, right);
    vtkMath::Normalize(right);
    vtkMath::Cross(backward, right, up);
    vtkMath::Normalize(up);

    // Matrix columns are [right, up, backward]. Decompose the same
    // Rz * Ry * Rx convention used by cameraBasisFromEuler().
    const double r00 = right[0];
    const double r10 = right[1];
    const double r20 = right[2];
    const double r01 = up[0];
    const double r11 = up[1];
    const double r21 = up[2];
    const double r22 = backward[2];

    CameraEulerAngles result;
    result.pitch = radiansToDegrees(std::asin(std::clamp(-r20, -1.0, 1.0)));

    const double cosPitch = std::cos(degreesToRadians(result.pitch));
    if (std::abs(cosPitch) > 1e-7)
    {
        result.roll = radiansToDegrees(std::atan2(r21, r22));
        result.yaw = radiansToDegrees(std::atan2(r10, r00));
    }
    else
    {
        // At gimbal lock Roll and Yaw are not independently observable.
        // Keep the canonical solution Roll=0 and encode rotation in Yaw.
        result.roll = 0.0;
        result.yaw = radiansToDegrees(std::atan2(-r01, r11));
    }

    return result;
}
} // namespace


SceneWidget::SceneWidget(QWidget* parent)
    : QVTKOpenGLNativeWidget(parent)
    , sceneWidgetVisualizerProxy{ std::make_unique<NullSceneWidgetVisualizer>() }
    , settingParameter{ std::make_unique<SettingParameter>() }
    , currentModelName{ sceneWidgetVisualizerProxy->getModelName() }
    , gridActor{ vtkSmartPointer<vtkActor>::New() }
    , backgroundActor{ vtkSmartPointer<vtkActor>::New() }
    , actorBuildLine{ vtkSmartPointer<vtkActor2D>::New() }
    , gridLinesOnSurfaceActor{ vtkSmartPointer<vtkActor>::New() }
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
    bool oldWarningState = vtkObject::GetGlobalWarningDisplay();
    vtkObject::GlobalWarningDisplayOff();

    auto camera = renderer->GetActiveCamera();
    if (! camera)
    {
        vtkObject::SetGlobalWarningDisplay(oldWarningState);
        return;
    }

    const CameraBasis basis = cameraBasisFromEuler({
        .roll = cameraRoll,
        .pitch = std::clamp(cameraPitch, -90.0, 90.0),
        .yaw = cameraYaw
    });

    camera->SetPosition(cameraPivot[0] + basis.backward[0],
                        cameraPivot[1] + basis.backward[1],
                        cameraPivot[2] + basis.backward[2]);
    camera->SetFocalPoint(cameraPivot.data());
    camera->SetViewUp(basis.up.data());
    camera->OrthogonalizeViewUp();
    renderer->ResetCamera();

    triggerRenderUpdate();
    vtkObject::SetGlobalWarningDisplay(oldWarningState);
}

void SceneWidget::applyCameraAnglesPreservingZoom()
{
    auto camera = renderer->GetActiveCamera();
    if (! camera)
        return;

    double distance = camera->GetDistance();
    if (distance < 1e-3)
        distance = 1.0;

    bool oldWarningState = vtkObject::GetGlobalWarningDisplay();
    vtkObject::GlobalWarningDisplayOff();

    const CameraBasis basis = cameraBasisFromEuler({
        .roll = cameraRoll,
        .pitch = std::clamp(cameraPitch, -90.0, 90.0),
        .yaw = cameraYaw
    });

    camera->SetPosition(cameraPivot[0] + distance * basis.backward[0],
                        cameraPivot[1] + distance * basis.backward[1],
                        cameraPivot[2] + distance * basis.backward[2]);
    camera->SetFocalPoint(cameraPivot.data());
    camera->SetViewUp(basis.up.data());
    camera->OrthogonalizeViewUp();

    renderer->ResetCameraClippingRange();
    triggerRenderUpdate();

    vtkObject::SetGlobalWarningDisplay(oldWarningState);
}

void SceneWidget::updateCameraPivotFromBounds()
{
    if (! renderer)
        return;

    double bounds[6];
    renderer->ComputeVisiblePropBounds(bounds);

    const bool validX = std::isfinite(bounds[0]) && std::isfinite(bounds[1]) && bounds[0] < bounds[1];
    const bool validY = std::isfinite(bounds[2]) && std::isfinite(bounds[3]) && bounds[2] < bounds[3];

    if (! validX || ! validY)
        return;

    const double midZ = (std::isfinite(bounds[4]) && std::isfinite(bounds[5]))
                        ? (bounds[4] + bounds[5]) * 0.5
                        : 0.0;

    cameraPivot = {
        (bounds[0] + bounds[1]) * 0.5,
        (bounds[2] + bounds[3]) * 0.5,
        midZ
    };
}

void SceneWidget::loadAndUpdateVisualizationForCurrentStep()
{
    if (! settingParameter)
    {
        return;
    }

    if (settingParameter->numberOfLines > 0)
    {
        // Resize lines vector to match expected number of lines
        lines.resize(settingParameter->numberOfLines);

        {
            // Create a performance session for step loading (latency measurement)
            PerformanceSession perfSession("Step Loading",
                                           static_cast<uint32_t>(settingParameter->numberOfColumnX),
                                           static_cast<uint32_t>(settingParameter->numberOfRowsY),
                                           static_cast<uint32_t>(settingParameter->numberOfSlicesZ),
                                           1);  // Single timestep
            perfSession.setStepNumber(settingParameter->step);
            perfSession.setCategory(PerformanceMetrics::MetricsCategory::Rendering);

            // Read stage state from files for the current step
            sceneWidgetVisualizerProxy->readStageStateFromFilesForStep(settingParameter.get(), &lines[0]);
        } // Session destructor prints metrics here

        // Refresh VTK visualization with optional 3D substate support
        refreshVisualizationWithOptional3DSubstate();

        // Update load balancing lines if we have any
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
    sceneWidgetVisualizerProxy->prepareStage(settingParameter->nNodeX, settingParameter->nNodeY, settingParameter->nNodeZ);
}

void SceneWidget::drawVisualizationWithOptional3DSubstate()
{
    // Remove old actors from renderer to avoid "shadow" artifacts
    if (gridActor && renderer)
    {
        renderer->RemoveActor(gridActor);
    }
    if (backgroundActor && renderer)
    {
        renderer->RemoveActor(backgroundActor);
    }
    if (gridLinesOnSurfaceActor && renderer)
    {
        renderer->RemoveActor(gridLinesOnSurfaceActor);
    }

    if (isNative3DModel())
    {
        const bool sliceView = isNative3DSliceView();
        if (flatSceneBackgroundVisible && !sliceView)
        {
            sceneWidgetVisualizerProxy->drawFlatSceneBackground(settingParameter->numberOfRowsY,
                                                                settingParameter->numberOfColumnX,
                                                                renderer,
                                                                backgroundActor);
        }

        const auto colorSubstateInfos = getColorSubstateInfos();
        sceneWidgetVisualizerProxy->drawWithVTK(settingParameter->numberOfRowsY,
                                                settingParameter->numberOfColumnX,
                                                renderer,
                                                gridActor,
                                                colorSubstateInfos,
                                                useCellRendering);

        if (!sliceView)
        {
            sceneWidgetVisualizerProxy->drawGridLinesOn3DSurface(settingParameter->numberOfRowsY,
                                                                 settingParameter->numberOfColumnX,
                                                                 lines,
                                                                 renderer,
                                                                 gridLinesOnSurfaceActor,
                                                                 {},
                                                                 0.0,
                                                                 1.0);
        }

        if (actorBuildLine)
            actorBuildLine->SetVisibility(false);
        if (gridLinesOnSurfaceActor)
            gridLinesOnSurfaceActor->SetVisibility(!sliceView && gridLinesVisible);

        updateCameraPivotFromBounds();
        return;
    }
    
    // Check if we should use 3D substate visualization
    const auto heightSubstateInfosTopToBottom = get3DSubstateInfosTopToBottom();
    if (settingParameter->numberOfSlicesZ <= 1 &&
        !heightSubstateInfosTopToBottom.empty())
    {
        auto heightSubstateInfosBottomToTop = heightSubstateInfosTopToBottom;
        std::reverse(heightSubstateInfosBottomToTop.begin(), heightSubstateInfosBottomToTop.end());
        const bool stackedSubstates = heightSubstateInfosBottomToTop.size() > 1;

        if (substateSliceEnabled)
        {
            const auto colorSubstateInfos = getColorSubstateInfos();
            if (stackedSubstates)
            {
                sceneWidgetVisualizerProxy->drawWithVTK3DSubstatesSlice(
                    settingParameter->numberOfRowsY,
                    settingParameter->numberOfColumnX,
                    renderer,
                    gridActor,
                    heightSubstateInfosBottomToTop,
                    colorSubstateInfos,
                    substateSliceAxis,
                    substateSliceIndex);
            }
            else
            {
                const auto* substateInfo = heightSubstateInfosBottomToTop.front();
                sceneWidgetVisualizerProxy->drawWithVTK3DSubstateSlice(
                    settingParameter->numberOfRowsY,
                    settingParameter->numberOfColumnX,
                    renderer,
                    gridActor,
                    substateInfo->name,
                    substateInfo->minValue,
                    substateInfo->maxValue,
                    colorSubstateInfos,
                    substateSliceAxis,
                    substateSliceIndex);
            }

            if (backgroundActor)
                backgroundActor->SetVisibility(false);
            if (actorBuildLine)
                actorBuildLine->SetVisibility(false);
            if (gridLinesOnSurfaceActor)
                gridLinesOnSurfaceActor->SetVisibility(false);

            updateCameraPivotFromBounds();
            return;
        }

        // Clear old background actor to remove any 2D artifacts
        if (backgroundActor && renderer)
        {
            renderer->RemoveActor(backgroundActor);
            backgroundActor = vtkSmartPointer<vtkActor>::New();
        }

        // Draw flat background scene if enabled
        if (flatSceneBackgroundVisible)
        {
            sceneWidgetVisualizerProxy->drawFlatSceneBackground(settingParameter->numberOfRowsY,
                                                                settingParameter->numberOfColumnX,
                                                                renderer,
                                                                backgroundActor);
        }

        const auto colorSubstateInfos = getColorSubstateInfos();
        if (stackedSubstates)
        {
            sceneWidgetVisualizerProxy->drawWithVTK3DSubstates(settingParameter->numberOfRowsY,
                                                               settingParameter->numberOfColumnX,
                                                               renderer,
                                                               gridActor,
                                                               heightSubstateInfosBottomToTop,
                                                               colorSubstateInfos);
        }
        else
        {
            const auto* substateInfo = heightSubstateInfosBottomToTop.front();
            sceneWidgetVisualizerProxy->drawWithVTK3DSubstate(settingParameter->numberOfRowsY,
                                                              settingParameter->numberOfColumnX,
                                                              renderer,
                                                              gridActor,
                                                              substateInfo->name,
                                                              substateInfo->minValue,
                                                              substateInfo->maxValue,
                                                              colorSubstateInfos);
        }

        // Handle 2D grid lines visibility
        if (actorBuildLine)
        {
            actorBuildLine->SetVisibility(false); // Always hide 2D lines in 3D mode
        }

        // Draw 3D grid lines on surface with proper visibility
        if (stackedSubstates)
        {
            sceneWidgetVisualizerProxy->drawGridLinesOn3DSubstateStack(settingParameter->numberOfRowsY,
                                                                       settingParameter->numberOfColumnX,
                                                                       lines,
                                                                       renderer,
                                                                       gridLinesOnSurfaceActor,
                                                                       heightSubstateInfosBottomToTop);
        }
        else
        {
            const auto* substateInfo = heightSubstateInfosBottomToTop.front();
            sceneWidgetVisualizerProxy->drawGridLinesOn3DSurface(settingParameter->numberOfRowsY,
                                                                 settingParameter->numberOfColumnX,
                                                                 lines,
                                                                 renderer,
                                                                 gridLinesOnSurfaceActor,
                                                                 substateInfo->name,
                                                                 substateInfo->minValue,
                                                                 substateInfo->maxValue);
        }

        // Apply visibility setting to 3D grid lines
        if (gridLinesOnSurfaceActor)
        {
            gridLinesOnSurfaceActor->SetVisibility(gridLinesVisible);
        }

        updateCameraPivotFromBounds();
        return;
    }

    // Fallback to regular 2D visualization
    const auto colorSubstateInfos = getColorSubstateInfos();
    sceneWidgetVisualizerProxy->drawWithVTK(settingParameter->numberOfRowsY, settingParameter->numberOfColumnX, renderer, gridActor, colorSubstateInfos, useCellRendering);
    updateCameraPivotFromBounds();
}

std::vector<const SubstateInfo*> SceneWidget::getColorSubstateInfos()
{
    std::vector<const SubstateInfo*> colorSubstateInfos;
    if (! activeSubstatesForColorring.empty())
    {
        for (const auto& fieldName : activeSubstatesForColorring)
        {
            if (settingParameter->substateInfo.count(fieldName) > 0)
            {
                colorSubstateInfos.push_back(&settingParameter->substateInfo[fieldName]);
            }
        }
    }
    return colorSubstateInfos;
}

std::vector<const SubstateInfo*> SceneWidget::get3DSubstateInfosTopToBottom() const
{
    std::vector<const SubstateInfo*> heightSubstateInfos;
    if (!settingParameter || isNative3DModel())
        return heightSubstateInfos;

    for (const auto& fieldName : activeSubstatesFor3D)
    {
        const auto it = settingParameter->substateInfo.find(fieldName);
        if (it == settingParameter->substateInfo.end())
            continue;

        const auto& info = it->second;
        if (!std::isnan(info.minValue) &&
            !std::isnan(info.maxValue) &&
            info.minValue < info.maxValue)
        {
            heightSubstateInfos.push_back(&info);
        }
    }

    return heightSubstateInfos;
}

std::vector<const SubstateInfo*> SceneWidget::get3DSubstateInfosBottomToTop() const
{
    auto heightSubstateInfos = get3DSubstateInfosTopToBottom();
    std::reverse(heightSubstateInfos.begin(), heightSubstateInfos.end());
    return heightSubstateInfos;
}

std::string SceneWidget::active3DSubstateStackLabel() const
{
    const auto heightSubstateInfos = get3DSubstateInfosBottomToTop();
    std::string label;

    for (const auto* info : heightSubstateInfos)
    {
        if (!info)
            continue;

        if (!label.empty())
            label += " + ";
        label += info->name;
    }

    return label;
}

void SceneWidget::refreshVisualizationWithOptional3DSubstate()
{
    if (isNative3DModel())
    {
        if (isNative3DSliceView())
        {
            // Rebuild instead of only replacing the lookup table: changing the
            // selected plane can also change its row/column dimensions.
            drawVisualizationWithOptional3DSubstate();
            update2DRulerAxesBounds();
            triggerRenderUpdate();
            return;
        }

        if (flatSceneBackgroundVisible && backgroundActor && backgroundActor->GetMapper())
        {
            sceneWidgetVisualizerProxy->refreshFlatSceneBackground(settingParameter->numberOfRowsY,
                                                                   settingParameter->numberOfColumnX,
                                                                   backgroundActor);
        }

        const auto colorSubstateInfos = getColorSubstateInfos();
        sceneWidgetVisualizerProxy->refreshWindowsVTK(settingParameter->numberOfRowsY,
                                                      settingParameter->numberOfColumnX,
                                                      gridActor,
                                                      colorSubstateInfos);
        sceneWidgetVisualizerProxy->refreshGridLinesOn3DSurface(settingParameter->numberOfRowsY,
                                                                settingParameter->numberOfColumnX,
                                                                lines,
                                                                gridLinesOnSurfaceActor,
                                                                {},
                                                                0.0,
                                                                1.0);

        if (gridLinesOnSurfaceActor)
            gridLinesOnSurfaceActor->SetVisibility(gridLinesVisible);

        updateCameraPivotFromBounds();
        return;
    }

    // Check if we should use 3D substate visualization
    const auto heightSubstateInfosTopToBottom = get3DSubstateInfosTopToBottom();
    if (settingParameter->numberOfSlicesZ <= 1 &&
        !heightSubstateInfosTopToBottom.empty())
    {
        auto heightSubstateInfosBottomToTop = heightSubstateInfosTopToBottom;
        std::reverse(heightSubstateInfosBottomToTop.begin(), heightSubstateInfosBottomToTop.end());
        const bool stackedSubstates = heightSubstateInfosBottomToTop.size() > 1;

        if (substateSliceEnabled)
        {
            drawVisualizationWithOptional3DSubstate();
            update2DRulerAxesBounds();
            triggerRenderUpdate();
            return;
        }

        // Refresh flat background scene if enabled
        if (flatSceneBackgroundVisible && backgroundActor && backgroundActor->GetMapper())
        {
            sceneWidgetVisualizerProxy->refreshFlatSceneBackground(settingParameter->numberOfRowsY,
                                                                   settingParameter->numberOfColumnX,
                                                                   backgroundActor);
        }

        const auto colorSubstateInfos = getColorSubstateInfos();
        if (stackedSubstates)
        {
            sceneWidgetVisualizerProxy->refreshWindowsVTK3DSubstates(settingParameter->numberOfRowsY,
                                                                     settingParameter->numberOfColumnX,
                                                                     gridActor,
                                                                     heightSubstateInfosBottomToTop,
                                                                     colorSubstateInfos);
        }
        else
        {
            const auto* substateInfo = heightSubstateInfosBottomToTop.front();
            sceneWidgetVisualizerProxy->refreshWindowsVTK3DSubstate(settingParameter->numberOfRowsY,
                                                                    settingParameter->numberOfColumnX,
                                                                    gridActor,
                                                                    substateInfo->name,
                                                                    substateInfo->minValue,
                                                                    substateInfo->maxValue,
                                                                    colorSubstateInfos);
        }

        // Refresh 3D grid lines on surface
        if (stackedSubstates)
        {
            sceneWidgetVisualizerProxy->refreshGridLinesOn3DSubstateStack(settingParameter->numberOfRowsY,
                                                                          settingParameter->numberOfColumnX,
                                                                          lines,
                                                                          gridLinesOnSurfaceActor,
                                                                          heightSubstateInfosBottomToTop);
        }
        else
        {
            const auto* substateInfo = heightSubstateInfosBottomToTop.front();
            sceneWidgetVisualizerProxy->refreshGridLinesOn3DSurface(settingParameter->numberOfRowsY,
                                                                    settingParameter->numberOfColumnX,
                                                                    lines,
                                                                    gridLinesOnSurfaceActor,
                                                                    substateInfo->name,
                                                                    substateInfo->minValue,
                                                                    substateInfo->maxValue);
        }

        // Apply visibility setting to 3D grid lines
        if (gridLinesOnSurfaceActor)
        {
            gridLinesOnSurfaceActor->SetVisibility(gridLinesVisible);
        }

        updateCameraPivotFromBounds();
        return;
    }
    
    // Fallback to regular 2D visualization
    const auto colorSubstateInfos = getColorSubstateInfos();
    sceneWidgetVisualizerProxy->refreshWindowsVTK(settingParameter->numberOfRowsY, settingParameter->numberOfColumnX, gridActor, colorSubstateInfos);
    updateCameraPivotFromBounds();
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

    // Each node has two XY boundary lines. Final edges are tracked for every Z node layer.
    const auto totalNodes =
        settingParameter->nNodeX * settingParameter->nNodeY * settingParameter->nNodeZ;
    settingParameter->numberOfLines =
        2 * totalNodes +
        settingParameter->nNodeX * settingParameter->nNodeZ +
        settingParameter->nNodeY * settingParameter->nNodeZ;
    settingParameter->step = stepNumber;
    settingParameter->changed = false;

    sceneWidgetVisualizerProxy->initMatrix(settingParameter->numberOfColumnX, settingParameter->numberOfRowsY);

    refreshBackgroundColorFromSettings();
}

void SceneWidget::refreshGridColorFromSettings()
{
    const auto color = ColorSettings::instance().gridColor();

    // Apply color to 2D grid lines
    if (actorBuildLine)
    {
        actorBuildLine->GetProperty()->SetColor(toVtkColor(color).GetData());
        actorBuildLine->GetProperty()->Modified();
    }
    
    // Apply color to 3D grid lines
    if (gridLinesOnSurfaceActor)
    {
        gridLinesOnSurfaceActor->GetProperty()->SetColor(toVtkColor(color).GetData());
        gridLinesOnSurfaceActor->GetProperty()->Modified();
    }

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
        
        // Read number_of_slices for 3D models (defaults to 1 for 2D models)
        auto slicesParam = generalContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_OF_SLICES);
        settingParameter->numberOfSlicesZ = slicesParam ? slicesParam->getValue<int>() : 1;
        
        settingParameter->nsteps = generalContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_STEPS)->getValue<int>();
        emit totalNumberOfStepsReadFromConfigFile(settingParameter->nsteps);
    }

    {
        ConfigCategory* execContext = config.getConfigCategory(ConfigConstants::CATEGORY_DISTRIBUTED);
        settingParameter->nNodeX = execContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_X)->getValue<int>();
        settingParameter->nNodeY = execContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_Y)->getValue<int>();
        
        // Read number_node_z for 3D models (defaults to 1 for 2D models)
        auto nodeZParam = execContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_Z);
        settingParameter->nNodeZ = nodeZParam ? nodeZParam->getValue<int>() : 1;
        
        /// Notice: there are much more params, which are not used: e.g. border_size_x, border_size_y, border_size_z
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
    sceneWidgetVisualizerProxy->prepareStage(settingParameter->nNodeX, settingParameter->nNodeY, settingParameter->nNodeZ);

    renderWindow()->AddRenderer(renderer);
    interactor()->SetRenderWindow(renderWindow());

    // Enable anti-aliasing for better visual quality, especially for small grids
    renderWindow()->SetMultiSamples(16);
    renderWindow()->SetSize(settingParameter->numberOfColumnX, settingParameter->numberOfRowsY + 10);

    /// Use custom interactor style that zooms towards cursor position.
    /// This provides intuitive zoom behavior when using mouse wheel.
    setupInteractorStyleWithWaitCursor();

    renderWindow()->SetWindowName(QApplication::applicationName().toLocal8Bit().data());

    // Setup orientation axes widget
    setupAxesWidget();

    // Setup 2D ruler axes (bounds will be updated when data is loaded)
    setup2DRulerAxes();

    connectKeyboardCallback();
    connectMouseCallback();
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
    rulerAxisX->SetLabelFormat("%.0f");
    rulerAxisX->GetTitleTextProperty()->SetColor(1.0, 1.0, 1.0);
    rulerAxisX->GetLabelTextProperty()->SetColor(1.0, 1.0, 1.0);
    rulerAxisX->GetProperty()->SetColor(0.8, 0.8, 0.8);

    // Configure Y axis (vertical, right side)
    // Use World coordinates so the axis matches the actual data coordinates
    rulerAxisY->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    rulerAxisY->GetPosition2Coordinate()->SetCoordinateSystemToWorld();
    rulerAxisY->SetTitle("Y");
    rulerAxisY->SetNumberOfLabels(5);
    rulerAxisY->SetLabelFormat("%.0f");
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

    const double dataWidth = bounds[1] - bounds[0];
    const double dataHeight = bounds[3] - bounds[2];

    // Set user-facing pixel ranges for axes. VTK world Y grows upward, but image
    // coordinates grow downward, so the grid Y ruler is intentionally reversed:
    // top label = 0, bottom label = data height. Do not apply this to vertical
    // altitude/Z cross-section axes.
    rulerAxisX->SetRange(0.0, dataWidth);
    const bool verticalTopOrigin = verticalRulerUsesTopOrigin();
    rulerAxisY->SetRange(verticalTopOrigin ? dataHeight : 0.0,
                         verticalTopOrigin ? 0.0 : dataHeight);

    // Position X axis at the bottom of the data (horizontal line)
    rulerAxisX->GetPositionCoordinate()->SetValue(bounds[0], bounds[2], 0.0);
    rulerAxisX->GetPosition2Coordinate()->SetValue(bounds[1], bounds[2], 0.0);

    // Position Y axis at the RIGHT of the data (vertical line) - labels won't overlap scene
    rulerAxisY->GetPositionCoordinate()->SetValue(bounds[1], bounds[2], 0.0);
    rulerAxisY->GetPosition2Coordinate()->SetValue(bounds[1], bounds[3], 0.0);

    std::cout << "Ruler axes updated: X=[0, " << dataWidth
              << "], vertical=[" << (verticalTopOrigin ? dataHeight : 0.0)
              << ", " << (verticalTopOrigin ? 0.0 : dataHeight)
              << "]" << std::endl;
}

void SceneWidget::update2DRulerAxisTitles()
{
    const char* horizontalAxis = "X";
    const char* verticalAxis = "Y";
    std::string verticalAxisStorage;

    if (isNative3DSliceView())
    {
        switch (sceneWidgetVisualizerProxy->native3DSliceAxis())
        {
            case GridSliceAxis::X:
                horizontalAxis = "Y";
                verticalAxis = "Z";
                break;
            case GridSliceAxis::Y:
                horizontalAxis = "X";
                verticalAxis = "Z";
                break;
            case GridSliceAxis::Z:
                break;
        }
    }
    else if (substateSliceEnabled)
    {
        horizontalAxis = substateSliceAxis == GridSliceAxis::Y ? "X" : "Y";
        verticalAxisStorage = active3DSubstateStackLabel();
        verticalAxis = verticalAxisStorage.empty() ? "Altitude" : verticalAxisStorage.c_str();
    }

    rulerAxisX->SetTitle(horizontalAxis);
    rulerAxisY->SetTitle(verticalAxis);
}

bool SceneWidget::verticalRulerUsesTopOrigin() const
{
    if (substateSliceEnabled)
        return false;

    if (isNative3DSliceView())
    {
        return sceneWidgetVisualizerProxy->native3DSliceAxis() == GridSliceAxis::Z;
    }

    return true;
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
    if (!interactor() || !interactor()->GetInteractorStyle())
        return;

    // Interaction events are emitted by vtkInteractorStyle, not by the render
    // window interactor itself. Listen both during dragging and on release.
    vtkNew<vtkCallbackCommand> cameraCallback;
    cameraCallback->SetCallback(SceneWidget::cameraCallbackFunction);
    cameraCallback->SetClientData(this);
    interactor()->GetInteractorStyle()->AddObserver(vtkCommand::InteractionEvent, cameraCallback);
    interactor()->GetInteractorStyle()->AddObserver(vtkCommand::EndInteractionEvent, cameraCallback);
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
            sw->refreshVisualization();
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

    // Synchronize only in 3D mode; 2D interaction has no rotation controls.
    if (self->currentViewMode == ViewMode::Mode3D && self->renderer)
    {
        vtkCamera* camera = self->renderer->GetActiveCamera();
        if (camera)
        {
            const CameraEulerAngles angles = cameraEulerFromVtk(*camera);
            double focalPoint[3];
            camera->GetFocalPoint(focalPoint);

            // Store the actual VTK orientation. The Qt side blocks slider signals
            // while displaying these values, so this cannot feed back into VTK.
            self->cameraRoll = angles.roll;
            self->cameraPitch = angles.pitch;
            self->cameraYaw = angles.yaw;
            self->cameraPivot = { focalPoint[0], focalPoint[1], focalPoint[2] };

            emit self->cameraOrientationChanged(angles.roll, angles.pitch, angles.yaw);
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

    self->m_lastMousePickedGrid = false;

    // 3) Use a picker restricted to the data grid actor. Picking any visible prop
    // would also hit ruler axes or load-balancing helper lines, which can make a
    // tooltip appear when the cursor is visually outside the simulation grid.
    vtkNew<vtkPropPicker> picker;
    bool picked = false;
    if (self->renderer && self->gridActor)
    {
        picker->PickFromListOn();
        picker->AddPickList(self->gridActor);
        if (picker->Pick(vtkX, vtkY, 0.0, self->renderer))
        {
            double pickPos[3];
            picker->GetPickPosition(pickPos);
            self->m_lastWorldPos = { pickPos[0], pickPos[1], pickPos[2] };
            self->m_lastMousePickedGrid = true;
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
                                                                     settingParameter->nNodeZ,
                                                                     settingParameter->outputFileName);

    const auto availableSteps = sceneWidgetVisualizerProxy->availableSteps();
    emit availableStepsReadFromConfigFile(availableSteps);

    {
        // Create a performance session for initial data loading
        PerformanceSession perfSession("Initial Data Loading",
                                       static_cast<uint32_t>(settingParameter->numberOfColumnX),
                                       static_cast<uint32_t>(settingParameter->numberOfRowsY),
                                       static_cast<uint32_t>(settingParameter->numberOfSlicesZ),
                                       static_cast<uint32_t>(availableSteps.size()));
        perfSession.setCategory(PerformanceMetrics::MetricsCategory::DataLoading);

        lines.resize(settingParameter->numberOfLines);
        sceneWidgetVisualizerProxy->readStageStateFromFilesForStep(settingParameter.get(), &lines[0]);
    } // Session destructor prints metrics here

    // Draw VTK visualization with optional 3D substate support
    drawVisualizationWithOptional3DSubstate();

    sceneWidgetVisualizerProxy->getVisualizer().buildLoadBalanceLine(lines,
                                                                     settingParameter->numberOfRowsY + 1,
                                                                     renderer,
                                                                     actorBuildLine);

    // Apply grid lines visibility and semi-transparency settings
    applyGridLinesSettings();

    sceneWidgetVisualizerProxy->getVisualizer().buildStepText(settingParameter->step,
                                                              settingParameter->font_size,
                                                              singleLineTextStep,
                                                              renderer);

    updateCameraPivotFromBounds();
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
    if (! settingParameter || ! sceneWidgetVisualizerProxy)
    {
        return {};
    }

    // Use unified bounds checking
    if (! isWorldPositionInGrid(worldPos.data()))
    {
        return {}; // Outside scene bounds
    }

    double bounds[6];
    if (! currentGridBounds(bounds))
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
    const int nodeYFromBottom = static_cast<int>((worldPos[1] - bounds[2]) / nodeHeight);
    const int nodeY = static_cast<int>(settingParameter->nNodeY) - 1 - nodeYFromBottom;

    // Check if the calculated node is within bounds
    if (nodeX >= 0 && nodeX < static_cast<int>(settingParameter->nNodeX) &&
        nodeY >= 0 && nodeY < static_cast<int>(settingParameter->nNodeY))
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

    if (m_lastMousePickedGrid && isCrossSectionView() && isWorldPositionInGrid(m_lastWorldPos.data()))
    {
        int planeRow = 0;
        int planeColumn = 0;
        if (convertWorldToGridCoordinates(m_lastWorldPos.data(), planeRow, planeColumn))
        {
            if (substateSliceEnabled)
            {
                const int x = substateSliceAxis == GridSliceAxis::Y
                    ? planeColumn
                    : substateSliceIndex;
                const int y = substateSliceAxis == GridSliceAxis::Y
                    ? substateSliceIndex
                    : planeRow;
                QString tooltipText =
                    QString("Cell Coordinates: (X=%1, Y=%2)").arg(x).arg(y);
                tooltipText += cellValueAtThisPositionAsText();
                QToolTip::showText(mapToGlobal(lastMousePos),
                                   tooltipText,
                                   this,
                                   QRect(lastMousePos, QSize(1, 1)),
                                   0);
                return;
            }

            int x = planeColumn;
            int y = planeRow;
            int z = sceneWidgetVisualizerProxy->native3DSliceIndex();
            switch (sceneWidgetVisualizerProxy->native3DSliceAxis())
            {
                case GridSliceAxis::X:
                    x = sceneWidgetVisualizerProxy->native3DSliceIndex();
                    y = planeColumn;
                    z = settingParameter->numberOfSlicesZ - 1 - planeRow;
                    break;
                case GridSliceAxis::Y:
                    y = sceneWidgetVisualizerProxy->native3DSliceIndex();
                    z = settingParameter->numberOfSlicesZ - 1 - planeRow;
                    break;
                case GridSliceAxis::Z:
                    break;
            }
            QString tooltipText =
                QString("Cell Coordinates: (X=%1, Y=%2, Z=%3)").arg(x).arg(y).arg(z);
            tooltipText += cellValueAtThisPositionAsText();
            QToolTip::showText(mapToGlobal(lastMousePos),
                               tooltipText,
                               this,
                               QRect(lastMousePos, QSize(1, 1)),
                               0);
            return;
        }
    }

    // m_lastMousePos is already in Qt coordinates (origin: top-left)
    // m_lastWorldPos is set by the VTK callback (picker or DisplayToWorld fallback)

    // Check if we're over a line (only if line detection is enabled)
    size_t lineIndex = 0;
    double distanceSq = 0.0;
    const Line* nearestLine = nullptr;
    if (lineDetectionEnabled)
    {
        nearestLine = findNearestLine(m_lastWorldPos, lineIndex, distanceSq);
    }

    // Prepare tooltip text
    QString tooltipText;

    if (nearestLine)
    {
        tooltipText += QString("Line %1/%2:").arg(lineIndex).arg(lines.size());
        tooltipText += QString("\n  From: (x1=%1, y1=%2)")
                           .arg(static_cast<int>(std::lround(nearestLine->x1)))
                           .arg(static_cast<int>(std::lround(nearestLine->y1)));
        tooltipText += QString("\n  To:   (x2=%1, y2=%2)")
                           .arg(static_cast<int>(std::lround(nearestLine->x2)))
                           .arg(static_cast<int>(std::lround(nearestLine->y2)));
        tooltipText += cellValueAtThisPositionAsText();
    }
    else if (m_lastMousePickedGrid)
    {
        int displayX = 0;
        int displayY = 0;
        int displayZ = 0;
        const QString nodeInfo = getNodeAtWorldPosition(m_lastWorldPos);
        if (nodeInfo.isEmpty() || !convertWorldToDisplayCoordinates(m_lastWorldPos.data(), displayX, displayY, displayZ))
        {
            tooltipText = "(Outside the grid)";
        }
        else
        {
            tooltipText = QString("Pixel Position: (x: %1, y: %2, z: %3)")
                              .arg(displayX)
                              .arg(displayY)
                              .arg(displayZ);

            tooltipText += QString("\n%1").arg(nodeInfo);

            tooltipText += cellValueAtThisPositionAsText();
        }
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

    if (!m_lastMousePickedGrid)
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
        refreshVisualization();
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
            settingParameter->nNodeZ,
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
    sceneWidgetVisualizerProxy->clearNative3DSlice();
    substateSliceEnabled = false;

    // Reset setting parameters to avoid stale data
    settingParameter = std::make_unique<SettingParameter>();

    // Reset VTK actors
    gridActor = vtkSmartPointer<vtkActor>::New();
    backgroundActor = vtkSmartPointer<vtkActor>::New();
    actorBuildLine = vtkSmartPointer<vtkActor2D>::New();
    gridLinesOnSurfaceActor = vtkSmartPointer<vtkActor>::New();
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
    if (isNative3DModel() && !isNative3DSliceView())
    {
        std::cerr << "2D view is unavailable for a native 3D model." << std::endl;
        return;
    }

    if (! interactor())
        return;

    // Show wait cursor during view mode change
    WaitCursorGuard waitCursor("Switching to 2D mode...");

    currentViewMode = ViewMode::Mode2D;
    
    // Disable 3D substate visualization when switching to 2D mode
    if (!substateSliceEnabled)
    {
        activeSubstateFor3D.clear();
        activeSubstatesFor3D.clear();
    }
    
    // In 2D mode, flat scene background is always visible (it's the 2D visualization itself)
    flatSceneBackgroundVisible = true;
    
    // Clear the 3D background actor (remove any 3D artifacts)
    if (backgroundActor && renderer)
    {
        renderer->RemoveActor(backgroundActor);
        backgroundActor = vtkSmartPointer<vtkActor>::New();
    }
    
    // Redraw visualization in 2D mode (without 3D substate)
    if (settingParameter && sceneWidgetVisualizerProxy)
    {
        drawVisualizationWithOptional3DSubstate(); // TODO: GB: Should it be called from 2D `SceneWidget::setViewMode2D()`?
        renderWindow()->Render();
    }

    // Use custom interactor style that zooms towards cursor position
    setupInteractorStyleWithWaitCursor();

    // Reset camera angles
    cameraRoll = {};
    cameraPitch = {};
    cameraYaw = {};

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

    // Setup 2D ruler axes (bounds will be updated when data is loaded)
    setup2DRulerAxes();
    update2DRulerAxisTitles();

    // Rebuild grid lines (they were removed when switching to 3D substate)
    if (settingParameter && sceneWidgetVisualizerProxy && !lines.empty() &&
        !isNative3DModel() && !substateSliceEnabled)
    {
        sceneWidgetVisualizerProxy->getVisualizer().buildLoadBalanceLine(lines,
                                                                         settingParameter->numberOfRowsY + 1,
                                                                         renderer,
                                                                         actorBuildLine);
        // Apply grid lines visibility and semi-transparency settings
        applyGridLinesSettings();
    }

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

    renderWindow()->Render();
    
    // Cursor restored automatically by WaitCursorGuard destructor
}

void SceneWidget::setViewMode3D()
{
    if (! interactor())
        return;

    // Show wait cursor during view mode change
    WaitCursorGuard waitCursor("Switching to 3D mode...");

    currentViewMode = ViewMode::Mode3D;

    // Use CustomInteractorStyle which supports both 3D rotation (TrackballCamera)
    // and cursor-based zoom with wait cursor feedback
    setupInteractorStyleWithWaitCursor();

    // Show orientation axes in 3D mode
    setAxesWidgetVisible(true);

    // Hide 2D ruler axes in 3D mode
    rulerAxisX->SetVisibility(false);
    rulerAxisY->SetVisibility(false);

    // Clear any 2D background artifacts before rendering 3D scene
    if (!isNative3DModel() && backgroundActor && renderer)
    {
        renderer->RemoveActor(backgroundActor);
        backgroundActor = vtkSmartPointer<vtkActor>::New();
    }

    std::cout << "Switched to 3D view mode" << std::endl;
    
    // Cursor restored automatically by WaitCursorGuard destructor
}

bool SceneWidget::isNative3DModel() const
{
    return settingParameter && settingParameter->numberOfSlicesZ > 1;
}

bool SceneWidget::isNative3DSliceView() const
{
    return isNative3DModel() &&
           sceneWidgetVisualizerProxy &&
           sceneWidgetVisualizerProxy->isNative3DSliceEnabled();
}

bool SceneWidget::is3DSubstateSurface() const
{
    if (!settingParameter || isNative3DModel())
        return false;

    return !get3DSubstateInfosTopToBottom().empty();
}

bool SceneWidget::hasSliceable3DView() const
{
    return isNative3DModel() || is3DSubstateSurface();
}

bool SceneWidget::isCrossSectionView() const
{
    return isNative3DSliceView() || substateSliceEnabled;
}

void SceneWidget::setNative3DSlice(GridSliceAxis axis, int fixedIndex)
{
    if (!isNative3DModel() || !sceneWidgetVisualizerProxy)
        return;

    const bool planeChanged =
        !isNative3DSliceView() ||
        sceneWidgetVisualizerProxy->native3DSliceAxis() != axis;

    sceneWidgetVisualizerProxy->setNative3DSlice(axis, fixedIndex);

    if (planeChanged || currentViewMode != ViewMode::Mode2D)
    {
        setViewMode2D();
        return;
    }

    drawVisualizationWithOptional3DSubstate();
    update2DRulerAxisTitles();
    update2DRulerAxesBounds();
    updateCameraPivotFromBounds();
    triggerRenderUpdate();
}

void SceneWidget::setNative3DVolumeView()
{
    if (!isNative3DModel() || !sceneWidgetVisualizerProxy)
        return;

    sceneWidgetVisualizerProxy->clearNative3DSlice();
    drawVisualizationWithOptional3DSubstate();
    setViewMode3D();
    updateCameraPivotFromBounds();
    applyCameraAngles();
}

void SceneWidget::setSubstate3DSlice(GridSliceAxis axis, int fixedIndex)
{
    if (!is3DSubstateSurface() ||
        (axis != GridSliceAxis::X && axis != GridSliceAxis::Y))
    {
        return;
    }

    const bool planeChanged = !substateSliceEnabled || substateSliceAxis != axis;
    substateSliceEnabled = true;
    substateSliceAxis = axis;
    substateSliceIndex = axis == GridSliceAxis::Y
        ? std::clamp(fixedIndex, 0, settingParameter->numberOfRowsY - 1)
        : std::clamp(fixedIndex, 0, settingParameter->numberOfColumnX - 1);

    if (planeChanged || currentViewMode != ViewMode::Mode2D)
    {
        setViewMode2D();
        return;
    }

    drawVisualizationWithOptional3DSubstate();
    update2DRulerAxisTitles();
    update2DRulerAxesBounds();
    updateCameraPivotFromBounds();
    triggerRenderUpdate();
}

void SceneWidget::clearCrossSection()
{
    if (isNative3DSliceView())
    {
        setNative3DVolumeView();
        return;
    }

    if (!substateSliceEnabled)
        return;

    substateSliceEnabled = false;
    drawVisualizationWithOptional3DSubstate();
    setViewMode3D();
    updateCameraPivotFromBounds();
    applyCameraAngles();
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

    applyGridLinesSettings();

    triggerRenderUpdate();
}

void SceneWidget::setLineDetectionEnabled(bool enabled)
{
    lineDetectionEnabled = enabled;
}

void SceneWidget::setFlatSceneBackgroundVisible(bool visible)
{
    flatSceneBackgroundVisible = visible;
    
    // Update background actor visibility
    if (backgroundActor)
    {
        backgroundActor->SetVisibility(visible && currentViewMode == ViewMode::Mode3D);
    }
    
    triggerRenderUpdate();
}

void SceneWidget::setUseCellRendering(bool useCellRenderingMode)
{
    useCellRendering = useCellRenderingMode;
    // No need to trigger render update here - caller will call refreshVisualization
}

void SceneWidget::setActiveSubstateFor3D(const std::string& fieldName)
{
    if (fieldName.empty())
    {
        setActiveSubstatesFor3D({});
    }
    else
    {
        setActiveSubstatesFor3D({fieldName});
    }
}

void SceneWidget::setActiveSubstatesFor3D(const std::vector<std::string>& fieldNames)
{
    if (fieldNames.empty())
        substateSliceEnabled = false;

    activeSubstatesFor3D = fieldNames;
    activeSubstateFor3D = fieldNames.empty() ? std::string{} : fieldNames.front();
}

void SceneWidget::setActiveSubstatesForColorring(const std::vector<std::string>& fieldNames)
{    
    activeSubstatesForColorring = fieldNames;

    refreshVisualization();
}

void SceneWidget::refreshVisualization()
{
    loadAndUpdateVisualizationForCurrentStep();
    triggerRenderUpdate();
}

void SceneWidget::setCameraRoll(double angle)
{
    // Store the new roll value
    cameraRoll = angle;

    // Apply camera angles using helper method
    if (currentViewMode == ViewMode::Mode3D)
        applyCameraAnglesPreservingZoom();
    else
        applyCameraAngles();
}

void SceneWidget::setCameraPitch(double angle)
{
    // Store the new pitch value
    cameraPitch = angle;

    // Apply camera angles using helper method
    if (currentViewMode == ViewMode::Mode3D)
        applyCameraAnglesPreservingZoom();
    else
        applyCameraAngles();
}

void SceneWidget::setCameraYaw(double angle)
{
    // Store the new yaw value
    cameraYaw = angle;

    // Apply camera angles using helper method
    if (currentViewMode == ViewMode::Mode3D)
        applyCameraAnglesPreservingZoom();
    else
        applyCameraAngles();
}

void SceneWidget::resetCameraZoom()
{
    auto camera = renderer->GetActiveCamera();
    if (! camera)
        return;

    // Reset camera to default position while preserving rotation angles
    // This is done by calling ResetCamera which fits all objects in view
    renderer->ResetCamera();

    triggerRenderUpdate();
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
        if (m_lastMousePickedGrid && isWorldPositionInGrid(m_lastWorldPos.data()))
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
    if (!settingParameter)
        return false;

    double bounds[6];
    if (! currentGridBounds(bounds) || ! isWorldPositionInGrid(worldPos))
        return false;

    // Calculate grid dimensions
    const double sceneWidth = bounds[1] - bounds[0];
    const double sceneHeight = bounds[3] - bounds[2];

    if (sceneWidth <= 0 || sceneHeight <= 0)
        return false;

    if (substateSliceEnabled)
    {
        const int sampleCount = substateSliceAxis == GridSliceAxis::Y
            ? settingParameter->numberOfColumnX
            : settingParameter->numberOfRowsY;
        int sample = static_cast<int>(
            ((worldPos[0] - bounds[0]) / sceneWidth) * sampleCount);
        sample = std::clamp(sample, 0, sampleCount - 1);

        if (substateSliceAxis == GridSliceAxis::Y)
        {
            outRow = substateSliceIndex;
            outCol = sample;
        }
        else
        {
            outRow = sample;
            outCol = substateSliceIndex;
        }
        return true;
    }

    const int columnCount = displayedColumnCount();
    const int rowCount = displayedRowCount();
    if (columnCount <= 0 || rowCount <= 0)
        return false;

    const double cellWidth = sceneWidth / columnCount;
    const double cellHeight = sceneHeight / rowCount;

    // Convert world position to grid indices
    // Points are positioned with Y inverted: (nRows - 1 - row)
    // So we need to invert the row calculation to get the correct matrix index
    int col = static_cast<int>((worldPos[0] - bounds[0]) / cellWidth);
    int row = static_cast<int>((worldPos[1] - bounds[2]) / cellHeight);
    
    // Invert row to match the inverted Y coordinates used in visualization
    row = rowCount - 1 - row;

    // Clamp to valid range
    col = std::max(0, std::min(col, columnCount - 1));
    row = std::max(0, std::min(row, rowCount - 1));

    outRow = row;
    outCol = col;
    return true;
}

bool SceneWidget::currentGridBounds(double bounds[6]) const
{
    if (!gridActor)
        return false;

    gridActor->GetBounds(bounds);

    return std::isfinite(bounds[0]) && std::isfinite(bounds[1]) &&
           std::isfinite(bounds[2]) && std::isfinite(bounds[3]) &&
           bounds[0] < bounds[1] && bounds[2] < bounds[3];
}

bool SceneWidget::convertWorldToDisplayCoordinates(const double worldPos[3], int& outX, int& outY, int& outZ) const
{
    double bounds[6];
    if (! currentGridBounds(bounds) || ! isWorldPositionInGrid(worldPos))
        return false;

    outX = static_cast<int>(std::lround(worldPos[0] - bounds[0]));
    outY = static_cast<int>(std::lround(bounds[3] - worldPos[1]));
    outZ = static_cast<int>(std::lround(worldPos[2]));

    return true;
}

int SceneWidget::displayedRowCount() const
{
    if (!settingParameter)
        return 0;
    if (!isNative3DSliceView())
        return settingParameter->numberOfRowsY;

    return sceneWidgetVisualizerProxy->native3DSliceAxis() == GridSliceAxis::Z
        ? settingParameter->numberOfRowsY
        : settingParameter->numberOfSlicesZ;
}

int SceneWidget::displayedColumnCount() const
{
    if (!settingParameter)
        return 0;
    if (!isNative3DSliceView())
        return settingParameter->numberOfColumnX;

    return sceneWidgetVisualizerProxy->native3DSliceAxis() == GridSliceAxis::X
        ? settingParameter->numberOfRowsY
        : settingParameter->numberOfColumnX;
}

bool SceneWidget::isWorldPositionInGrid(const double worldPos[3]) const
{
    if (!settingParameter)
        return false;

    double bounds[6];
    if (! currentGridBounds(bounds))
        return false;

    // Check if position is within grid bounds
    if (worldPos[0] < bounds[0] || worldPos[0] > bounds[1] ||
        worldPos[1] < bounds[2] || worldPos[1] > bounds[3])
    {
        return false;  // Outside grid bounds
    }

    return true;  // Inside grid bounds
}

void SceneWidget::setupInteractorStyleWithWaitCursor()
{
    // Features: Ray-plane zoom (zoom towards cursor), wait cursor, Shift+Drag panning
    // Cost: ~5% overhead due to ray-plane calculations
    vtkNew<CustomInteractorStyle> style;
    style->Set3DInteractionEnabled(currentViewMode == ViewMode::Mode3D);
    interactor()->SetInteractorStyle(style);
    connectCameraCallback();
}

void SceneWidget::applyGridLinesSettings()
{
    const bool isNativeModel = settingParameter && settingParameter->numberOfSlicesZ > 1;
    const bool isNative3D = isNativeModel && !isNative3DSliceView();

    // The existing load-balancing line data describes XY partitions only.
    // Do not display it on XZ/YZ slices (or stale over an XY slice) until a
    // plane-specific node-boundary representation is built.
    if (isNativeModel && isNative3DSliceView())
    {
        if (actorBuildLine)
            actorBuildLine->SetVisibility(false);
        if (gridLinesOnSurfaceActor)
            gridLinesOnSurfaceActor->SetVisibility(false);
        return;
    }

    if (substateSliceEnabled)
    {
        if (actorBuildLine)
            actorBuildLine->SetVisibility(false);
        if (gridLinesOnSurfaceActor)
            gridLinesOnSurfaceActor->SetVisibility(false);
        return;
    }

    // Native volumes never use the flat XY node overlay. The established
    // 2D "substate as altitude" mode keeps its surface-line behavior.
    const bool isSubstateSurface =
        !isNative3D &&
        settingParameter &&
        !get3DSubstateInfosTopToBottom().empty();
    const bool isIn3DMode = isNative3D || isSubstateSurface;
    
    if (isIn3DMode)
    {
        // In 3D mode: apply settings only to 3D lines, hide 2D lines
        if (gridLinesOnSurfaceActor)
        {
            gridLinesOnSurfaceActor->SetVisibility(gridLinesVisible);
        }
        if (actorBuildLine)
        {
            actorBuildLine->SetVisibility(false);
        }
    }
    else
    {
        // In 2D mode: apply settings only to 2D lines with semi-transparency, hide 3D lines
        if (actorBuildLine)
        {
            actorBuildLine->SetVisibility(gridLinesVisible);
            // Set grid lines to semi-transparent (50% opacity)
            actorBuildLine->GetProperty()->SetOpacity(0.5);
        }
        if (gridLinesOnSurfaceActor)
        {
            gridLinesOnSurfaceActor->SetVisibility(false);
        }
    }
}

void SceneWidget::initializeAndDraw3DSubstateVisualization()
{
    {
        // Create a performance session for 3D substate loading
        PerformanceSession perfSession("3D Substate Loading",
                                       static_cast<uint32_t>(settingParameter->numberOfColumnX),
                                       static_cast<uint32_t>(settingParameter->numberOfRowsY),
                                       static_cast<uint32_t>(settingParameter->numberOfSlicesZ),
                                       1);
        perfSession.setCategory(PerformanceMetrics::MetricsCategory::Rendering);

        // Read the current step data from files
        lines.resize(settingParameter->numberOfLines);
        sceneWidgetVisualizerProxy->readStageStateFromFilesForStep(settingParameter.get(), &lines[0]);
    } // Session destructor prints metrics here

    // Draw the 3D substate visualization (initializes the scene with quad mesh)
    drawVisualizationWithOptional3DSubstate();

    // Update load balancing lines if we have any
    if (settingParameter->numberOfLines > 0)
    {
        sceneWidgetVisualizerProxy->getVisualizer().refreshBuildLoadBalanceLine(lines,
                                                                                settingParameter->numberOfRowsY + 1,
                                                                                actorBuildLine);
    }

    // Update step number display
    sceneWidgetVisualizerProxy->getVisualizer().buildStepLine(settingParameter->step, singleLineTextStep);

    // Trigger render update
    triggerRenderUpdate();
}
