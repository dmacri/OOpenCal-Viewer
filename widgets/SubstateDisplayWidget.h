/** @file SubstateDisplayWidget.h
 * @brief Widget for displaying and editing substate field information.
 * 
 * This widget displays information about a single substate field including
 * its current value, minimum, maximum, and format parameters. */

#pragma once

#include <QWidget>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

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

private:
    /** @brief Setup the UI layout. */
    void setupUI();

    /** @brief Connect signals and slots. */
    void connectSignals();

    std::string m_fieldName;

    QLabel* m_nameLabel;
    QLabel* m_valueLabel;
    QDoubleSpinBox* m_minSpinBox;
    QDoubleSpinBox* m_maxSpinBox;
    QLineEdit* m_formatLineEdit;
    QPushButton* m_use3dButton;
};
