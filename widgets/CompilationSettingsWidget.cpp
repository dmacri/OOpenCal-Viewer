/** @file CompilationSettingsWidget.cpp
 * @brief Implementation of CompilationSettingsWidget for displaying C++ compilation settings. */

#include <QTableWidgetItem>
#include <QHeaderView>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QLabel>
#include <QString>
#include <QTableWidget>
#include <QPushButton>

#include "CompilationSettingsWidget.h"
#include "ui_CompilationSettingsWidget.h"
#include "plugins/CppModuleBuilder.h"
#include "plugins/CompilationConfig.h"


CompilationSettingsWidget::CompilationSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::CompilationSettingsWidget)
{
    ui->setupUi(this);
    setupConnections();
    
    // Initialize with default values
    loadEnvironmentVariables();
    detectCppStandard();
    updateCompilerStatus();
}

CompilationSettingsWidget::~CompilationSettingsWidget()
{
    delete ui;
}

void CompilationSettingsWidget::setModuleBuilder(std::shared_ptr<viz::plugins::CppModuleBuilder> builder)
{
    m_moduleBuilder = builder;
    refreshSettings();
}

void CompilationSettingsWidget::refreshSettings()
{
    loadCompilationSettings();
    loadEnvironmentVariables();
    updateCompilerStatus();
    detectCppStandard();
}

void CompilationSettingsWidget::setupConnections()
{
    connect(ui->refreshButton, &QPushButton::clicked, this, &CompilationSettingsWidget::onRefreshClicked);
    
    connect(ui->environmentTableWidget, &QTableWidget::itemSelectionChanged, this, &CompilationSettingsWidget::onEnvironmentVariableSelectionChanged);
}

void CompilationSettingsWidget::loadCompilationSettings()
{
    auto& config = viz::plugins::CompilationConfig::getInstance();
    
    // Load compiler settings
    QString compilerPath = QString::fromStdString(m_moduleBuilder ? m_moduleBuilder->getCompilerPath() : "clang++");
    ui->compilerValueLabel->setText(compilerPath.isEmpty() ? "Default (clang++)" : compilerPath);

    // Load OOpenCAL directory configuration
    auto oopencalConfig = getOopencalDirConfig();
    ui->oopencalCmakeValueLabel->setText(oopencalConfig.cmakeValue);
    ui->oopencalEnvValueLabel->setText(oopencalConfig.envValue);
    ui->oopencalPathValueLabel->setText(oopencalConfig.currentValue);
    validatePath(oopencalConfig.currentValue, ui->oopencalPathValueLabel, "OOpenCAL Directory");

    // Load viewer root directory configuration
    auto viewerRootConfig = getViewerRootConfig();
    ui->projectCmakeValueLabel->setText(viewerRootConfig.cmakeValue);
    ui->projectEnvValueLabel->setText(viewerRootConfig.envValue);
    ui->projectPathValueLabel->setText(viewerRootConfig.currentValue);
    validatePath(viewerRootConfig.currentValue, ui->projectPathValueLabel, "OOpenCal Viewer Root");

    // Update additional paths based on project root
    if (!viewerRootConfig.currentValue.isEmpty()) {
        QString additionalPaths = QString("-I\"%1/visualiserProxy\" -I\"%1/config\"").arg(viewerRootConfig.currentValue);
        ui->additionalPathsValueLabel->setText(additionalPaths);
    } else {
        ui->additionalPathsValueLabel->setText("-I\"<project_root>/visualiserProxy\" -I\"<project_root>/config\"");
    }

    // Load VTK flags configuration
    auto vtkConfig = getVtkFlagsConfig();
    ui->vtkCmakeValueLabel->setText(vtkConfig.cmakeValue);
    ui->vtkEnvValueLabel->setText(vtkConfig.envValue);
    ui->vtkCurrentValueLabel->setText(vtkConfig.currentValue);

    // Load compilation flags from singleton
    QString flags = QString::fromStdString(config.getCompilationFlags());
    ui->flagsLabel->setText(flags);

    // Generate example command
    QString command = generateExampleCommand();
    ui->commandTextEdit->setPlainText(command);
}

QString CompilationSettingsWidget::generateExampleCommand()
{
    auto& config = viz::plugins::CompilationConfig::getInstance();
    
    QString compiler = ui->compilerValueLabel->text();
    if (compiler == "Default (clang++)")
    {
        compiler = "clang++";
    }

    QString standard = ui->cppStandardValueLabel->text();
    if (standard == "Auto-detect")
    {
        standard = QString::fromStdString(viz::plugins::detectCppStandard());
    }

    QString oopencalPath = ui->oopencalPathValueLabel->text();
    QString projectRoot = ui->projectPathValueLabel->text();

    // Use flags from singleton
    QString flags = QString::fromStdString(config.getCompilationFlags());
    QString vtkFlags = QString::fromStdString(config.getVtkFlags());

    QString command = QString("%1 %2 -std=%3").arg(compiler, flags, standard);

    // Add include paths from singleton
    auto includePaths = config.getIncludePaths();
    for (const auto& path : includePaths) 
    {
        command += " " + QString::fromStdString(path);
    }

    // Add VTK flags if available
    if (! vtkFlags.isEmpty()) 
    {
        command += " " + vtkFlags;
    }

    command += " \"<source_file>\" -o \"<output_file>\"";

    return command;
}

void CompilationSettingsWidget::loadEnvironmentVariables()
{
    // Clear existing items
    ui->environmentTableWidget->setRowCount(0);

    // List of relevant environment variables for compilation
    QStringList relevantVars = {
        "OOPENCAL_DIR",
        "OOPENCAL_VIEWER_ROOT", 
        "PATH",
        "CPLUS_INCLUDE_PATH",
        "C_INCLUDE_PATH",
        "LD_LIBRARY_PATH",
        "PKG_CONFIG_PATH",
        "CC",
        "CXX",
        "CXXFLAGS",
        "LDFLAGS"
    };

    // Add all environment variables
    QStringList envVars = QProcess::systemEnvironment();
    
    ui->environmentTableWidget->setRowCount(envVars.size());
    
    for (int i = 0; i < envVars.size(); ++i)
    {
        QString envVar = envVars[i];
        int separatorPos = envVar.indexOf('=');
        
        QString varName = envVar.left(separatorPos);
        QString varValue = envVar.mid(separatorPos + 1);
        
        // Highlight relevant variables
        bool isRelevant = relevantVars.contains(varName);
        
        QTableWidgetItem* nameItem = new QTableWidgetItem(varName);
        QTableWidgetItem* valueItem = new QTableWidgetItem(varValue);
        
        if (isRelevant) {
            QFont font = nameItem->font();
            font.setBold(true);
            nameItem->setFont(font);
            valueItem->setFont(font);
            
            // Use a light background for relevant variables
            nameItem->setBackground(QColor(230, 240, 250));
            valueItem->setBackground(QColor(230, 240, 250));
        }
        
        ui->environmentTableWidget->setItem(i, 0, nameItem);
        ui->environmentTableWidget->setItem(i, 1, valueItem);
    }

    // Resize columns to content
    ui->environmentTableWidget->resizeColumnsToContents();
    
    // Make the table read-only
    ui->environmentTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void CompilationSettingsWidget::updateCompilerStatus()
{
    QString compilerPath = ui->compilerValueLabel->text();
    
    if (compilerPath == "Default (clang++)" || compilerPath == "Not set")
    {
        compilerPath = "clang++";
    }

    if (viz::plugins::isCompilerAvailable(compilerPath.toStdString()))
    {
        ui->statusValueLabel->setText("Available");
        ui->statusValueLabel->setStyleSheet("color: green;");
    }
    else
    {
        ui->statusValueLabel->setText("Not found");
        ui->statusValueLabel->setStyleSheet("color: red;");
    }
}

void CompilationSettingsWidget::detectCppStandard()
{
    auto detectedStandard = viz::plugins::detectCppStandard();
    ui->cppStandardValueLabel->setText(QString::fromStdString(detectedStandard));
}

void CompilationSettingsWidget::onRefreshClicked()
{
    refreshSettings();
}

void CompilationSettingsWidget::onEnvironmentVariableSelectionChanged()
{
    // This could be used to show detailed information about selected variables
    // For now, it's just a placeholder for future functionality
    QItemSelectionModel* selection = ui->environmentTableWidget->selectionModel();
    if (selection->hasSelection())
    {
        // Could show additional details in a status bar or tooltip
    }
}

CompilationSettingsWidget::ConfigValues CompilationSettingsWidget::getOopencalDirConfig()
{
    ConfigValues values;
    auto& config = viz::plugins::CompilationConfig::getInstance();
    
    // Get CMake value (compile-time)
#ifdef OOPENCAL_DIR
    values.cmakeValue = OOPENCAL_DIR;
#else
    values.cmakeValue = "";
#endif
    
    // Get environment value
    if (const char* envPath = std::getenv("OOPENCAL_DIR")) {
        values.envValue = envPath;
    }
    
    // Get current value (with overrides)
    values.currentValue = QString::fromStdString(config.getOopencalDir());
    
    return values;
}

CompilationSettingsWidget::ConfigValues CompilationSettingsWidget::getViewerRootConfig()
{
    ConfigValues values;
    auto& config = viz::plugins::CompilationConfig::getInstance();
    
    // Get CMake value (compile-time)
#ifdef OOPENCAL_VIEWER_ROOT
    values.cmakeValue = OOPENCAL_VIEWER_ROOT;
#else
    values.cmakeValue = "";
#endif
    
    // Get environment value
    if (const char* envPath = std::getenv("OOPENCAL_VIEWER_ROOT")) {
        values.envValue = envPath;
    }
    
    // Get current value (with overrides)
    values.currentValue = QString::fromStdString(config.getViewerRootDir());
    
    return values;
}

CompilationSettingsWidget::ConfigValues CompilationSettingsWidget::getVtkFlagsConfig()
{
    ConfigValues values;
    auto& config = viz::plugins::CompilationConfig::getInstance();
    
    // Get CMake value (compile-time)
#ifdef VTK_COMPILE_FLAGS
    values.cmakeValue = VTK_COMPILE_FLAGS;
#else
    values.cmakeValue = "";
#endif
    
    // Get environment value (VTK flags usually not from env)
    values.envValue = ""; // VTK flags are typically compile-time only
    
    // Get current value (with overrides)
    values.currentValue = QString::fromStdString(config.getVtkFlags());
    
    return values;
}

void CompilationSettingsWidget::validatePath(const QString& path, QLabel* label, const QString& fieldName)
{
    if (path.isEmpty() || path == "Not set") {
        label->setStyleSheet("");
        label->setToolTip("");
        return;
    }
    
    QDir dir(path);
    if (!dir.exists()) {
        label->setStyleSheet("color: red; background-color: #ffe6e6;");
        label->setToolTip(QString("Path does not exist: %1").arg(path));
    } else {
        label->setStyleSheet("color: green;");
        label->setToolTip(QString("Path exists: %1").arg(path));
    }
}
