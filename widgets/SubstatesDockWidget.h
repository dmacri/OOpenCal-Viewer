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

signals:
    /** @brief Signal emitted when a field is requested to be used as 3rd dimension.
     * 
     * @param fieldName The name of the field */
    void use3rdDimensionRequested(const std::string& fieldName);

private:
    /** @brief Clear all substate widgets. */
    void clearWidgets();

    QScrollArea* m_scrollArea;
    QWidget* m_containerWidget;
    QVBoxLayout* m_containerLayout;
    std::map<std::string, SubstateDisplayWidget*> m_substateWidgets;
};
