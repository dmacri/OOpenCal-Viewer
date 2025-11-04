/** @file mainwindow.h
 *  @brief Declaration of the MainWindow class - the main application window. */

#pragma once

#include <QMainWindow>
#include <QStyle>

#include "utilities/types.h"

namespace Ui
{
class MainWindow;
}

class QPushButton;
class QActionGroup;
class QTimer;

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

    void loadInitialConfiguration(const QString &configFileName);
    void applyCommandLineOptions(class CommandLineParser &cmdParser);

    void setSilentMode(bool newSilentMode)
    {
        silentMode = newSilentMode;
    }

private slots:
    void showAboutThisApplicationDialog();
    void showConfigDetailsDialog();
    void exportVideoDialog();
    void onStepNumberChanged();
    void onOpenConfigurationRequested();
    void onColorSettingsRequested();

    void on2DModeRequested();
    void on3DModeRequested();
    void onAzimuthChanged(int value);
    void onElevationChanged(int value);
    void onCameraOrientationChanged(double azimuth, double elevation);
    void syncCameraSliders();

    void onModelSelected();
    void onReloadDataRequested();
    void onLoadPluginRequested();
    void onLoadModelFromDirectoryRequested();

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

    void recordVideoToFile(const QString &outputFilePath, int fps);

    void setWidgetsEnabledState(bool enabled);
    void enterNoConfigurationFileMode();

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
    void openConfigurationFile(const QString& configFileName);
    void loadModelFromDirectory(const QString& modelDirectory);

    static constexpr int MAX_RECENT_FILES = 10;

    Ui::MainWindow *ui;
    QTimer *playbackTimer;
    QActionGroup *modelActionGroup = nullptr;

    StepIndex currentStep;

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
