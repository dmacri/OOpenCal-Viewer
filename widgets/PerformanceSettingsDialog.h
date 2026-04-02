/** @file PerformanceSettingsDialog.h
 * @brief Dialog for configuring performance metrics settings.
 *
 * Allows users to enable/disable metrics and select reporting mode. */

#pragma once

#include <QDialog>

namespace Ui { class PerformanceSettingsDialog; }

/** @class PerformanceSettingsDialog
 * @brief Dialog for configuring performance metrics collection and reporting. */
class PerformanceSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PerformanceSettingsDialog(QWidget *parent = nullptr);
    ~PerformanceSettingsDialog();

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    void loadSettings();
    void saveSettings();
    void updateModeComboBox();

    Ui::PerformanceSettingsDialog *ui;
};
