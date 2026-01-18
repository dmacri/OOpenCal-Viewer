/** @file CompilationSettingsWidget.cpp
 * @brief Implementation of CompilationSettingsWidget for displaying C++ compilation settings. */

#include <QTableWidgetItem>
#include <QHeaderView>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>

#include "CompilationSettingsWidget.h"
#include "ui_CompilationSettingsWidget.h"

#include "plugins/CppModuleBuilder.h"
#include "plugins/ModelLoader.h"


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
    if (! m_moduleBuilder)
    {
        // Set default values when no builder is available
        ui->compilerValueLabel->setText("clang++ (default)");
        ui->oopencalPathValueLabel->setText("Not available");
        ui->projectPathValueLabel->setText("Not available");
        ui->commandTextEdit->setPlainText("No module builder available");
        return;
    }

    // Load compiler settings
    QString compilerPath = QString::fromStdString(m_moduleBuilder->getCompilerPath());
    ui->compilerValueLabel->setText(compilerPath.isEmpty() ? "Default (clang++)" : compilerPath);

    // Load OOpenCAL path
    QString oopencalDir = QString::fromStdString(m_moduleBuilder->getProjectRootPath());
    ui->oopencalPathValueLabel->setText(oopencalDir.isEmpty() ? "Not set" : oopencalDir);

    // Load project root path
    QString projectRoot = QString::fromStdString(m_moduleBuilder->getProjectRootPath());
    ui->projectPathValueLabel->setText(projectRoot.isEmpty() ? "Not set" : projectRoot);

    // Update additional paths based on project root
    if (!
        projectRoot.isEmpty())
    {
        QString additionalPaths = QString("-I\"%1/visualiserProxy\" -I\"%1/config\"").arg(projectRoot);
        ui->additionalPathsValueLabel->setText(additionalPaths);
    }
    else
    {
        ui->additionalPathsValueLabel->setText("-I\"<project_root>/visualiserProxy\" -I\"<project_root>/config\"");
    }

    // Generate example command
    QString command = generateExampleCommand();
    ui->commandTextEdit->setPlainText(command);
}

QString CompilationSettingsWidget::generateExampleCommand()
{
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

    QString command = QString("%1 -shared -fPIC -std=%2").arg(compiler, standard);

    if (oopencalPath != "Not set" && !oopencalPath.isEmpty())
    {
        command += QString(" -I\"%1/OOpenCAL/base\" -I\"%1\"").arg(oopencalPath);
    }

    if (projectRoot != "Not set" && !projectRoot.isEmpty())
    {
        command += QString(" -I\"%1\" -I\"%1/visualiserProxy\" -I\"%1/config\"").arg(projectRoot);
    }

    command += " <vtk_flags> \"<source_file>\" -o \"<output_file>\"";

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
