/** @file mainwindow.h
 *  @brief Declaration of the MainWindow class - the main application window. */

#pragma once

#include <QMainWindow>
#include <QStyle>
#include <QTimer>
#include <memory>
#include "utilities/types.h"

namespace Ui
{
class MainWindow;
}

class QPushButton;
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

    /// @param config is optional, if not provided: data will be read from file
    void openConfigurationFile(const QString& configFileName, std::shared_ptr<Config> optionalConfig={});
    void applyCommandLineOptions(const CommandLineParser& cmdParser);
    void loadModelFromDirectory(const QString& modelDirectory);

    void setSilentMode(bool newSilentMode)
    {
        silentMode = newSilentMode;
    }

private slots: // menu actions
    // File submenu
    void onOpenConfigurationRequested();
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

    // Model submenu
    void onModelSelected();
    void onReloadDataRequested();

    // Help submenu:
    void showAboutThisApplicationDialog();

    // other slots
    void onStepNumberChanged();
    void onColorSettingsRequested();

    void onAzimuthChanged(int value);
    void onElevationChanged(int value);
    void onCameraOrientationChanged(double azimuth, double elevation);
    void syncCameraSliders();

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

    void onRecentFileTriggered();
    void onPlaybackTimerTick();

private:
    enum class PlayingDirection
    {
        Forward = +1,
        Backward = -1
    };

    void playingRequested(PlayingDirection direction);

    void configureUIElements(const QString &configFileName);
    void setupConnections();
    void configureButtons();
    void configureButton(QPushButton *button, QStyle::StandardPixmap icon);
    void initializeSceneWidget(const QString &configFileName);
    void connectButtons();
    void connectSliders();
    void connectMenuActions();
    void showInputFilePathOnBarLabel(const QString &inputFilePath);

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

    // Recent files management
    void addToRecentFiles(const QString &filePath);
    QStringList loadRecentFiles() const;
    void saveRecentFiles(const QStringList &files) const;
    QString getSmartDisplayName(const QString &filePath, const QStringList &allPaths) const;
    QString generateTooltipForFile(const QString &filePath) const;
    void updateRecentFilesMenu();
    
    // Recent directories management
    void addToRecentDirectories(const QString &directoryPath);
    QStringList loadRecentDirectories() const;
    void saveRecentDirectories(const QStringList &directories) const;
    QString getSmartDisplayNameForDirectory(const QString &directoryPath, const QStringList &allPaths) const;
    QString generateTooltipForDirectory(const QString &directoryPath) const;
    void updateRecentDirectoriesMenu();
    void onRecentDirectoryTriggered();
    
    /// @brief Initialize reduction manager for the current configuration.
    /// @param configFileName Path to the configuration file
    /// @param optionalConfig Optional pre-loaded Config object. If provided, avoids re-reading the file.
    ///                       If nullptr, the config will be read from configFileName.
    void initializeReductionManager(const QString& configFileName, std::shared_ptr<Config> optionalConfig = {});
    void updateReductionDisplay();

private:
    static constexpr int MAX_RECENT_FILES = 10;
    Ui::MainWindow *ui;
    QTimer playbackTimer;
    QActionGroup *modelActionGroup = nullptr;
    std::unique_ptr<ReductionManager> reductionManager;

    StepIndex currentStep;
    std::vector<StepIndex> availableSteps;

    // Playback state for timer-based playback
    PlayingDirection playbackDirection = PlayingDirection::Forward;

    QString noSelectionMessage;
    QString directorySelectionMessage;
    QString compilationSuccessfulMessage;
    QString compilationFailedMessage;
    QString deleteSuccessfulMessage;
    QString deleteFailedMessage;

    // Command-line options
    bool silentMode = false;
};
