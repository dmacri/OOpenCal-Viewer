#include <iostream>
#include <utility> // std::to_underlying, which requires C++23
#include <filesystem>
#include <source_location>
#include <algorithm>
#include <QCommonStyle>
#include <QSettings>
#include <QDebug>
#include <QMessageBox>
#include <QTextStream>
#include <QColorDialog>
#include <QStandardPaths>
#include <QFileDialog>
#include <QActionGroup>
#include <QProgressDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "config/Config.h"
#include "config/ConfigConstants.h"
#include "utilities/CommandLineParser.h"
#include "utilities/CppModuleBuilder.h"
#include "utilities/ModelLoader.h"
#include "utilities/PluginLoader.h"
#include "utilities/ReductionManager.h"
#include "utilities/directoryConstants.h"
#include "visualiser/SettingParameter.h"
#include "visualiser/VideoExporter.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
#include "widgets/SceneWidget.h"
#include "widgets/SubstatesDockWidget.h"
#include "widgets/AboutDialog.h"
#include "widgets/ColorSettingsDialog.h"
#include "widgets/CompilationLogWidget.h"
#include "widgets/ConfigDetailsDialog.h"
#include "widgets/ReductionDialog.h"


namespace
{
constexpr StepIndex FIRST_STEP_NUMBER = 0;

inline std::string sourceFileParentDirectoryAbsolutePath(const std::source_location& location = std::source_location::current())
{
    const auto path2CurrentFile = std::filesystem::absolute(location.file_name());
    return path2CurrentFile.parent_path().string();
}

/** @brief Returns the starting directory path for OOpenCal models.
 *
 * This function determines the appropriate base directory for opening or locating
 * OOpenCal-related files, following these rules:
 *
 * 1. Checks if the environment variable `OOPENCAL_DIR` is set.
 *    - If yes, uses its value as the base directory.
 * 2. If not set, falls back to the CMake-defined macro `OOPENCAL_DIR`
 *    (if available at compile time).
 * 3. Validates that the directory actually exists.
 * 4. If the base directory exists, the function attempts to locate the subdirectory
 *    named `models/` inside it.
 *    - If the subdirectory exists, it is returned.
 *    - Otherwise, the base directory itself is returned.
 * 5. If neither environment variable nor CMake path exists or is invalid,
 *    an empty QString is returned, meaning the current working directory should be used.
 *
 * @note The returned path is absolute and normalized.
 *
 * @return QString
 *         - Absolute path to the `models/` directory if it exists.
 *         - Absolute path to the base OOpenCal directory if `models/` does not exist.
 *         - Empty QString if no valid directory could be determined.
 *
 * @example
 * @code
 * QString startPath = getOOpenCalStartPath();
 * if (startPath.isEmpty())
 * {
 *     startPath = QDir::currentPath(); // fallback to current working directory
 * }
 * qDebug() << "Start path:" << startPath;
 * @endcode */
QString getOOpenCalStartPath()
{
    QString baseDir;

    // Step 1: Try to get OOPENCAL_DIR from environment variable
    if (qEnvironmentVariableIsSet("OOPENCAL_DIR"))
    {
        baseDir = qEnvironmentVariable("OOPENCAL_DIR");
    }

#ifdef OOPENCAL_DIR
    // Step 2: If environment not set, use CMake-defined path
    if (baseDir.isEmpty())
    {
        baseDir = QString::fromLocal8Bit(OOPENCAL_DIR);
    }
#endif

    // Step 3: Verify that base directory exists
    QDir dir(baseDir);
    if (baseDir.isEmpty() || !dir.exists())
    {
        // Invalid or missing directory → return empty (current path)
        return QString();
    }

    // Step 4: Check if "OOpenCAL/models/" subdirectory exists
    QDir modelsDir(dir.filePath("OOpenCAL/models"));
    if (modelsDir.exists())
    {
        return modelsDir.absolutePath();
    }

    // Step 5: Return base directory if "models" does not exist
    return dir.absolutePath();
}

void updateMenu2ShowTheSelectedModeAsActive(const QString& modelName, QActionGroup *modelActionGroup)
{
    if (modelActionGroup)
    {
        bool modelFound = false;
        for (QAction* action : modelActionGroup->actions())
        {
            const bool isCurrent = action->text() == modelName;
            action->setChecked(isCurrent);
            modelFound |= isCurrent;
        }
        if (! modelFound)
        {
            std::cerr << "Model: '" << modelName.toStdString() << "' not found!" << std::endl;
        }
    }
}
} // namespace


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , playbackTimer{ this }
    , currentStep{ FIRST_STEP_NUMBER }
{
    ui->setupUi(this);
    setWindowTitle(QApplication::applicationName());

    // Initialize substate dock widget from UI
    ui->substatesDockWidget->initializeFromUI();
    ui->substatesDockWidget->hide();  // Hidden by default until configuration is loaded

    setupConnections();
    configureButtons();
    loadStrings();
    recreateModelMenuActions();
    createViewModeActionGroup();
    updateRecentFilesMenu();
    updateRecentDirectoriesMenu();

    enterNoConfigurationFileMode();
    updateSilentModeUi(isSilentModeEnabled());
}

void MainWindow::configureUIElements(const QString& configFileName)
{
    initializeSceneWidget(configFileName);
    showInputFilePathOnBarLabel(configFileName);

    setWidgetsEnabledState(true);
    changeWhichButtonsAreEnabled();
}

void MainWindow::setupConnections()
{
    connectMenuActions();

    connectButtons();
    connectSliders();

    connect(ui->positionSpinBox, &QSpinBox::editingFinished, this, &MainWindow::onStepNumberChanged);
    connect(ui->inputFilePathLabel, &ClickableLabel::doubleClicked, this, &MainWindow::showConfigDetailsDialog);
    connect(ui->sceneWidget, &SceneWidget::changedStepNumberWithKeyboardKeys, ui->updatePositionSlider, &QSlider::setValue);
    connect(ui->sceneWidget, &SceneWidget::totalNumberOfStepsReadFromConfigFile, this, &MainWindow::totalStepsNumberChanged);
    connect(ui->sceneWidget, &SceneWidget::availableStepsReadFromConfigFile, this, &MainWindow::availableStepsLoadedFromConfigFile);

    connect(&playbackTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTimerTick);
}

void MainWindow::connectMenuActions()
{
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAboutThisApplicationDialog);
    connect(ui->actionShow_config_details, &QAction::triggered, this, &MainWindow::showConfigDetailsDialog);
    connect(ui->actionExport_Video, &QAction::triggered, this, &MainWindow::exportVideoDialog);
    connect(ui->actionOpenConfiguration, &QAction::triggered, this, &MainWindow::onOpenConfigurationRequested);
    connect(ui->actionReloadData, &QAction::triggered, this, &MainWindow::onReloadDataRequested);
    connect(ui->actionLoadPlugin, &QAction::triggered, this, &MainWindow::onLoadPluginRequested);
    connect(ui->actionLoadModelFromDirectory, &QAction::triggered, this, &MainWindow::onLoadModelFromDirectoryRequested);
    connect(ui->actionColor_settings, &QAction::triggered, this, &MainWindow::onColorSettingsRequested);
    connect(ui->actionShow_reduction, &QAction::triggered, this, &MainWindow::onShowReductionRequested);

    // View mode actions
    connect(ui->action2DMode, &QAction::triggered, this, &MainWindow::on2DModeRequested);
    connect(ui->action3DMode, &QAction::triggered, this, &MainWindow::on3DModeRequested);
    connect(ui->actionGridLines, &QAction::triggered, this, &MainWindow::onGridLinesToggled);

    // Settings actions
    connect(ui->actionSilentMode, &QAction::toggled, this, &MainWindow::onSilentModeToggled);
    /// Model selection actions are connected dynamically in createModelMenuActions()
}

void MainWindow::setSilentMode(bool newSilentMode)
{
    if (! ui->actionSilentMode)
    {
        return;
    }

    if (ui->actionSilentMode->isChecked() == newSilentMode)
    {
        updateSilentModeUi(newSilentMode);
        return;
    }

    {
        QSignalBlocker blocker(ui->actionSilentMode);
        ui->actionSilentMode->setChecked(newSilentMode);
    }

    updateSilentModeUi(newSilentMode);
}

bool MainWindow::isSilentModeEnabled() const
{
    if (! ui->actionSilentMode)
    {
        return false;
    }
    return ui->actionSilentMode->isChecked();
}

void MainWindow::updateSilentModeUi(bool checked)
{
    if (!ui->actionSilentMode)
    {
        return;
    }

    const QString statusText = checked
        ? tr("Silent mode enabled: confirmation dialogs are suppressed.")
        : tr("Silent mode disabled: confirmation dialogs will be shown.");

    ui->actionSilentMode->setStatusTip(statusText);
    ui->actionSilentMode->setToolTip(statusText);
}

void MainWindow::configureButtons()
{
    configureButton(ui->rightButton, QStyle::SP_ArrowRight);
    configureButton(ui->leftButton, QStyle::SP_ArrowLeft);
    configureButton(ui->skipForwardButton, QStyle::SP_MediaSkipForward);
    configureButton(ui->skipBackwardButton, QStyle::SP_MediaSkipBackward);
    configureButton(ui->playButton, QStyle::SP_MediaPlay);
    configureButton(ui->stopButton, QStyle::SP_MediaStop);
    configureButton(ui->backButton, QStyle::SP_MediaSeekBackward);
}

void MainWindow::configureButton(QPushButton* button, QStyle::StandardPixmap icon)
{
    button->setIcon(QCommonStyle().standardIcon(icon));
    button->setIconSize(QSize(32, 32));
    button->setMinimumSize(QSize(36, 32));
    button->setStyleSheet(NULL);
}


void MainWindow::showInputFilePathOnBarLabel(const QString& inputFilePath)
{
    ui->inputFilePathLabel->setFileName(inputFilePath);
}

void MainWindow::initializeSceneWidget(const QString& configFileName)
{
    ui->sceneWidget->addVisualizer(configFileName.toStdString(), currentStep);
    ui->openConfigurationFileLabel->hide();
    ui->sceneWidget->setHidden(false);

    // Initialize substate dock widget
    if (ui->substatesDockWidget && ui->sceneWidget->getSettingParameter())
    {
        updateSubstateDockeWidget();

        ui->sceneWidget->setSubstatesDockWidget(ui->substatesDockWidget);
        // Keep dock widget hidden until user clicks on a cell
        ui->substatesDockWidget->hide();
    }
}

void MainWindow::availableStepsLoadedFromConfigFile(std::vector<StepIndex> availableSteps)
{
    // Store the list of available steps for intelligent step navigation
    this->availableSteps = availableSteps;
    
    // Update button states based on available steps
    changeWhichButtonsAreEnabled();
    
    const auto lastStepAvailableInAvailableSteps = std::ranges::contains(availableSteps, totalSteps());
    if (! lastStepAvailableInAvailableSteps && ! isSilentModeEnabled())
    {
        QMessageBox::warning(this,
                             tr("Number of steps mismatch"),
                             tr("Total number of steps from config file is %1, but last step number from index file is %2")
                                 .arg(totalSteps())
                                 .arg(availableSteps.back()));
    }
}

void MainWindow::totalStepsNumberChanged(StepIndex totalStepsValue)
{
    ui->totalStep->setText(QString("/") + QString::number(totalStepsValue));
    ui->updatePositionSlider->setMaximum(static_cast<int>(totalStepsValue));
    ui->positionSpinBox->setMaximum(static_cast<int>(totalStepsValue));
    ui->speedSpinBox->setMaximum(std::max(static_cast<int>(totalStepsValue), 1)); // std::max to avoid 0 as default step speed
}

StepIndex MainWindow::totalSteps() const
{
    return static_cast<StepIndex>(ui->updatePositionSlider->maximum());
}

void MainWindow::connectButtons()
{
    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::onStopButtonClicked);
    connect(ui->skipForwardButton, &QPushButton::clicked, this, &MainWindow::onSkipForwardButtonClicked);
    connect(ui->skipBackwardButton, &QPushButton::clicked, this, &MainWindow::onSkipBackwardButtonClicked);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::onBackButtonClicked);
    connect(ui->leftButton, &QPushButton::clicked, this, &MainWindow::onLeftButtonClicked);
    connect(ui->rightButton, &QPushButton::clicked, this, &MainWindow::onRightButtonClicked);
}

void MainWindow::connectSliders()
{
    connect(ui->updatePositionSlider, &QSlider::valueChanged, this, &MainWindow::onUpdateStepPositionOnSlider);

    // Camera control sliders
    connect(ui->azimuthSlider, &QSlider::valueChanged, this, &MainWindow::onAzimuthChanged);
    connect(ui->azimuthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), ui->azimuthSlider, &QSlider::setValue);
    connect(ui->azimuthSlider, &QSlider::valueChanged, ui->azimuthSpinBox, &QSpinBox::setValue);

    connect(ui->elevationSlider, &QSlider::valueChanged, this, &MainWindow::onElevationChanged);
    connect(ui->elevationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), ui->elevationSlider, &QSlider::setValue);
    connect(ui->elevationSlider, &QSlider::valueChanged, ui->elevationSpinBox, &QSpinBox::setValue);

    // Update sliders when camera changes (e.g., via mouse rotation in 3D mode)
    connect(ui->sceneWidget, &SceneWidget::cameraOrientationChanged, this, &MainWindow::onCameraOrientationChanged);
}

void MainWindow::loadStrings()
{
    noSelectionMessage = tr("Nessun elemento selezionato!");
    directorySelectionMessage = tr("Nessun elemento selezionato!");
    compilationSuccessfulMessage = tr("Compilation successful.");
    compilationFailedMessage = tr("Compilation failed.");
    deleteSuccessfulMessage = tr("Delete SceneWidgetVisualizerProxy.h successful.");
    deleteFailedMessage = tr("Delete SceneWidgetVisualizerProxy.h failed.");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showAboutThisApplicationDialog()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::showConfigDetailsDialog()
{
    const auto configFileName = ui->inputFilePathLabel->getFileName();
    if (configFileName.isEmpty())
    {
        QMessageBox::warning(this, tr("No Configuration"),
                           tr("No configuration file has been loaded."));
        return;
    }

    ConfigDetailsDialog dialog(configFileName.toStdString(), this);
    dialog.exec();
}

void MainWindow::exportVideoDialog()
{
    QString outputFilePath = QFileDialog::getSaveFileName(this,
                                                          tr("Export Video"),
                                                          /*dir=*/QString(),
                                                          tr("OGG Video Files (*.ogv);;All Files (*)"));

    if (outputFilePath.isEmpty())
    {
        return; // User cancelled
    }

    // Ensure .ogv extension
    if (! outputFilePath.endsWith(".ogv", Qt::CaseInsensitive))
    {
        outputFilePath += ".ogv";
    }

    const int fps = ui->speedSpinBox->value();

    // Record video
    try
    {
        recordVideoToFile(outputFilePath, fps);
        if (! isSilentModeEnabled())
        {
            QMessageBox::information(this, tr("Export Complete"),
                                     tr("Video exported successfully to:\n%1").arg(outputFilePath));
        }
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Export Failed"), tr("Failed to export video:\n%1").arg(e.what()));
    }
}

void MainWindow::recordVideoToFile(const QString& outputFilePath, int fps)
{
    // Save current state
    const auto originalStep = currentStep;
    const bool wasPlaying = playbackTimer.isActive();
    playbackTimer.stop();

    // Create progress dialog
    QProgressDialog progress(tr("Exporting video..."), tr("Cancel"), 1, static_cast<int>(totalSteps()), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    // Create video exporter
    VideoExporter exporter;

    // Define callback to update visualization for each step
    auto updateStepCallback = [this](StepIndex step)
    {
        currentStep = step;
        QSignalBlocker blockSlider(ui->updatePositionSlider);
        setPositionOnWidgets(currentStep);
        QApplication::processEvents();
    };

    // Define callback to report progress
    auto progressCallback = [&progress](StepIndex step, StepIndex total)
    {
        progress.setValue(static_cast<int>(step));
        progress.setLabelText(tr("Exporting video... Step %1 of %2").arg(step).arg(total));
    };

    // Define callback to check if cancelled
    auto cancelledCallback = [&progress]() -> bool
    {
        return progress.wasCanceled();
    };

    // Export video using VideoExporter
    exporter.exportVideo(ui->sceneWidget->renderWindow(),
                         outputFilePath,
                         fps,
                         totalSteps(),
                         updateStepCallback,
                         progressCallback,
                         cancelledCallback);

    // Restore original state
    currentStep = originalStep;
    setPositionOnWidgets(currentStep);
    if (wasPlaying)
    {
        playbackTimer.start(ui->sleepSpinBox->value());
    }

    progress.setValue(static_cast<int>(totalSteps()));
}
void MainWindow::playingRequested(PlayingDirection direction)
{
    if (playbackTimer.isActive() && playbackDirection == direction)
    {
        // Already playing in this direction, stop it
        playbackTimer.stop();
        return;
    }

    // Start playback in the specified direction
    playbackDirection = direction;

    // Start timer with interval from sleepSpinBox
    playbackTimer.start(ui->sleepSpinBox->value());
}

void MainWindow::onPlaybackTimerTick()
{
    // Calculate target step
    // Note: We need to use signed arithmetic to handle backward direction correctly
    // to avoid unsigned integer underflow
    const auto stepsToMove = static_cast<int>(ui->speedSpinBox->value());
    const auto directionValue = std::to_underlying(playbackDirection);
    const auto targetStepSigned = static_cast<int>(currentStep) + (stepsToMove * directionValue);
    
    // Clamp to valid range and convert back to unsigned
    const auto clampedStep = static_cast<StepIndex>(
        std::clamp(targetStepSigned, static_cast<int>(FIRST_STEP_NUMBER), static_cast<int>(totalSteps()))
    );
    
    // Check if we reached the end
    if ((playbackDirection == PlayingDirection::Forward && clampedStep >= totalSteps())
        || (playbackDirection == PlayingDirection::Backward && clampedStep <= FIRST_STEP_NUMBER))
    {
        playbackTimer.stop();
        return;
    }
    
    // Check if target step exists in available steps
    if (! std::ranges::contains(availableSteps, clampedStep))
    {
        // Handle missing step
        if (! handleMissingStepDuringPlayback(clampedStep, playbackDirection))
        {
            playbackTimer.stop();
            return;
        }
        // If handleMissingStepDuringPlayback returns true, currentStep was updated
    }
    else
    {
        currentStep = clampedStep;
    }

    // Update UI
    {
        QSignalBlocker blockSlider(ui->updatePositionSlider);
        if (bool changingPositionSuccess = setPositionOnWidgets(currentStep); ! changingPositionSuccess)
        {
            playbackTimer.stop();
            return;
        }
    }

    // Update timer interval in case sleepSpinBox changed
    playbackTimer.setInterval(ui->sleepSpinBox->value());
}

bool MainWindow::findNearestAvailableStep(StepIndex targetStep, PlayingDirection direction, StepIndex& outNextStep) const
{
    if (availableSteps.empty())
    {
        return false;
    }
    
    if (direction == PlayingDirection::Forward)
    {
        // Find the nearest available step after the target
        auto it = std::ranges::upper_bound(availableSteps, targetStep);
        
        if (it != availableSteps.end())
        {
            outNextStep = *it;
            return true;
        }
        
        return false;
    }
    else // PlayingDirection::Backward
    {
        // Find the nearest available step before the target
        auto it = std::ranges::lower_bound(availableSteps, targetStep);
        
        if (it != availableSteps.begin())
        {
            --it;
            outNextStep = *it;
            return true;
        }
        
        return false;
    }
}

bool MainWindow::handleMissingStepDuringPlayback(StepIndex targetStep, PlayingDirection direction)
{
    StepIndex nextStep;
    
    // Try to find the nearest available step in the given direction
    if (! findNearestAvailableStep(targetStep, direction, nextStep))
    {
        // No available step in this direction
        return false;
    }
    
    // In silent mode, just skip to the next available step
    if (isSilentModeEnabled())
    {
        currentStep = nextStep;
        return true;
    }
    
    // In normal mode, ask user what to do
    const QString nextStepText = (direction == PlayingDirection::Forward) 
        ? tr("next available step is %1").arg(nextStep)
        : tr("previous available step is %1").arg(nextStep);
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Missing Step During Playback"),
        tr("Step %1 is not available.\nThe %2.\n\nDo you want to continue playback skipping to the next available step, "
           "or stop playback at the current step?")
            .arg(targetStep)
            .arg(nextStepText),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );
    
    if (reply == QMessageBox::Yes)
    {
        currentStep = nextStep;
        return true;
    }
    else
    {
        return false; // Stop playback
    }
}

void MainWindow::onPlayButtonClicked()
{
    if (playbackTimer.isActive())
    {
        playbackTimer.stop();
    }
    else
    {
        playingRequested(PlayingDirection::Forward);
    }
}

void MainWindow::onStopButtonClicked()
{
    playbackTimer.stop();
    ui->playButton->setIcon(QCommonStyle().standardIcon(QStyle::SP_MediaPlay));
}

void MainWindow::onSkipForwardButtonClicked()
{
    if (availableSteps.empty())
    {
        return;
    }
    
    // Jump to the last available step
    currentStep = availableSteps.back();
    setPositionOnWidgets(currentStep);
}

void MainWindow::onSkipBackwardButtonClicked()
{
    if (availableSteps.empty())
    {
        return;
    }
    
    // Jump to the first available step
    currentStep = availableSteps.front();
    setPositionOnWidgets(currentStep);
}

void MainWindow::onBackButtonClicked()
{
    playingRequested(PlayingDirection::Backward);
}

void MainWindow::navigateToNearestAvailableStep(PlayingDirection direction, StepIndex stepsToMove)
{
    if (availableSteps.empty())
    {
        return;
    }
    
    // Note: We need to use signed arithmetic to handle backward direction correctly
    // to avoid unsigned integer underflow
    const auto directionValue = std::to_underlying(direction);
    const auto targetStepSigned = static_cast<int>(currentStep) + (static_cast<int>(stepsToMove) * directionValue);
    const auto targetStep = static_cast<StepIndex>(
        std::clamp(targetStepSigned, static_cast<int>(FIRST_STEP_NUMBER), static_cast<int>(totalSteps()))
    );
    
    // Try to go to the target step
    if (std::ranges::contains(availableSteps, targetStep))
    {
        currentStep = targetStep;
        setPositionOnWidgets(currentStep);
        return;
    }
    
    StepIndex nextStep;
    
    // Try to find the nearest available step in the given direction
    if (findNearestAvailableStep(targetStep, direction, nextStep))
    {
        // Found a step in the given direction
        if (direction == PlayingDirection::Forward)
        {
            std::cerr << "Warning: Step " << targetStep << " not available. "
                      << "Nearest next step is " << nextStep << std::endl;
        }
        else
        {
            std::cerr << "Warning: Step " << targetStep << " not available. "
                      << "Nearest previous step is " << nextStep << std::endl;
        }
    }
    else
    {
        // No step in the given direction, use boundary step
        if (direction == PlayingDirection::Forward)
        {
            nextStep = availableSteps.back();
            std::cerr << "Warning: Step " << targetStep << " not available. "
                      << "Going to last available step " << nextStep << std::endl;
        }
        else
        {
            nextStep = availableSteps.front();
            std::cerr << "Warning: Step " << targetStep << " not available. "
                      << "Going to first available step " << nextStep << std::endl;
        }
    }
    
    currentStep = nextStep;
    setPositionOnWidgets(currentStep);
}

void MainWindow::onLeftButtonClicked()
{
    const auto stepsPerClick = static_cast<StepIndex>(ui->speedSpinBox->value());
    navigateToNearestAvailableStep(PlayingDirection::Backward, stepsPerClick);
}

void MainWindow::onRightButtonClicked()
{
    const auto stepsPerClick = static_cast<StepIndex>(ui->speedSpinBox->value());
    navigateToNearestAvailableStep(PlayingDirection::Forward, stepsPerClick);
}

bool MainWindow::setPositionOnWidgets(StepIndex stepPosition, bool updateSlider)
{
    bool changingPositionSuccess = true;

    const auto stepBeforeTrying2ChangePosition = currentStep;
    try
    {
        ui->sceneWidget->selectedStepParameter(stepPosition);
        if (updateSlider)
        {
            QSignalBlocker sliderBlocker(ui->updatePositionSlider);
            ui->updatePositionSlider->setValue(static_cast<int>(stepPosition));
        }
        ui->positionSpinBox->setValue(static_cast<int>(stepPosition));
    }
    catch (const std::exception& e)
    {
        if (! isSilentModeEnabled())
        {
            QMessageBox::warning(this,
                                 "Changing position error",
                                 tr("It was impossible to change position to %1, because:\n").arg(stepPosition) + e.what());
        }
        currentStep = stepBeforeTrying2ChangePosition;
        changingPositionSuccess = false;
    }
    changeWhichButtonsAreEnabled();
    updateReductionDisplay();

    return changingPositionSuccess;
}

void MainWindow::changeWhichButtonsAreEnabled()
{
    // Check if there are available steps
    if (availableSteps.empty())
    {
        // No available steps, disable all navigation buttons
        ui->rightButton->setDisabled(true);
        ui->playButton->setDisabled(true);
        ui->leftButton->setDisabled(true);
        ui->backButton->setDisabled(true);
        ui->skipBackwardButton->setDisabled(true);
        ui->skipForwardButton->setDisabled(true);
        return;
    }
    
    // Check if current step is the last available step
    const bool isAtLastAvailableStep = (currentStep == availableSteps.back());
    ui->rightButton->setDisabled(isAtLastAvailableStep);
    ui->playButton->setDisabled(isAtLastAvailableStep);
    ui->skipForwardButton->setDisabled(isAtLastAvailableStep);
    
    // Check if current step is the first available step
    const bool isAtFirstAvailableStep = (currentStep == availableSteps.front());
    ui->leftButton->setDisabled(isAtFirstAvailableStep);
    ui->backButton->setDisabled(isAtFirstAvailableStep);
    ui->skipBackwardButton->setDisabled(isAtFirstAvailableStep);
}

void MainWindow::onStepNumberChanged()
{
    auto step = static_cast<StepIndex>(ui->positionSpinBox->value());
    if (step != currentStep)
    {
        currentStep = step;
        setPositionOnWidgets(currentStep);
    }
    updateReductionDisplay();
}

void MainWindow::onUpdateStepPositionOnSlider(StepIndex value)
{
    qDebug() << "Step is " << value;

    {
        QSignalBlocker blockSpinBox(ui->positionSpinBox);
        ui->positionSpinBox->setValue(static_cast<int>(value));
    }

    currentStep = value;

    setPositionOnWidgets(value, /*updateSlider=*/false);
}

void MainWindow::onModelSelected()
{
    const QAction* action = qobject_cast<QAction*>(sender());
    if (! action)
        return;

    QString modelName = action->text();
    switchToModel(modelName);
}

void MainWindow::switchToModel(const QString& modelName)
{
    try
    {
        // Verify that model is registered
        if (! SceneWidgetVisualizerFactory::isModelRegistered(modelName.toStdString()))
        {
            throw std::invalid_argument("Model not registered: " + modelName.toStdString());
        }

        ui->sceneWidget->switchModel(modelName.toStdString());

        // Update substate dock widget for new model
        updateSubstateDockeWidget();

        updateMenu2ShowTheSelectedModeAsActive(modelName, modelActionGroup);

        if (! isSilentModeEnabled())
        {
            QMessageBox::
                information(this,
                            tr("Model Changed"),
                            tr("Successfully switched to %1 model, but no data was reloaded from files.\n"
                               "Use 'Reload Data' (F5), or open another configuration file to load data files.\n"
                               "Notice: Model has to be compatible with configuration file, if not - the behaviour is undefined")
                                .arg(modelName));
        }
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Model Switch Failed"), tr("Failed to switch model:\n%1").arg(e.what()));

        // Revert checkbox state to current model
        const auto currentModel = QString::fromStdString(ui->sceneWidget->getCurrentModelName());
        updateMenu2ShowTheSelectedModeAsActive(currentModel, modelActionGroup);
    }
}
void MainWindow::updateSubstateDockeWidget()
{
    if (ui->substatesDockWidget && ui->sceneWidget->getSettingParameter())
    {
        auto settingParam = const_cast<SettingParameter*>(ui->sceneWidget->getSettingParameter());
        settingParam->initializeSubstateInfo();
        ui->substatesDockWidget->updateSubstates(settingParam);

        // Connect signal for 3D visualization request
        connect(ui->substatesDockWidget, &SubstatesDockWidget::use3rdDimensionRequested,
                this, &MainWindow::onUse3rdDimensionRequested);
    }
}

void MainWindow::onReloadDataRequested()
{
    try
    {
        ui->sceneWidget->reloadData();

        // Update substate dock widget after reload
        updateSubstateDockeWidget();

        if (! isSilentModeEnabled())
        {
            QMessageBox::information(this, tr("Data Reloaded"),
                                     tr("Data files successfully reloaded for model: %1")
                                        .arg(QString::fromStdString(ui->sceneWidget->getCurrentModelName())));
        }
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Reload Failed"),
                            tr("Failed to reload data:\n%1").arg(e.what()));
    }
}

void MainWindow::onOpenConfigurationRequested()
{
    // Open file dialog to select configuration file
    QString configFileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Configuration File"),
        getOOpenCalStartPath(),
        tr("Configuration Files (*.txt *.ini);;All Files (*)")
    );
    
    if (configFileName.isEmpty())
    {
        return; // User cancelled
    }

    openConfigurationFile(configFileName);
}

void MainWindow::openConfigurationFile(const QString& configFileName, std::shared_ptr<Config> optionalConfig)
{
    try
    {
        // Stop any ongoing playback
        playbackTimer.stop();

        if (bool isFirstConfiguration [[maybe_unused]] = ui->inputFilePathLabel->getFileName().isEmpty())
        {
            initializeSceneWidget(configFileName);
        }
        else
        {
            // Reload with new configuration
            ui->sceneWidget->loadNewConfiguration(configFileName.toStdString(), 0);

            // Update substate dock widget for new configuration
            updateSubstateDockeWidget();
        }

        // Initialize reduction manager for this configuration
        // If config is provided, use it; otherwise read from file
        initializeReductionManager(configFileName, optionalConfig);

        // Synchronize grid lines checkbox with current visibility state
        syncGridLinesCheckbox();

        // Update UI with new configuration
        showInputFilePathOnBarLabel(configFileName);

        // Reset to first step
        currentStep = 0;
        setPositionOnWidgets(currentStep);

        // Enable all widgets now that we have configuration
        setWidgetsEnabledState(true);

        // Add to recent files
        addToRecentFiles(configFileName);

        if (! isSilentModeEnabled())
        {
            QMessageBox::information(this,
                                     tr("Configuration Loaded"),
                                     tr("Successfully loaded configuration:\n%1").arg(configFileName));
        }
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Load Failed"), tr("Failed to load configuration:\n%1").arg(e.what()));
    }
}

void MainWindow::onColorSettingsRequested()
{
    auto* colorSettings = new ColorSettingsDialog(this);

    colorSettings->show();
}

void MainWindow::onSilentModeToggled(bool checked)
{
    setSilentMode(checked);
}

void MainWindow::enterNoConfigurationFileMode()
{
    ui->sceneWidget->setHidden(true);

    // Set UI to show no configuration loaded
    ui->inputFilePathLabel->setFileName("");
    ui->inputFilePathLabel->setText(tr("No configuration loaded - use File → Open Configuration"));

    totalStepsNumberChanged(0);
    currentStep = 0;
    ui->positionSpinBox->setValue(0);
    ui->updatePositionSlider->setValue(0);

    // Disable all playback and navigation widgets
    setWidgetsEnabledState(false);
}

void MainWindow::recreateModelMenuActions()
{
    const auto availableModels = SceneWidgetVisualizerFactory::getAvailableModels();

    if (availableModels.empty())
    {
        std::cerr << "Warning: No models available from factory!" << std::endl;
        return;
    }

    // Create action group for exclusive selection
    modelActionGroup = new QActionGroup(this);
    modelActionGroup->setExclusive(true);

    // Clear existing model actions from menu (if any from .ui file)
    ui->menuModel->clear();

    // Create action for each model
    for (const auto& modelName : availableModels)
    {
        QAction* action = new QAction(QString::fromStdString(modelName), this);
        action->setCheckable(true);

        // First model is checked by default
        if (modelName == availableModels[0])
        {
            action->setChecked(true);
        }

        modelActionGroup->addAction(action);
        ui->menuModel->addAction(action);

        connect(action, &QAction::triggered, this, &MainWindow::onModelSelected);
        cout << "+ Model: " << modelName << endl;
    }

    // Add separator and actions
    ui->menuModel->addSeparator();
    ui->menuModel->addAction(ui->actionLoadPlugin);
    ui->menuModel->addAction(ui->actionReloadData);

    std::cout << "Created " << availableModels.size() << " model menu actions" << std::endl;
}

void MainWindow::onLoadPluginRequested()
{
    QString pluginPath = QFileDialog::getOpenFileName(this,
                                                      tr("Load Plugin"),
                                                      "./plugins",
                                                      tr("Shared Libraries (*.so);;All Files (*)"));

    if (pluginPath.isEmpty())
    {
        return; // User cancelled
    }

    // Load the plugin
    PluginLoader& loader = PluginLoader::instance();
    if (loader.loadPlugin(pluginPath.toStdString()))
    {
        // Refresh the models menu to show new model
        recreateModelMenuActions();

        QMessageBox::information(this,
                                 tr("Plugin Loaded"),
                                 tr("Plugin loaded successfully!\n\nNew models are now available in the Model menu.\n\nPath: %1")
                                     .arg(pluginPath));
    }
    else
    {
        QMessageBox::critical(this,
                              tr("Plugin Load Failed"),
                              tr("Failed to load plugin:\n%1\n\nError: %2")
                                  .arg(pluginPath)
                                  .arg(QString::fromStdString(loader.getLastError())));
    }
}

void MainWindow::onLoadModelFromDirectoryRequested()
{
    QString modelDirectory = QFileDialog::getExistingDirectory(
        this,
        tr("Load Model from Directory"),
        getOOpenCalStartPath(),
        QFileDialog::ShowDirsOnly);

    if (modelDirectory.isEmpty())
    {
        return; // User cancelled
    }

    loadModelFromDirectory(modelDirectory);
}

void MainWindow::loadModelFromDirectory(const QString& modelDirectory)
{
    // Enable silent mode temporarily to suppress dialogs during loading
    const bool previousSilentMode = isSilentModeEnabled();
    setSilentMode(true);

    try
    {
        // The model directory should contain everything: Header.txt, model .h file, and data files
        namespace fs = std::filesystem;
        fs::path actualModelDir = fs::path(modelDirectory.toStdString());

        // Show progress dialog
        QProgressDialog progress(tr("Loading model from directory: ") + modelDirectory, tr("Cancel"), 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(/*ms=*/500);

        QApplication::processEvents();

        // Step 1: Load and compile model
        progress.setLabelText(tr("Loading model..."));
        QApplication::processEvents();

        ModelLoader loader;
        loader.getBuilder()->setProjectRootPath(sourceFileParentDirectoryAbsolutePath());
        const auto result = loader.loadModelFromDirectory(actualModelDir.string());

        if (! result.success)
        {
            progress.close();
            setSilentMode(previousSilentMode);

            // If compilation was attempted and failed, show detailed error dialog
            if (result.compilationResult.has_value())
            {
                CompilationLogWidget logWidget(this);
                logWidget.displayCompilationResult(result.compilationResult.value());
                logWidget.exec();
            }
            else
            {
                QMessageBox::critical(this, tr("Model Load Failed"),
                    tr("Failed to load model from:\n%1").arg(modelDirectory));
            }
            return;
        }

        // Step 2: Load compiled module
        progress.setLabelText(tr("Loading compiled module..."));
        QApplication::processEvents();

        PluginLoader& pluginLoader = PluginLoader::instance();
        if (! pluginLoader.loadPlugin(result.compiledModulePath, /*overridePlugin=*/true))
        {
            progress.close();
            setSilentMode(previousSilentMode);

            QMessageBox::critical(this,
                                  tr("Module Load Failed"),
                                  tr("Failed to load compiled module:\n%1\n\nError: %2")
                                      .arg(QString::fromStdString(result.compiledModulePath))
                                      .arg(QString::fromStdString(pluginLoader.getLastError())));
            return;
        }

        // Step 3: Switch to model
        progress.setLabelText(tr("Switching to model..."));
        QApplication::processEvents();

        recreateModelMenuActions();
        
        // Get the actual model name from the loaded plugin
        const auto& loadedPlugins = pluginLoader.getLoadedPlugins();
        std::string pluginModelName = result.outputFileName; // fallback to output file name
        if (!loadedPlugins.empty())
        {
            pluginModelName = loadedPlugins.back().name;
        }
        
        switchToModel(QString::fromStdString(pluginModelName));

        // Step 4: Load configuration from Header.txt
        progress.setLabelText(tr("Loading configuration..."));
        QApplication::processEvents();

        // Build path to Header.txt in the model directory
        fs::path headerPath = actualModelDir / DirectoryConstants::HEADER_FILE_NAME;
        
        if (!fs::exists(headerPath))
        {
            progress.close();
            setSilentMode(previousSilentMode);

            QMessageBox::critical(this, tr("Configuration Load Failed"),
                tr("Header.txt not found in model directory:\n%1").arg(QString::fromStdString(actualModelDir.string())));
            return;
        }

        // Open configuration using the config object from ModelLoader
        // This avoids reading the file twice
        openConfigurationFile(QString::fromStdString(headerPath.string()), result.config);

        // Add directory to recent directories list
        addToRecentDirectories(QString::fromStdString(actualModelDir.string()));

        progress.close();
        setSilentMode(previousSilentMode);

        // Show success message only if not in silent mode
        if (! isSilentModeEnabled())
        {
            QMessageBox::information(this,
                                     tr("Model Changed"),
                                     tr("Model '%1' loaded successfully from:\n%2\n\nConfiguration loaded and ready to use.")
                                         .arg(QString::fromStdString(pluginModelName))
                                         .arg(QString::fromStdString(actualModelDir.string())));
        }
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Error"),
            tr("An error occurred while loading the model:\n%1").arg(e.what()));
    }

    // Restore silent mode
    setSilentMode(previousSilentMode);
}

void MainWindow::createViewModeActionGroup()
{
    // Create action group for exclusive selection between 2D and 3D modes
    QActionGroup* viewModeGroup = new QActionGroup(this);
    viewModeGroup->setExclusive(true);

    viewModeGroup->addAction(ui->action2DMode);
    viewModeGroup->addAction(ui->action3DMode);

    // 2D mode is checked by default
    ui->action2DMode->setChecked(true);
}

void MainWindow::on2DModeRequested()
{
    ui->sceneWidget->setViewMode2D();
    updateCameraControlsVisibility();

    // Synchronize menu checkboxes - ensure only 2D mode is checked
    QSignalBlocker blocker2D(ui->action2DMode);
    QSignalBlocker blocker3D(ui->action3DMode);
    ui->action2DMode->setChecked(true);
    ui->action3DMode->setChecked(false);

    if (! isSilentModeEnabled())
    {
        QMessageBox::information(this,
                                 tr("View Mode Changed"),
                                 tr("Switched to 2D mode.\nCamera is now in top-down view with rotation disabled."));
    }
}

void MainWindow::on3DModeRequested()
{
    ui->sceneWidget->setViewMode3D();

    // Reset sliders to default position (0, 0) when entering 3D mode
    QSignalBlocker azimuthBlocker(ui->azimuthSlider);
    QSignalBlocker elevationBlocker(ui->elevationSlider);
    ui->azimuthSlider->setValue(0);
    ui->elevationSlider->setValue(0);

    // Reset camera angles to default
    ui->sceneWidget->setCameraAzimuth(0);
    ui->sceneWidget->setCameraElevation(0);

    updateCameraControlsVisibility();

    // Synchronize menu checkboxes - ensure only 3D mode is checked
    QSignalBlocker blocker2D(ui->action2DMode);
    QSignalBlocker blocker3D(ui->action3DMode);
    ui->action3DMode->setChecked(true);
    ui->action2DMode->setChecked(false);

    if (! isSilentModeEnabled())
    {
        QMessageBox::information(this,
                                 tr("View Mode Changed"),
                                 tr("Switched to 3D mode.\nYou can now rotate the camera using mouse or the sliders below."));
    }
}

void MainWindow::onGridLinesToggled(bool checked)
{
    ui->sceneWidget->setGridLinesVisible(checked);
}

void MainWindow::syncGridLinesCheckbox()
{
    // Synchronize the checkbox state with the actual grid lines visibility
    QSignalBlocker blocker(ui->actionGridLines);
    ui->actionGridLines->setChecked(ui->sceneWidget->getGridLinesVisible());
}

void MainWindow::updateCameraControlsVisibility()
{
    const bool is3DMode = (ui->sceneWidget->getViewMode() == ViewMode::Mode3D);
    ui->camera3DControlsWidget->setVisible(is3DMode);
}

void MainWindow::syncCameraSliders()
{
    // Sync sliders with current camera position
    const double azimuth = ui->sceneWidget->getCameraAzimuth();
    const double elevation = ui->sceneWidget->getCameraElevation();

    QSignalBlocker azimuthBlocker(ui->azimuthSlider);
    QSignalBlocker elevationBlocker(ui->elevationSlider);

    ui->azimuthSlider->setValue(static_cast<int>(azimuth));
    ui->elevationSlider->setValue(static_cast<int>(elevation));
}

void MainWindow::onAzimuthChanged(int value)
{
    ui->sceneWidget->setCameraAzimuth(value);
}

void MainWindow::onElevationChanged(int value)
{
    ui->sceneWidget->setCameraElevation(value);
}

void MainWindow::onCameraOrientationChanged(double azimuth, double elevation)
{
    // Block signals to avoid circular updates
    QSignalBlocker azimuthBlocker(ui->azimuthSlider);
    QSignalBlocker elevationBlocker(ui->elevationSlider);
    QSignalBlocker azimuthSpinBoxBlocker(ui->azimuthSpinBox);
    QSignalBlocker elevationSpinBoxBlocker(ui->elevationSpinBox);

    ui->azimuthSlider->setValue(static_cast<int>(azimuth));
    ui->azimuthSpinBox->setValue(static_cast<int>(azimuth));
    ui->elevationSlider->setValue(static_cast<int>(elevation));
    ui->elevationSpinBox->setValue(static_cast<int>(elevation));
}

// ============================================================================
// Recent Files Management
// ============================================================================

void MainWindow::updateRecentFilesMenu()
{
    ui->menuRecentFiles->clear();
    ui->menuRecentFiles->setToolTipsVisible(true);

    QStringList recentFiles = loadRecentFiles();

    // Remove files that don't exist anymore
    recentFiles.erase(std::remove_if(recentFiles.begin(),
                                     recentFiles.end(),
                                     [](const QString& path)
                                     {
                                         return ! QFileInfo::exists(path);
                                     }),
                      recentFiles.end());

    if (recentFiles.isEmpty())
    {
        QAction* noFilesAction = ui->menuRecentFiles->addAction(tr("No recent files"));
        noFilesAction->setEnabled(false);
        return;
    }

    // Save cleaned list
    saveRecentFiles(recentFiles);

    for (const QString& filePath : recentFiles)
    {
        QString displayName = getSmartDisplayName(filePath, recentFiles);

        // Get last opened time from QSettings
        QSettings settings;
        QString timeKey = QString("recentFiles/time_%1").arg(QString(filePath.toUtf8().toBase64()));
        QDateTime lastOpened = settings.value(timeKey, QDateTime::currentDateTime()).toDateTime();

        // Format: "filename [2024-10-20 15:30:25]"
        QString actionText = QString("%1 [%2]").arg(displayName).arg(lastOpened.toString("yyyy-MM-dd HH:mm:ss"));

        QAction* action = ui->menuRecentFiles->addAction(actionText);
        action->setData(filePath);
        action->setToolTip(generateTooltipForFile(filePath));

        connect(action, &QAction::triggered, this, &MainWindow::onRecentFileTriggered);
    }

    ui->menuRecentFiles->addSeparator();
    QAction* clearAction = ui->menuRecentFiles->addAction(tr("Clear Recent Files"));
    connect(clearAction,
            &QAction::triggered,
            this,
            [this]()
            {
                saveRecentFiles(QStringList());
                updateRecentFilesMenu();
            });
}

void MainWindow::addToRecentFiles(const QString& filePath)
{
    QStringList recentFiles = loadRecentFiles();

    // Remove if already exists (to move it to the top)
    recentFiles.removeAll(filePath);

    // Add to the beginning
    recentFiles.prepend(filePath);

    // Limit to MAX_RECENT_FILES
    while (recentFiles.size() > MAX_RECENT_FILES)
    {
        recentFiles.removeLast();
    }

    saveRecentFiles(recentFiles);

    // Store the timestamp when this file was opened
    QSettings settings;
    QString timeKey = QString("recentFiles/time_%1").arg(QString(filePath.toUtf8().toBase64()));
    settings.setValue(timeKey, QDateTime::currentDateTime());

    updateRecentFilesMenu();
}

QStringList MainWindow::loadRecentFiles() const
{
    QSettings settings;
    return settings.value("recentFiles/list").toStringList();
}

void MainWindow::saveRecentFiles(const QStringList& files) const
{
    QSettings settings;
    settings.setValue("recentFiles/list", files);
}

QString MainWindow::getSmartDisplayName(const QString& filePath, const QStringList& allPaths) const
{
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QDir fileDir = fileInfo.dir();

    // Start with parent/filename as default (depth = 1)
    QString displayName = fileDir.dirName() + "/" + fileName;

    // Count how many files have the same filename
    const int sameNameCount = std::count_if(allPaths.begin(), allPaths.end(), [&](const QString& otherPath)
        {
            return QFileInfo(otherPath).fileName() == fileName;
        });

    // If unique filename, return parent/filename
    if (sameNameCount == 1)
    {
        return displayName;
    }

    // Otherwise, check if parent/filename is already unique
    for (int depth = 1; depth <= 4; ++depth) // Try up to 4 parent directories
    {
        // Build display name with current depth
        QDir currentDir = fileInfo.dir();
        QString currentDisplayName = fileInfo.fileName();
        for (int d = 0; d < depth; ++d)
        {
            currentDisplayName = currentDir.dirName() + "/" + currentDisplayName;
            currentDir.cdUp();
        }

        // Check if this display name is unique among conflicting files
        bool isUnique = true;
        for (const QString& otherPath : allPaths)
        {
            if (otherPath == filePath)
                continue;

            QFileInfo otherInfo(otherPath);
            if (otherInfo.fileName() != fileName)
                continue; // Not a conflicting file

            // Build same-depth display name for other file
            QDir otherDir = otherInfo.dir();
            QString otherDisplayName = otherInfo.fileName();
            for (int d = 0; d < depth; ++d)
            {
                otherDisplayName = otherDir.dirName() + "/" + otherDisplayName;
                otherDir.cdUp();
            }

            if (otherDisplayName == currentDisplayName)
            {
                isUnique = false;
                break;
            }
        }

        if (isUnique)
        {
            return currentDisplayName;
        }
    }

    // If still not unique, return full path
    return filePath;
}

QString MainWindow::generateTooltipForFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);

    if (! fileInfo.exists())
    {
        return tr("File does not exist:\n%1").arg(filePath);
    }

    QString tooltip = QString("<b>%1</b><br/>").arg(tr("Full path:"));
    tooltip += QString("%1<br/><br/>").arg(filePath);
    
    tooltip += QString("<b>%1</b> %2<br/>")
        .arg(tr("Created:"))
        .arg(fileInfo.birthTime().toString("yyyy-MM-dd HH:mm:ss"));
    
    tooltip += QString("<b>%1</b> %2<br/><br/>")
        .arg(tr("Modified:"))
        .arg(fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
    
    // Try to read configuration parameters
    try
    {
        Config config(filePath.toStdString(), /*printWarnings=*/false);

        tooltip += QString("<b>%1</b><br/>").arg(tr("Configuration parameters:"));

        auto addParam = [&](const QString& category, const QString& paramName)
        {
            ConfigCategory* cat = config.getConfigCategory(category.toStdString(), /*ignoreCase=*/true);
            if (cat)
            {
                const ConfigParameter* param = cat->getConfigParameter(paramName.toStdString());
                if (param)
                {
                    tooltip += QString("&nbsp;&nbsp;• <b>%1:</b> %2<br/>")
                                   .arg(paramName)
                                   .arg(QString::fromStdString(param->getDefaultValue()));
                }
            }
        };

        addParam(ConfigConstants::CATEGORY_GENERAL, ConfigConstants::PARAM_NUMBER_STEPS);
        addParam(ConfigConstants::CATEGORY_GENERAL, ConfigConstants::PARAM_NUMBER_OF_ROWS);
        addParam(ConfigConstants::CATEGORY_GENERAL, ConfigConstants::PARAM_NUMBER_OF_COLUMNS);
        addParam(ConfigConstants::CATEGORY_DISTRIBUTED, ConfigConstants::PARAM_NUMBER_NODE_X);
        addParam(ConfigConstants::CATEGORY_DISTRIBUTED, ConfigConstants::PARAM_NUMBER_NODE_Y);
        addParam(ConfigConstants::CATEGORY_VISUALIZATION, ConfigConstants::PARAM_MODE);
        addParam(ConfigConstants::CATEGORY_VISUALIZATION, ConfigConstants::PARAM_SUBSTATES);
        addParam(ConfigConstants::CATEGORY_VISUALIZATION, ConfigConstants::PARAM_REDUCTION);
    }
    catch (const std::exception& e)
    {
        tooltip += QString("<br/><i>%1: %2</i>")
            .arg(tr("Could not read configuration"))
            .arg(e.what());
    }

    return tooltip;
}

void MainWindow::onRecentFileTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (! action)
        return;

    QString filePath = action->data().toString();

    if (! QFileInfo::exists(filePath))
    {
        QMessageBox::warning(this, tr("File Not Found"),
            tr("The file no longer exists:\n%1").arg(filePath));
        updateRecentFilesMenu();
        return;
    }

    openConfigurationFile(filePath);
}

void MainWindow::setWidgetsEnabledState(bool enabled)
{
    // Playback controls
    ui->playButton->setEnabled(enabled);
    ui->stopButton->setEnabled(enabled);
    ui->backButton->setEnabled(enabled);
    ui->leftButton->setEnabled(enabled);
    ui->rightButton->setEnabled(enabled);
    ui->skipForwardButton->setEnabled(enabled);
    ui->skipBackwardButton->setEnabled(enabled);

    // Position controls
    ui->updatePositionSlider->setEnabled(enabled);
    ui->positionSpinBox->setEnabled(enabled);

    // Speed/sleep controls (if they exist)
    if (ui->speedSpinBox)
        ui->speedSpinBox->setEnabled(enabled);
    if (ui->sleepSpinBox)
        ui->sleepSpinBox->setEnabled(enabled);

    // Menu actions - some should remain enabled
    // actionQuit - always enabled
    // actionAbout - always enabled
    // actionOpenConfiguration - always enabled
    // Model actions - always enabled (can switch before loading config)

    // These should be disabled without configuration:
    ui->actionShow_config_details->setEnabled(enabled);
    ui->actionExport_Video->setEnabled(enabled);
    ui->actionReloadData->setEnabled(enabled);

    // View mode actions should always be enabled
    ui->action2DMode->setEnabled(true);
    ui->action3DMode->setEnabled(true);

    changeWhichButtonsAreEnabled();
}


void MainWindow::applyCommandLineOptions(const CommandLineParser& cmdParser)
{
    // Store silent mode flag
    setSilentMode(cmdParser.isSilentMode());

    // Set starting model if specified
    if (cmdParser.getStartingModel())
    {
        const auto& modelName = cmdParser.getStartingModel().value();
        const QString modelQStr = QString::fromStdString(modelName);

        // Check if model is registered
        if (SceneWidgetVisualizerFactory::isModelRegistered(modelName))
        {
            // Switch to the model (switchToModel now handles menu update)
            switchToModel(modelQStr);

            // Reload data with the new model only if configuration was loaded
            if (cmdParser.getConfigFile())
            {
                try
                {
                    ui->sceneWidget->reloadData();
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Error reloading data with new model: " << e.what() << std::endl;
                }
            }
        }
        else
        {
            std::cerr << "Warning: Starting model not found: " << modelName << std::endl;
        }
    }

    // Set step if specified
    if (cmdParser.getStep())
    {
        const auto stepValue = static_cast<StepIndex>(cmdParser.getStep().value());
        if (stepValue <= totalSteps())
        {
            currentStep = stepValue;
            setPositionOnWidgets(currentStep);
        }
        else
        {
            std::cerr << "Warning: Invalid step value: " << stepValue << std::endl;
        }
    }

    // Handle generateImagePath - generate image and optionally exit
    if (cmdParser.getGenerateImagePath())
    {
        const auto& imagePath = cmdParser.getGenerateImagePath().value();
        try
        {
            // Generate image using Qt's screenshot functionality
            QPixmap screenshot = ui->sceneWidget->grab();
            if (screenshot.save(QString::fromStdString(imagePath)))
            {
                std::cout << "Image saved to: " << imagePath << std::endl;
            }
            else
            {
                std::cerr << "Error: Failed to save image to: " << imagePath << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        // Exit if requested - schedule it for after event loop processes
        if (cmdParser.shouldExitAfterLastStep())
        {
            QTimer::singleShot(100, this, &MainWindow::close);
        }
    }

    // Handle generateMoviePath - run all steps and optionally exit
    if (cmdParser.getGenerateMoviePath())
    {
        const auto& moviePath = cmdParser.getGenerateMoviePath().value();

        // Use existing video export functionality
        try
        {
            const int fps = ui->speedSpinBox->value();
            recordVideoToFile(QString::fromStdString(moviePath), fps);
            std::cout << "Movie saved to: " << moviePath << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error saving movie: " << e.what() << std::endl;
        }

        // Exit if requested
        if (cmdParser.shouldExitAfterLastStep())
        {
            QTimer::singleShot(100, this, &MainWindow::close);
        }
    }
}

void MainWindow::updateReductionDisplay()
{
    ui->reductionWidget->updateDisplay(currentStep);
}

void MainWindow::initializeReductionManager(const QString& configFileName, std::shared_ptr<Config> optionalConfig)
{
    ui->reductionWidget->setReductionManager(nullptr);

    // Get reduction configuration from SettingParameter
    const auto* settingParam = this->ui->sceneWidget->getSettingParameter();
    if (!settingParam || settingParam->reduction.empty())
    {
        // No reduction configured
        reductionManager.reset();
        ui->actionShow_reduction->setEnabled(false);
        return;
    }

    // Build path to reduction file
    namespace fs = std::filesystem;
    fs::path configPath(configFileName.toStdString());
    fs::path configDir = configPath.parent_path();
    
    // Get output filename from config
    try
    {
        if (! optionalConfig)
        {
            optionalConfig = std::make_shared<Config>(configFileName.toStdString());
        }
        ConfigCategory* generalContext = optionalConfig->getConfigCategory(ConfigConstants::CATEGORY_GENERAL);
        std::string outputFileNameFromCfg = generalContext->getConfigParameter(ConfigConstants::PARAM_OUTPUT_FILE_NAME)->getValue<std::string>();
        
        // Determine reduction file directory: check flat structure first, then nested
        fs::path reductionDir = configDir;
        fs::path reductionFilePath = reductionDir / (outputFileNameFromCfg + "-red.txt");
        
        // If not found in current directory, try Output/ subdirectory
        if (!fs::exists(reductionFilePath))
        {
            reductionDir = configDir / "Output";
            reductionFilePath = reductionDir / (outputFileNameFromCfg + "-red.txt");
        }
        
        // Create ReductionManager with the reduction file path and configuration
        reductionManager = std::make_unique<ReductionManager>(
            QString::fromStdString(reductionFilePath.string()),
            QString::fromStdString(settingParam->reduction)
        );
        ui->reductionWidget->setReductionManager(reductionManager.get());
        
        // Enable "Show Reduction" action if reduction data is available
        ui->actionShow_reduction->setEnabled(reductionManager && reductionManager->isAvailable());
    }
    catch (const std::exception& e)
    {
        reductionManager.reset();
        ui->actionShow_reduction->setEnabled(false);
        if (ui->actionSilentMode->isChecked())
            std::cerr << "Error initializing ReductionManager: " << e.what() << std::endl;
        else
            QMessageBox::warning(this, tr("Reduction initialization error"), tr("Details: ") + e.what());
    }
}

void MainWindow::onShowReductionRequested()
{
    // Check if reduction manager is available
    if (!reductionManager || !reductionManager->isAvailable())
    {
        QMessageBox::warning(this, tr("No Reduction Data"),
            tr("Reduction data is not available for the current configuration."));
        return;
    }

    // Get reduction data for current step
    ReductionData reductionData = reductionManager->getReductionForStep(currentStep);
    if (reductionData.values.empty())
    {
        QMessageBox::information(this, tr("No Data"),
            tr("No reduction data available for step %1").arg(currentStep));
        return;
    }

    // Show reduction dialog
    ReductionDialog dialog(reductionData.values, currentStep, this);
    dialog.exec();
}

void MainWindow::addToRecentDirectories(const QString& directoryPath)
{
    // Load existing recent directories list
    QStringList recentDirectories = loadRecentDirectories();

    // Remove if already exists (to move it to the top)
    recentDirectories.removeAll(directoryPath);

    // Add to the beginning
    recentDirectories.prepend(directoryPath);

    // Limit to MAX_RECENT_FILES
    while (recentDirectories.size() > MAX_RECENT_FILES)
    {
        recentDirectories.removeLast();
    }

    // Save the updated list
    saveRecentDirectories(recentDirectories);

    // Store the timestamp when this directory was opened
    QSettings settings;
    QString timeKey = QString("recentDirectories/time_%1").arg(QString(directoryPath.toUtf8().toBase64()));
    settings.setValue(timeKey, QDateTime::currentDateTime());

    // Update the menu to reflect the new list
    updateRecentDirectoriesMenu();
}

QStringList MainWindow::loadRecentDirectories() const
{
    QSettings settings;
    return settings.value("recentDirectories/list").toStringList();
}

void MainWindow::saveRecentDirectories(const QStringList& directories) const
{
    QSettings settings;
    settings.setValue("recentDirectories/list", directories);
}

QString MainWindow::getSmartDisplayNameForDirectory(const QString& directoryPath, const QStringList& allPaths) const
{
    QFileInfo dirInfo(directoryPath);
    
    // Start with depth = 1 (parent/dirname)
    int depth = 1;
    
    while (depth <= 4)
    {
        // Build display name for this directory at current depth
        QString displayName = dirInfo.fileName();
        QDir currentDir = dirInfo.dir();
        
        for (int d = 0; d < depth; ++d)
        {
            displayName = currentDir.dirName() + "/" + displayName;
            currentDir.cdUp();
        }
        
        // Check if this display name is unique among all paths at this depth
        bool isUnique = true;
        
        for (const QString& otherPath : allPaths)
        {
            if (otherPath == directoryPath)
                continue;
            
            // Build the same display name for the other path at current depth
            QFileInfo otherInfo(otherPath);
            QString otherDisplayName = otherInfo.fileName();
            QDir otherDir = otherInfo.dir();
            
            for (int d = 0; d < depth; ++d)
            {
                otherDisplayName = otherDir.dirName() + "/" + otherDisplayName;
                otherDir.cdUp();
            }
            
            // If display names match, this depth is not sufficient
            if (otherDisplayName == displayName)
            {
                isUnique = false;
                break;
            }
        }
        
        // If unique at this depth, return it
        if (isUnique)
        {
            return displayName;
        }
        
        // Otherwise, try next depth
        depth++;
    }
    
    // Fallback to full path if still not unique
    return directoryPath;
}

QString MainWindow::generateTooltipForDirectory(const QString& directoryPath) const
{
    QFileInfo dirInfo(directoryPath);

    // Check if directory exists
    if (!dirInfo.exists() || !dirInfo.isDir())
    {
        return tr("Directory does not exist:\n%1").arg(directoryPath);
    }

    // Check if Header.txt exists in the directory
    QString headerPath = QDir(directoryPath).filePath(DirectoryConstants::HEADER_FILE_NAME);
    if (!QFileInfo::exists(headerPath))
    {
        return tr("Directory is empty or does not contain %2:\n%1").arg(directoryPath, DirectoryConstants::HEADER_FILE_NAME);
    }

    QString tooltip = QString("<b>%1</b><br/>").arg(tr("Full path:"));
    tooltip += QString("%1<br/><br/>").arg(directoryPath);

    tooltip += QString("<b>%1</b> %2<br/>")
        .arg(tr("Created:"))
        .arg(dirInfo.birthTime().toString("yyyy-MM-dd HH:mm:ss"));

    tooltip += QString("<b>%1</b> %2<br/><br/>")
        .arg(tr("Modified:"))
        .arg(dirInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss"));

    // Try to read configuration parameters from Header.txt
    try
    {
        Config config(headerPath.toStdString(), /*printWarnings=*/false);

        tooltip += QString("<b>%1</b><br/>").arg(tr("Configuration parameters:"));

        auto addParam = [&](const QString& category, const QString& paramName)
        {
            ConfigCategory* cat = config.getConfigCategory(category.toStdString(), /*ignoreCase=*/true);
            if (cat)
            {
                const ConfigParameter* param = cat->getConfigParameter(paramName.toStdString());
                if (param)
                {
                    tooltip += QString("&nbsp;&nbsp;• <b>%1:</b> %2<br/>")
                                   .arg(paramName)
                                   .arg(QString::fromStdString(param->getDefaultValue()));
                }
            }
        };

        addParam(ConfigConstants::CATEGORY_GENERAL, ConfigConstants::PARAM_NUMBER_STEPS);
        addParam(ConfigConstants::CATEGORY_GENERAL, ConfigConstants::PARAM_NUMBER_OF_ROWS);
        addParam(ConfigConstants::CATEGORY_GENERAL, ConfigConstants::PARAM_NUMBER_OF_COLUMNS);
        addParam(ConfigConstants::CATEGORY_DISTRIBUTED, ConfigConstants::PARAM_NUMBER_NODE_X);
        addParam(ConfigConstants::CATEGORY_DISTRIBUTED, ConfigConstants::PARAM_NUMBER_NODE_Y);
        addParam(ConfigConstants::CATEGORY_VISUALIZATION, ConfigConstants::PARAM_MODE);
        addParam(ConfigConstants::CATEGORY_VISUALIZATION, ConfigConstants::PARAM_SUBSTATES);
        addParam(ConfigConstants::CATEGORY_VISUALIZATION, ConfigConstants::PARAM_REDUCTION);
    }
    catch (const std::exception& e)
    {
        tooltip += QString("<br/><i>%1: %2</i>")
            .arg(tr("Could not read configuration"))
            .arg(e.what());
    }

    return tooltip;
}

void MainWindow::updateRecentDirectoriesMenu()
{
    // Clear existing menu items
    ui->menuRecentDirectories->clear();
    ui->menuRecentDirectories->setToolTipsVisible(true);

    // Load recent directories list
    QStringList recentDirectories = loadRecentDirectories();

    // Filter out non-existent directories and those without Header.txt
    QStringList validDirectories;
    for (const QString& dirPath : recentDirectories)
    {
        QFileInfo dirInfo(dirPath);
        if (dirInfo.exists() && dirInfo.isDir())
        {
            // Check if Header.txt exists in the directory
            QString headerPath = QDir(dirPath).filePath(DirectoryConstants::HEADER_FILE_NAME);
            if (QFileInfo::exists(headerPath))
            {
                validDirectories.append(dirPath);
            }
        }
    }

    // If there are invalid directories, update the saved list
    if (validDirectories.size() != recentDirectories.size())
    {
        saveRecentDirectories(validDirectories);
        recentDirectories = validDirectories;
    }

    // Add menu items for each valid recent directory
    for (const QString& dirPath : recentDirectories)
    {
        // Get last opened time from QSettings
        QSettings settings;
        QString timeKey = QString("recentDirectories/time_%1").arg(QString(dirPath.toUtf8().toBase64()));
        QDateTime lastOpened = settings.value(timeKey, QDateTime::currentDateTime()).toDateTime();

        // Use smart display name (handles duplicate directory names)
        QString displayName = getSmartDisplayNameForDirectory(dirPath, validDirectories);

        // Format: "directory_name [2024-10-20 15:30:25]"
        QString actionText = QString("%1 [%2]").arg(displayName).arg(lastOpened.toString("yyyy-MM-dd HH:mm:ss"));

        QAction* action = ui->menuRecentDirectories->addAction(actionText);
        action->setData(dirPath);
        action->setToolTip(generateTooltipForDirectory(dirPath));

        connect(action, &QAction::triggered, this, &MainWindow::onRecentDirectoryTriggered);
    }

    // Add separator and clear action
    ui->menuRecentDirectories->addSeparator();
    QAction* clearAction = ui->menuRecentDirectories->addAction(tr("Clear Recent Directories"));
    connect(clearAction,
            &QAction::triggered,
            this,
            [this]()
            {
                saveRecentDirectories(QStringList());
                updateRecentDirectoriesMenu();
            });
}

void MainWindow::onRecentDirectoryTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (! action)
        return;

    QString directoryPath = action->data().toString();

    // Check if directory still exists
    if (! QFileInfo(directoryPath).exists())
    {
        QMessageBox::warning(this, tr("Directory Not Found"),
            tr("The directory no longer exists:\n%1").arg(directoryPath));
        updateRecentDirectoriesMenu();
        return;
    }

    // Check if Header.txt exists
    QString headerPath = QDir(directoryPath).filePath(DirectoryConstants::HEADER_FILE_NAME);
    if (! QFileInfo::exists(headerPath))
    {
        QMessageBox::warning(this, tr("Invalid Directory"),
            tr("The directory does not contain Header.txt:\n%1").arg(directoryPath));
        updateRecentDirectoriesMenu();
        return;
    }

    // Load the model from the directory
    loadModelFromDirectory(directoryPath);
}

void MainWindow::onUse3rdDimensionRequested(const std::string& fieldName)
{
    // Store the active substate for 3D visualization in MainWindow
    activeSubstateFor3D = fieldName;

    // Also set it in SceneWidget so it knows which substate to use for 3D
    ui->sceneWidget->setActiveSubstateFor3D(fieldName);

    // Switch to 3D mode
    on3DModeRequested();

    // Refresh the visualization with the new substate for the current step
    ui->sceneWidget->selectedStepParameter(currentStep);
}
