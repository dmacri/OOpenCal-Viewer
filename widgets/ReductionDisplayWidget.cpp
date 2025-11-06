/** @file ReductionDisplayWidget.cpp
 *  @brief Implementation of the ReductionDisplayWidget class. */

#include <QLabel>
#include <QEvent>
#include <QFont>
#include <QVBoxLayout>
#include <QMessageBox>

#include "ReductionDisplayWidget.h"
#include "ReductionManager.h"
#include "ReductionDialog.h"


/// @brief Constructs the ReductionDisplayWidget and sets up the label.
ReductionDisplayWidget::ReductionDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    label = new QLabel(this);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(label);
    setLayout(layout);

    // Default appearance
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setStyleSheet("color: gray;");
    label->setText("No reduction configured");
    label->setToolTip("<span style='color: #FF6B6B;'><b>Reduction not configured in the model file.</b></span>");

    // catch double click on label
    label->installEventFilter(this);
}

/// @brief Assigns a reduction manager to the widget.
void ReductionDisplayWidget::setReductionManager(ReductionManager* manager)
{
    reductionManager = manager;
    updateLabel();
}

/// @brief Updates the label for the given simulation step.
void ReductionDisplayWidget::updateDisplay(int step)
{
    currentStep = step;
    updateLabel();
}

/// @brief Clears the label and resets internal state.
void ReductionDisplayWidget::clear()
{
    reductionManager = nullptr;
    currentStep = 0;
    label->setText("No reduction configured");
    label->setStyleSheet("color: gray;");
    label->setToolTip("<span style='color: #FF6B6B;'><b>Reduction not configured in the model file.</b></span>");
}

/// @brief Updates label text, color, and tooltip based on reduction data or errors.
void ReductionDisplayWidget::updateLabel()
{
    if (! reductionManager)
    {
        label->setText("No reduction configured");
        label->setStyleSheet("color: gray;");
        label->setToolTip("<span style='color: #FF6B6B;'><b>Reduction not configured in the model file.</b></span>");
        return;
    }

    // Check if reduction file/data is available
    if (! reductionManager->isAvailable())
    {
        QString errorMsg = reductionManager->getErrorMessage();
        if (errorMsg.isEmpty())
            errorMsg = "Reduction file not found";

        label->setText("Reduction error");
        label->setStyleSheet("color: red;");
        label->setToolTip(
            QString("<span style='color: #FF6B6B;'><b>Reduction Error:</b></span><br/>%1")
                .arg(errorMsg));
        return;
    }

    // Get formatted reduction string for the current step
    QString reductionStr = reductionManager->getFormattedReductionString(currentStep);
    if (reductionStr.isEmpty())
    {
        label->setText("No reduction data");
        label->setStyleSheet("color: #FFA500;"); // orange
        label->setToolTip(
            QString("<span style='color: #FFB84D;'><b>No reduction data for step %1</b></span>")
                .arg(currentStep));
        return;
    }

    // Show reduction summary in the label
    label->setText(QString("Step %1: %2").arg(currentStep).arg(reductionStr));
    label->setStyleSheet("color: #2ECC71;"); // green
    label->setToolTip(
        QString("<b>Reduction (Step %1):</b><br/>%2<br/><br/><i>Double-click for detailed view</i>")
            .arg(currentStep)
            .arg(reductionStr));
}

bool ReductionDisplayWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == label && event->type() == QEvent::MouseButtonDblClick)
    {
        onReductionLabelClicked();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void ReductionDisplayWidget::onReductionLabelClicked()
{
    if (! reductionManager || !reductionManager->isAvailable())
    {
        return; // No reduction data available
    }

    // Get reduction data for current step
    ReductionData reductionData = reductionManager->getReductionForStep(currentStep);
    if (reductionData.values.empty())
    {
        QMessageBox::information(this, tr("No Data"),
                                 tr("No reduction data available for step %1").arg(currentStep));
        return;
    }

    // Show reduction dialog
    ReductionDialog dialog(reductionData.values, currentStep, this);
    dialog.exec();
}
