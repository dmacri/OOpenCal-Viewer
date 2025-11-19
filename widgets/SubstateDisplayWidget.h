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
    std::string getFieldName() const { return m_fieldName; }

signals:
    /** @brief Signal emitted when "Use as 3rd dimension" button is clicked.
     * 
     * @param fieldName The name of the field */
    void use3rdDimensionRequested(const std::string& fieldName);

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

protected:
    /// @brief Override context menu event to add custom actions.
    void contextMenuEvent(QContextMenuEvent* event) override;

    /// @brief Override event filter to intercept right-click on child widgets.
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    /// @brief Setup the UI layout.
    void setupUI();

    /// @brief Connect signals and slots.
    void connectSignals();

    /// @brief Update button enabled state based on min/max values.
    void updateButtonState();

    /// @brief Install event filter on all child widgets to catch right-click.
    void installEventFiltersOnChildren();

    std::string m_fieldName;

    QLabel* m_nameLabel;
    QLabel* m_valueLabel;
    QDoubleSpinBox* m_minSpinBox;
    QDoubleSpinBox* m_maxSpinBox;
    QLineEdit* m_formatLineEdit;
    QPushButton* m_use3dButton;
};
