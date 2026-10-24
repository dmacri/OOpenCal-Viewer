#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "data/StepTimeMap.h"

namespace
{
StepTimeMap fromText(const std::string& text)
{
    std::istringstream stream(text);
    return StepTimeMap(stream);
}

/// Temporary directory removed together with the object
struct TempDir
{
    std::filesystem::path path = std::filesystem::temp_directory_path()
                                 / ("StepTimeMapTests_" + std::to_string(std::random_device{}()));
    TempDir() { std::filesystem::create_directories(path); }
    ~TempDir() { std::filesystem::remove_all(path); }
};
} // namespace

TEST(StepTimeMap, ReadsStepsAndTimes)
{
    const auto map = fromText("0 0.5\n1 1\n2 1.5\n");

    ASSERT_TRUE(map.isAvailable());
    EXPECT_EQ(map.size(), 3u);
    EXPECT_DOUBLE_EQ(*map.secondsForStep(0), 0.5);
    EXPECT_DOUBLE_EQ(*map.secondsForStep(1), 1.0);
    EXPECT_DOUBLE_EQ(*map.secondsForStep(2), 1.5);
    EXPECT_TRUE(map.errorMessage().empty());
}

TEST(StepTimeMap, StepMissingFromFileHasNoTime)
{
    const auto map = fromText("0 0\n500 2500\n");
    EXPECT_FALSE(map.secondsForStep(1).has_value());
    EXPECT_TRUE(map.secondsForStep(500).has_value());
}

TEST(StepTimeMap, IgnoresCommentsBlankLinesAndWindowsLineEndings)
{
    const auto map = fromText("# step time\r\n\r\n0   0\r\n   \r\n500\t2500.25   # trailing comment\r\n");

    ASSERT_TRUE(map.isAvailable());
    EXPECT_EQ(map.size(), 2u);
    EXPECT_DOUBLE_EQ(*map.secondsForStep(500), 2500.25);
}

TEST(StepTimeMap, AcceptsCommaAsDecimalSeparatorAndSignedNumbers)
{
    const auto map = fromText("0 0,5\n1 +2.25\n2 1e3\n");

    ASSERT_TRUE(map.isAvailable());
    EXPECT_DOUBLE_EQ(*map.secondsForStep(0), 0.5);
    EXPECT_DOUBLE_EQ(*map.secondsForStep(1), 2.25);
    EXPECT_DOUBLE_EQ(*map.secondsForStep(2), 1000.0);
}

TEST(StepTimeMap, ExtraColumnsAreIgnored)
{
    const auto map = fromText("0 0 5\n1 5 5\n");
    ASSERT_TRUE(map.isAvailable());
    EXPECT_DOUBLE_EQ(*map.secondsForStep(1), 5.0);
}

TEST(StepTimeMap, LastValueWinsForDuplicatedStep)
{
    const auto map = fromText("3 1\n3 2\n");
    EXPECT_DOUBLE_EQ(*map.secondsForStep(3), 2.0);
}

TEST(StepTimeMap, MalformedFileIsRejectedAsAWholeWithLineNumber)
{
    for (const char* bad : {"0 0\nabc 5\n", "0 0\n1\n", "0 0\n1 x\n", "-1 5\n", "1.5 5\n", "0 nan\n", "0 inf\n", "0 1.2.3\n",
                            "99999999999999999999 1\n"})
    {
        const auto map = fromText(bad);
        EXPECT_FALSE(map.isAvailable()) << bad;
        EXPECT_FALSE(map.errorMessage().empty()) << bad;
    }

    EXPECT_NE(fromText("0 0\n1 x\n").errorMessage().find("line 2"), std::string::npos);
}

TEST(StepTimeMap, EmptyInputIsNotAvailableAndNotAnError)
{
    const auto map = fromText("# only a comment\n\n");
    EXPECT_FALSE(map.isAvailable());
    EXPECT_TRUE(map.errorMessage().empty());
}

TEST(StepTimeMap, MissingFileIsNotAnError)
{
    const StepTimeMap map(std::filesystem::path("/definitely/not/here-time.txt"));
    EXPECT_FALSE(map.isAvailable());
    EXPECT_TRUE(map.errorMessage().empty());
}

TEST(StepTimeMap, LoadsFromFileAndRemembersItsPath)
{
    TempDir dir;
    const auto file = dir.path / "model-time.txt";
    std::ofstream(file) << "0 0\n1 3600\n";

    const StepTimeMap map(file);
    ASSERT_TRUE(map.isAvailable());
    EXPECT_EQ(map.filePath(), file);
    EXPECT_DOUBLE_EQ(*map.secondsForStep(1), 3600.0);
}

TEST(StepTimeMap, PathIsBuiltNextToTheOutputFiles)
{
    EXPECT_EQ(StepTimeMap::pathForOutput("/data/OutputSciddicaT/sciddicaTout"),
              std::filesystem::path("/data/OutputSciddicaT/sciddicaTout-time.txt"));
}
