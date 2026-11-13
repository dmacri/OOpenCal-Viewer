/** @file SubstateRangeTests.cpp
 *  @brief Tests of the minimum / maximum of a substate over a range of steps (header-only, no Qt/VTK). */

#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "visualiser/SubstateRange.h"

namespace
{
constexpr double notANumber = std::numeric_limits<double>::quiet_NaN();

/** @brief Cells of every step as a list of values, scanned unit by unit like the visualizer scans the rows. */
struct FakeSimulation
{
    std::map<StepIndex, std::vector<double>> cellsOfStep;
    std::vector<StepIndex> readOrder;
    StepIndex loaded = 0;

    /** @brief Number of units (here: one cell is one unit). */
    [[nodiscard]] std::size_t unitsPerStep() const
    {
        return cellsOfStep.empty() ? 0 : cellsOfStep.begin()->second.size();
    }

    /** @brief Loads a step, like ModelReader does; a step which is not saved throws. */
    void readStep(StepIndex step)
    {
        if (cellsOfStep.find(step) == cellsOfStep.end())
            throw std::runtime_error("no such step");
        readOrder.push_back(step);
        loaded = step;
    }

    /** @brief Adds the cells of the loaded step which belong to the units `[begin, end)`. */
    void scan(SubstateRange& range, std::size_t begin, std::size_t end) const
    {
        const auto& cells = cellsOfStep.at(loaded);
        for (std::size_t unit = begin; unit < end; ++unit)
            range.add(cells[unit]);
    }

    /** @brief Runs the accumulation over the steps. */
    SubstateRange run(const std::vector<StepIndex>& steps, const SubstateRangeProgressCallback& progress = {})
    {
        return accumulateAcrossSteps(steps,
                                     unitsPerStep(),
                                     [this](StepIndex step) { readStep(step); },
                                     [this](SubstateRange& range, std::size_t begin, std::size_t end) { scan(range, begin, end); },
                                     progress);
    }
};
} // namespace

TEST(StepRangeTest, LengthCountsBothEnds)
{
    EXPECT_EQ((StepRange{5, 5}).length(), 1u);
    EXPECT_EQ((StepRange{0, 9}).length(), 10u);
    EXPECT_EQ((StepRange{3, 2}).length(), 0u); // not a valid interval
}

TEST(StepRangeTest, LengthDoesNotOverflowForTheWholeUnsignedRange)
{
    const StepRange everything{0, std::numeric_limits<StepIndex>::max()};
    EXPECT_EQ(everything.length(), static_cast<std::size_t>(std::numeric_limits<StepIndex>::max()) + 1);
}

TEST(StepRangeTest, ContainsIncludesBothEnds)
{
    const StepRange range{10, 20};
    EXPECT_TRUE(range.contains(10));
    EXPECT_TRUE(range.contains(20));
    EXPECT_TRUE(range.contains(15));
    EXPECT_FALSE(range.contains(9));
    EXPECT_FALSE(range.contains(21));
}

TEST(StepRangesTest, BoundsOfEmptyListIsNothing)
{
    EXPECT_FALSE(StepRanges::boundsOf({}).has_value());
}

TEST(StepRangesTest, BoundsOfUnsortedList)
{
    const auto bounds = StepRanges::boundsOf({30, 10, 20, 0, 40});
    ASSERT_TRUE(bounds.has_value());
    EXPECT_EQ(*bounds, (StepRange{0, 40}));
}

TEST(StepRangesTest, ClampedToKeepsAnInnerRange)
{
    EXPECT_EQ(StepRanges::clampedTo({5, 8}, {0, 10}), (StepRange{5, 8}));
}

TEST(StepRangesTest, ClampedToCutsBothEndsToTheBounds)
{
    EXPECT_EQ(StepRanges::clampedTo({0, 500}, {10, 100}), (StepRange{10, 100}));
}

TEST(StepRangesTest, ClampedToNeverLetsFirstExceedLast)
{
    // both above the bounds: the whole interval collapses on the last step
    EXPECT_EQ(StepRanges::clampedTo({200, 300}, {0, 100}), (StepRange{100, 100}));
    // inverted
    const StepRange fixedUp = StepRanges::clampedTo({60, 40}, {0, 100});
    EXPECT_LE(fixedUp.first, fixedUp.last);
}

TEST(StepRangesTest, StepsWithinKeepsOnlyTheSavedOnesInOrder)
{
    const std::vector<StepIndex> available{0, 10, 20, 30, 40, 50};
    EXPECT_EQ(StepRanges::stepsWithin(available, {10, 40}), (std::vector<StepIndex>{10, 20, 30, 40}));
    EXPECT_EQ(StepRanges::stepsWithin(available, {11, 19}), std::vector<StepIndex>{});
}

TEST(StepRangesTest, StepsWithinDropsRepetitionsAndSorts)
{
    EXPECT_EQ(StepRanges::stepsWithin({5, 3, 5, 4, 3}, {0, 10}), (std::vector<StepIndex>{3, 4, 5}));
}

TEST(StepRangesTest, NothingIsMissingWhenEveryStepIsSaved)
{
    const std::vector<StepIndex> available{0, 1, 2, 3, 4, 5};
    EXPECT_EQ(StepRanges::missingStepsCount(available, {1, 4}), 0u);
}

TEST(StepRangesTest, MissingStepsAreCountedForASimulationSavedEveryTenthStep)
{
    const std::vector<StepIndex> available{0, 10, 20, 30};
    // 0..30 are 31 numbers, 4 of them are saved
    EXPECT_EQ(StepRanges::missingStepsCount(available, {0, 30}), 27u);
    EXPECT_EQ(StepRanges::missingStepsCount(available, {10, 20}), 9u);
}

TEST(StepRangesTest, MissingStepsDoNotCountRepeatedAvailableSteps)
{
    EXPECT_EQ(StepRanges::missingStepsCount({1, 1, 2, 2}, {1, 2}), 0u);
}

TEST(SubstateRangeTest, StartsEmpty)
{
    const SubstateRange range;
    EXPECT_FALSE(range.minimum.has_value());
    EXPECT_FALSE(range.maximum.has_value());
    EXPECT_FALSE(range.hasValues());
    EXPECT_FALSE(range.cancelled);
}

TEST(SubstateRangeTest, AddTracksLowestAndHighest)
{
    SubstateRange range;
    for (const double value : {3.0, -2.5, 7.25, 0.0})
        range.add(value);

    ASSERT_TRUE(range.hasValues());
    EXPECT_DOUBLE_EQ(*range.minimum, -2.5);
    EXPECT_DOUBLE_EQ(*range.maximum, 7.25);
    EXPECT_EQ(range.valuesCount, 4u);
}

TEST(SubstateRangeTest, AddIgnoresValuesWhichAreNotFiniteNumbers)
{
    SubstateRange range;
    range.add(notANumber);
    range.add(std::numeric_limits<double>::infinity());
    range.add(-std::numeric_limits<double>::infinity());
    EXPECT_FALSE(range.hasValues());

    range.add(4.0);
    range.add(notANumber);
    EXPECT_EQ(range.valuesCount, 1u);
    EXPECT_DOUBLE_EQ(*range.minimum, 4.0);
    EXPECT_DOUBLE_EQ(*range.maximum, 4.0);
}

TEST(AccumulateAcrossStepsTest, FindsExtremesOverAllStepsOfTheList)
{
    FakeSimulation simulation;
    simulation.cellsOfStep = {{0, {1.0, 2.0, 3.0}}, {1, {0.5, 9.0, 4.0}}, {2, {2.0, 2.0, -7.0}}};

    const SubstateRange range = simulation.run({0, 1, 2});

    ASSERT_TRUE(range.hasValues());
    EXPECT_DOUBLE_EQ(*range.minimum, -7.0);
    EXPECT_DOUBLE_EQ(*range.maximum, 9.0);
    EXPECT_EQ(range.valuesCount, 9u);
    EXPECT_EQ(range.stepsRead, 3u);
    EXPECT_TRUE(range.failedSteps.empty());
    EXPECT_FALSE(range.cancelled);
}

TEST(AccumulateAcrossStepsTest, OnlyTheRequestedStepsAreRead)
{
    FakeSimulation simulation;
    simulation.cellsOfStep = {{0, {100.0}}, {1, {5.0}}, {2, {6.0}}, {3, {-100.0}}};

    const SubstateRange range = simulation.run({1, 2});

    EXPECT_DOUBLE_EQ(*range.minimum, 5.0);
    EXPECT_DOUBLE_EQ(*range.maximum, 6.0);
    EXPECT_EQ(simulation.readOrder, (std::vector<StepIndex>{1, 2}));
}

TEST(AccumulateAcrossStepsTest, StepsAreReadInTheGivenOrder)
{
    FakeSimulation simulation;
    simulation.cellsOfStep = {{0, {0.0}}, {1, {1.0}}, {2, {2.0}}};

    simulation.run({2, 0, 1});

    EXPECT_EQ(simulation.readOrder, (std::vector<StepIndex>{2, 0, 1}));
}

TEST(AccumulateAcrossStepsTest, ValuesWhichAreNotNumbersDoNotSpoilTheResult)
{
    FakeSimulation simulation;
    simulation.cellsOfStep = {{0, {notANumber, 2.0, notANumber}}, {1, {notANumber, 8.0, 1.0}}};

    const SubstateRange range = simulation.run({0, 1});

    EXPECT_DOUBLE_EQ(*range.minimum, 1.0);
    EXPECT_DOUBLE_EQ(*range.maximum, 8.0);
    EXPECT_EQ(range.valuesCount, 3u);
}

TEST(AccumulateAcrossStepsTest, AStepWhichCannotBeReadIsSkippedAndReported)
{
    FakeSimulation simulation;
    simulation.cellsOfStep = {{0, {1.0}}, {2, {3.0}}}; // step 1 is not saved

    const SubstateRange range = simulation.run({0, 1, 2});

    EXPECT_EQ(range.failedSteps, std::vector<StepIndex>{1});
    EXPECT_EQ(range.stepsRead, 2u);
    EXPECT_DOUBLE_EQ(*range.minimum, 1.0);
    EXPECT_DOUBLE_EQ(*range.maximum, 3.0);
    EXPECT_FALSE(range.cancelled);
}

TEST(AccumulateAcrossStepsTest, NoStepsGiveAnEmptyResultAndReadNothing)
{
    FakeSimulation simulation;
    simulation.cellsOfStep = {{0, {1.0}}};

    const SubstateRange range = simulation.run({});

    EXPECT_FALSE(range.hasValues());
    EXPECT_TRUE(simulation.readOrder.empty());
}

TEST(AccumulateAcrossStepsTest, NoUnitsGiveAnEmptyResultAndReadNothing)
{
    std::size_t reads = 0;
    const SubstateRange range = accumulateAcrossSteps({0, 1},
                                                      0,
                                                      [&reads](StepIndex) { ++reads; },
                                                      [](SubstateRange&, std::size_t, std::size_t) {});
    EXPECT_FALSE(range.hasValues());
    EXPECT_EQ(reads, 0u);
}

TEST(AccumulateAcrossStepsTest, EveryUnitIsScannedExactlyOnceForAnyNumberOfUnits)
{
    for (const std::size_t units : {1u, 2u, 99u, 100u, 101u, 250u, 1000u})
    {
        std::vector<int> scanned(units, 0);
        const SubstateRange range = accumulateAcrossSteps({7},
                                                          units,
                                                          [](StepIndex) {},
                                                          [&scanned](SubstateRange&, std::size_t begin, std::size_t end)
                                                          {
                                                              for (std::size_t unit = begin; unit < end; ++unit)
                                                                  ++scanned[unit];
                                                          });
        EXPECT_EQ(range.stepsRead, 1u);
        for (const int count : scanned)
            ASSERT_EQ(count, 1) << "units=" << units;
    }
}

TEST(AccumulateAcrossStepsTest, ProgressNeverGoesBackAndEndsAtOne)
{
    FakeSimulation simulation;
    for (StepIndex step = 0; step < 5; ++step)
        simulation.cellsOfStep[step] = std::vector<double>(300, 1.0);

    std::vector<SubstateRangeProgress> reports;
    simulation.run({0, 1, 2, 3, 4},
                   [&reports](const SubstateRangeProgress& state)
                   {
                       reports.push_back(state);
                       return true;
                   });

    ASSERT_FALSE(reports.empty());
    EXPECT_DOUBLE_EQ(reports.front().fraction, 0.0);
    EXPECT_DOUBLE_EQ(reports.back().fraction, 1.0);
    for (std::size_t i = 1; i < reports.size(); ++i)
        EXPECT_GE(reports[i].fraction, reports[i - 1].fraction);
    for (const auto& state : reports)
    {
        EXPECT_EQ(state.stepsTotal, 5u);
        EXPECT_GE(state.stepNumber, 1u);
        EXPECT_LE(state.stepNumber, 5u);
        EXPECT_EQ(state.step, state.stepNumber - 1);
    }
}

TEST(AccumulateAcrossStepsTest, ProgressIsReportedInsideABigStepToo)
{
    FakeSimulation simulation;
    simulation.cellsOfStep[0] = std::vector<double>(1000, 1.0);

    std::size_t reports = 0;
    simulation.run({0},
                   [&reports](const SubstateRangeProgress&)
                   {
                       ++reports;
                       return true;
                   });

    EXPECT_GT(reports, 50u); // not only "before" and "after" the only step
}

TEST(AccumulateAcrossStepsTest, CancelBeforeTheFirstStepReadsNothing)
{
    FakeSimulation simulation;
    simulation.cellsOfStep = {{0, {1.0}}, {1, {2.0}}};

    const SubstateRange range = simulation.run({0, 1}, [](const SubstateRangeProgress&) { return false; });

    EXPECT_TRUE(range.cancelled);
    EXPECT_TRUE(simulation.readOrder.empty());
    EXPECT_EQ(range.stepsRead, 0u);
}

TEST(AccumulateAcrossStepsTest, CancelInTheMiddleStopsAtOnce)
{
    FakeSimulation simulation;
    for (StepIndex step = 0; step < 10; ++step)
        simulation.cellsOfStep[step] = std::vector<double>(50, static_cast<double>(step));

    const SubstateRange range = simulation.run({0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
                                               [](const SubstateRangeProgress& state) { return state.stepNumber <= 3; });

    EXPECT_TRUE(range.cancelled);
    EXPECT_LT(simulation.readOrder.size(), 10u);
    EXPECT_LE(range.stepsRead, 3u);
}
