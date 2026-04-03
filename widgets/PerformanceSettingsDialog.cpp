#include "PerformanceSettingsDialog.h"
#include "ui_PerformanceSettingsDialog.h"
#include <QSettings>
#include "data/PerformanceMetrics.h"

PerformanceSettingsDialog::PerformanceSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PerformanceSettingsDialog)
{
    ui->setupUi(this);

    // Connect buttons
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &PerformanceSettingsDialog::onOkClicked);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &PerformanceSettingsDialog::onCancelClicked);

    // Load current settings
    loadSettings();
}

PerformanceSettingsDialog::~PerformanceSettingsDialog() = default;

void PerformanceSettingsDialog::loadSettings()
{
    QSettings settings;

    // Load enable/disable state
    const bool metricsEnabled = settings.value("metrics/enabled", true).toBool();
    ui->enableMetricsCheckBox->setChecked(metricsEnabled);

    // Load reporting mode
    const QString mode = settings.value("metrics/mode", "all").toString();
    updateModeComboBox();
    
    int index = ui->modeComboBox->findData(mode);
    if (index >= 0)
        ui->modeComboBox->setCurrentIndex(index);
    
    // Disable mode selection if metrics are disabled
    ui->modeComboBox->setEnabled(metricsEnabled);
    ui->modeLabel->setEnabled(metricsEnabled);
}

void PerformanceSettingsDialog::updateModeComboBox()
{
    if (ui->modeComboBox->count() == 0)
    {
        ui->modeComboBox->addItem("All (per-step + summary)", "all");
        ui->modeComboBox->addItem("Summary Only", "summary");
        ui->modeComboBox->addItem("Steps Only (no summary)", "steps");
    }
}

void PerformanceSettingsDialog::onOkClicked()
{
    saveSettings();
    accept();
}

void PerformanceSettingsDialog::onCancelClicked()
{
    reject();
}

void PerformanceSettingsDialog::saveSettings()
{
    QSettings settings;

    const bool metricsEnabled = ui->enableMetricsCheckBox->isChecked();
    settings.setValue("metrics/enabled", metricsEnabled);

    // Get selected reporting mode
    const QString mode = ui->modeComboBox->currentData().toString();
    settings.setValue("metrics/mode", mode);

    // Apply to runtime
    PerformanceMetrics::setEnabled(metricsEnabled);
    
    if (mode == "summary")
    {
        PerformanceMetrics::setReportingMode(PerformanceMetrics::ReportingMode::SummaryOnly);
    }
    else if (mode == "steps")
    {
        PerformanceMetrics::setReportingMode(PerformanceMetrics::ReportingMode::StepsOnly);
    }
    else
    {
        PerformanceMetrics::setReportingMode(PerformanceMetrics::ReportingMode::All);
    }
}
