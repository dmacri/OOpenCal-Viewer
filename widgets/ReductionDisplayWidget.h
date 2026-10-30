/** @file ReductionDisplayWidget.h
 *  @brief Declaration of the ReductionDisplayWidget class for displaying reduction values. */

#pragma once

#include <QWidget>

class QLabel;
class ReductionManager;


/** @class ReductionDisplayWidget
 * @brief A widget that displays reduction values for the current simulation step.
 *
 * Displays reduction values directly inside a QLabel and emits a signal
 * when the user double-clicks on it to open a detailed view. */
class ReductionDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ReductionDisplayWidget(QWidget *parent = nullptr);
    ~ReductionDisplayWidget() override = default;

    /// @brief Assigns the ReductionManager providing reduction data.
    void setReductionManager(ReductionManager* manager);

    /// @brief Updates the display with the current step’s reduction data.
    void updateDisplay(int currentStep);

    /// @brief Clears the display and resets internal state.
    void clear();

    /** @brief Shows why the reduction could not be loaded (e.g. the reduction file cannot be parsed).
     *
     * The text stays visible while stepping through the simulation, until the next setReductionManager()
     * or clear(). Without it the widget would say "No reduction configured", which is misleading when the
     * model does configure a reduction but the file is broken. */
    void showError(const QString& message);

protected:
    /// @brief Capture mouse double-clicks on the label
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    /// @brief Updates the visual label text and tooltip based on current data/state.
    void updateLabel();

    /// @brief When the label is clicked - display widget
    void onReductionLabelClicked();

    QLabel* label = nullptr;
    ReductionManager* reductionManager = nullptr;
    int currentStep = 0;
    QString configurationError; ///< Why the reduction could not be loaded; empty when there was no error
};
