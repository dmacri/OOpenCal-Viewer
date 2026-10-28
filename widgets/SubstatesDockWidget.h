/** @file SubstatesDockWidget.h
 * @brief Dockable widget for displaying and managing substate fields.
 * 
 * This widget provides a dockable area that displays all available substate fields
 * with their values and editable parameters. */

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <QDockWidget>
#include "core/types.h" // StepIndex
#include "visualiserProxy/ContiguousGrid.h" // GridCellIndex
#include <QScrollArea>
#include <QVBoxLayout>

class QDragEnterEvent;
class QDropEvent;

class SubstateDisplayWidget;
class CellHistoryDialog;
class QPushButton;
struct SettingParameter;

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

    /** @brief Update cell values for a cell clicked in the complete 3D volume of a native 3D model.
     *
     *  Like updateCellValues(), but the cell is given by its position in the whole grid (row, column
     *  and layer), not in the displayed plane: the volume is not a cross-section. The header shows
     *  all three coordinates and the plot of the history follows the same cell.
     *
     *  @param settingParameter The setting parameter
     *  @param cell Position of the cell in the whole 3D grid
     *  @param visualizer Visualizer to get cell values from */
    void updateVolumeCellValues(SettingParameter* settingParameter, const GridCellIndex& cell, class ISceneWidgetVisualizer* visualizer);

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

    /** @brief Set highlighted substate widgets.
     *
     * Used by stacked 3D altitude visualization where more than one substate
     * can contribute to the height field.
     *
     * @param fieldNames Field names to highlight */
    void setActiveSubstates(const std::vector<std::string>& fieldNames);

    /** @brief Get the currently active substate widget.
     * 
     * @return Pointer to active SubstateDisplayWidget, or nullptr if none is active */
    class SubstateDisplayWidget* getActiveSubstateWidget() const;

    /** @brief Return all checked 3D altitude substates in current dock order.
     *
     * The order is top-to-bottom as displayed in the dock widget.
     *
     * @return Checked substate field names in display order */
    std::vector<std::string> checked3DSubstatesInDisplayOrder() const;

    /** @brief Inform the dock which simulation step is shown in the scene.
     *
     * The step is marked on the plot of the cell history, if that window is open.
     *
     * @param step The step shown in the scene */
    void setCurrentStep(StepIndex step);

signals:
    /** @brief Signal emitted when a field's 3D state changes.
     * 
     * @param fieldName The name of the field
     * @param checked True if 3D is now enabled, false if disabled */
    void use3dStateChanged(const std::string& fieldName, bool checked);

    /** @brief Signal emitted when checked 3D substates keep their state but change order. */
    void use3dSubstateOrderChanged();

    /// @brief Signal emitted when a field is requested to be used in colorring. It is returning names of all selected substates
    void useSubstatesColorringRequested(const std::vector<std::string>& fieldNames);

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

    /** @brief Handle click of "Plot over time...": read the history of the selected cell and show it */
    void onPlotHistoryClicked();
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

    /** @brief Handle noValue changes from SubstateDisplayWidget.
     * 
     * Updates the substateInfo in SettingParameter when user changes noValue.
     * 
     * @param fieldName The name of the field
     * @param noValue The new noValue (or NaN if not set)
     * @param isEnabled True if noValue checkbox is checked */
    void onNoValueChanged(const std::string& fieldName, double noValue, bool isEnabled);

    /** @brief Handle optional 3D altitude scale changes from SubstateDisplayWidget.
     *
     * @param fieldName The name of the field
     * @param scale Positive multiplier, or NaN if empty/default */
    void onAltitudeScaleChanged(const std::string& fieldName, double scale);

    /** @brief Handle use2D checkbox changes - ensures only one checkbox is checked at a time.
     * 
     * When a checkbox is checked, unchecks all other checkboxes.
     * 
     * @param fieldName The name of the field whose checkbox changed */
    void onUse2DCheckboxChanged(const std::string& fieldName);

    /** @brief Handle 3D checkbox state changes.
     *
     * Multiple substates can be checked at the same time and are stacked in
     * current dock order.
     * 
     * @param fieldName The name of the field whose 3D checkbox changed
     * @param checked True if checkbox is now checked, false if unchecked */
    void onUse3DStateChanged(const std::string& fieldName, bool checked);

public slots:
    /// @brief Uncheck all use2D checkboxes. */
    void uncheckAllUse2DCheckboxes();

    /** @brief Whether the panel stays open when the scene is clicked outside of any cell.
     *
     *  A panel opened by clicking a cell is hidden again by a click on the background. A panel opened from the
     *  menu (View -> Substate parameters) is not: it is needed to set up the substates, e.g. when nothing is
     *  visible yet to click on, and is closed from the menu or with its close button. */
    bool isKeptOpen() const { return m_keptOpen; }

    /// @brief Sets whether the panel stays open when the background of the scene is clicked (see isKeptOpen())
    void setKeptOpen(bool keptOpen) { m_keptOpen = keptOpen; }

    /** @brief Uncheck all "3D" checkboxes without notifying anybody (the caller redraws the scene itself). */
    void uncheckAllUse3DCheckboxes();

    /** @brief Whether the substates of a native 3D model are shown (see SubstateDisplayWidget::setNative3DVolumeMode()).
     *
     *  Applies to the widgets which exist now and to the ones created by updateSubstates() later.
     *  @param enabled true for a native 3D model */
    void setNative3DVolumeMode(bool enabled);

    /// @brief Informs about changes in substat colloring
    void onUseSubstateColorringRequested(const std::string& fieldName);

protected:
    /// @brief Override drag enter event for drag-and-drop support
    void dragEnterEvent(QDragEnterEvent* event) override;

    /// @brief Override drop event for drag-and-drop support
    void dropEvent(QDropEvent* event) override;

private:
    /** @brief Read the history of the selected cell and show it in the history window.
     *
     * A progress dialog with a cancel button is shown while the files are read.
     *
     * @param showWindow Show (raise) the window; false only refreshes it when it is already visible */
    void showHistoryOfSelectedCell(bool showWindow);

    /// @brief Forget the selected cell and close the history window (the data it came from is gone)
    void resetSelectedCell();

    /// @brief Clear all substate widgets.
    void clearWidgets();

    /** @brief Reorder widgets based on drag and drop operation.
     * 
     * @param draggedField The field name being dragged
     * @param dropPosition The position where the widget was dropped */
    void reorderWidgets(const std::string& draggedField, const QPoint& dropPosition);

    /** @brief Save the current field order to SettingParameter.
     * 
     * Updates the order field in SubstateInfo based on current widget layout. */
    void saveFieldOrder();

    QScrollArea* m_scrollArea;
    QWidget* m_containerWidget;
    QVBoxLayout* m_containerLayout;
    class QPushButton* m_deactivateButton;
    std::map<std::string, SubstateDisplayWidget*> m_substateWidgets;
    SettingParameter* m_currentSettingParameter = nullptr;
    class ISceneWidgetVisualizer* m_currentVisualizer = nullptr;

    QPushButton* m_plotHistoryButton = nullptr;
    CellHistoryDialog* m_historyDialog = nullptr;
    bool m_hasSelectedCell = false;  ///< A cell was clicked since the data were (re)loaded
    int m_selectedRow = 0;           ///< Row of the clicked cell in the displayed plane
    int m_selectedCol = 0;           ///< Column of the clicked cell in the displayed plane
    bool m_keptOpen = false; ///< Opened from the menu, a click on the background does not hide it
    bool m_native3DVolumeMode = false; ///< The substates are shown in a 3D volume, not as altitude
    std::optional<GridCellIndex> m_selectedVolumeCell; ///< Set when the cell was clicked in the complete 3D volume (then it is the position in the whole grid)
    std::optional<StepIndex> m_currentStep;
};
