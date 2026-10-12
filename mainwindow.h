/** @file mainwindow.h
 *  @brief Declaration of the MainWindow class - the main application window. */

#pragma once

#include <QMainWindow>
#include <QStyle>
#include <QTimer>
#include <memory>
#include "core/types.h"

namespace Ui
{
class MainWindow;
}

class QPushButton;
class QSlider;
class QActionGroup;
class ReductionManager;
class Config;
class CommandLineParser;

/** @class MainWindow
 * @brief The main application window class that manages the user interface.
 *
 * This class handles the main window's user interface, including menu actions,
 * toolbars, and interaction with the 3D visualization widget. */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void applyCommandLineOptions(const CommandLineParser& cmdParser);
    void loadModelFromDirectory(const QString& modelDirectory, bool forceCompilation=false);
    
    /// @brief Load model data using an existing model from the system
    /// @param modelDirectory Directory containing Header.txt and data files
    /// @param existingModelName Name of the existing model to use for visualization
    void loadModelDataWithExistingModel(const QString& modelDirectory, const QString& existingModelName);

    /// @brief Get the name of the substate field currently used for 3D visualization
    /// @return Field name (e.g., "h", "z") or empty string if no 3D substate is active
    std::string getActiveSubstateFor3D() const
    {
        return activeSubstatesFor3D.empty() ? std::string{} : activeSubstatesFor3D.front();
    }

    /// @brief Get active 3D altitude substates in SubstatesDockWidget display order.
    const std::vector<std::string>& getActiveSubstatesFor3D() const
    {
        return activeSubstatesFor3D;
    }

private slots: // menu actions
    // File submenu
    void showConfigDetailsDialog();
    void exportVideoDialog();
    void onLoadPluginRequested();
    void onLoadModelFromDirectoryRequested();
    void onShowReductionRequested();

    // View submenu
    void on2DModeRequested();
    void on3DModeRequested();
    void onGridLinesToggled(bool checked);
    void syncGridLinesCheckbox();
    void onLineDetectionToggled(bool checked);
    void syncLineDetectionCheckbox();
    void onFlatSceneBackgroundToggled(bool checked);
    void syncFlatSceneBackgroundCheckbox();

    // Model submenu
    void onModelSelected();
    void onReloadDataRequested();

    // Settings submenu
    void onColorSettingsRequested();
    void onCompilationSettingsRequested();
    void onPerformanceSettingsRequested();
    void onCellRenderingToggled(bool checked);
    void syncCellRenderingCheckbox();
    void onPausePlaybackOnRotationToggled(bool checked);
    void onToolTipToggled(bool checked);
    void syncToolTipCheckbox();

    // Help submenu:
    void showAboutThisApplicationDialog();

    // other slots
    void onStepNumberChanged();

    void onRollChanged(int value);
    void onPitchChanged(int value);
    void onYawChanged(int value);
    void onResetCameraRequested();
    void onCameraOrientationChanged(double roll, double pitch, double yaw);
    void onCameraInteractionStarted();
    void onCameraInteractionFinished();
    void syncCameraSliders();
    void onCrossSectionControlsToggled(bool checked);
    void onSliceViewChanged(int viewIndex);
    void onSliceChanged(int fixedIndex);

    void onUse3dStateChanged(const std::string& fieldName, bool checked);
    void onUse3dSubstateOrderChanged();
    void onUseSubstatesColorringRequested(const std::vector<std::string>& fieldNames);
    void onDeactivateRequested();

    void onPlayButtonClicked();
    void onStopButtonClicked();
    void onSkipForwardButtonClicked();
    void onSkipBackwardButtonClicked();
    void onBackButtonClicked();
    void onLeftButtonClicked();
    void onRightButtonClicked();

    void onUpdateStepPositionOnSlider(StepIndex value);

    void totalStepsNumberChanged(StepIndex totalStepsValue);
    void availableStepsLoadedFromConfigFile(std::vector<StepIndex> availableSteps);

    void onPlaybackTimerTick();

private:
    enum class PlayingDirection
    {
        Forward = +1,
        Backward = -1
    };

    void playingRequested(PlayingDirection direction);

    /// @brief Stop playback for good (user request, end of data, ...). Cancels any pending auto-resume.
    void stopPlayback();

    /// @brief Temporarily pause playback while the camera is being moved (if enabled in Settings).
    /// Playback is resumed by resumePlaybackAfterCameraInteraction().
    void pausePlaybackForCameraInteraction();

    /// @brief Resume playback paused by pausePlaybackForCameraInteraction() (no-op otherwise).
    void resumePlaybackAfterCameraInteraction();

    /// @brief Called when a camera slider/spin box value changes; pauses playback during the change.
    /// @param slider The camera slider related to the change. If it is being dragged, playback is
    ///        resumed on release; otherwise (keyboard, spin box, wheel) after a short delay.
    void onCameraControlChanged(const QSlider* slider);

    void connectCameraSliderInteraction(QSlider* slider);

    void configureUIElements(const QString &configFileName);
    void setupConnections();
    void configureButtons();
    void configureButton(QPushButton *button, QStyle::StandardPixmap icon);
    void initializeSceneWidget(const QString &configFileName);
    void connectButtons();
    void connectSliders();
    void connectMenuActions();
    void showInputDirectoryOnBarLabel(const QString &configFilePath);

    void loadStrings();

    bool setPositionOnWidgets(StepIndex stepPosition, bool updateSlider = true);

    StepIndex totalSteps() const;

    void changeWhichButtonsAreEnabled();
    
    /// @brief Navigate to the nearest available step in the given direction
    /// @param direction Forward to go to next step, Backward to go to previous step
    /// @param stepsToMove Number of steps to move
    void navigateToNearestAvailableStep(PlayingDirection direction, StepIndex stepsToMove);

    void recordVideoToFile(const QString &outputFilePath, int fps);

    void setWidgetsEnabledState(bool enabled);
    void enterNoConfigurationFileMode();

    void updateSubstateDockeWidget();
    void switchToModel(const QString &modelName);
    void recreateModelMenuActions();
    void createViewModeActionGroup();
    void updateCameraControlsVisibility();
    void updateSliceControls(bool resetSelection = false);
    void synchronizeViewModeWithLoadedModel();

    /// @brief Clear all active substates (2D and 3D)
    void clearActiveSubstates();
    void apply3DSubstateSelection(const std::vector<std::string>& fieldNamesTopToBottom);

    // Recent directories management
    void addToRecentDirectories(const QString &directoryPath);
    QStringList loadRecentDirectories() const;
    void saveRecentDirectories(const QStringList &directories) const;
    QString getSmartDisplayNameForDirectory(const QString &directoryPath, const QStringList &allPaths) const;
    QString generateTooltipForDirectory(const QString &directoryPath) const;
    void updateRecentDirectoriesMenu();
    void onRecentDirectoryTriggered();
    
    /// @brief Initialize reduction manager for the current Header.txt.
    /// @param configFileName Path to Header.txt inside the selected simulation directory
    /// @param optionalConfig Optional pre-loaded Config object. If provided, avoids re-reading the file.
    ///                       If nullptr, the config will be read from configFileName.
    void initializeReductionManager(const QString& configFileName, std::shared_ptr<Config> optionalConfig = {});
    void updateReductionDisplay();
    void openConfigurationFile(const QString& configFileName, std::shared_ptr<Config> optionalConfig = {});

    /// @brief Handle missing step during playback
    /// @param targetStep The step that was attempted but not found
    /// @param direction The playback direction
    /// @return true if playback should continue, false if it should stop
    bool handleMissingStepDuringPlayback(StepIndex targetStep, PlayingDirection direction);
    
    /// @brief Find the nearest available step in the given direction
    /// @param targetStep The target step to search from
    /// @param direction Forward to find next step, Backward to find previous step
    /// @param outNextStep Output parameter: the found step (only valid if function returns true)
    /// @return true if a step was found in the given direction, false otherwise
    bool findNearestAvailableStep(StepIndex targetStep, PlayingDirection direction, StepIndex& outNextStep) const;

private:
    static constexpr int MAX_RECENT_DIRECTORIES = 10;
    Ui::MainWindow *ui;
    QTimer playbackTimer;

    /// @brief Single-shot timer resuming playback shortly after the last camera slider/spin box change
    QTimer cameraResumeTimer;

    /// @brief true if playback was running and got paused only because the camera is being moved
    bool playbackPausedForCamera = false;
    static constexpr int CAMERA_RESUME_DELAY_MS = 300;
    QActionGroup *modelActionGroup = nullptr;
    std::unique_ptr<ReductionManager> reductionManager;

    StepIndex currentStep;
    std::vector<StepIndex> availableSteps;

    /// @brief Names of substates used as stacked 3D altitude layers in dock display order.
    std::vector<std::string> activeSubstatesFor3D;

    // Playback state for timer-based playback
    PlayingDirection playbackDirection = PlayingDirection::Forward;
    bool shouldExitAfterPlayback = false;  ///< true if --autoPlay + --exitAfterLastStep

    QString noSelectionMessage;
    QString directorySelectionMessage;
    QString compilationSuccessfulMessage;
    QString compilationFailedMessage;
    QString deleteSuccessfulMessage;
    QString deleteFailedMessage;
};
