/** @file StepRangeDialog.h
 *  @brief Dialog for choosing the steps over which the minimum / maximum of a substate is calculated. */
#pragma once

#include <vector>

#include <QDialog>
#include <QString>

#include "core/types.h"
#include "visualiser/SubstateRange.h"

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QSlider;
class QSpinBox;

/** @class StepRangeDialog
 *  @brief Chooses the first and the last step (spin box and slider for each, kept in sync) and what to calculate.
 *
 *  The first step can not exceed the last one and both stay between the lowest and the highest saved step of the
 *  simulation: the controls do not let the user go outside (a value which is too high or too low is held at the
 *  other end of the interval). Both sliders use the whole simulation as their scale. When the interval contains numbers of steps which were
 *  not saved (e.g. the simulation writes every 10th step) a yellow warning says that they are skipped. */
class StepRangeDialog : public QDialog
{
    Q_OBJECT

public:
    /** @brief Creates the dialog for the given simulation.
     *
     *  @param substateName    substate the calculation is for, only shown
     *  @param availableSteps  steps which were saved; they decide the limits of the controls
     *  @param initialRange    interval shown at first (put into the limits if it does not fit)
     *  @param calculateMinimum  whether "Minimum" is checked at first
     *  @param calculateMaximum  whether "Maximum" is checked at first
     *  @param parent          parent widget */
    StepRangeDialog(const QString& substateName,
                    std::vector<StepIndex> availableSteps,
                    StepRange initialRange,
                    bool calculateMinimum,
                    bool calculateMaximum,
                    QWidget* parent = nullptr);

    /** @brief The interval chosen by the user. */
    [[nodiscard]] StepRange selectedRange() const;

    /** @brief The saved steps which lie in the chosen interval, in ascending order. */
    [[nodiscard]] std::vector<StepIndex> stepsToRead() const;

    /** @brief Whether the minimum is to be calculated. */
    [[nodiscard]] bool calculatesMinimum() const;

    /** @brief Whether the maximum is to be calculated. */
    [[nodiscard]] bool calculatesMaximum() const;

    /** @brief Text of the yellow warning (empty when it is not shown); for the tests. */
    [[nodiscard]] QString warningText() const;

    /** @brief Whether the "Calculate" button can be pressed. */
    [[nodiscard]] bool canCalculate() const;

private slots:
    /** @brief The first step was changed with the spin box or the slider: keeps the other controls in sync. */
    void onFirstStepChanged(int value);

    /** @brief The last step was changed with the spin box or the slider: keeps the other controls in sync. */
    void onLastStepChanged(int value);

    /** @brief Refreshes the number of steps to read, the warning and the state of the "Calculate" button. */
    void updateSummary();

private:
    /** @brief Creates the widgets and puts them into the layout. */
    void buildUi(const QString& substateName, bool calculateMinimum, bool calculateMaximum);

    /** @brief Sets the limits of the spin boxes and sliders from the saved steps and the value they start with. */
    void setUpLimits(StepRange initialRange);

    /** @brief Connects the spin boxes, the sliders and the check boxes. */
    void connectControls();

    std::vector<StepIndex> m_availableSteps; ///< Steps which were saved
    StepRange m_limits;                      ///< From the lowest to the highest saved step

    QSpinBox* m_firstSpin = nullptr;
    QSlider* m_firstSlider = nullptr;
    QSpinBox* m_lastSpin = nullptr;
    QSlider* m_lastSlider = nullptr;
    QCheckBox* m_minimumCheck = nullptr;
    QCheckBox* m_maximumCheck = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_warningLabel = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
};
