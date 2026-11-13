/** @file StepRangeDialogTests.cpp
 *  @brief Tests of the dialog which chooses the interval of steps (the widgets are created without a screen). */

#include <memory>

#include <QApplication>
#include <QAbstractSlider>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

#include <gtest/gtest.h>

#include "widgets/StepRangeDialog.h"

namespace
{
/** @brief Every step from 0 to 100 is saved. */
std::vector<StepIndex> everyStep()
{
    std::vector<StepIndex> steps;
    for (StepIndex step = 0; step <= 100; ++step)
        steps.push_back(step);
    return steps;
}

/** @brief Every tenth step from 0 to 100 is saved. */
std::vector<StepIndex> everyTenthStep()
{
    std::vector<StepIndex> steps;
    for (StepIndex step = 0; step <= 100; step += 10)
        steps.push_back(step);
    return steps;
}

/** @brief Creates the QApplication once; the offscreen platform lets the tests run without a display. */
class StepRangeDialogTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        static int argc = 1;
        static char name[] = "StepRangeDialogTests";
        static char* argv[] = {name, nullptr};
        if (! QApplication::instance())
            application = std::make_unique<QApplication>(argc, argv);
    }

    /** @brief The widgets of the dialog in the order of creation: first spin, first slider, last spin, last slider. */
    struct Controls
    {
        QSpinBox* firstSpin;
        QSlider* firstSlider;
        QSpinBox* lastSpin;
        QSlider* lastSlider;
    };

    static Controls controlsOf(StepRangeDialog& dialog)
    {
        const auto spins = dialog.findChildren<QSpinBox*>();
        const auto sliders = dialog.findChildren<QSlider*>();
        return Controls{spins.at(0), sliders.at(0), spins.at(1), sliders.at(1)};
    }

    static inline std::unique_ptr<QApplication> application;
};
} // namespace

TEST_F(StepRangeDialogTest, StartsWithTheGivenRange)
{
    StepRangeDialog dialog("water", everyStep(), {20, 60}, true, true);
    EXPECT_EQ(dialog.selectedRange(), (StepRange{20, 60}));
    EXPECT_EQ(dialog.stepsToRead().size(), 41u);
}

TEST_F(StepRangeDialogTest, ARangeWhichDoesNotFitIsPutIntoTheLimits)
{
    StepRangeDialog dialog("water", everyStep(), {500, 900}, true, true);
    EXPECT_LE(dialog.selectedRange().last, 100u);
    EXPECT_LE(dialog.selectedRange().first, dialog.selectedRange().last);
}

TEST_F(StepRangeDialogTest, SpinBoxMovesTheSliderAndTheOtherWayRound)
{
    StepRangeDialog dialog("water", everyStep(), {0, 100}, true, true);
    const auto controls = controlsOf(dialog);

    controls.firstSpin->setValue(30);
    EXPECT_EQ(controls.firstSlider->value(), 30);

    controls.lastSlider->setValue(70);
    EXPECT_EQ(controls.lastSpin->value(), 70);
    EXPECT_EQ(dialog.selectedRange(), (StepRange{30, 70}));
}

TEST_F(StepRangeDialogTest, FirstStepCanNotPassTheLastStep)
{
    StepRangeDialog dialog("water", everyStep(), {0, 50}, true, true);
    const auto controls = controlsOf(dialog);

    controls.firstSpin->setValue(90); // above the last step: stops at it
    EXPECT_EQ(controls.firstSpin->value(), 50);
    controls.firstSlider->setValue(100);
    EXPECT_EQ(controls.firstSlider->value(), 50);
    EXPECT_LE(dialog.selectedRange().first, dialog.selectedRange().last);
}

TEST_F(StepRangeDialogTest, LastStepCanNotGoBelowTheFirstStep)
{
    StepRangeDialog dialog("water", everyStep(), {40, 100}, true, true);
    const auto controls = controlsOf(dialog);

    controls.lastSpin->setValue(5);
    EXPECT_EQ(controls.lastSpin->value(), 40);
    controls.lastSlider->setValue(0);
    EXPECT_EQ(controls.lastSlider->value(), 40);
}

TEST_F(StepRangeDialogTest, BothSlidersAndSpinBoxesUseTheWholeSimulationAsScale)
{
    StepRangeDialog dialog("water", everyStep(), {30, 60}, true, true);
    const auto controls = controlsOf(dialog);

    for (const QAbstractSlider* slider : {static_cast<QAbstractSlider*>(controls.firstSlider), static_cast<QAbstractSlider*>(controls.lastSlider)})
    {
        EXPECT_EQ(slider->minimum(), 0);
        EXPECT_EQ(slider->maximum(), 100);
    }
    for (const QSpinBox* spin : {controls.firstSpin, controls.lastSpin})
    {
        EXPECT_EQ(spin->minimum(), 0);
        EXPECT_EQ(spin->maximum(), 100);
    }
}

TEST_F(StepRangeDialogTest, DraggingOneEndPastTheOtherPushesNothingAndKeepsBothInSync)
{
    StepRangeDialog dialog("water", everyStep(), {40, 60}, true, true);
    const auto controls = controlsOf(dialog);

    controls.firstSlider->setValue(90); // dragged past the last step
    EXPECT_EQ(controls.firstSlider->value(), 60);
    EXPECT_EQ(controls.firstSpin->value(), 60);
    EXPECT_EQ(controls.lastSpin->value(), 60); // the last step did not move

    controls.lastSlider->setValue(10); // dragged below the first step
    EXPECT_EQ(controls.lastSlider->value(), 60);
    EXPECT_EQ(controls.lastSpin->value(), 60);
    EXPECT_EQ(dialog.selectedRange(), (StepRange{60, 60}));
}

TEST_F(StepRangeDialogTest, NeitherEndCanLeaveTheSimulation)
{
    StepRangeDialog dialog("water", everyStep(), {0, 100}, true, true);
    const auto controls = controlsOf(dialog);

    controls.firstSpin->setValue(-50);
    EXPECT_EQ(controls.firstSpin->value(), 0);
    controls.lastSpin->setValue(1000);
    EXPECT_EQ(controls.lastSpin->value(), 100);
}

TEST_F(StepRangeDialogTest, LimitsFollowTheSavedStepsNotZero)
{
    std::vector<StepIndex> steps{50, 60, 70, 80};
    StepRangeDialog dialog("water", steps, {0, 1000}, true, true);
    const auto controls = controlsOf(dialog);

    EXPECT_EQ(controls.firstSpin->minimum(), 50);
    EXPECT_EQ(controls.lastSpin->maximum(), 80);
    EXPECT_EQ(dialog.selectedRange(), (StepRange{50, 80}));
}

TEST_F(StepRangeDialogTest, NoWarningWhenEveryStepIsSaved)
{
    StepRangeDialog dialog("water", everyStep(), {10, 20}, true, true);
    EXPECT_TRUE(dialog.warningText().isEmpty());
}

TEST_F(StepRangeDialogTest, WarnsWhenStepsOfTheIntervalAreMissing)
{
    StepRangeDialog dialog("water", everyTenthStep(), {0, 100}, true, true);
    const QString warning = dialog.warningText();
    EXPECT_FALSE(warning.isEmpty());
    EXPECT_TRUE(warning.contains("0")) << warning.toStdString();
    EXPECT_TRUE(warning.contains("100")) << warning.toStdString();
    EXPECT_TRUE(warning.contains("11 of 101")) << warning.toStdString();
}

TEST_F(StepRangeDialogTest, WarningAppearsAndDisappearsWithTheInterval)
{
    StepRangeDialog dialog("water", everyTenthStep(), {10, 10}, true, true);
    const auto controls = controlsOf(dialog);
    EXPECT_TRUE(dialog.warningText().isEmpty()); // a single saved step: nothing missing

    controls.lastSpin->setValue(30);
    EXPECT_FALSE(dialog.warningText().isEmpty()); // 10..30 has 21 numbers, 3 saved

    controls.lastSpin->setValue(10);
    EXPECT_TRUE(dialog.warningText().isEmpty());
}

TEST_F(StepRangeDialogTest, OnlySavedStepsAreRead)
{
    StepRangeDialog dialog("water", everyTenthStep(), {5, 45}, true, true);
    EXPECT_EQ(dialog.stepsToRead(), (std::vector<StepIndex>{10, 20, 30, 40}));
}

TEST_F(StepRangeDialogTest, IntervalWithoutSavedStepsCanNotBeCalculated)
{
    StepRangeDialog dialog("water", everyTenthStep(), {10, 20}, true, true);
    const auto controls = controlsOf(dialog);
    const auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);

    controls.firstSpin->setValue(11);
    controls.lastSpin->setValue(19);
    EXPECT_TRUE(dialog.stepsToRead().empty());
    EXPECT_FALSE(dialog.canCalculate());
    EXPECT_FALSE(ok->isEnabled());
    EXPECT_FALSE(dialog.warningText().isEmpty());
}

TEST_F(StepRangeDialogTest, NeedsMinimumOrMaximumChecked)
{
    StepRangeDialog dialog("water", everyStep(), {0, 10}, false, false);
    const auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    EXPECT_FALSE(dialog.canCalculate());
    EXPECT_FALSE(ok->isEnabled());

    dialog.findChildren<QCheckBox*>().at(1)->setChecked(true);
    EXPECT_TRUE(dialog.calculatesMaximum());
    EXPECT_FALSE(dialog.calculatesMinimum());
    EXPECT_TRUE(dialog.canCalculate());
    EXPECT_TRUE(ok->isEnabled());
}

TEST_F(StepRangeDialogTest, SingleStepSimulationWorks)
{
    StepRangeDialog dialog("water", {7}, {0, 100}, true, true);
    EXPECT_EQ(dialog.selectedRange(), (StepRange{7, 7}));
    EXPECT_EQ(dialog.stepsToRead(), std::vector<StepIndex>{7});
    EXPECT_TRUE(dialog.canCalculate());
    EXPECT_TRUE(dialog.warningText().isEmpty());
}
