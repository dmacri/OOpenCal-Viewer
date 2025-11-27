/** @file SubstateDisplayWidget.h
 * @brief Widget for displaying and editing substate field information.
 * 
 * This widget displays information about a single substate field including
 * its current value, minimum, maximum, and format parameters. */

#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QDoubleSpinBox;
class QPushButton;
class SettingParameter;

namespace Ui
{
class SubstateDisplayWidget;
}

/** @class SubstateDisplayWidget
 * @brief Widget for displaying a single substate field with editable parameters.
 * 
 * This widget shows:
 * - Field name
 * - Current cell value (read-only)
 * - Minimum value (editable)
 * - Maximum value (editable)
 * - Format string (editable)
 * - Button to use as 3rd dimension */
class SubstateDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    /** @brief Constructs a SubstateDisplayWidget.
     * 
     * @param fieldName The name of the substate field (e.g., "h", "z")
     * @param parent Parent widget */
    explicit SubstateDisplayWidget(const std::string& fieldName, QWidget* parent = nullptr);

    ~SubstateDisplayWidget();

    /** @brief Update the displayed cell value.
     * 
     * @param value The new cell value to display */
    void setCellValue(const std::string& value);

    /** @brief Get the current minimum value.
     * 
     * @return Minimum value as double, or NaN if empty */
    double getMinValue() const;

    /** @brief Set the minimum value.
     * 
     * @param value The minimum value (use NaN for empty) */
    void setMinValue(double value);

    /** @brief Get the current maximum value.
     * 
     * @return Maximum value as double, or NaN if empty */
    double getMaxValue() const;

    /** @brief Set the maximum value.
     * 
     * @param value The maximum value (use NaN for empty) */
    void setMaxValue(double value);

    /** @brief Get the format string.
     * 
     * @return Format string (e.g., "f", "0.6f", "d") without % prefix */
    std::string getFormat() const;

    /** @brief Set the format string.
     * 
     * @param format The format string (without % prefix) */
    void setFormat(const std::string& format);

    /** @brief Check if min value is set.
     * 
     * @return True if min value is set (not empty) */
    bool hasMinValue() const;

    /** @brief Check if max value is set.
     * 
     * @return True if max value is set (not empty) */
    bool hasMaxValue() const;

    /** @brief Get the field name.
     * 
     * @return Field name */
    std::string fieldName() const;

    /// @brief Set the min color for this substate
    /// @param color Hex color string (e.g., "#FF0000"), or empty string to disable
    void setMinColor(const std::string& color);

    /// @brief Set the max color for this substate
    /// @param color Hex color string (e.g., "#0000FF"), or empty string to disable
    void setMaxColor(const std::string& color);

    /// @brief Set widget as active (highlighted background).
    /// @param active True to highlight, false to remove highlight
    void setActive(bool active);

signals:
    /** @brief Signal emitted when "Use as 3rd dimension" button is clicked.
     * 
     * @param fieldName The name of the field */
    void use3rdDimensionRequested(const std::string& fieldName);

    /** @brief Signal emitted when "Use as 2D" button is clicked.
     * 
     * @param fieldName The name of the field */
    void use2DRequested(const std::string& fieldName);

    /** @brief Signal emitted when min or max values change.
     * 
     * @param fieldName The name of the field
     * @param minValue The new minimum value (or NaN if not set)
     * @param maxValue The new maximum value (or NaN if not set) */
    void minMaxValuesChanged(const std::string& fieldName, double minValue, double maxValue);

    /** @brief Signal emitted when user requests to calculate minimum value.
     * 
     * @param fieldName The name of the field */
    void calculateMinimumRequested(const std::string& fieldName);

    /** @brief Signal emitted when user requests to calculate minimum > 0.
     * 
     * @param fieldName The name of the field */
    void calculateMinimumGreaterThanZeroRequested(const std::string& fieldName);

    /** @brief Signal emitted when user requests to calculate maximum value.
     * 
     * @param fieldName The name of the field */
    void calculateMaximumRequested(const std::string& fieldName);

    /** @brief Signal emitted when min or max colors change.
     * 
     * @param fieldName The name of the field
     * @param minColor The new minimum color (hex string or empty)
     * @param maxColor The new maximum color (hex string or empty) */
    void colorsChanged(const std::string& fieldName, const std::string& minColor, const std::string& maxColor);

    /** @brief Signal emitted when visualization needs to be refreshed.
     * 
     * Emitted when colors, min/max values, or other visualization settings change. */
    void visualizationRefreshRequested();

private slots:
    /** @brief Calculate and set minimum value from all cells in current step.
     * 
     * Finds the minimum value across all cells and sets it as the min value. */
    void onCalculateMinimum();

    /** @brief Calculate and set minimum value > 0 from all cells in current step.
     * 
     * Finds the minimum value > 0 across all cells and sets it as the min value. */
    void onCalculateMinimumGreaterThanZero();

    /** @brief Calculate and set maximum value from all cells in current step.
     * 
     * Finds the maximum value across all cells and sets it as the max value. */
    void onCalculateMaximum();

    /// @brief This is sum of onCalculateMinimumGreaterThanZero() and onCalculateMaximum()
    void onCalculateMinimumGreaterThanZeroAndMaximum();

    /// @brief Handle "Use as 2D" button click
    void onUse2DClicked();

protected:
    /// @brief Override context menu event to add custom actions.
    void contextMenuEvent(QContextMenuEvent* event) override;

    /// @brief Override event filter to intercept right-click on child widgets.
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    /// @brief Connect signals to slots.
    void connectSignals();

    /// @brief Handle min color button click
    void onMinColorClicked();

    /// @brief Handle max color button click
    void onMaxColorClicked();

    /// @brief Handle clear colors button click
    void onClearColorsClicked();

    /// @brief Update color button appearance based on current color
    void updateColorButtonAppearance();

    /// @brief Update button enabled state based on min/max values.
    void updateButtonState();

    /// @brief Install event filter on all child widgets to catch right-click.
    void installEventFiltersOnChildren();

    /// @brief Handle focus out on min spinbox to trigger refresh
    void onMinSpinBoxFocusOut();

    /// @brief Handle focus out on max spinbox to trigger refresh
    void onMaxSpinBoxFocusOut();

    Ui::SubstateDisplayWidget *ui;
    
    // Current colors (empty string = inactive)
    std::string m_minColor;
    std::string m_maxColor;
};
