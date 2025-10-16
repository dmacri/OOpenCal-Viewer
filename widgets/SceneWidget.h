/** @file SceneWidget.h
 * @brief Declaration of the SceneWidget class for 3D visualization.
 * The class wraps QVTKOpenGLNativeWidget which is embedded VTK widget in Qt application. */

#pragma once

#include <QVTKOpenGLNativeWidget.h>
#include <QTimer>
#include <QMouseEvent>
#include <QToolTip>
#include <vtkDataSet.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkNamedColors.h>
#include <vtkTextMapper.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkAxesActor.h>
#include <vtkAxisActor2D.h>
#include "visualiserProxy/ISceneWidgetVisualizer.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
#include "types.h"

/** @enum ViewMode
 * @brief Defines the camera view mode for the scene.
 * 
 * This enum is used to switch between 2D (top-down orthographic) and 3D (perspective with rotation) views. */
enum class ViewMode
{
    Mode2D,  ///< 2D top-down view with rotation disabled
    Mode3D   ///< 3D perspective view with full camera control
};

class SettingParameter;

/** @class SceneWidget
 * @brief A widget for 3D visualization using VTK in Qt app.
 * 
 * This widget provides a 3D visualization environment with support for multiple model types and interactive features. */
class SceneWidget : public QVTKOpenGLNativeWidget
{
    Q_OBJECT

public:
    /// @brief Constructs a SceneWidget with parent (which will takes care about deallecation of the widget, Qt is using that pattern)
    explicit SceneWidget(QWidget* parent);
    
    /// @brief Destroys the SceneWidget. It is on purpose defined in cpp file to clean up properly incomplete types.
    ~SceneWidget();

    /** @brief Adds a visualizer for the specified config file. It moves position to provided step number.
     *  @param filename The config file to visualize
     *  @param stepNumber The simulation step to display **/
    void addVisualizer(const std::string &filename, int stepNumber);

    /** @brief Updates the visualization widget to show the specified step.
     *  @param stepNumber The step number to display */
    void selectedStepParameter(StepIndex stepNumber);

    /** @brief Switch to a different model type.
     * 
     * This method allows changing the visualization model at runtime.
     * @note This does NOT reload data files. Use reloadData() after switching.
     * 
     * @param modelType The new model type to use */
    void switchModel(ModelType modelType);

    /** @brief Reload data files for the current model.
     * 
     * This method reads selected config file from disk and refreshes the visualization.
     * Call this after switching models or when data files have changed.
     * @note reloading data with not compatible model can crash the application */
    void reloadData();

    /** @brief Clear the entire scene (remove all VTK actors and data).
     * 
     * This method clears the renderer and resets the visualizer state.
     * Call this before loading a new configuration file. */
    void clearScene();

    /** @brief Load a new configuration file and reinitialize the scene.
     * 
     * @param configFileName Path to the new configuration file
     * @param stepNumber Initial step number to display */
    void loadNewConfiguration(const std::string& configFileName, int stepNumber = 0);

    /** @brief Get the current model name.
     * 
     * @return std::string The name of the currently active model */
    auto getCurrentModelName() const
    {
        return sceneWidgetVisualizerProxy->getModelName();
    }

    /** @brief Get the current setting parameter
     *  @return Pointer to the current SettingParameter object */
    const SettingParameter* getSettingParameter() const
    {
        return settingParameter.get();
    }

    /** @brief Set the view mode to 2D (top-down view with rotation disabled).
     * 
     * This method configures the camera for a 2D orthographic view from above
     * and disables rotation controls. */
    void setViewMode2D();

    /** @brief Set the view mode to 3D (perspective view with full camera control).
     * 
     * This method enables full 3D camera controls including rotation and elevation. */
    void setViewMode3D();

    /** @brief Show or hide the orientation axes widget.
     * 
     * @param visible If true, shows the axes widget; if false, hides it */
    void setAxesWidgetVisible(bool visible);

    /** @brief Get the current view mode.
     * 
     * @return The current ViewMode (2D or 3D) */
    ViewMode getViewMode() const { return currentViewMode; }

    /** @brief Set camera azimuth (rotation around Z axis).
     * 
     * @param angle Azimuth angle in degrees */
    void setCameraAzimuth(double angle);

    /** @brief Set camera elevation (rotation around X axis).
     * 
     * @param angle Elevation angle in degrees */
    void setCameraElevation(double angle);

    /** @brief Get current camera azimuth.
     * 
     * @return Current azimuth angle in degrees */
    double getCameraAzimuth() const
    {
        return cameraAzimuth;
    }

    /** @brief Get current camera elevation.
     * 
     * @return Current elevation angle in degrees */
    double getCameraElevation() const
    {
        return cameraElevation;
    }

    /** @brief Callback function for VTK keypress events.
     *  It handles arrow_up and arrow_down keys pressed and changes view of the widget.
     *  Provided argument types are compatible with vtkCallbackCommand::SetCallback
     * 
     * @param caller The VTK object that triggered the event
     * @param eventId The ID of the event
     * @param clientData User data passed to the callback
     * @param callData Event-specific data */
    static void keypressCallbackFunction(vtkObject* caller, long unsigned int eventId, void* clientData, void* callData);

    /** @brief Callback function for VTK mouse move events.
     *
     *  This function is triggered whenever a mouse movement event occurs within the VTK
     *  interactor. It captures the current mouse position in VTK coordinate space
     *  (origin at bottom-left), converts it into Qt widget coordinates (origin at top-left),
     *  and updates both the last known mouse position and corresponding world position.
     *
     *  The world position is determined using a VTK picker if a prop is hit.
     *  If no object is detected by the picker, a DisplayToWorld transformation is used
     *  as a fallback to approximate the world coordinates.
     *  After updating positions, the tooltip is refreshed to display context information.
     *
     * @param caller       The VTK object (typically vtkRenderWindowInteractor) that triggered the event.
     * @param eventId      The ID of the event (expected to be vtkCommand::MouseMoveEvent).
     * @param clientData   Pointer to user data passed when registering the callback (used to access the owning SceneWidget instance).
     * @param callData     Additional event-specific data (unused in this implementation). */
    static void mouseCallbackFunction(vtkObject* caller, long unsigned int eventId, void* clientData, void* callData);

    /** @brief Callback function for VTK camera modified events.
     *
     * This function is triggered whenever the camera is modified (e.g., rotated via mouse).
     * It emits a Qt signal to notify UI elements (like sliders) to update.
     *
     * @param caller       The VTK camera object that was modified.
     * @param eventId      The ID of the event (expected to be vtkCommand::ModifiedEvent).
     * @param clientData   Pointer to user data (the owning SceneWidget instance).
     * @param callData     Additional event-specific data (unused). */
    static void cameraCallbackFunction(vtkObject* caller, long unsigned int eventId, void* clientData, void* callData);

signals:
    /** @brief Signal emitted when step number is changed using keyboard keys (sent from method keypressCallbackFunction)
     *  @param stepNumber The new step number */
    void changedStepNumberWithKeyboardKeys(StepIndex stepNumber);

    /** @brief Signal emitted when total number of steps is read from config file.
     *  @param totalSteps Total number of steps available */
    void totalNumberOfStepsReadFromConfigFile(StepIndex totalSteps);

    /** @brief Signal emitted when available steps are read from config file.
     *  @param availableSteps Vector of available step numbers */
    void availableStepsReadFromConfigFile(std::vector<StepIndex> availableSteps);

    /** @brief Signal emitted when camera orientation changes (e.g., via mouse interaction).
     * 
     * This allows UI elements (like sliders) to update when the user rotates the camera.
     * @param azimuth Current camera azimuth in degrees
     * @param elevation Current camera elevation in degrees */
    void cameraOrientationChanged(double azimuth, double elevation);

public slots:
    /** @brief Slot called when color settings need to be reloaded (at least one of them was changed)
     *
     * This method is typically connected to a signal indicating that color settings
     * have changed. It updates all visual elements in the widget to reflect the
     * current color settings from the ColorSettings singleton. */
    void onColorsReloadRequested();

protected:   
    /// @brief Renders the VTK scene. It needs to be called when reading from config file
    void renderVtkScene();

    /// @brief Upgrades the model in the central panel
    void upgradeModelInCentralPanel();

    /// @brief Enables tooltip display when mouse is above the widget
    void enableToolTipWhenMouseAboveWidget();
    
    /** @brief Updates the tooltip with current mouse position
     *  @param lastMousePos Current mouse position in widget coordinates */
    void updateToolTip(const QPoint& lastMousePos);
    
    /** @brief Converts screen coordinates to VTK world coordinates
     *  @param pos The screen position in widget coordinates
     *  @return The corresponding world coordinates in VTK space */
    std::array<double, 3> screenToWorldCoordinates(const QPoint& pos) const;
    
    /** @brief Determines which node the mouse is currently over using VTK coordinates
     *  @param worldPos The current position in VTK world coordinates
     *  @return A string indicating which node the mouse is over, or empty string if not over any node */
    QString getNodeAtWorldPosition(const std::array<double, 3>& worldPos) const;
    
    /** @brief Finds the nearest line to the given world position
     *  @param worldPos The position to check
     *  @param[out] lineIndex Index of the found line in the lines vector
     *  @param[out] distanceSquared Squared distance to the nearest point on the line
     *  @return Pointer to the nearest line, or nullptr if no lines exist */
    const Line* findNearestLine(const std::array<double, 3>& worldPos, size_t& lineIndex, double& distanceSquared) const;

    /// @brief Reads settings from a configuration file
    /// @param filename Path to the configuration file
    void readSettingsFromConfigFile(const std::string &filename);

    /// @brief Sets up the VTK scene, it is called when reading config file
    void setupVtkScene();
    
    /// @brief Sets up the orientation axes widget
    void setupAxesWidget();
    
    /// @brief Sets up the 2D ruler axes
    void setup2DRulerAxes();
    
    /// @brief Connects the VTK camera modified callback to track camera changes
    void connectCameraCallback();
    
    /// @brief Updates the 2D ruler axes bounds based on current data
    void update2DRulerAxesBounds();
    
    /** @param configFilename Path to the configuration file and move view to provided step
     *  @param stepNumber Initial step number to display */
    void setupSettingParameters(const std::string & configFilename, int stepNumber);

    /** @brief Updates the background color from application settings.
     *
     * This method retrieves the current background color from the ColorSettings
     * singleton and applies it to the scene's renderer. It should be called
     * whenever the background color setting changes. */
    void refreshBackgroundColorFromSettings();

    /** @brief Updates the step number text color from application settings.
     *
     * This method retrieves the current text color from the ColorSettings
     * singleton and applies it to any step number text elements in the scene. */
    void refreshStepNumberTextColorFromSettings();

    /** @brief Updates the grid color from application settings.
     *
     * This method retrieves the current grid color from the ColorSettings
     * singleton and applies it to the scene's grid. It should be called
     * whenever the grid color setting changes. */
    void refreshGridColorFromSettings();

    /** @brief Connects the VTK keyboard callback to the interactor.
     *
     * This method registers a key press observer on the VTK interactor associated
     * with the widget. It binds the static callback function SceneWidget::keypressCallbackFunction to vtkCommand::KeyPressEvent.
     *
     * The callback enables keyboard-based navigation through simulation steps
     * (e.g., using the Up and Down arrow keys) and triggers visualization updates.
     *
     * @note The callback uses SetClientData(this) to pass the owning SceneWidget
     *       instance so that visualization state can be modified from within the static callback function. */
    void connectKeyboardCallback();

    /** @brief Connects the VTK mouse movement callback to the interactor.
     *
     * This method registers a mouse move observer on the VTK interactor tied to
     * the widget. It binds the static callback function SceneWidget::mouseCallbackFunction to vtkCommand::MouseMoveEvent.
     *
     * The callback captures live mouse movement, converts the VTK event position
     * into Qt widget coordinates, updates the last known world position using
     * a picker or DisplayToWorld transformation, and triggers tooltip updates.
     *
     * @note Similar to the keyboard callback, SetClientData(this) is used to allow
     *       the static callback function to interact with the SceneWidget instance. */
    void connectMouseCallback();

    /** @brief Trigger a render update of the VTK scene.
     * 
     * This helper marks the renderer as modified and requests a render pass.
     * Used throughout the class to update the display after modifications. */
    void triggerRenderUpdate();
    
    /** @brief Reset camera to default position and apply stored azimuth and elevation angles.
     * 
     * This method resets the camera to the default top-down view, then applies
     * the currently stored azimuth and elevation transformations. This ensures
     * consistent camera positioning when angles are modified. */
    void applyCameraAngles();
    
    /** @brief Load and update visualization data for the current step.
     * 
     * This helper reads stage state from files for the current step and refreshes
     * all VTK visualization elements (grid, lines, text). It's used when the step
     * number changes or when data needs to be refreshed. */
    void loadAndUpdateVisualizationForCurrentStep();
    
    /** @brief Prepare the stage for visualization with current node configuration.
     * 
     * This helper initializes the visualizer stage using the current nNodeX and nNodeY
     * settings from settingParameter. It's called during initialization and when
     * reloading data. */
    void prepareStageWithCurrentNodeConfiguration();
    
private:
    /** @brief Proxy for the scene widget visualizer
     *  This proxy provides access to the visualizer implementation
     *  and is responsible for updating the visualization when settings change. */
    std::unique_ptr<ISceneWidgetVisualizer> sceneWidgetVisualizerProxy;
    
    /// @brief Current setting parameter for the visualization.
    std::unique_ptr<SettingParameter> settingParameter;
    
    /// @brief Currently active model type
    ModelType currentModelType;
    
    /// @brief Current view mode (2D or 3D)
    ViewMode currentViewMode = ViewMode::Mode2D;
    
    /// @brief Current camera azimuth angle (cached to avoid recalculation)
    double cameraAzimuth{};
    
    /// @brief Current camera elevation angle (cached to avoid recalculation)
    double cameraElevation{};
    
    /** @brief Last recorded position in VTK world coordinates. */
    std::array<double, 3> m_lastWorldPos;
    
    /// @brief VTK renderer for the scene: This renderer is responsible for rendering the 3D scene.
    vtkNew<vtkRenderer> renderer;
    
    /** @brief Actor for the grid in the scene: This actor is responsible for rendering the grid in the scene.
     *  The grid provides information about what part was calculated by which node */
    vtkNew<vtkActor> gridActor;

    /// @brief Actor for load balancing lines: This actor is responsible for rendering the load balancing lines.
    vtkNew<vtkActor2D> actorBuildLine;
    
    /// @brief Text mapper for step display: This text mapper is responsible for rendering the step number in the scene.
    vtkNew<vtkTextMapper> singleLineTextStep;

    /// @brief Axes actor for showing coordinate system orientation
    vtkNew<vtkAxesActor> axesActor;
    
    /// @brief Orientation marker widget for displaying axes in corner
    vtkNew<vtkOrientationMarkerWidget> axesWidget;
    
    /// @brief 2D ruler axes for showing scale in 2D mode (X and Y axes)
    vtkNew<vtkAxisActor2D> rulerAxisX;
    vtkNew<vtkAxisActor2D> rulerAxisY;

    /** @brief Collection of line segments used for visualization.
     *
     * This vector stores all the line segments that are currently being rendered
     * in the scene. Each segment is from different node. */
    std::vector<Line> lines;
};
