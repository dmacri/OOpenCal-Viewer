#include <gtest/gtest.h>
#include "core/BenchmarkStatistics.h"

using namespace Benchmark;

TEST(BenchmarkStatistics, EmptyInputGivesZeros)
{
    const auto summary = summarize(std::vector<double>{});
    EXPECT_EQ(summary.mean, 0.0);
    EXPECT_EQ(summary.median, 0.0);
    EXPECT_EQ(summary.min, 0.0);
    EXPECT_EQ(summary.max, 0.0);
}

TEST(BenchmarkStatistics, SingleSample)
{
    const auto summary = summarize(std::vector<double>{4.5});
    EXPECT_DOUBLE_EQ(summary.mean, 4.5);
    EXPECT_DOUBLE_EQ(summary.median, 4.5);
    EXPECT_DOUBLE_EQ(summary.min, 4.5);
    EXPECT_DOUBLE_EQ(summary.max, 4.5);
}

TEST(BenchmarkStatistics, MedianOfOddNumberOfSamplesIsTheMiddleOne)
{
    const auto summary = summarize(std::vector<double>{9.0, 1.0, 5.0});
    EXPECT_DOUBLE_EQ(summary.median, 5.0);
    EXPECT_DOUBLE_EQ(summary.mean, 5.0);
    EXPECT_DOUBLE_EQ(summary.min, 1.0);
    EXPECT_DOUBLE_EQ(summary.max, 9.0);
}

TEST(BenchmarkStatistics, MedianOfEvenNumberOfSamplesIsTheMeanOfTheMiddleOnes)
{
    const auto summary = summarize(std::vector<double>{10.0, 2.0, 4.0, 100.0});
    EXPECT_DOUBLE_EQ(summary.median, 7.0); // (4 + 10) / 2
    EXPECT_DOUBLE_EQ(summary.mean, 29.0);
}

TEST(BenchmarkStatistics, MedianIsNotAffectedByOneSlowStep)
{
    const auto summary = summarize(std::vector<double>{10.0, 11.0, 10.5, 9.5, 500.0});
    EXPECT_DOUBLE_EQ(summary.median, 10.5);
    EXPECT_GT(summary.mean, 100.0);
}

TEST(BenchmarkStatistics, InputIsNotRequiredToBeSorted)
{
    EXPECT_DOUBLE_EQ(summarize(std::vector<double>{3, 1, 2}).median, summarize(std::vector<double>{1, 2, 3}).median);
}

TEST(BenchmarkStatistics, StepTotalIsTheSumOfThePhases)
{
    const StepTimes step{1.0, 2.0, 3.0, 4.0};
    EXPECT_DOUBLE_EQ(step.totalMs(), 10.0);
}

TEST(BenchmarkStatistics, PhasesAreSummarizedSeparately)
{
    const std::vector<StepTimes> steps = {{10, 1, 2, 3}, {20, 1, 4, 5}, {30, 1, 6, 7}};
    const auto summaries = summarize(steps);
    EXPECT_EQ(summaries.steps, 3u);
    EXPECT_DOUBLE_EQ(summaries.read.median, 20.0);
    EXPECT_DOUBLE_EQ(summaries.coloring.mean, 1.0);
    EXPECT_DOUBLE_EQ(summaries.vtkUpdate.max, 6.0);
    EXPECT_DOUBLE_EQ(summaries.render.min, 3.0);
    EXPECT_DOUBLE_EQ(summaries.total.median, 30.0); // totals are 16, 30, 44; the middle one is 20 + 1 + 4 + 5
}

TEST(BenchmarkStatistics, TableHasARowForEveryPhase)
{
    const auto table = formatTable(summarize(std::vector<StepTimes>{{1.5, 2, 3, 4}}));
    for (const char* phase : {"phase [ms]", "read", "coloring (CPU)", "VTK update", "render (GPU finished)", "total"})
        EXPECT_NE(table.find(phase), std::string::npos) << phase;
    EXPECT_NE(table.find("1.50"), std::string::npos);
    EXPECT_NE(table.find("10.50"), std::string::npos); // total
}
