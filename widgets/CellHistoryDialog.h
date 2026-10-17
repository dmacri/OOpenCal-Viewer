/** @file CellHistoryDialog.h
 *  @brief Window with the plot of the substates of one cell over the whole simulation. */
#pragma once

#include <optional>
#include <string>

#include <QDialog>

#include "visualiserProxy/CellHistory.h"

class QComboBox;
class QLabel;
class TimeSeriesPlot;

/** @class CellHistoryDialog
 *  @brief Non-modal window which plots a substate of the clicked cell versus the simulation step.
 *
 *  The user chooses the substate in a combo box. The step currently shown in the scene is marked on
 *  the plot, and the values of all substates can be saved as a CSV file. */
class CellHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CellHistoryDialog(QWidget* parent = nullptr);

    /** @brief Show a history.
     *  @param history       the values to plot
     *  @param preferredField substate selected when it is in the history and no substate was chosen
     *                        before; the substate chosen by the user is kept when a new cell is shown */
    void setHistory(const CellHistory& history, const std::string& preferredField = {});

    /// @brief Mark the step currently shown in the scene
    void setCurrentStep(StepIndex step);

private slots:
    void onFieldChanged();
    void onExportCsv();

private:
    void updatePlot();

    CellHistory m_history;
    std::optional<StepIndex> m_currentStep;

    QLabel* m_titleLabel = nullptr;
    QComboBox* m_fieldComboBox = nullptr;
    TimeSeriesPlot* m_plot = nullptr;
    QLabel* m_summaryLabel = nullptr;
};
