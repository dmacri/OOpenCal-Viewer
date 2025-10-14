/** @file mainwindow.h
 * @brief Declaration of the MainWindow class - the main application window. */

#pragma once

#include <QMainWindow>
#include <QStyle>
#include "types.h"

namespace Ui {
class MainWindow;
}

class QPushButton;
class QActionGroup;


/** @class MainWindow
 * @brief The main application window class that manages the user interface.
 * 
 * This class handles the main window's user interface, including menu actions,
 * toolbars, and interaction with the 3D visualization widget. */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    void loadInitialConfiguration(const QString& configFileName);

private slots:
    void showAboutThisApplicationDialog();
    void showConfigDetailsDialog();
    void exportVideoDialog();
    void onStepNumberChanged();
    void onOpenConfigurationRequested();

    void onModelSelected();
    void onReloadDataRequested();

    void onPlayButtonClicked();
    void onStopButtonClicked();
    void onSkipForwardButtonClicked();
    void onSkipBackwardButtonClicked();
    void onBackButtonClicked();
    void onLeftButtonClicked();
    void onRightButtonClicked();

    void onUpdatePositionOnSlider(int value);

    void totalStepsNumberChanged(int totalStepsValue);
    void availableStepsLoadedFromConfigFile(std::vector<StepIndex> availableSteps);

private:
    enum class PlayingDirection
    {
        Forward = +1,
        Backward = -1
    };

    void playingRequested(PlayingDirection direction);

    void configureUIElements(const QString& configFileName);
    void setupConnections();
    void configureButtons();
    void configureButton(QPushButton* button, QStyle::StandardPixmap icon);
    void initializeSceneWidget(const QString& configFileName);
    void connectButtons();
    void connectSliders();
    void connectMenuActions();
    void showInputFilePathOnBarLabel(const QString& inputFilePath);

    void loadStrings();

    bool setPositionOnWidgets(int stepPosition, bool updateSlider=true);

    int totalSteps() const;

    void changeWhichButtonsAreEnabled();

    void recordVideoToFile(const QString& outputFilePath, int fps);

    void setWidgetsEnabledState(bool enabled);
    void enterNoConfigurationFileMode();
    
    void switchToModel(const QString& modelName);
    void createModelMenuActions();

    Ui::MainWindow* ui;
    
    QActionGroup* modelActionGroup;

    int currentStep;
    bool isPlaying = false;
    bool isBacking = false;

    QString noSelectionMessage;
    QString directorySelectionMessage;
    QString compilationSuccessfulMessage;
    QString compilationFailedMessage;
    QString deleteSuccessfulMessage;
    QString deleteFailedMessage;
};
