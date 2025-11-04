#include <utility> // std::to_underlying, which requires C++23
#include <filesystem>
#include <source_location>
#include <QCommonStyle>
#include <QSettings>
#include <QDebug>
#include <QMessageBox>
#include <QTextStream>
#include <iostream>
#include <QColorDialog>
#include <QStandardPaths>
#include <QFileDialog>
#include <QActionGroup>
#include <QProgressDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QTimer>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utilities/PluginLoader.h"
#include "utilities/CommandLineParser.h"
#include "utilities/ModelLoader.h"
#include "utilities/CppModuleBuilder.h"
#include "widgets/ConfigDetailsDialog.h"
#include "widgets/ColorSettingsDialog.h"
#include "widgets/AboutDialog.h"
#include "widgets/CompilationLogWidget.h"
#include "visualiser/VideoExporter.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
#include "config/Config.h"


namespace
{
constexpr StepIndex FIRST_STEP_NUMBER = 0;

inline std::string sourceFileParentDirectoryAbsolutePath(const std::source_location& location = std::source_location::current())
{
    const auto path2CurrentFile = std::filesystem::absolute(location.file_name());
    return path2CurrentFile.parent_path().string();
}
} // namespace


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , modelActionGroup(nullptr)
    , playbackTimer(new QTimer(this))
    , currentStep{ FIRST_STEP_NUMBER }
{
    ui->setupUi(this);
    setWindowTitle(QApplication::applicationName());

    setupConnections();
    configureButtons();
    loadStrings();
    recreateModelMenuActions();
    createViewModeActionGroup();
    updateRecentFilesMenu();

    enterNoConfigurationFileMode();
}

void MainWindow::loadInitialConfiguration(const QString& configFileName)
{
    if (! configFileName.isEmpty())
    {
        configureUIElements(configFileName);
        addToRecentFiles(configFileName);
    }
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

    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTimerTick);
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

    // View mode actions
    connect(ui->action2DMode, &QAction::triggered, this, &MainWindow::on2DModeRequested);
    connect(ui->action3DMode, &QAction::triggered, this, &MainWindow::on3DModeRequested);

    /// Model selection actions are connected dynamically in createModelMenuActions()
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
}

void MainWindow::availableStepsLoadedFromConfigFile(std::vector<StepIndex> availableSteps)
{
    const auto lastStepAvailableInAvailableSteps = std::ranges::contains(availableSteps, totalSteps());
    if (! lastStepAvailableInAvailableSteps && ! silentMode)
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
        if (! silentMode)
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
    const bool wasPlaying = playbackTimer->isActive();
    playbackTimer->stop();

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
        playbackTimer->start(ui->sleepSpinBox->value());
    }

    progress.setValue(static_cast<int>(totalSteps()));
}
void MainWindow::playingRequested(PlayingDirection direction)
{
    if (playbackTimer->isActive() && playbackDirection == direction)
    {
        // Already playing in this direction, stop it
        playbackTimer->stop();
        return;
    }

    // Start playback in the specified direction
    playbackDirection = direction;

    // Start timer with interval from sleepSpinBox
    playbackTimer->start(ui->sleepSpinBox->value());
}

void MainWindow::onPlaybackTimerTick()
{
    // Update current step
    currentStep += static_cast<StepIndex>(std::to_underlying(playbackDirection) * ui->speedSpinBox->value());
    currentStep = std::clamp<StepIndex>(currentStep, FIRST_STEP_NUMBER, totalSteps());

    // Update UI
    {
        QSignalBlocker blockSlider(ui->updatePositionSlider);
        if (bool changingPositionSuccess = setPositionOnWidgets(currentStep); ! changingPositionSuccess)
        {
            playbackTimer->stop();
            return;
        }
    }

    // Check if we reached the end
    if ((playbackDirection == PlayingDirection::Forward && currentStep >= totalSteps())
        || (playbackDirection == PlayingDirection::Backward && currentStep <= FIRST_STEP_NUMBER))
    {
        playbackTimer->stop();
    }

    // Update timer interval in case sleepSpinBox changed
    playbackTimer->setInterval(ui->sleepSpinBox->value());
}


void MainWindow::onPlayButtonClicked()
{
    if (playbackTimer->isActive())
    {
        playbackTimer->stop();
    }
    else
    {
        playingRequested(PlayingDirection::Forward);
    }
}

void MainWindow::onStopButtonClicked()
{
    playbackTimer->stop();
    ui->playButton->setIcon(QCommonStyle().standardIcon(QStyle::SP_MediaPlay));
}

void MainWindow::onSkipForwardButtonClicked()
{
    currentStep = totalSteps();
    setPositionOnWidgets(currentStep);
}

void MainWindow::onSkipBackwardButtonClicked()
{
    currentStep = FIRST_STEP_NUMBER;
    setPositionOnWidgets(currentStep);
}

void MainWindow::onBackButtonClicked()
{
    playingRequested(PlayingDirection::Backward);
}

void MainWindow::onLeftButtonClicked()
{
    const auto stepsPerClick = static_cast<StepIndex>(ui->speedSpinBox->value());
    currentStep = std::max(currentStep - stepsPerClick, FIRST_STEP_NUMBER);
    setPositionOnWidgets(currentStep);
}

void MainWindow::onRightButtonClicked()
{
    const auto stepsPerClick = static_cast<StepIndex>(ui->speedSpinBox->value());
    currentStep = std::min(currentStep + stepsPerClick, totalSteps());
    setPositionOnWidgets(currentStep);
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
        if (! silentMode)
        {
            QMessageBox::warning(this,
                                 "Changing position error",
                                 tr("It was impossible to change position to %1, because:\n").arg(stepPosition) + e.what());
        }
        currentStep = stepBeforeTrying2ChangePosition;
        changingPositionSuccess = false;
    }
    changeWhichButtonsAreEnabled();

    return changingPositionSuccess;
}

void MainWindow::changeWhichButtonsAreEnabled()
{
    ui->rightButton->setDisabled(currentStep == totalSteps());
    ui->playButton->setDisabled(currentStep == totalSteps());
    ui->leftButton->setDisabled(currentStep <= FIRST_STEP_NUMBER);
    ui->backButton->setDisabled(currentStep <= FIRST_STEP_NUMBER);
    ui->skipBackwardButton->setDisabled(currentStep <= FIRST_STEP_NUMBER);
    ui->skipForwardButton->setDisabled(currentStep == totalSteps());
}

void MainWindow::onStepNumberChanged()
{
    auto step = static_cast<StepIndex>(ui->positionSpinBox->value());
    if (step != currentStep)
    {
        currentStep = step;
        setPositionOnWidgets(currentStep);
    }
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
    QAction* action = qobject_cast<QAction*>(sender());
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

        if (! silentMode)
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
        for (QAction* action : modelActionGroup->actions())
        {
            action->setChecked(action->text() == currentModel);
        }
    }
}

void MainWindow::onReloadDataRequested()
{
    try
    {
        ui->sceneWidget->reloadData();

        if (! silentMode)
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
        QString(), // Start in current directory
        tr("Configuration Files (*.txt *.ini);;All Files (*)")
    );
    
    if (configFileName.isEmpty())
    {
        return; // User cancelled
    }

    openConfigurationFile(configFileName);
}

void MainWindow::openConfigurationFile(const QString& configFileName)
{
    try
    {
        // Stop any ongoing playback
        playbackTimer->stop();

        if (bool isFirstConfiguration [[maybe_unused]] = ui->inputFilePathLabel->getFileName().isEmpty())
        {
            initializeSceneWidget(configFileName);
        }
        else
        {
            // Reload with new configuration
            ui->sceneWidget->loadNewConfiguration(configFileName.toStdString(), 0);
        }

        // Update UI with new configuration
        showInputFilePathOnBarLabel(configFileName);

        // Reset to first step
        currentStep = 0;
        setPositionOnWidgets(currentStep);

        // Enable all widgets now that we have configuration
        setWidgetsEnabledState(true);

        // Add to recent files
        addToRecentFiles(configFileName);

        if (! silentMode)
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
    QString modelDirectory = QFileDialog::getExistingDirectory(this,
                                                               tr("Load Model from Directory"),
                                                               ".",
                                                               QFileDialog::ShowDirsOnly);

    if (modelDirectory.isEmpty())
    {
        return; // User cancelled
    }

    loadModelFromDirectory(modelDirectory);
}

void MainWindow::loadModelFromDirectory(const QString& modelDirectory)
{
    try
    {
        // Show progress dialog
        QProgressDialog progress(tr("Loading model from directory..."), tr("Cancel"), 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(500);

        QApplication::processEvents();

        // Use ModelLoader to handle all loading logic
        ModelLoader loader;
        
        // Set the project root path for include paths during compilation
        // Get the directory where the executable is located
        loader.getBuilder()->setProjectRootPath(sourceFileParentDirectoryAbsolutePath());
        
        const auto result = loader.loadModelFromDirectory(modelDirectory.toStdString());

        if (! result.success)
        {
            progress.close();

            // If compilation was attempted and failed, show detailed error dialog
            if (result.compilationResult)
            {
                CompilationLogWidget logWidget(this);
                logWidget.displayCompilationResult(*result.compilationResult);
                logWidget.exec();
            }
            else
            {
                QMessageBox::critical(this, tr("Model Load Failed"),
                    tr("Failed to load model from:\n%1").arg(modelDirectory));
            }
            return;
        }

        progress.setLabelText(tr("Loading compiled module..."));
        QApplication::processEvents();

        // Load the compiled module
        PluginLoader& pluginLoader = PluginLoader::instance();
        if (pluginLoader.loadPlugin(result.compiledModulePath, /*overridePlugin=*/true))
        {
            // Refresh the models menu
            recreateModelMenuActions();

            progress.close();

            QMessageBox::information(this,
                                     tr("Model Loaded"),
                                     tr("Model '%1' loaded successfully from:\n%2\n\nNew model is now available in the Model menu.")
                                         .arg(QString::fromStdString(result.modelName))
                                         .arg(modelDirectory));
        }
        else
        {
            progress.close();

            QMessageBox::critical(this,
                                  tr("Module Load Failed"),
                                  tr("Failed to load compiled module:\n%1\n\nError: %2")
                                      .arg(QString::fromStdString(result.compiledModulePath))
                                      .arg(QString::fromStdString(pluginLoader.getLastError())));
        }
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Error"),
            tr("An error occurred while loading the model:\n%1").arg(e.what()));
    }
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

    QMessageBox::information(this,
                             tr("View Mode Changed"),
                             tr("Switched to 2D mode.\nCamera is now in top-down view with rotation disabled."));
}

void MainWindow::on3DModeRequested()
{
    ui->sceneWidget->setViewMode3D();

    // Reset sliders to default position (0, 0) when entering 3D mode
    QSignalBlocker azimuthBlocker(ui->azimuthSlider);
    QSignalBlocker elevationBlocker(ui->elevationSlider);
    ui->azimuthSlider->setValue(0);
    ui->elevationSlider->setValue(0);

    updateCameraControlsVisibility();

    QMessageBox::information(this,
                             tr("View Mode Changed"),
                             tr("Switched to 3D mode.\nYou can now rotate the camera using mouse or the sliders below."));
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
    int sameNameCount = 0;
    for (const QString& otherPath : allPaths)
    {
        if (QFileInfo(otherPath).fileName() == fileName)
        {
            sameNameCount++;
        }
    }

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
        Config config(filePath.toStdString());

        tooltip += QString("<b>%1</b><br/>").arg(tr("Configuration parameters:"));

        auto addParam = [&](const QString& category, const QString& paramName)
        {
            ConfigCategory* cat = config.getConfigCategory(category.toStdString(), /*ignoreCase=*/true);
            if (cat)
            {
                ConfigParameter* param = cat->getConfigParameter(paramName.toStdString());
                if (param)
                {
                    tooltip += QString("&nbsp;&nbsp;• <b>%1:</b> %2<br/>")
                                   .arg(paramName)
                                   .arg(QString::fromStdString(param->getDefaultValue()));
                }
            }
        };

        addParam("GENERAL", "number_steps");
        addParam("GENERAL", "number_of_rows");
        addParam("GENERAL", "number_of_columns");
        addParam("DISTRIBUTED", "number_node_x");
        addParam("DISTRIBUTED", "number_node_y");
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
}


void MainWindow::applyCommandLineOptions(CommandLineParser& cmdParser)
{
    // Store silent mode flag
    silentMode = cmdParser.isSilentMode();

    // Set starting model if specified
    if (cmdParser.getStartingModel())
    {
        const auto& modelName = cmdParser.getStartingModel().value();
        const QString modelQStr = QString::fromStdString(modelName);

        // Check if model is registered
        if (SceneWidgetVisualizerFactory::isModelRegistered(modelName))
        {
            // Find and check the corresponding action in the menu
            for (QAction* action : modelActionGroup->actions())
            {
                if (action->text() == modelQStr)
                {
                    action->setChecked(true);
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
                    break;
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
