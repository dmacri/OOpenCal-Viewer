// Tests of the reading of Header.txt shared by the main window and the actions of plugins (data/SimulationHeader.h).

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "data/SimulationHeader.h"
#include "visualiser/SettingParameter.h"

namespace fs = std::filesystem;

namespace
{
constexpr char CROWD_HEADER[] =
    "GENERAL:\n\tnumber_of_columns=38\n\tnumber_of_rows=38\n\tnumber_steps=160\n\toutput_file_name=crowd_simulation\n"
    "\tvisualization_step_rate=1\n\tvisualization_first_step=0\n"
    "DISTRIBUTED:\n\tborder_size_x=3\n\tborder_size_y=3\n\tnumber_node_x=2\n\tnumber_node_y=3\n"
    "VISUALIZATION:\n\tsubstates=(staticFloorField,%f,159),(pedestriansIds,%d,0,70)\n\tmode=binary\n\treduction=(exit_count,%d)\n";

class SimulationHeaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        directory = fs::temp_directory_path() /
                    (std::string("viz_header_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(directory);
        fs::create_directories(directory);
    }
    void TearDown() override { fs::remove_all(directory); }

    std::string writeHeader(const std::string& text) const
    {
        const fs::path path = directory / "Header.txt";
        std::ofstream(path) << text;
        return path.string();
    }
    void touch(const std::string& name) const { std::ofstream(directory / name) << "x"; }

    fs::path directory;
};

TEST_F(SimulationHeaderTest, ReadsGridNodesAndVisualizationSettings)
{
    SettingParameter settings{};
    SimulationHeader::readSettingParameter(writeHeader(CROWD_HEADER), settings, /*printWarnings=*/false);

    EXPECT_EQ(settings.numberOfColumnX, 38);
    EXPECT_EQ(settings.numberOfRowsY, 38);
    EXPECT_EQ(settings.numberOfSlicesZ, 1) << "2D model: no number_of_slices in the header";
    EXPECT_EQ(settings.nsteps, 160u);
    EXPECT_EQ(settings.nNodeX, 2u);
    EXPECT_EQ(settings.nNodeY, 3u);
    EXPECT_EQ(settings.nNodeZ, 1u);
    EXPECT_EQ(settings.readMode, "binary");
    EXPECT_EQ(settings.substates, "(staticFloorField,%f,159),(pedestriansIds,%d,0,70)");
    EXPECT_EQ(settings.reduction, "(exit_count,%d)");
}

TEST_F(SimulationHeaderTest, DataInTheOutputSubdirectoryIsTheDefault)
{
    const std::string header = writeHeader(CROWD_HEADER);
    SettingParameter settings{};
    SimulationHeader::readSettingParameter(header, settings, false);

    EXPECT_EQ(fs::path(settings.outputFileName), directory / "Output" / "crowd_simulation");
    EXPECT_FALSE(fs::exists(directory / "Output")) << "reading the header creates nothing";
}

TEST_F(SimulationHeaderTest, DataNextToTheHeaderIsFoundInEveryFormat)
{
    for (const char* extension : {".txt", ".bin", ".blosc"})
    {
        for (const auto& entry : fs::directory_iterator(directory))
            if (entry.path().filename() != "Header.txt")
                fs::remove(entry.path());
        touch(std::string("crowd_simulation0") + extension);

        const std::string path = SimulationHeader::resolveOutputFileName((directory / "Header.txt").string(), "crowd_simulation");
        EXPECT_EQ(fs::path(path), directory / "crowd_simulation") << extension;
    }
}

TEST_F(SimulationHeaderTest, OtherFilesNextToTheHeaderAreNotData)
{
    touch("notes.txt");
    touch("crowd_simulation0_index.log");
    const std::string path = SimulationHeader::resolveOutputFileName((directory / "Header.txt").string(), "crowd_simulation");
    EXPECT_EQ(fs::path(path), directory / "Output" / "crowd_simulation");
}

TEST_F(SimulationHeaderTest, CountsBoundaryLinesOfAllNodes)
{
    SettingParameter settings{};
    settings.nNodeX = 2;
    settings.nNodeY = 3;
    settings.nNodeZ = 1;
    EXPECT_EQ(SimulationHeader::numberOfBoundaryLines(settings), 2 * 6 + 2 * 1 + 3 * 1);

    settings.nNodeX = settings.nNodeY = settings.nNodeZ = 1;
    EXPECT_EQ(SimulationHeader::numberOfBoundaryLines(settings), 2 + 1 + 1);
}

TEST_F(SimulationHeaderTest, ParametersMissingInTheFileGetTheDefaultsOfConfig)
{
    SettingParameter settings{};
    SimulationHeader::readSettingParameter(
        writeHeader("GENERAL:\n\tnumber_of_columns=4\n\tnumber_of_rows=5\n\tnumber_steps=6\n\toutput_file_name=m\n"
                    "DISTRIBUTED:\n\tnumber_node_x=1\n\tnumber_node_y=1\n"),
        settings, false);

    EXPECT_EQ(settings.numberOfColumnX, 4);
    EXPECT_EQ(settings.numberOfRowsY, 5);
    EXPECT_TRUE(settings.substates.empty()) << "no VISUALIZATION section: no substates";
    EXPECT_TRUE(settings.reduction.empty());
}

TEST_F(SimulationHeaderTest, ReadsThe3DSizeWhenTheHeaderHasIt)
{
    SettingParameter settings{};
    SimulationHeader::readSettingParameter(
        writeHeader("GENERAL:\n\tnumber_of_columns=9\n\tnumber_of_rows=8\n\tnumber_of_slices=7\n\tnumber_steps=6\n\toutput_file_name=ball\n"
                    "DISTRIBUTED:\n\tnumber_node_x=2\n\tnumber_node_y=1\n\tnumber_node_z=3\n"),
        settings, false);

    EXPECT_EQ(settings.numberOfSlicesZ, 7);
    EXPECT_EQ(settings.nNodeZ, 3u);
}
} // namespace
