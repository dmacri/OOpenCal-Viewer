/** @file StepRangeDialog.cpp
 *  @brief Implementation of StepRangeDialog. */

#include <algorithm>
#include <climits>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QAbstractSlider>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "widgets/StepRangeDialog.h"

namespace
{
/** @brief A step as the int the Qt controls work with (a step above INT_MAX can not be chosen with them). */
int toControlValue(StepIndex step)
{
    return static_cast<int>(std::min<StepIndex>(step, static_cast<StepIndex>(INT_MAX)));
}

/** @brief Style of the yellow warning: dark text on a light yellow ground, so it reads in a dark theme too. */
const char* const warningStyle = "QLabel { background-color: #fff4cc; color: #5c4600; border: 1px solid #e0b800; "
                                 "border-radius: 3px; padding: 5px; }";
} // namespace

StepRangeDialog::StepRangeDialog(const QString& substateName,
                                 std::vector<StepIndex> availableSteps,
                                 StepRange initialRange,
                                 bool calculateMinimum,
                                 bool calculateMaximum,
                                 QWidget* parent)
    : QDialog(parent)
    , m_availableSteps(std::move(availableSteps))
{
    setWindowTitle(tr("Minimum / maximum over steps"));
    setModal(true);
    setMinimumWidth(440);

    m_limits = StepRanges::boundsOf(m_availableSteps).value_or(StepRange{});

    buildUi(substateName, calculateMinimum, calculateMaximum);
    setUpLimits(initialRange);
    connectControls();
    updateSummary();
}

void StepRangeDialog::buildUi(const QString& substateName, bool calculateMinimum, bool calculateMaximum)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    auto* titleLabel = new QLabel(tr("Substate: <b>%1</b>").arg(substateName.toHtmlEscaped()));
    titleLabel->setTextFormat(Qt::RichText);
    mainLayout->addWidget(titleLabel);

    auto* hintLabel = new QLabel(tr("The cells of every saved step from the interval are looked through. "
                                    "Saved steps: %1 to %2.")
                                     .arg(m_limits.first)
                                     .arg(m_limits.last));
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    // Every end of the interval: a label, a slider and a spin box which show the same value
    auto* grid = new QGridLayout();
    grid->setColumnStretch(1, 1);

    m_firstSlider = new QSlider(Qt::Horizontal);
    m_firstSpin = new QSpinBox();
    m_lastSlider = new QSlider(Qt::Horizontal);
    m_lastSpin = new QSpinBox();

    grid->addWidget(new QLabel(tr("From step:")), 0, 0);
    grid->addWidget(m_firstSlider, 0, 1);
    grid->addWidget(m_firstSpin, 0, 2);
    grid->addWidget(new QLabel(tr("To step:")), 1, 0);
    grid->addWidget(m_lastSlider, 1, 1);
    grid->addWidget(m_lastSpin, 1, 2);
    mainLayout->addLayout(grid);

    m_firstSpin->setToolTip(tr("First step of the interval (it can not be above the last step)"));
    m_firstSlider->setToolTip(m_firstSpin->toolTip());
    m_lastSpin->setToolTip(tr("Last step of the interval (it can not be below the first step)"));
    m_lastSlider->setToolTip(m_lastSpin->toolTip());

    // The value which is being typed is taken when the user finishes (Enter, Tab), not after every digit
    m_firstSpin->setKeyboardTracking(false);
    m_lastSpin->setKeyboardTracking(false);

    m_summaryLabel = new QLabel();
    mainLayout->addWidget(m_summaryLabel);

    m_warningLabel = new QLabel();
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet(warningStyle);
    m_warningLabel->setVisible(false);
    mainLayout->addWidget(m_warningLabel);

    auto* whatLayout = new QHBoxLayout();
    whatLayout->addWidget(new QLabel(tr("Calculate:")));
    m_minimumCheck = new QCheckBox(tr("minimum"));
    m_minimumCheck->setChecked(calculateMinimum);
    m_maximumCheck = new QCheckBox(tr("maximum"));
    m_maximumCheck->setChecked(calculateMaximum);
    whatLayout->addWidget(m_minimumCheck);
    whatLayout->addWidget(m_maximumCheck);
    whatLayout->addStretch();
    mainLayout->addLayout(whatLayout);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Calculate"));
    mainLayout->addWidget(m_buttons);
}

void StepRangeDialog::setUpLimits(StepRange initialRange)
{
    const StepRange start = StepRanges::clampedTo(initialRange, m_limits);
    const int lowest = toControlValue(m_limits.first);
    const int highest = toControlValue(m_limits.last);
    const int first = toControlValue(start.first);
    const int last = toControlValue(start.last);
    const int pageStep = std::max(1, (highest - lowest) / 10);

    // Both ends use the whole simulation as the scale, so the two sliders can be compared by eye;
    // the first step is held at the last one (and the other way round) by the slots below
    for (QAbstractSlider* slider : {static_cast<QAbstractSlider*>(m_firstSlider), static_cast<QAbstractSlider*>(m_lastSlider)})
        slider->setRange(lowest, highest);
    for (QSpinBox* spin : {m_firstSpin, m_lastSpin})
        spin->setRange(lowest, highest);

    for (QSlider* slider : {m_firstSlider, m_lastSlider})
        slider->setPageStep(pageStep);

    m_firstSpin->setValue(first);
    m_firstSlider->setValue(first);
    m_lastSpin->setValue(last);
    m_lastSlider->setValue(last);
}

void StepRangeDialog::connectControls()
{
    connect(m_firstSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &StepRangeDialog::onFirstStepChanged);
    connect(m_firstSlider, &QSlider::valueChanged, this, &StepRangeDialog::onFirstStepChanged);
    connect(m_lastSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &StepRangeDialog::onLastStepChanged);
    connect(m_lastSlider, &QSlider::valueChanged, this, &StepRangeDialog::onLastStepChanged);
    connect(m_minimumCheck, &QCheckBox::toggled, this, &StepRangeDialog::updateSummary);
    connect(m_maximumCheck, &QCheckBox::toggled, this, &StepRangeDialog::updateSummary);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void StepRangeDialog::onFirstStepChanged(int value)
{
    // The first step can not go above the last one
    const int accepted = std::min(value, m_lastSpin->value());

    const QSignalBlocker spinBlocker(m_firstSpin);
    const QSignalBlocker sliderBlocker(m_firstSlider);
    m_firstSpin->setValue(accepted);
    m_firstSlider->setValue(accepted);
    updateSummary();
}

void StepRangeDialog::onLastStepChanged(int value)
{
    // The last step can not go below the first one
    const int accepted = std::max(value, m_firstSpin->value());

    const QSignalBlocker spinBlocker(m_lastSpin);
    const QSignalBlocker sliderBlocker(m_lastSlider);
    m_lastSpin->setValue(accepted);
    m_lastSlider->setValue(accepted);
    updateSummary();
}

void StepRangeDialog::updateSummary()
{
    const StepRange range = selectedRange();
    const std::size_t toRead = stepsToRead().size();
    const std::size_t missing = StepRanges::missingStepsCount(m_availableSteps, range);

    m_summaryLabel->setText(tr("Steps to read: %1").arg(toRead));

    // Every number of the interval is a step which could have been saved: some of them were not
    if (toRead == 0)
    {
        m_warningLabel->setText(tr("No saved step lies between %1 and %2.").arg(range.first).arg(range.last));
    }
    else if (missing > 0)
    {
        m_warningLabel->setText(tr("Warning: not all steps between %1 and %2 exist. Only %3 of %4 steps are saved "
                                   "there; the missing ones are skipped.")
                                    .arg(range.first)
                                    .arg(range.last)
                                    .arg(toRead)
                                    .arg(range.length()));
    }
    m_warningLabel->setVisible(toRead == 0 || missing > 0);

    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(canCalculate());
}

StepRange StepRangeDialog::selectedRange() const
{
    return StepRange{static_cast<StepIndex>(m_firstSpin->value()), static_cast<StepIndex>(m_lastSpin->value())};
}

std::vector<StepIndex> StepRangeDialog::stepsToRead() const
{
    return StepRanges::stepsWithin(m_availableSteps, selectedRange());
}

bool StepRangeDialog::calculatesMinimum() const
{
    return m_minimumCheck->isChecked();
}

bool StepRangeDialog::calculatesMaximum() const
{
    return m_maximumCheck->isChecked();
}

QString StepRangeDialog::warningText() const
{
    return m_warningLabel->isHidden() ? QString() : m_warningLabel->text();
}

bool StepRangeDialog::canCalculate() const
{
    return (calculatesMinimum() || calculatesMaximum()) && ! stepsToRead().empty();
}
