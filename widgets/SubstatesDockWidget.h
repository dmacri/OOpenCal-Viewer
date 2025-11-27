/** @file SubstatesDockWidget.h
 * @brief Dockable widget for displaying and managing substate fields.
 * 
 * This widget provides a dockable area that displays all available substate fields
 * with their values and editable parameters. */

#pragma once

#include <map>
#include <string>
#include <QDockWidget>
#include <QScrollArea>
#include <QVBoxLayout>

class SubstateDisplayWidget;
class SettingParameter;

/** @class SubstatesDockWidget
 * @brief Dockable widget for managing substate field display.
 * 
 * This widget contains SubstateDisplayWidget instances for each available substate field.
 * It allows users to view and edit parameters for each field. */
class SubstatesDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    /** @brief Constructs a SubstatesDockWidget.
     * 
     * @param parent Parent widget */
    explicit SubstatesDockWidget(QWidget* parent = nullptr);

    /** @brief Initialize widget from UI (must be called after ui->setupUi).
     * 
     * Finds and initializes scroll area and container layout from UI. */
    void initializeFromUI();

    /** @brief Update the substate widgets based on SettingParameter.
     * 
     * Creates or updates SubstateDisplayWidget instances for each field in substates.
     * 
     * @param settingParameter The setting parameter containing substate information */
    void updateSubstates(SettingParameter* settingParameter);

    /** @brief Update cell values for the current position.
     * 
     * Updates the displayed values in all substate widgets and displays row/col indices.
     * 
     * @param settingParameter The setting parameter
     * @param row Grid row index
     * @param col Grid column index
     * @param visualizer Optional visualizer to get cell values from */
    void updateCellValues(SettingParameter* settingParameter, int row, int col, class ISceneWidgetVisualizer* visualizer = nullptr);

    /** @brief Save current parameter values to SettingParameter.
     * 
     * Saves min, max, and format values from all widgets back to substateInfo.
     * 
     * @param settingParameter The setting parameter to update */
    void saveParametersToSettings(SettingParameter* settingParameter);

    /** @brief Set active substate widget (highlights it).
     * 
     * @param fieldName The name of the field to highlight, or empty to clear all */
    void setActiveSubstate(const std::string& fieldName);

signals:
    /** @brief Signal emitted when a field is requested to be used as 3rd dimension.
     * 
     * @param fieldName The name of the field */
    void use3rdDimensionRequested(const std::string& fieldName);

    /** @brief Signal emitted when a field is requested to be used as 2D visualization.
     * 
     * @param fieldName The name of the field */
    void use2DRequested(const std::string& fieldName);

    /** @brief Signal emitted when deactivation of substate is requested.
     * 
     * Requests to deactivate any active substate (use default colors) */
    void deactivateRequested();

    /** @brief Signal emitted when visualization needs to be refreshed.
     * 
     * Emitted when colors, min/max values, or other visualization settings change. */
    void visualizationRefreshRequested();

private slots:
    /** @brief Handle min/max value changes from SubstateDisplayWidget.
     * 
     * Updates the substateInfo in SettingParameter when user manually changes min/max values.
     * 
     * @param fieldName The name of the field
     * @param minValue The new minimum value (or NaN if not set)
     * @param maxValue The new maximum value (or NaN if not set) */
    void onMinMaxValuesChanged(const std::string& fieldName, double minValue, double maxValue);

    /** @brief Calculate and set minimum value for a field.
     * 
     * @param fieldName The name of the field */
    void onCalculateMinimumRequested(const std::string& fieldName);

    /** @brief Calculate and set minimum value > 0 for a field.
     * 
     * @param fieldName The name of the field */
    void onCalculateMinimumGreaterThanZeroRequested(const std::string& fieldName);

    /** @brief Calculate and set maximum value for a field.
     * 
     * @param fieldName The name of the field */
    void onCalculateMaximumRequested(const std::string& fieldName);

    /** @brief Handle deactivate button click */
    void onDeactivateClicked();

    /** @brief Handle color changes from SubstateDisplayWidget.
     * 
     * Updates the substateInfo in SettingParameter when user changes colors.
     * 
     * @param fieldName The name of the field
     * @param minColor The new minimum color (hex string or empty)
     * @param maxColor The new maximum color (hex string or empty) */
    void onColorsChanged(const std::string& fieldName, const std::string& minColor, const std::string& maxColor);

    /** @brief Handle visualization refresh request from SubstateDisplayWidget.
     * 
     * Refreshes the visualization when colors, min/max values, or other settings change. */
    void onVisualizationRefreshRequested();

    /** @brief Handle noValue changes from SubstateDisplayWidget.
     * 
     * Updates the substateInfo in SettingParameter when user changes noValue.
     * 
     * @param fieldName The name of the field
     * @param noValue The new noValue (or NaN if not set)
     * @param isEnabled True if noValue checkbox is checked */
    void onNoValueChanged(const std::string& fieldName, double noValue, bool isEnabled);

private:
    /** @brief Clear all substate widgets. */
    void clearWidgets();

    QScrollArea* m_scrollArea;
    QWidget* m_containerWidget;
    QVBoxLayout* m_containerLayout;
    class QPushButton* m_deactivateButton;
    std::map<std::string, SubstateDisplayWidget*> m_substateWidgets;
    SettingParameter* m_currentSettingParameter = nullptr;
    class ISceneWidgetVisualizer* m_currentVisualizer = nullptr;
};
