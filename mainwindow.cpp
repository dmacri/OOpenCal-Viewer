#include <iostream>
#include <utility> // std::to_underlying, which requires C++23
#include <filesystem>
#include <source_location>
#include <algorithm>
#include <cmath>
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
#include <QSizePolicy>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "widgets/WaitCursorGuard.h"

#include "config/Config.h"
#include "config/ConfigConstants.h"
#include "core/CommandLineParser.h"
#include "plugins/CppModuleBuilder.h"
#include "plugins/ModelLoader.h"
#include "plugins/PluginLoader.h"
#include "data/ReductionManager.h"
#include "core/directoryConstants.h"
#include "visualiser/SettingParameter.h"
#include "visualiser/VideoExporter.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
#include "widgets/SceneWidget.h"
#include "widgets/SubstatesDockWidget.h"
#include "widgets/AboutDialog.h"
#include "widgets/ColorSettingsDialog.h"
#include "widgets/CompilationLogWidget.h"
#include "widgets/CompilationSettingsWidget.h"
#include "widgets/ConfigDetailsDialog.h"
#include "widgets/ReductionDialog.h"
#include "widgets/CustomDirectoryDialog.h"
#include "widgets/PerformanceSettingsDialog.h"


namespace
{
constexpr StepIndex FIRST_STEP_NUMBER = 0;
constexpr int NATIVE_3D_DEFAULT_PITCH = 0;
constexpr int NATIVE_3D_DEFAULT_YAW = 0;
constexpr int NATIVE_3D_DEFAULT_ROLL = 30;

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
    const auto opencalDir = viz::plugins::getOopencalDir();
    QString baseDir = QString::fromStdString(opencalDir);

    // Verify that base directory exists
    QDir dir(baseDir);
    if (baseDir.isEmpty() || !dir.exists())
    {
        // Invalid or missing directory → return empty (current path)
        return QString();
    }

    // Check if "OOpenCAL/models/" subdirectory exists
    QDir modelsDir(dir.filePath("OOpenCAL/models"));
    if (modelsDir.exists())
    {
        return modelsDir.absolutePath();
    }

    // Step 5: Return base directory if "models" does not exist
    return dir.absolutePath();
}

struct FileGroup
{
    QFileInfoList files;
    qint64 totalBytes = 0;
};

struct SimulationDirectorySummary
{
    QString directoryPath;
    QString headerPath;
    qint64 headerBytes = 0;
    QString outputPrefix;
    QString gridDescription;
    QString nodeDescription;
    QString readMode;
    QString substates;
    FileGroup dataFiles;
    FileGroup indexFiles;
    FileGroup reductionFiles;
    FileGroup sourceHeaders;
    FileGroup compiledModules;
};

QString formatByteSize(qint64 bytes)
{
    static constexpr double KIB = 1024.0;
    static constexpr double MIB = KIB * 1024.0;
    static constexpr double GIB = MIB * 1024.0;

    if (bytes < 1024)
    {
        return QString("%1 B").arg(bytes);
    }
    if (bytes < static_cast<qint64>(MIB))
    {
        return QString("%1 KiB").arg(QString::number(bytes / KIB, 'f', 1));
    }
    if (bytes < static_cast<qint64>(GIB))
    {
        return QString("%1 MiB").arg(QString::number(bytes / MIB, 'f', 1));
    }
    return QString("%1 GiB").arg(QString::number(bytes / GIB, 'f', 2));
}

void addFileToGroup(FileGroup& group, const QFileInfo& fileInfo)
{
    group.files.append(fileInfo);
    group.totalBytes += fileInfo.size();
}

QString describeGroupCount(const FileGroup& group)
{
    if (group.files.isEmpty())
    {
        return QStringLiteral("none");
    }

    return QString("%1, %2")
        .arg(QObject::tr("%n file(s)", nullptr, group.files.size()))
        .arg(formatByteSize(group.totalBytes));
}

QString formatModifiedDate(const QFileInfo& fileInfo)
{
    return fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss");
}

QString fileTableHtml(const QString& title, const FileGroup& group, int maxItems = 80)
{
    QString html =
        QString("<p style='margin-top:10px; margin-bottom:3px;'>"
                "<span style='font-size:10pt; font-weight:600; color:#24527a;'>%1</span><br/>"
                "<span style='color:#666666;'>%2</span>"
                "</p>")
            .arg(title.toHtmlEscaped())
            .arg(describeGroupCount(group).toHtmlEscaped());

    if (group.files.isEmpty())
    {
        return html + QObject::tr("<span style='color:#777777;'>&nbsp;&nbsp;none</span><br/>");
    }

    html += "<table cellspacing='0' cellpadding='3' border='0'>"
            "<tr bgcolor='#eeeeee'>"
            "<td><b>Name</b></td>"
            "<td align='right'><b>Size</b></td>"
            "<td><b>Modified</b></td>"
            "</tr>";

    int shown = 0;
    for (const QFileInfo& fileInfo : group.files)
    {
        if (shown >= maxItems)
        {
            html += QString("<tr><td colspan='3' style='color:#777777;'>… %1</td></tr>")
                        .arg(QObject::tr("%n more file(s)", nullptr, group.files.size() - shown).toHtmlEscaped());
            break;
        }

        html += QString("<tr>"
                        "<td><span style='font-family:monospace;'>%1</span></td>"
                        "<td align='right'>%2</td>"
                        "<td>%3</td>"
                        "</tr>")
                    .arg(fileInfo.fileName().toHtmlEscaped())
                    .arg(formatByteSize(fileInfo.size()).toHtmlEscaped())
                    .arg(formatModifiedDate(fileInfo).toHtmlEscaped());
        ++shown;
    }

    html += "</table>";
    return html;
}

SimulationDirectorySummary inspectSimulationDirectory(const QString& configFilePath)
{
    SimulationDirectorySummary summary;

    const QFileInfo headerInfo(configFilePath);
    summary.headerPath = headerInfo.absoluteFilePath();
    summary.directoryPath = headerInfo.dir().absolutePath();
    summary.headerBytes = headerInfo.exists() ? headerInfo.size() : 0;

    try
    {
        Config config(summary.headerPath.toStdString(), /*printWarnings=*/false);
        if (ConfigCategory* general = config.getConfigCategory(ConfigConstants::CATEGORY_GENERAL, /*ignoreCase=*/true))
        {
            if (const ConfigParameter* output = general->getConfigParameter(ConfigConstants::PARAM_OUTPUT_FILE_NAME))
            {
                summary.outputPrefix = QString::fromStdString(output->getValue<std::string>());
            }

            const auto readInt = [&](const std::string& parameterName, int fallback = 1)
            {
                if (const ConfigParameter* parameter = general->getConfigParameter(parameterName))
                {
                    return parameter->getValue<int>();
                }
                return fallback;
            };

            const int columns = readInt(ConfigConstants::PARAM_NUMBER_OF_COLUMNS);
            const int rows = readInt(ConfigConstants::PARAM_NUMBER_OF_ROWS);
            const int slices = readInt(ConfigConstants::PARAM_NUMBER_OF_SLICES);
            summary.gridDescription = QString("%1×%2×%3").arg(columns).arg(rows).arg(slices);
        }

        if (ConfigCategory* distributed = config.getConfigCategory(ConfigConstants::CATEGORY_DISTRIBUTED, /*ignoreCase=*/true))
        {
            const auto readInt = [&](const std::string& parameterName, int fallback = 1)
            {
                if (const ConfigParameter* parameter = distributed->getConfigParameter(parameterName))
                {
                    return parameter->getValue<int>();
                }
                return fallback;
            };

            const int nodeX = readInt(ConfigConstants::PARAM_NUMBER_NODE_X);
            const int nodeY = readInt(ConfigConstants::PARAM_NUMBER_NODE_Y);
            const int nodeZ = readInt(ConfigConstants::PARAM_NUMBER_NODE_Z);
            summary.nodeDescription = QString("%1×%2×%3").arg(nodeX).arg(nodeY).arg(nodeZ);
        }

        if (ConfigCategory* visualization = config.getConfigCategory(ConfigConstants::CATEGORY_VISUALIZATION, /*ignoreCase=*/true))
        {
            if (const ConfigParameter* mode = visualization->getConfigParameter(ConfigConstants::PARAM_MODE))
            {
                summary.readMode = QString::fromStdString(mode->getValue<std::string>());
            }
            if (const ConfigParameter* substates = visualization->getConfigParameter(ConfigConstants::PARAM_SUBSTATES))
            {
                summary.substates = QString::fromStdString(substates->getDefaultValue());
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Could not inspect simulation Header.txt: " << e.what() << std::endl;
    }

    QDir directory(summary.directoryPath);
    const QFileInfoList files = directory.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo& fileInfo : files)
    {
        const QString fileName = fileInfo.fileName();
        const QString lowerName = fileName.toLower();
        const bool startsWithOutputPrefix = !summary.outputPrefix.isEmpty() && fileName.startsWith(summary.outputPrefix);

        if (lowerName == QString::fromLatin1(DirectoryConstants::HEADER_FILE_NAME).toLower())
        {
            continue;
        }
        if (lowerName.endsWith(".h") || lowerName.endsWith(".hpp"))
        {
            addFileToGroup(summary.sourceHeaders, fileInfo);
        }
        else if (lowerName.endsWith(".so") || lowerName.endsWith(".dll") || lowerName.endsWith(".dylib"))
        {
            addFileToGroup(summary.compiledModules, fileInfo);
        }
        else if (startsWithOutputPrefix && lowerName.endsWith("_index.txt"))
        {
            addFileToGroup(summary.indexFiles, fileInfo);
        }
        else if (startsWithOutputPrefix && lowerName.endsWith("-red.txt"))
        {
            addFileToGroup(summary.reductionFiles, fileInfo);
        }
        else if (startsWithOutputPrefix && (lowerName.endsWith(".bin") || lowerName.endsWith(".txt")))
        {
            addFileToGroup(summary.dataFiles, fileInfo);
        }
    }

    return summary;
}

QString buildSimulationDirectoryTooltipHtml(const SimulationDirectorySummary& summary)
{
    const QFileInfo headerInfo(summary.headerPath);
    QString html =
        QString("<qt>"
                "<div style='font-family:Sans Serif; font-size:9pt;'>"
                "<p style='margin:0 0 6px 0;'>"
                "<span style='font-size:12pt; font-weight:700; color:#1d3557;'>%1</span><br/>"
                "<span style='font-family:monospace;'>%2</span>"
                "</p>")
            .arg(QObject::tr("Simulation directory").toHtmlEscaped())
            .arg(summary.directoryPath.toHtmlEscaped());

    html += "<table cellspacing='0' cellpadding='3' border='0'>";
    auto addSummaryRow = [&](const QString& label, const QString& value)
    {
        html += QString("<tr>"
                        "<td style='color:#666666;'>%1</td>"
                        "<td><b>%2</b></td>"
                        "</tr>")
                    .arg(label.toHtmlEscaped())
                    .arg(value.toHtmlEscaped());
    };

    addSummaryRow(QObject::tr("Header"),
                  QString("%1, %2, %3")
                      .arg(headerInfo.fileName())
                      .arg(formatByteSize(summary.headerBytes))
                      .arg(formatModifiedDate(headerInfo)));
    addSummaryRow(QObject::tr("Output prefix"), summary.outputPrefix.isEmpty() ? QObject::tr("unknown") : summary.outputPrefix);
    addSummaryRow(QObject::tr("Grid"), summary.gridDescription.isEmpty() ? QObject::tr("unknown") : summary.gridDescription);
    addSummaryRow(QObject::tr("Nodes"), summary.nodeDescription.isEmpty() ? QObject::tr("unknown") : summary.nodeDescription);
    addSummaryRow(QObject::tr("Read mode"), summary.readMode.isEmpty() ? QObject::tr("default") : summary.readMode);
    addSummaryRow(QObject::tr("Substates"), summary.substates.isEmpty() ? QObject::tr("not specified") : summary.substates);
    addSummaryRow(QObject::tr("Data files"), describeGroupCount(summary.dataFiles));
    addSummaryRow(QObject::tr("Index files"), describeGroupCount(summary.indexFiles));
    addSummaryRow(QObject::tr("Reduction files"), describeGroupCount(summary.reductionFiles));
    html += "</table>";

    html += fileTableHtml(QObject::tr("C++ model header files"), summary.sourceHeaders);
    html += fileTableHtml(QObject::tr("Compiled model modules (.so, .dll, .dylib)"), summary.compiledModules);
    html += fileTableHtml(QObject::tr("Simulation data files"), summary.dataFiles, 40);
    html += fileTableHtml(QObject::tr("Index files"), summary.indexFiles, 40);
    html += fileTableHtml(QObject::tr("Reduction files"), summary.reductionFiles, 20);

    html += "</div></qt>";
    return html;
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
    ui->inputFilePathLabel->setMinimumWidth(160);
    ui->inputFilePathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->reductionWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    ui->horizontalLayout->setStretchFactor(ui->inputFilePathLabel, 1);
    ui->horizontalLayout->setStretchFactor(ui->reductionWidget, 0);

    // Initialize substate dock widget from UI
    ui->substatesDockWidget->initializeFromUI();
    ui->substatesDockWidget->hide();  // Hidden by default until simulation data is loaded

    setupConnections();
    configureButtons();
    loadStrings();
    recreateModelMenuActions();
    createViewModeActionGroup();
    updateRecentDirectoriesMenu();

    enterNoConfigurationFileMode();
}

void MainWindow::configureUIElements(const QString& configFileName)
{
    initializeSceneWidget(configFileName);
    showInputDirectoryOnBarLabel(configFileName);

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
    connect(ui->actionReloadData, &QAction::triggered, this, &MainWindow::onReloadDataRequested);
    connect(ui->actionLoadPlugin, &QAction::triggered, this, &MainWindow::onLoadPluginRequested);
    connect(ui->actionLoadModelFromDirectory, &QAction::triggered, this, &MainWindow::onLoadModelFromDirectoryRequested);
    connect(ui->actionColor_settings, &QAction::triggered, this, &MainWindow::onColorSettingsRequested);
    connect(ui->actionCompilation_settings, &QAction::triggered, this, &MainWindow::onCompilationSettingsRequested);
    connect(ui->actionPerformance_settings, &QAction::triggered, this, &MainWindow::onPerformanceSettingsRequested);
    connect(ui->actionCellRendering, &QAction::triggered, this, &MainWindow::onCellRenderingToggled);
    connect(ui->actionShow_reduction, &QAction::triggered, this, &MainWindow::onShowReductionRequested);

    // View mode actions
    connect(ui->action2DMode, &QAction::triggered, this, &MainWindow::on2DModeRequested);
    connect(ui->action3DMode, &QAction::triggered, this, &MainWindow::on3DModeRequested);
    connect(ui->actionCrossSectionControls, &QAction::toggled, this, &MainWindow::onCrossSectionControlsToggled);
    connect(ui->actionGridLines, &QAction::triggered, this, &MainWindow::onGridLinesToggled);
    connect(ui->actionFlatSceneBackground, &QAction::triggered, this, &MainWindow::onFlatSceneBackgroundToggled);

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


void MainWindow::showInputDirectoryOnBarLabel(const QString& configFilePath)
{
    ui->inputFilePathLabel->setFileName(configFilePath);
    ui->inputFilePathLabel->setToolTip(QString());

    if (configFilePath.isEmpty())
    {
        return;
    }

    const auto summary = inspectSimulationDirectory(configFilePath);
    ui->inputFilePathLabel->setDisplayDirectoryPath(summary.directoryPath);
    ui->inputFilePathLabel->setToolTip(buildSimulationDirectoryTooltipHtml(summary));
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
    if (! lastStepAvailableInAvailableSteps)
    {
        std::cerr << tr("[Warning] Number of steps mismatch: total number of steps from Header.txt is %1, but last step number from index file is %2")
                         .arg(totalSteps())
                         .arg(availableSteps.back()).toStdString() << std::endl;
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

    // Camera rotation controls - Roll (X axis)
    connect(ui->rollSlider, &QSlider::valueChanged, this, &MainWindow::onRollChanged);
    connect(ui->rollSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), ui->rollSlider, &QSlider::setValue);
    connect(ui->rollSlider, &QSlider::valueChanged, ui->rollSpinBox, &QSpinBox::setValue);

    // Camera rotation controls - Pitch (Y axis)
    connect(ui->pitchSlider, &QSlider::valueChanged, this, &MainWindow::onPitchChanged);
    connect(ui->pitchSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), ui->pitchSlider, &QSlider::setValue);
    connect(ui->pitchSlider, &QSlider::valueChanged, ui->pitchSpinBox, &QSpinBox::setValue);

    // Camera rotation controls - Yaw (Z axis)
    connect(ui->yawSlider, &QSlider::valueChanged, this, &MainWindow::onYawChanged);
    connect(ui->yawSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), ui->yawSlider, &QSlider::setValue);
    connect(ui->yawSlider, &QSlider::valueChanged, ui->yawSpinBox, &QSpinBox::setValue);

    // Reset camera button
    connect(ui->resetCameraButton, &QPushButton::clicked, this, &MainWindow::onResetCameraRequested);

    // Update sliders when camera changes (e.g., via mouse rotation in 3D mode)
    connect(ui->sceneWidget, &SceneWidget::cameraOrientationChanged, this, &MainWindow::onCameraOrientationChanged);

    // Native 3D volume / axis-aligned cross-section controls
    connect(ui->sliceViewComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::onSliceViewChanged);
    connect(ui->sliceSlider,
            &QSlider::valueChanged,
            this,
            &MainWindow::onSliceChanged);
    connect(ui->sliceSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            ui->sliceSlider,
            &QSlider::setValue);
    connect(ui->sliceSlider,
            &QSlider::valueChanged,
            ui->sliceSpinBox,
            &QSpinBox::setValue);
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
        QMessageBox::warning(this, tr("No Simulation Directory"),
                           tr("No simulation directory has been loaded."));
        return;
    }

    const auto summary = inspectSimulationDirectory(configFileName);
    ConfigDetailsDialog dialog(configFileName.toStdString(), buildSimulationDirectoryTooltipHtml(summary), this);
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
        QMessageBox::information(this, tr("Export Complete"), tr("Video exported successfully to:\n%1").arg(outputFilePath));
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
    
    // Check if we reached the end (based on totalSteps)
    if ((playbackDirection == PlayingDirection::Forward && clampedStep >= totalSteps())
        || (playbackDirection == PlayingDirection::Backward && clampedStep <= FIRST_STEP_NUMBER))
    {
        playbackTimer.stop();
        
        // Exit application if autoPlay + exitAfterLastStep was requested
        if (shouldExitAfterPlayback)
        {
            QTimer::singleShot(100, this, &MainWindow::close);
        }
        return;
    }
    
    // Also check if we've reached the last available step
    // This handles cases where not all steps are available (e.g., steps [0, 10, 20, ..., 90])
    if (!availableSteps.empty())
    {
        const StepIndex lastAvailableStep = availableSteps.back();
        const StepIndex firstAvailableStep = availableSteps.front();
        
        if ((playbackDirection == PlayingDirection::Forward && currentStep >= lastAvailableStep)
            || (playbackDirection == PlayingDirection::Backward && currentStep <= firstAvailableStep))
        {
            playbackTimer.stop();
            
            // Exit application if autoPlay + exitAfterLastStep was requested
            if (shouldExitAfterPlayback)
            {
                QTimer::singleShot(100, this, &MainWindow::close);
            }
            return;
        }
    }
    
    // Check if target step exists in available steps
    if (! std::ranges::contains(availableSteps, clampedStep))
    {
        // Handle missing step
        if (! handleMissingStepDuringPlayback(clampedStep, playbackDirection))
        {
            // No more available steps in this direction
            playbackTimer.stop();
            
            // Exit application if autoPlay + exitAfterLastStep was requested
            if (shouldExitAfterPlayback)
            {
                QTimer::singleShot(100, this, &MainWindow::close);
            }
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
    
    // In normal mode, ask user what to do
    const QString nextStepText = (direction == PlayingDirection::Forward)
                                     ? tr("next available step is %1").arg(nextStep)
                                     : tr("previous available step is %1").arg(nextStep);

    std::cerr << tr("Missing Step During Playback").toStdString()
              << tr("Step %1 is not available.\nThe %2")
                     .arg(targetStep)
                     .arg(nextStepText).toStdString() << endl;
    currentStep = nextStep;
    return true;
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
        std::cerr << "Changing position error: " << tr("It was impossible to change position to %1, because: ").arg(stepPosition).toStdString() << e.what() << std::endl;

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

        // Clear active substates when switching models
        clearActiveSubstates();

        ui->sceneWidget->switchModel(modelName.toStdString());

        // Update substate dock widget for new model
        updateSubstateDockeWidget();

        updateMenu2ShowTheSelectedModeAsActive(modelName, modelActionGroup);

        std::cout << "[DEBUG] Model Changed: " <<
            tr("Successfully switched to %1 model, but no data was reloaded from files.\n"
               "Use 'Reload Data' (F5), or open another simulation directory to load data files.\n"
               "Notice: Model has to be compatible with the loaded directory, if not - the behaviour is undefined")
                .arg(modelName).toStdString() << std::endl;

    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Model Switch Failed"), tr("Failed to switch model:\n%1").arg(e.what()));

        // Revert checkbox state to current model
        const auto currentModel = QString::fromStdString(ui->sceneWidget->getCurrentModelName());
        updateMenu2ShowTheSelectedModeAsActive(currentModel, modelActionGroup);
    }
}
void MainWindow::clearActiveSubstates()
{
    // Clear active substates for both 2D and 3D visualization
    ui->sceneWidget->setActiveSubstatesForColorring({});
    ui->sceneWidget->setActiveSubstatesFor3D({});
    activeSubstatesFor3D.clear();
}

void MainWindow::updateSubstateDockeWidget()
{
    if (ui->substatesDockWidget && ui->sceneWidget->getSettingParameter())
    {
        auto settingParam = const_cast<SettingParameter*>(ui->sceneWidget->getSettingParameter());
        settingParam->initializeSubstateInfo();
        ui->substatesDockWidget->updateSubstates(settingParam);

        // TODO: GB: Do we really need to send this to MainWindow? Maybe SubstatesDockWidget can directly control SceneWidget?
        // Connect signal for 3D visualization state changes
        connect(ui->substatesDockWidget, &SubstatesDockWidget::use3dStateChanged, this, &MainWindow::onUse3dStateChanged, Qt::UniqueConnection);
        connect(ui->substatesDockWidget, &SubstatesDockWidget::use3dSubstateOrderChanged, this, &MainWindow::onUse3dSubstateOrderChanged, Qt::UniqueConnection);
        connect(ui->substatesDockWidget, &SubstatesDockWidget::useSubstatesColorringRequested, this, &MainWindow::onUseSubstatesColorringRequested, Qt::UniqueConnection);
        connect(ui->substatesDockWidget, &SubstatesDockWidget::deactivateRequested, this, &MainWindow::onDeactivateRequested, Qt::UniqueConnection);
        connect(ui->substatesDockWidget, &SubstatesDockWidget::visualizationRefreshRequested, ui->sceneWidget, &SceneWidget::refreshVisualization, Qt::UniqueConnection);
    }
}

void MainWindow::onReloadDataRequested()
{
    try
    {
        // Clear active substates when reloading data
        clearActiveSubstates();

        ui->sceneWidget->reloadData();

        // Update substate dock widget after reload
        updateSubstateDockeWidget();

        std::cout << "[DEBUG] Data Reloaded: Data files successfully reloaded for model: " << ui->sceneWidget->getCurrentModelName() << std::endl;
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Reload Failed"),
                            tr("Failed to reload data:\n%1").arg(e.what()));
    }
}

void MainWindow::openConfigurationFile(const QString& configFileName, std::shared_ptr<Config> optionalConfig)
{
    try
    {
        if (SceneWidgetVisualizerFactory::getAvailableModels().empty())
        {
            QMessageBox::information(this,
                                     tr("No Models Loaded"),
                                     tr("No models are currently loaded.\n\n"
                                        "Load a model first using:\n"
                                        "• Model → Load Plugin...\n"
                                    "• File → Open Model Directory..."));
            return;
        }

        // Stop any ongoing playback
        playbackTimer.stop();

        // Clear active substates when opening a new simulation directory
        clearActiveSubstates();

        if (bool isFirstConfiguration [[maybe_unused]] = ui->inputFilePathLabel->getFileName().isEmpty())
        {
            initializeSceneWidget(configFileName);
        }
        else
        {
            // Reload with the new Header.txt
            ui->sceneWidget->loadNewConfiguration(configFileName.toStdString(), 0);

            // Update substate dock widget for the new simulation data
            updateSubstateDockeWidget();
        }

        synchronizeViewModeWithLoadedModel();

        // Initialize reduction manager for this Header.txt
        // If config is provided, use it; otherwise read from file
        initializeReductionManager(configFileName, optionalConfig);

        // Synchronize grid lines checkbox with current visibility state
        syncGridLinesCheckbox();

        // Synchronize flat scene background checkbox with current visibility state
        syncFlatSceneBackgroundCheckbox();

        // Synchronize cell rendering checkbox with current setting
        syncCellRenderingCheckbox();

        // Update UI with the simulation directory that owns this Header.txt
        showInputDirectoryOnBarLabel(configFileName);

        // Reset to first step
        currentStep = 0;
        setPositionOnWidgets(currentStep);

        // Enable all widgets now that we have simulation data
        setWidgetsEnabledState(true);

        std::cout << "[DEBUG] Simulation Directory Loaded: Successfully loaded Header.txt: " << configFileName.toStdString() << std::endl;
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Load Failed"), tr("Failed to load simulation directory:\n%1").arg(e.what()));
    }
}

void MainWindow::onColorSettingsRequested()
{
    auto* colorSettings = new ColorSettingsDialog(this);

    colorSettings->show();
}

void MainWindow::onCompilationSettingsRequested()
{
    auto* compilationSettings = new CompilationSettingsWidget(this);
    
    // Set up the widget as a window
    compilationSettings->setWindowFlags(Qt::Window);
    compilationSettings->setWindowTitle("Compilation Settings");
    compilationSettings->resize(800, 600);
    
    // Create a default module builder for now
    auto moduleBuilder = std::make_shared<viz::plugins::CppModuleBuilder>();
    compilationSettings->setModuleBuilder(moduleBuilder);
    
    compilationSettings->show();
}

void MainWindow::onPerformanceSettingsRequested()
{
    auto* perfSettings = new PerformanceSettingsDialog(this);
    perfSettings->exec();
    delete perfSettings;  // Modal dialog, delete after use
}

void MainWindow::onCellRenderingToggled(bool checked)
{
    if (ui->sceneWidget)
    {
        ui->sceneWidget->setUseCellRendering(checked);
        ui->sceneWidget->setViewMode2D(); // this is to refresh visualization
    }
}

void MainWindow::syncCellRenderingCheckbox()
{
    if (ui->sceneWidget)
    {
        bool useCellRendering = ui->sceneWidget->getUseCellRendering();
        ui->actionCellRendering->setChecked(useCellRendering);
    }
}

void MainWindow::enterNoConfigurationFileMode()
{
    ui->sceneWidget->setHidden(true);

    // Set UI to show no simulation directory loaded
    ui->inputFilePathLabel->setFileName("");
    ui->inputFilePathLabel->setToolTip(QString());
    if (SceneWidgetVisualizerFactory::getAvailableModels().empty())
    {
        ui->inputFilePathLabel->setText(tr("No models loaded - use Model → Load Plugin or File → Open Model Directory"));
    }
    else
    {
        ui->inputFilePathLabel->setText(tr("No simulation directory loaded - use File → Open Model Directory"));
    }

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

    // Create action group for exclusive selection
    modelActionGroup = new QActionGroup(this);
    modelActionGroup->setExclusive(true);

    // Clear existing model actions from menu (if any from .ui file)
    ui->menuModel->clear();

    if (availableModels.empty())
    {
        QAction* noModelsAction = ui->menuModel->addAction(tr("No models loaded"));
        noModelsAction->setEnabled(false);
    }
    else
    {
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
    }

    // Add separator and actions
    ui->menuModel->addSeparator();
    ui->menuModel->addAction(ui->actionLoadPlugin);
    ui->menuModel->addAction(ui->actionLoadModelFromDirectory);
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
    CustomDirectoryDialog dialog(this);

    // Set the starting directory using the same logic as the original dialog
    QString startPath = getOOpenCalStartPath();
    if (! startPath.isEmpty())
    {
        dialog.setStartDirectory(startPath);
    }

    if (QDialog::Accepted == dialog.exec())
    {
        QString modelDirectory = dialog.getSelectedDirectory();
        if (! modelDirectory.isEmpty())
        {
            // Check which loading mode is selected
            if (CustomDirectoryDialog::LoadingMode::UseExistingModel == dialog.getLoadingMode())
            {
                // Load data using existing model
                QString existingModelName = dialog.getSelectedExistingModel();
                if (! existingModelName.isEmpty())
                {
                    loadModelDataWithExistingModel(modelDirectory, existingModelName);
                }
                else
                {
                    QMessageBox::warning(this, tr("No Model Selected"),
                        tr("Please select a model from the available models list."));
                }
            }
            else // CompileModule mode (default behavior)
            {
                // Load model with compilation as before
                loadModelFromDirectory(modelDirectory, dialog.compilationRequested());
            }
        }
    }
}

void MainWindow::loadModelFromDirectory(const QString& modelDirectory, bool forceCompilation)
{
    // Show wait cursor during model loading
    WaitCursorGuard waitCursor("Loading model from directory...");

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
        const auto result = loader.loadModelFromDirectory(actualModelDir.string(), forceCompilation);

        if (! result.success)
        {
            progress.close();

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
        
        loadModelDataWithExistingModel(modelDirectory, QString::fromStdString(pluginModelName));
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Error"),
            tr("An error occurred while loading the model:\n%1").arg(e.what()));
    }
}

void MainWindow::loadModelDataWithExistingModel(const QString& modelDirectory, const QString& existingModelName)
{
    try
    {
        // The model directory should contain Header.txt and data files
        namespace fs = std::filesystem;
        fs::path actualModelDir = fs::path(modelDirectory.toStdString());

        if (! SceneWidgetVisualizerFactory::isModelRegistered(existingModelName.toStdString()))
        {
            QMessageBox::critical(this, tr("Model Not Available"),
                tr("The requested model '%1' is not available in the system.").arg(existingModelName));
            return;
        }

        // Clear scene before loading new simulation data to avoid stale data
        ui->sceneWidget->clearScene();

        switchToModel(existingModelName);

        // Build path to Header.txt in the model directory
        fs::path headerPath = actualModelDir / DirectoryConstants::HEADER_FILE_NAME;
        
        if (! fs::exists(headerPath))
        {
            QMessageBox::critical(this, tr("Configuration Load Failed"),
                tr("Header.txt not found in model directory:\n%1").arg(QString::fromStdString(actualModelDir.string())));
            return;
        }

        // Load settings and data from Header.txt in the selected directory
        openConfigurationFile(QString::fromStdString(headerPath.string()));

        // Add directory to recent directories list
        addToRecentDirectories(QString::fromStdString(actualModelDir.string()));

        std::cout << "[DEBUG] " << tr("Model Data Loaded").toStdString()
                  << tr("Model data loaded successfully using existing model '%1' from:\n%2\n\nConfiguration loaded and ready to use.")
                         .arg(existingModelName)
                         .arg(QString::fromStdString(actualModelDir.string())).toStdString() << std::endl;
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Error"), tr("An error occurred while loading model data:\n%1").arg(e.what()));
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
    if (ui->sceneWidget->isNative3DModel())
    {
        QSignalBlocker blocker2D(ui->action2DMode);
        QSignalBlocker blocker3D(ui->action3DMode);
        ui->action2DMode->setChecked(false);
        ui->action3DMode->setChecked(true);
        return;
    }

    if (ui->sceneWidget->isCrossSectionView())
        ui->sceneWidget->clearCrossSection();

    ui->sceneWidget->setViewMode2D();
    {
        QSignalBlocker blocker(ui->actionCrossSectionControls);
        ui->actionCrossSectionControls->setChecked(false);
    }
    updateSliceControls(true);
    updateCameraControlsVisibility();

    // Synchronize menu checkboxes - ensure only 2D mode is checked
    QSignalBlocker blocker2D(ui->action2DMode);
    QSignalBlocker blocker3D(ui->action3DMode);
    ui->action2DMode->setChecked(true);
    ui->action3DMode->setChecked(false);

    // Synchronize flat scene background checkbox (disabled in 2D mode)
    syncFlatSceneBackgroundCheckbox();

    std::cout << "[DEBUG] View Mode Changed: Switched to 2D mode.\nCamera is now in top-down view with rotation disabled." << std::endl;
}

void MainWindow::on3DModeRequested()
{
    if (ui->sceneWidget->isCrossSectionView())
    {
        ui->sceneWidget->clearCrossSection();
        QSignalBlocker viewBlocker(ui->sliceViewComboBox);
        ui->sliceViewComboBox->setCurrentIndex(0);
        ui->sliceAxisLabel->setEnabled(false);
        ui->sliceSlider->setEnabled(false);
        ui->sliceSpinBox->setEnabled(false);
        ui->sliceRangeLabel->setEnabled(false);
    }
    else
    {
        ui->sceneWidget->setViewMode3D();
    }

    onResetCameraRequested();

    updateCameraControlsVisibility();

    // Synchronize menu checkboxes - ensure only 3D mode is checked
    QSignalBlocker blocker2D(ui->action2DMode);
    QSignalBlocker blocker3D(ui->action3DMode);
    ui->action3DMode->setChecked(true);
    ui->action2DMode->setChecked(false);

    // Synchronize flat scene background checkbox (enabled in 3D mode)
    syncFlatSceneBackgroundCheckbox();

    std::cout << "[DEBUG] View Mode Changed: Switched to 3D mode.\nYou can now rotate the camera using mouse or the sliders below." << std::endl;
}

void MainWindow::synchronizeViewModeWithLoadedModel()
{
    const bool native3D = ui->sceneWidget->isNative3DModel();

    // A native 3D volume has no meaningful flat-view representation.
    // A 2D model still keeps 3D available for "substate as altitude".
    ui->action2DMode->setEnabled(!native3D);
    ui->action3DMode->setEnabled(true);
    {
        QSignalBlocker blocker(ui->actionCrossSectionControls);
        ui->actionCrossSectionControls->setChecked(false);
    }
    updateSliceControls(true);

    if (native3D)
        on3DModeRequested();
    else
        on2DModeRequested();
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

void MainWindow::onFlatSceneBackgroundToggled(bool checked)
{
    ui->sceneWidget->setFlatSceneBackgroundVisible(checked);
}

void MainWindow::syncFlatSceneBackgroundCheckbox()
{
    // Synchronize the checkbox state with the actual flat scene background visibility
    QSignalBlocker blocker(ui->actionFlatSceneBackground);
    ui->actionFlatSceneBackground->setChecked(ui->sceneWidget->getFlatSceneBackgroundVisible());
    
    // In 2D mode, disable the checkbox (always show background)
    const bool is3DMode = (ui->sceneWidget->getViewMode() == ViewMode::Mode3D);
    ui->actionFlatSceneBackground->setEnabled(is3DMode);
}

void MainWindow::updateCameraControlsVisibility()
{
    const bool is3DMode =
        ui->sceneWidget->getViewMode() == ViewMode::Mode3D &&
        !ui->sceneWidget->isCrossSectionView();
    ui->camera3DControlsWidget->setVisible(is3DMode);
}

void MainWindow::updateSliceControls(bool resetSelection)
{
    const bool available = ui->sceneWidget->hasSliceable3DView();
    ui->actionCrossSectionControls->setEnabled(available);
    ui->sliceControlsWidget->setVisible(
        available && ui->actionCrossSectionControls->isChecked());

    if (!available)
    {
        QSignalBlocker blocker(ui->actionCrossSectionControls);
        ui->actionCrossSectionControls->setChecked(false);
        return;
    }

    if (!resetSelection)
        return;

    QSignalBlocker viewBlocker(ui->sliceViewComboBox);
    QSignalBlocker sliderBlocker(ui->sliceSlider);
    QSignalBlocker spinBlocker(ui->sliceSpinBox);
    ui->sliceViewComboBox->clear();
    if (ui->sceneWidget->isNative3DModel())
    {
        ui->sliceViewComboBox->addItem(tr("Volume"));
        ui->sliceViewComboBox->addItem(tr("XY plane (fixed Z)"));
        ui->sliceViewComboBox->addItem(tr("XZ plane (fixed Y)"));
        ui->sliceViewComboBox->addItem(tr("YZ plane (fixed X)"));
    }
    else
    {
        ui->sliceViewComboBox->addItem(tr("3D substate surface"));
        ui->sliceViewComboBox->addItem(tr("XZ profile (fixed Y)"));
        ui->sliceViewComboBox->addItem(tr("YZ profile (fixed X)"));
    }
    ui->sliceViewComboBox->setCurrentIndex(0);
    ui->sliceSlider->setRange(0, 0);
    ui->sliceSpinBox->setRange(0, 0);
    ui->sliceSlider->setValue(0);
    ui->sliceSpinBox->setValue(0);
    ui->sliceAxisLabel->setText("Z:");
    ui->sliceRangeLabel->setText("/ 0");
    ui->sliceAxisLabel->setEnabled(false);
    ui->sliceSlider->setEnabled(false);
    ui->sliceSpinBox->setEnabled(false);
    ui->sliceRangeLabel->setEnabled(false);
}

void MainWindow::onCrossSectionControlsToggled(bool checked)
{
    if (!checked)
    {
        const bool crossSectionWasActive = ui->sceneWidget->isCrossSectionView();
        if (crossSectionWasActive)
            ui->sceneWidget->clearCrossSection();
        ui->sliceControlsWidget->hide();
        if (crossSectionWasActive)
        {
            QSignalBlocker blocker2D(ui->action2DMode);
            QSignalBlocker blocker3D(ui->action3DMode);
            ui->action2DMode->setChecked(false);
            ui->action3DMode->setChecked(true);
            onResetCameraRequested();
        }
        updateCameraControlsVisibility();
        syncFlatSceneBackgroundCheckbox();
        return;
    }

    if (!ui->sceneWidget->hasSliceable3DView())
    {
        QSignalBlocker blocker(ui->actionCrossSectionControls);
        ui->actionCrossSectionControls->setChecked(false);
        return;
    }

    updateSliceControls(true);
    ui->sliceControlsWidget->show();
}

void MainWindow::onSliceViewChanged(int viewIndex)
{
    if (!ui->sceneWidget->hasSliceable3DView())
        return;

    if (viewIndex <= 0)
    {
        on3DModeRequested();
        return;
    }

    const SettingParameter* settings = ui->sceneWidget->getSettingParameter();
    if (!settings)
        return;

    GridSliceAxis axis;
    int axisSize;
    QString axisName;
    if (ui->sceneWidget->isNative3DModel())
    {
        axis = GridSliceAxis::Z;
        axisSize = settings->numberOfSlicesZ;
        axisName = "Z:";
        if (viewIndex == 2)
        {
            axis = GridSliceAxis::Y;
            axisSize = settings->numberOfRowsY;
            axisName = "Y:";
        }
        else if (viewIndex == 3)
        {
            axis = GridSliceAxis::X;
            axisSize = settings->numberOfColumnX;
            axisName = "X:";
        }
    }
    else
    {
        axis = viewIndex == 1 ? GridSliceAxis::Y : GridSliceAxis::X;
        axisSize = axis == GridSliceAxis::Y
            ? settings->numberOfRowsY
            : settings->numberOfColumnX;
        axisName = axis == GridSliceAxis::Y ? "Y:" : "X:";
    }

    const int maximum = std::max(0, axisSize - 1);
    const int initialIndex = maximum / 2;
    {
        QSignalBlocker sliderBlocker(ui->sliceSlider);
        QSignalBlocker spinBlocker(ui->sliceSpinBox);
        ui->sliceSlider->setRange(0, maximum);
        ui->sliceSpinBox->setRange(0, maximum);
        ui->sliceSlider->setValue(initialIndex);
        ui->sliceSpinBox->setValue(initialIndex);
    }
    ui->sliceAxisLabel->setText(axisName);
    ui->sliceRangeLabel->setText(QString("/ %1").arg(maximum));
    ui->sliceAxisLabel->setEnabled(true);
    ui->sliceSlider->setEnabled(true);
    ui->sliceSpinBox->setEnabled(true);
    ui->sliceRangeLabel->setEnabled(true);

    if (ui->sceneWidget->isNative3DModel())
        ui->sceneWidget->setNative3DSlice(axis, initialIndex);
    else
        ui->sceneWidget->setSubstate3DSlice(axis, initialIndex);

    QSignalBlocker blocker2D(ui->action2DMode);
    QSignalBlocker blocker3D(ui->action3DMode);
    ui->action2DMode->setChecked(true);
    ui->action3DMode->setChecked(false);
    updateCameraControlsVisibility();
    syncFlatSceneBackgroundCheckbox();
}

void MainWindow::onSliceChanged(int fixedIndex)
{
    const int viewIndex = ui->sliceViewComboBox->currentIndex();
    if (viewIndex <= 0 || !ui->sceneWidget->hasSliceable3DView())
        return;

    if (ui->sceneWidget->isNative3DModel())
    {
        GridSliceAxis axis = GridSliceAxis::Z;
        if (viewIndex == 2)
            axis = GridSliceAxis::Y;
        else if (viewIndex == 3)
            axis = GridSliceAxis::X;
        ui->sceneWidget->setNative3DSlice(axis, fixedIndex);
    }
    else
    {
        const GridSliceAxis axis =
            viewIndex == 1 ? GridSliceAxis::Y : GridSliceAxis::X;
        ui->sceneWidget->setSubstate3DSlice(axis, fixedIndex);
    }
}

void MainWindow::syncCameraSliders()
{
    // Sync sliders with current camera position
    const double roll = ui->sceneWidget->getCameraRoll();
    const double pitch = ui->sceneWidget->getCameraPitch();
    const double yaw = ui->sceneWidget->getCameraYaw();

    QSignalBlocker rollBlocker(ui->rollSlider);
    QSignalBlocker pitchBlocker(ui->pitchSlider);
    QSignalBlocker yawBlocker(ui->yawSlider);
    QSignalBlocker rollSpinBoxBlocker(ui->rollSpinBox);
    QSignalBlocker pitchSpinBoxBlocker(ui->pitchSpinBox);
    QSignalBlocker yawSpinBoxBlocker(ui->yawSpinBox);

    ui->rollSlider->setValue(static_cast<int>(roll));
    ui->rollSpinBox->setValue(static_cast<int>(roll));
    ui->pitchSlider->setValue(static_cast<int>(pitch));
    ui->pitchSpinBox->setValue(static_cast<int>(pitch));
    ui->yawSlider->setValue(static_cast<int>(yaw));
    ui->yawSpinBox->setValue(static_cast<int>(yaw));
}

void MainWindow::onRollChanged(int value)
{
    ui->sceneWidget->setCameraRoll(value);
}

void MainWindow::onPitchChanged(int value)
{
    ui->sceneWidget->setCameraPitch(value);
}

void MainWindow::onYawChanged(int value)
{
    ui->sceneWidget->setCameraYaw(value);
}

void MainWindow::onResetCameraRequested()
{
    const int defaultPitch = ui->sceneWidget->isNative3DModel()
                                 ? NATIVE_3D_DEFAULT_PITCH
                                 : 0;
    const int defaultYaw = ui->sceneWidget->isNative3DModel()
                               ? NATIVE_3D_DEFAULT_YAW
                               : 0;
    const int defaultRoll = ui->sceneWidget->isNative3DModel()
                                ? NATIVE_3D_DEFAULT_ROLL
                                : 0;

    // Block the controls while applying one coherent camera preset.
    QSignalBlocker rollBlocker(ui->rollSlider);
    QSignalBlocker pitchBlocker(ui->pitchSlider);
    QSignalBlocker yawBlocker(ui->yawSlider);
    QSignalBlocker rollSpinBoxBlocker(ui->rollSpinBox);
    QSignalBlocker pitchSpinBoxBlocker(ui->pitchSpinBox);
    QSignalBlocker yawSpinBoxBlocker(ui->yawSpinBox);

    ui->sceneWidget->setCameraRoll(defaultRoll);
    ui->sceneWidget->setCameraPitch(defaultPitch);
    ui->sceneWidget->setCameraYaw(defaultYaw);

    // Reset zoom to default level
    ui->sceneWidget->resetCameraZoom();

    ui->rollSlider->setValue(defaultRoll);
    ui->rollSpinBox->setValue(defaultRoll);
    ui->pitchSlider->setValue(defaultPitch);
    ui->pitchSpinBox->setValue(defaultPitch);
    ui->yawSlider->setValue(defaultYaw);
    ui->yawSpinBox->setValue(defaultYaw);
}

void MainWindow::onCameraOrientationChanged(double roll, double pitch, double yaw)
{
    // Block signals to avoid circular updates
    QSignalBlocker rollBlocker(ui->rollSlider);
    QSignalBlocker pitchBlocker(ui->pitchSlider);
    QSignalBlocker yawBlocker(ui->yawSlider);
    QSignalBlocker rollSpinBoxBlocker(ui->rollSpinBox);
    QSignalBlocker pitchSpinBoxBlocker(ui->pitchSpinBox);
    QSignalBlocker yawSpinBoxBlocker(ui->yawSpinBox);

    const int rollDegrees = static_cast<int>(std::lround(roll));
    const int pitchDegrees = static_cast<int>(std::lround(pitch));
    const int yawDegrees = static_cast<int>(std::lround(yaw));

    ui->rollSlider->setValue(rollDegrees);
    ui->rollSpinBox->setValue(rollDegrees);
    ui->pitchSlider->setValue(pitchDegrees);
    ui->pitchSpinBox->setValue(pitchDegrees);
    ui->yawSlider->setValue(yawDegrees);
    ui->yawSpinBox->setValue(yawDegrees);
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
    // actionLoadModelFromDirectory - always enabled
    // Model actions - always enabled (can switch before loading config)

    // These should be disabled without loaded simulation data:
    ui->actionShow_config_details->setEnabled(enabled);
    ui->actionExport_Video->setEnabled(enabled);
    ui->actionReloadData->setEnabled(enabled);
    ui->sliceControlsWidget->setEnabled(enabled);
    ui->actionCrossSectionControls->setEnabled(
        enabled && ui->sceneWidget->hasSliceable3DView());

    // Native 3D models cannot be flattened; 2D models retain optional 3D substates.
    ui->action2DMode->setEnabled(!ui->sceneWidget->isNative3DModel());
    ui->action3DMode->setEnabled(true);

    changeWhichButtonsAreEnabled();
}


void MainWindow::applyCommandLineOptions(const CommandLineParser& cmdParser)
{
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

            // Reload data with the new model only if simulation data was loaded
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

    // Handle autoPlay - automatically start playback when simulation data loads
    if (cmdParser.isAutoPlayRequested())
    {
        // Set flag so we know to exit after playback completes
        shouldExitAfterPlayback = cmdParser.shouldExitAfterLastStep();

        // Start playback automatically
        playingRequested(PlayingDirection::Forward);
    }
}

void MainWindow::updateReductionDisplay()
{
    ui->reductionWidget->updateDisplay(currentStep);
}

void MainWindow::initializeReductionManager(const QString& configFileName, std::shared_ptr<Config> optionalConfig)
{
    ui->reductionWidget->setReductionManager(nullptr);

    // Get reduction settings from SettingParameter
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
        
        // Create ReductionManager with the reduction file path and settings
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
        std::cerr << "Error initializing ReductionManager: " << e.what() << std::endl;
    }
}

void MainWindow::onShowReductionRequested()
{
    // Check if reduction manager is available
    if (!reductionManager || !reductionManager->isAvailable())
    {
        QMessageBox::warning(this, tr("No Reduction Data"),
            tr("Reduction data is not available for the current simulation directory."));
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

    // Limit to MAX_RECENT_DIRECTORIES
    while (recentDirectories.size() > MAX_RECENT_DIRECTORIES)
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

    return buildSimulationDirectoryTooltipHtml(inspectSimulationDirectory(headerPath));
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

void MainWindow::apply3DSubstateSelection(const std::vector<std::string>& fieldNamesTopToBottom)
{
    if (fieldNamesTopToBottom.empty())
    {
        // Show wait cursor during view mode change
        WaitCursorGuard waitCursor("Switching to 2D visualization...");

        if (ui->sceneWidget->isCrossSectionView())
            ui->sceneWidget->clearCrossSection();
        {
            QSignalBlocker blocker(ui->actionCrossSectionControls);
            ui->actionCrossSectionControls->setChecked(false);
        }
        
        // Disable 3D substate visualization
        ui->sceneWidget->setActiveSubstatesFor3D({});
        
        // Clear active substate in MainWindow
        activeSubstatesFor3D.clear();
        ui->substatesDockWidget->setActiveSubstates({});
        
        // Switch back to 2D mode
        on2DModeRequested();
        
        // Refresh visualization
        ui->sceneWidget->refreshVisualization();
        updateSliceControls(true);
        return;
    }
    
    // At least one checkbox is checked - enable 3D mode for this stack.
    // Show wait cursor during view mode change
    WaitCursorGuard waitCursor("Switching to 3D substate visualization...");

    // Store the active substates for 3D visualization in MainWindow
    activeSubstatesFor3D = fieldNamesTopToBottom;

    // Also set them in SceneWidget so it knows which substates to stack in 3D
    ui->sceneWidget->setActiveSubstatesFor3D(fieldNamesTopToBottom);

    // Highlight the active substates in the dock widget
    ui->substatesDockWidget->setActiveSubstates(fieldNamesTopToBottom);

    // Switch to 3D mode
    on3DModeRequested();

    // Initialize and draw the 3D substate visualization (this will create the quad mesh)
    ui->sceneWidget->initializeAndDraw3DSubstateVisualization();

    // Cross-section controls are available for the height field, but remain
    // opt-in through View -> Cross-section controls.
    {
        QSignalBlocker blocker(ui->actionCrossSectionControls);
        ui->actionCrossSectionControls->setChecked(false);
    }
    updateSliceControls(true);
    
    // Cursor restored automatically by WaitCursorGuard destructor
}

void MainWindow::onUse3dStateChanged(const std::string&, bool)
{
    apply3DSubstateSelection(ui->substatesDockWidget->checked3DSubstatesInDisplayOrder());
}

void MainWindow::onUse3dSubstateOrderChanged()
{
    apply3DSubstateSelection(ui->substatesDockWidget->checked3DSubstatesInDisplayOrder());
}

void MainWindow::onUseSubstatesColorringRequested(const std::vector<std::string>& fieldNames)
{
    // Show wait cursor during visualization change
    WaitCursorGuard waitCursor("Updating substate coloring...");

    // Set the active substate for 2D coloring (works in both 2D and 3D modes)
    // In 2D mode: controls the cell coloring
    // In 3D mode: controls the surface coloring (while 3D button controls height)
    ui->sceneWidget->setActiveSubstatesForColorring(fieldNames);

    // Cursor restored automatically by WaitCursorGuard destructor
}

void MainWindow::onDeactivateRequested()
{
    // Show wait cursor during visualization change
    WaitCursorGuard waitCursor("Deactivating substate visualization...");

    // Clear the active substate for 2D visualization in SceneWidget
    ui->sceneWidget->setActiveSubstatesForColorring({});
    
    // Clear highlight from all substates in the dock widget
    ui->substatesDockWidget->setActiveSubstate("");
    
    // Uncheck all use2D checkboxes
    ui->substatesDockWidget->uncheckAllUse2DCheckboxes();
    
    // Immediately refresh visualization to show the change
    ui->sceneWidget->refreshVisualization();
    
    // Cursor restored automatically by WaitCursorGuard destructor
}
