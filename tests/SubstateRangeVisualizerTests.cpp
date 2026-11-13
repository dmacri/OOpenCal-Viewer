/** @file SubstateRangeVisualizerTests.cpp
 *  @brief The minimum / maximum over steps computed by a real visualizer from real (temporary) step files. */

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/types.h"
#include "visualiser/SettingParameter.h"
#include "visualiserProxy/SceneWidgetVisualizerProxy.h"

namespace
{
namespace fs = std::filesystem;

/** @brief Cell with two substates "a" and "b", read from the text format as `a;b`. */
class RangeCell
{
public:
    virtual ~RangeCell() = default;

    void composeElement(char* text)
    {
        char* separator = std::strchr(text, ';');
        a = std::atof(text);
        b = separator ? std::atof(separator + 1) : 0.0;
    }

    std::string stringEncoding(const char* name = nullptr) const
    {
        if (name && std::string(name) == "a")
            return std::to_string(a);
        if (name && std::string(name) == "b")
            return std::to_string(b);
        return "a=" + std::to_string(a);
    }

    Color outputValue(const char*, GlobalValueManager*) const
    {
        return Color(0, 0, 0);
    }

    void startStep(int)
    {
    }

    double a = 0.0;
    double b = 0.0;
};

/** @brief Value of substate "a" of a cell at a step: different for every cell and every step. */
double valueA(unsigned step, int row, int col)
{
    return static_cast<double>(step) * 100.0 + row * 10.0 + col;
}

/** @brief Steps 0, 10, 20, 30 of a 4 x 4 grid split into 2 x 2 nodes; substate "b" is -step, "no data" cells hold -9999. */
class SubstateRangeVisualizerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        dir = fs::temp_directory_path() / "oopencal-viewer-range-test";
        fs::remove_all(dir);
        fs::create_directories(dir);
        base = (dir / "m").string();

        for (int node = 0; node < 4; ++node)
        {
            const int offX = (node % 2) * 2;
            const int offY = (node / 2) * 2;
            std::ofstream index(base + std::to_string(node) + "_index.txt");
            std::ofstream data(base + std::to_string(node) + ".txt");
            for (const unsigned step : steps)
            {
                index << step << " " << data.tellp() << "\n";
                data << "2-2\n";
                for (int row = 0; row < 2; ++row)
                {
                    for (int col = 0; col < 2; ++col)
                    {
                        const int globalRow = offY + row;
                        const int globalCol = offX + col;
                        // one cell of step 20 has the "no data" value in substate b
                        const double b = (step == 20 && globalRow == 1 && globalCol == 1) ? -9999.0 : -static_cast<double>(step);
                        data << valueA(step, globalRow, globalCol) << ";" << b << " ";
                    }
                    data << "\n";
                }
            }
        }

        visualizer = std::make_unique<SceneWidgetVisualizerTemplate<RangeCell>>("RangeCell");
        settings = SettingParameter{};
        settings.outputFileName = base;
        settings.readMode = "text";
        settings.nNodeX = 2;
        settings.nNodeY = 2;
        settings.nNodeZ = 1;
        settings.numberOfColumnX = 4;
        settings.numberOfRowsY = 4;
        settings.numberOfSlicesZ = 1;
        settings.numberOfLines = 2 * 4 + 2 + 2;
        settings.step = 30;

        visualizer->initMatrix(4, 4);
        visualizer->prepareStage(2, 2, 1);
        visualizer->readStepsOffsetsForAllNodesFromFiles(2, 2, 1, base);

        std::vector<Line> lines(settings.numberOfLines);
        visualizer->readStageStateFromFilesForStep(&settings, lines.data()); // the step which the scene shows
    }

    void TearDown() override
    {
        fs::remove_all(dir);
    }

    fs::path dir;
    std::string base;
    const std::vector<unsigned> steps{0, 10, 20, 30};
    SettingParameter settings{};
    std::unique_ptr<SceneWidgetVisualizerTemplate<RangeCell>> visualizer;
};
} // namespace

TEST_F(SubstateRangeVisualizerTest, FindsExtremesOverTheChosenSteps)
{
    const SubstateRange range = visualizer->substateRangeAcrossSteps(settings, "a", {10, 20}, std::nullopt);

    ASSERT_TRUE(range.hasValues());
    EXPECT_DOUBLE_EQ(*range.minimum, valueA(10, 0, 0));
    EXPECT_DOUBLE_EQ(*range.maximum, valueA(20, 3, 3));
    EXPECT_EQ(range.valuesCount, 32u); // 2 steps x 16 cells
    EXPECT_EQ(range.stepsRead, 2u);
    EXPECT_TRUE(range.failedSteps.empty());
}

TEST_F(SubstateRangeVisualizerTest, WholeSimulation)
{
    const SubstateRange range = visualizer->substateRangeAcrossSteps(settings, "a", {0, 10, 20, 30}, std::nullopt);

    EXPECT_DOUBLE_EQ(*range.minimum, valueA(0, 0, 0));
    EXPECT_DOUBLE_EQ(*range.maximum, valueA(30, 3, 3));
    EXPECT_EQ(range.valuesCount, 64u);
}

TEST_F(SubstateRangeVisualizerTest, TheNoDataValueIsLeftOut)
{
    const SubstateRange withValue = visualizer->substateRangeAcrossSteps(settings, "b", {0, 10, 20, 30}, std::nullopt);
    EXPECT_DOUBLE_EQ(*withValue.minimum, -9999.0);

    const SubstateRange without = visualizer->substateRangeAcrossSteps(settings, "b", {0, 10, 20, 30}, -9999.0);
    EXPECT_DOUBLE_EQ(*without.minimum, -30.0);
    EXPECT_DOUBLE_EQ(*without.maximum, 0.0);
    EXPECT_EQ(without.valuesCount, 63u);
}

TEST_F(SubstateRangeVisualizerTest, AStepWhichIsNotSavedIsReportedAndSkipped)
{
    const SubstateRange range = visualizer->substateRangeAcrossSteps(settings, "a", {10, 15, 20}, std::nullopt);

    EXPECT_EQ(range.failedSteps, std::vector<StepIndex>{15});
    EXPECT_EQ(range.stepsRead, 2u);
    EXPECT_DOUBLE_EQ(*range.minimum, valueA(10, 0, 0));
}

TEST_F(SubstateRangeVisualizerTest, TheShownStepIsPutBackAfterwards)
{
    ASSERT_EQ(visualizer->getCellStringEncoding(2, 3, "a"), std::to_string(valueA(30, 2, 3)));

    visualizer->substateRangeAcrossSteps(settings, "a", {0, 10}, std::nullopt);

    // the grid was overwritten with steps 0 and 10 while reading: it has to show step 30 again
    EXPECT_EQ(visualizer->getCellStringEncoding(2, 3, "a"), std::to_string(valueA(30, 2, 3)));
    EXPECT_EQ(settings.step, 30u); // the caller's settings are not touched
}

TEST_F(SubstateRangeVisualizerTest, TheShownStepIsPutBackAfterACancel)
{
    const SubstateRange range = visualizer->substateRangeAcrossSteps(settings,
                                                                      "a",
                                                                      {0, 10, 20},
                                                                      std::nullopt,
                                                                      [](const SubstateRangeProgress& state) { return state.stepNumber < 2; });

    EXPECT_TRUE(range.cancelled);
    EXPECT_EQ(visualizer->getCellStringEncoding(2, 3, "a"), std::to_string(valueA(30, 2, 3)));
}

TEST_F(SubstateRangeVisualizerTest, ProgressCoversAllStepsAndFinishesAtOne)
{
    std::vector<SubstateRangeProgress> reports;
    visualizer->substateRangeAcrossSteps(settings,
                                         "a",
                                         {0, 10, 20, 30},
                                         std::nullopt,
                                         [&reports](const SubstateRangeProgress& state)
                                         {
                                             reports.push_back(state);
                                             return true;
                                         });

    ASSERT_FALSE(reports.empty());
    EXPECT_DOUBLE_EQ(reports.back().fraction, 1.0);
    EXPECT_EQ(reports.back().stepsTotal, 4u);
    EXPECT_EQ(reports.back().step, 30u);
}

TEST_F(SubstateRangeVisualizerTest, NoStepsGiveNothing)
{
    const SubstateRange range = visualizer->substateRangeAcrossSteps(settings, "a", {}, std::nullopt);
    EXPECT_FALSE(range.hasValues());
}
