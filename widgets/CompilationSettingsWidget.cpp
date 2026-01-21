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
#include <cstdlib>

#include "CompilationSettingsWidget.h"
#include "ui_CompilationSettingsWidget.h"
#include "plugins/CppModuleBuilder.h"
#include "plugins/CompilationConfig.h"


namespace
{
constexpr const char defaultCompiler[] = "clang++";
} // namespace


CompilationSettingsWidget::CompilationSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::CompilationSettingsWidget)
    , m_moduleBuilder(nullptr)
{
    ui->setupUi(this);
    
    // Initialize with default values
    loadEnvironmentVariables();
    detectCppStandard();
    updateCompilerStatus();
    
    // Connect environment group box toggle signal
    connect(ui->environmentGroupBox, &QGroupBox::clicked, this, [this]() {
        toggleEnvironmentGroup();
    });
    
    // Set initial arrow state to collapsed (hidden)
    updateEnvironmentGroupArrow(false);
    ui->environmentTableWidget->setVisible(false);
    
    setupConnections();
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
    QString compilerPath = QString::fromStdString(m_moduleBuilder ? m_moduleBuilder->getCompilerPath() : defaultCompiler);
    ui->compilerValueLabel->setText(compilerPath.isEmpty() ? defaultCompiler : compilerPath);
    
    // Validate compiler availability and update status
    validateCompilerAvailability(compilerPath);

    // Setup configuration table with 3 rows
    setupConfigTable(ui->configTableWidget);

    // Update additional paths based on project root
    auto viewerRootConfig = getViewerRootConfig();
    if (!viewerRootConfig.currentValue.isEmpty()) {
        QString additionalPaths = QString("-I\"%1/visualiserProxy\" -I\"%1/config\"").arg(viewerRootConfig.currentValue);
        ui->additionalPathsValueLabel->setText(additionalPaths);
    } else {
        ui->additionalPathsValueLabel->setText("-I\"<project_root>/visualiserProxy\" -I\"<project_root>/config\"");
    }

    // Load compilation flags from singleton
    QString flags = QString::fromStdString(config.getCompilationFlags());
    ui->flagsLabel->setText(flags);

    // Generate example command
    QString command = generateExampleCommand();
    ui->commandTextEdit->setPlainText(command);
}

void CompilationSettingsWidget::setupConfigTable(QTableWidget* table)
{
    table->setRowCount(3);
    table->setColumnCount(4);
    
    // Set headers
    QStringList headers;
    headers << "Variable" << "Compilation Value" << "Environment Variable" << "Current Value";
    table->setHorizontalHeaderLabels(headers);
    
    // Get configuration values
    auto oopencalConfig = getOopencalDirConfig();
    auto viewerRootConfig = getViewerRootConfig();
    auto vtkConfig = getVtkFlagsConfig();
    
    // Row 0: OOPENCAL_DIR
    table->setItem(0, 0, new QTableWidgetItem("OOPENCAL_DIR"));
    auto* oopencalCmakeItem = new QTableWidgetItem(oopencalConfig.cmakeValue);
    table->setItem(0, 1, oopencalCmakeItem);
    validatePath(oopencalConfig.cmakeValue, oopencalCmakeItem, "OOPENCAL_DIR");
    
    auto* oopencalEnvItem = new QTableWidgetItem(oopencalConfig.envValue);
    table->setItem(0, 2, oopencalEnvItem);
    validatePath(oopencalConfig.envValue, oopencalEnvItem, "OOPENCAL_DIR");
    
    auto* oopencalCurrentItem = new QTableWidgetItem(oopencalConfig.currentValue);
    table->setItem(0, 3, oopencalCurrentItem);
    validatePath(oopencalConfig.currentValue, oopencalCurrentItem, "OOPENCAL_DIR");
    
    // Row 1: OOPENCAL_VIEWER_ROOT
    table->setItem(1, 0, new QTableWidgetItem("OOPENCAL_VIEWER_ROOT"));
    auto* viewerRootCmakeItem = new QTableWidgetItem(viewerRootConfig.cmakeValue);
    table->setItem(1, 1, viewerRootCmakeItem);
    validatePath(viewerRootConfig.cmakeValue, viewerRootCmakeItem, "OOPENCAL_VIEWER_ROOT");
    
    auto* viewerRootEnvItem = new QTableWidgetItem(viewerRootConfig.envValue);
    table->setItem(1, 2, viewerRootEnvItem);
    validatePath(viewerRootConfig.envValue, viewerRootEnvItem, "OOPENCAL_VIEWER_ROOT");
    
    auto* viewerRootItem = new QTableWidgetItem(viewerRootConfig.currentValue);
    table->setItem(1, 3, viewerRootItem);
    validatePath(viewerRootConfig.currentValue, viewerRootItem, "OOPENCAL_VIEWER_ROOT");
    
    // Row 2: VTK_INCLUDES (no path validation for flags)
    table->setItem(2, 0, new QTableWidgetItem("VTK_INCLUDES"));
    table->setItem(2, 1, new QTableWidgetItem(vtkConfig.cmakeValue));
    table->setItem(2, 2, new QTableWidgetItem(vtkConfig.envValue));
    auto* vtkItem = new QTableWidgetItem(vtkConfig.currentValue);
    table->setItem(2, 3, vtkItem);
    
    // Make first three columns read-only, but allow editing in Current Value column
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            if (auto* item = table->item(row, col))
            {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        }
    }
    
    // Connect cell changed signal for validation updates
    connect(table, &QTableWidget::cellChanged, this, [this, table](int row, int column) {
        if (column == 3) // Only Current Value column
        {
            auto* item = table->item(row, column);
            if (item)
            {
                QString variableName = table->item(row, 0)->text();
                validatePath(item->text(), item, variableName);
                
                // Update the configuration in singleton without causing recursion
                updateConfigValue(variableName, item->text());
            }
        }
    });
    
    // Block signals temporarily to prevent recursion during initial setup
    table->blockSignals(true);
    
    // Resize columns to content
    table->resizeColumnsToContents();
    
    // Unblock signals after initial setup
    table->blockSignals(false);
}

void CompilationSettingsWidget::updateConfigValue(const QString& variableName, const QString& value)
{
    auto& config = viz::plugins::CompilationConfig::getInstance();
    
    if (variableName == "OOPENCAL_DIR")
    {
        config.setOopencalDir(value.toStdString());
    }
    else if (variableName == "OOPENCAL_VIEWER_ROOT")
    {
        config.setViewerRootDir(value.toStdString());
    }
    else if (variableName == "VTK_INCLUDES")
    {
        config.setVtkFlags(value.toStdString());
    }
    
    // Update only the additional paths and example command - don't reload the whole table
    auto viewerRootConfig = getViewerRootConfig();
    if (! viewerRootConfig.currentValue.isEmpty())
    {
        QString additionalPaths = QString("-I\"%1/visualiserProxy\" -I\"%1/config\"").arg(viewerRootConfig.currentValue);
        ui->additionalPathsValueLabel->setText(additionalPaths);
    }
    else
    {
        ui->additionalPathsValueLabel->setText("-I\"<project_root>/visualiserProxy\" -I\"<project_root>/config\"");
    }
    
    // Update compilation flags and example command
    QString flags = QString::fromStdString(config.getCompilationFlags());
    ui->flagsLabel->setText(flags);
    
    QString command = generateExampleCommand();
    ui->commandTextEdit->setPlainText(command);
}

void CompilationSettingsWidget::validateCompilerAvailability(const QString& compilerPath)
{
    QString resolvedPath;

    // Try to resolve compiler path:
    // 1. If a path was provided, check it directly
    // 2. Otherwise, try to find it in PATH (e.g. default compiler like "gcc")
    if (! compilerPath.isEmpty())
    {
        QFileInfo compilerInfo(compilerPath);
        if (compilerInfo.exists() && compilerInfo.isExecutable())
        {
            resolvedPath = compilerInfo.absoluteFilePath();
        }
        else
        {
            resolvedPath = QStandardPaths::findExecutable(compilerPath);
        }
    }

    // Compiler found
    if (! resolvedPath.isEmpty())
    {
        ui->compilerValueLabel->setStyleSheet("color: green; background-color: #e6ffe6;");
        ui->compilerValueLabel->setToolTip(tr("Compiler is available: %1").arg(resolvedPath));
    }
    // Compiler not found
    else
    {
        ui->compilerValueLabel->setStyleSheet("color: red; background-color: #ffe6e6;");
        ui->compilerValueLabel->setToolTip(tr("Compiler not found: %1").arg(compilerPath));
    }
}

QString CompilationSettingsWidget::generateExampleCommand()
{
    auto& config = viz::plugins::CompilationConfig::getInstance();
    
    QString compiler = ui->compilerValueLabel->text();
    QString standard = ui->cppStandardValueLabel->text();
    if (standard == "Auto-detect")
    {
        standard = QString::fromStdString(viz::plugins::detectCppStandard());
    }

    QString oopencalPath = "";
    QString projectRoot = "";
    
    // Get current values from the unified config table
    if (ui->configTableWidget->item(0, 3))
    {
        oopencalPath = ui->configTableWidget->item(0, 3)->text();
    }
    if (ui->configTableWidget->item(1, 3))
    {
        projectRoot = ui->configTableWidget->item(1, 3)->text();
    }

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
    
    if (compilerPath == "Not set")
    {
        compilerPath = defaultCompiler;
    }

    validateCompilerAvailability(compilerPath);
}

void CompilationSettingsWidget::detectCppStandard()
{
    auto detectedStandard = viz::plugins::detectCppStandard();
    ui->cppStandardValueLabel->setText(QString::fromStdString(detectedStandard));
}

void CompilationSettingsWidget::toggleEnvironmentGroup()
{
    bool isExpanded = ui->environmentGroupBox->isChecked();
    updateEnvironmentGroupArrow(isExpanded);
    
    // Show/hide the table widget
    ui->environmentTableWidget->setVisible(isExpanded);
}

void CompilationSettingsWidget::updateEnvironmentGroupArrow(bool isExpanded)
{
    QString title = isExpanded ? "Environment Variables ▼" : "Environment Variables ▶";
    ui->environmentGroupBox->setTitle(title);
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
#ifdef VTK_INCLUDES
    values.cmakeValue = VTK_INCLUDES;
#else
    values.cmakeValue = "";
#endif
    
    // Get environment value (VTK includes from environment variable)
    const char* envVtk = std::getenv("VTK_INCLUDES");
    values.envValue = envVtk ? QString(envVtk) : "";
    
    // Get current value (with overrides) - show clean paths without -I
    values.currentValue = QString::fromStdString(config.getVtkIncludePaths());
    
    return values;
}

void CompilationSettingsWidget::validatePath(const QString& path, QTableWidgetItem* item, const QString& fieldName)
{
    if (!item) return;
    
    if (path.isEmpty() || path == "Not set") {
        item->setBackground(QBrush());
        item->setToolTip("");
        return;
    }
    
    QDir dir(path);
    bool exists = dir.exists();
    
    if (!exists)
    {
        item->setBackground(QColor("#ffe6e6")); // Light red
        item->setToolTip(QString("Path does not exist: %1").arg(path));
    }
    else
    {
        item->setBackground(QColor("#e6ffe6")); // Light green
        item->setToolTip(QString("Path exists: %1").arg(path));
    }
}
