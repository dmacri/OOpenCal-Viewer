/** @file ColorSettingsDialog.h
 * @brief Provides a dialog for configuring application color settings.
 *
 * This file contains the ColorSettingsDialog class which provides a user interface
 * for modifying the application's color scheme. The dialog allows users to:
 * - Change colors for different UI elements
 * - Preview color changes in real-time
 * - Reset colors to their default values
 * - Apply or discard changes
 *
 * The dialog integrates with ColorSettings to persist changes and notify other components about color updates. */
#pragma once

#include <QColor>
#include <QDialog>

namespace Ui
{
class ColorSettingsDialog;
}

/** @class ColorSettingsDialog
 * @brief Dialog for configuring application color settings.
 *
 * This class provides a user interface for modifying the application's color scheme.
 * It allows users to customize various colors used in the application's UI and
 * provides immediate visual feedback for the changes.
 *
 * The dialog communicates with the ColorSettings singleton to:
 * - Load current color settings
 * - Apply new color settings
 * - Reset to default colors
 *
 * @note The dialog uses Qt's color picker for color selection and provides preview functionality to see changes before applying them. */
class ColorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorSettingsDialog(QWidget* parent = nullptr);
    ~ColorSettingsDialog();

private slots:
    void onBackgroundColorClicked();
    void onTextColorClicked();
    void onGridColorClicked();
    void onHighlightColorClicked();
    void onFlatSceneBackgroundColorClicked();
    void onResetColors();
    void onAccepted();
    void onRejected();
    void updateColorPreviews();

protected:
    void updateColorButton(QPushButton* button, const QColor& color);
    bool selectColor(QColor& color, const QString& title);
    void loadSettings();
    void saveSettings();

private:
    Ui::ColorSettingsDialog* ui;

    QColor m_backgroundColor;
    QColor m_textColor;
    QColor m_gridColor;
    QColor m_highlightColor;
    QColor m_flatSceneBackgroundColor;
};
