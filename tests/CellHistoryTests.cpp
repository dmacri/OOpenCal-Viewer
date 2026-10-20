#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "core/types.h"
#include "data/ModelReader.hpp"
#include "visualiserProxy/CellHistory.h"
#include "visualiserProxy/ContiguousGrid.h"

namespace
{
/// Cell holding two numbers; works for the text format ("a;b") and as a plain-old-data record (binary)
class HistoryCell
{
public:
    virtual ~HistoryCell() = default;

    void composeElement(char* text)
    {
        // "<a>;<b>"
        char* separator = std::strchr(text, ';');
        a = std::atoi(text);
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

    void startStep(int s)
    {
        lastStep = s;
    }

    int a = 0;
    double b = 0.0;
    int lastStep = -1;
};

namespace fs = std::filesystem;

struct TempDir
{
    fs::path path;
    explicit TempDir(const std::string& name)
        : path(fs::temp_directory_path() / name)
    {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir()
    {
        fs::remove_all(path);
    }
    std::string base(const std::string& file) const
    {
        return (path / file).string();
    }
};

SettingParameter makeSettings(const std::string& base, const std::string& mode, int nodesX, int nodesY, int nodesZ)
{
    SettingParameter sp{};
    sp.outputFileName = base;
    sp.readMode = mode;
    sp.nNodeX = nodesX;
    sp.nNodeY = nodesY;
    sp.nNodeZ = nodesZ;
    return sp;
}

/// Value of cell (row, col, slice) at a step in these tests
int expectedA(unsigned step, int row, int col, int slice)
{
    return static_cast<int>(step) * 1000 + slice * 100 + row * 10 + col;
}
} // namespace

// ---------------------------------------------------------------------------------------------
// parseSubstateValue
// ---------------------------------------------------------------------------------------------
TEST(ParseSubstateValue, AcceptsDotAndCommaAsDecimalSeparator)
{
    EXPECT_DOUBLE_EQ(parseSubstateValue("1008.000000"), 1008.0);
    EXPECT_DOUBLE_EQ(parseSubstateValue("1008,000000"), 1008.0);
    EXPECT_DOUBLE_EQ(parseSubstateValue("-0,5"), -0.5);
    EXPECT_DOUBLE_EQ(parseSubstateValue("+2.5"), 2.5);
    EXPECT_DOUBLE_EQ(parseSubstateValue("  42 \n"), 42.0);
    EXPECT_DOUBLE_EQ(parseSubstateValue("1e3"), 1000.0);
    EXPECT_DOUBLE_EQ(parseSubstateValue("7"), 7.0);
}

TEST(ParseSubstateValue, NonNumbersGiveNaN)
{
    EXPECT_TRUE(std::isnan(parseSubstateValue("")));
    EXPECT_TRUE(std::isnan(parseSubstateValue("   ")));
    EXPECT_TRUE(std::isnan(parseSubstateValue("abc")));
    EXPECT_TRUE(std::isnan(parseSubstateValue("12abc")));
    EXPECT_TRUE(std::isnan(parseSubstateValue("[1,000000,2,000000]")));
    EXPECT_TRUE(std::isnan(parseSubstateValue("1.2.3")));
    EXPECT_TRUE(std::isnan(parseSubstateValue("1,2,3")));
}

// ---------------------------------------------------------------------------------------------
// cellHistoryToCsv
// ---------------------------------------------------------------------------------------------
TEST(CellHistoryCsv, WritesStepColumnAndOneColumnPerSubstateWithGapsEmpty)
{
    CellHistory history;
    history.fields = {"h", "z"};
    history.steps = {0, 500, 1000};
    history.values = {{1.5, std::nan(""), 3.0}, {10.0, 20.0, 30.0}};

    EXPECT_EQ(cellHistoryToCsv(history), "step,h,z\n0,1.5,10\n500,,20\n1000,3,30\n");
}

// ---------------------------------------------------------------------------------------------
// gridIndexFromSliceView
// ---------------------------------------------------------------------------------------------
TEST(GridIndexFromSliceView, AgreesWithTheSliceViewForAllAxes)
{
    ContiguousGrid<int> grid(3, 4, 5);
    for (std::size_t r = 0; r < grid.rowCount(); ++r)
        for (std::size_t c = 0; c < grid.columnCount(); ++c)
            for (std::size_t l = 0; l < grid.layerCount(); ++l)
                grid[r, c, l] = static_cast<int>(100 * r + 10 * c + l);

    for (const auto axis : {GridSliceAxis::X, GridSliceAxis::Y, GridSliceAxis::Z})
    {
        const std::size_t fixedSize = axis == GridSliceAxis::X ? grid.columnCount()
                                      : axis == GridSliceAxis::Y ? grid.rowCount()
                                                                 : grid.layerCount();
        for (std::size_t fixed = 0; fixed < fixedSize; ++fixed)
        {
            const ContiguousGridSliceView<int> view(grid, axis, fixed);
            for (std::size_t vr = 0; vr < view.rowCount(); ++vr)
            {
                for (std::size_t vc = 0; vc < view.columnCount(); ++vc)
                {
                    const auto index = gridIndexFromSliceView(axis, fixed, grid.layerCount(), vr, vc);
                    EXPECT_EQ((grid[index.row, index.column, index.layer]), view[vr][vc])
                        << "axis=" << static_cast<int>(axis) << " fixed=" << fixed << " vr=" << vr << " vc=" << vc;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// ModelReader::readCellAcrossSteps - text format
// ---------------------------------------------------------------------------------------------
TEST(ReadCellAcrossSteps, TextFormat2x2NodesPicksTheRightNodeAndCell)
{
    // Grid 4 columns x 4 rows, 2x2 nodes of 2x2 cells, 3 steps (0, 10, 20)
    TempDir dir("oopencal-viewer-history-text");
    const auto base = dir.base("m");
    const std::vector<unsigned> steps = {0, 10, 20};

    for (int node = 0; node < 4; ++node)
    {
        const int offX = (node % 2) * 2;
        const int offY = (node / 2) * 2;
        std::ofstream index(base + std::to_string(node) + "_index.txt");
        std::ofstream data(base + std::to_string(node) + ".txt");
        for (std::size_t s = 0; s < steps.size(); ++s)
        {
            index << steps[s] << " " << data.tellp() << "\n";
            data << "2-2\n";
            for (int row = 0; row < 2; ++row)
            {
                for (int col = 0; col < 2; ++col)
                    data << expectedA(steps[s], offY + row, offX + col, 0) << ";" << 0.5 * steps[s] << " ";
                data << "\n";
            }
        }
    }

    ModelReader<HistoryCell> reader;
    reader.readStepsOffsetsForAllNodesFromFiles(2, 2, 1, base);
    const auto settings = makeSettings(base, "text", 2, 2, 1);

    for (const auto [row, col] : {std::pair{0, 0}, {1, 3}, {3, 1}, {2, 2}, {3, 3}})
    {
        const auto cells = reader.readCellAcrossSteps(settings, row, col, 0, steps);
        ASSERT_EQ(cells.size(), steps.size());
        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            ASSERT_TRUE(cells[i].has_value()) << "row=" << row << " col=" << col << " step=" << steps[i];
            EXPECT_EQ(cells[i]->a, expectedA(steps[i], row, col, 0)) << "row=" << row << " col=" << col;
            EXPECT_DOUBLE_EQ(cells[i]->b, 0.5 * steps[i]);
            EXPECT_EQ(cells[i]->lastStep, static_cast<int>(steps[i])); // startStep() was called
        }
    }
}

TEST(ReadCellAcrossSteps, TextFormat3DReadsTheRequestedSlice)
{
    // 2 nodes in X (2 + 1 columns), 3 rows, 2 slices, 2 steps
    TempDir dir("oopencal-viewer-history-text3d");
    const auto base = dir.base("v");
    const std::vector<unsigned> steps = {0, 5};
    const int columns[2] = {2, 1};
    const int offsets[2] = {0, 2};

    for (int node = 0; node < 2; ++node)
    {
        std::ofstream index(base + std::to_string(node) + "_index.txt");
        std::ofstream data(base + std::to_string(node) + ".txt");
        for (const auto step : steps)
        {
            index << step << " " << data.tellp() << "\n";
            data << columns[node] << "-3-2\n";
            for (int slice = 0; slice < 2; ++slice)
                for (int row = 0; row < 3; ++row)
                {
                    for (int col = 0; col < columns[node]; ++col)
                        data << expectedA(step, row, offsets[node] + col, slice) << ";0 ";
                    data << "\n";
                }
        }
    }

    ModelReader<HistoryCell> reader;
    reader.readStepsOffsetsForAllNodesFromFiles(2, 1, 1, base);
    const auto settings = makeSettings(base, "text", 2, 1, 1);

    for (int slice = 0; slice < 2; ++slice)
    {
        const auto cells = reader.readCellAcrossSteps(settings, 2, 2, slice, steps); // column 2 lives in node 1
        ASSERT_EQ(cells.size(), 2u);
        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            ASSERT_TRUE(cells[i].has_value());
            EXPECT_EQ(cells[i]->a, expectedA(steps[i], 2, 2, slice));
        }
    }
}

// ---------------------------------------------------------------------------------------------
// ModelReader::readCellAcrossSteps - binary format
// ---------------------------------------------------------------------------------------------
TEST(ReadCellAcrossSteps, BinaryFormatSeeksStraightToTheCellRecord)
{
    // 1 node 3 columns x 2 rows x 2 slices, 3 steps. Records are written as raw objects, like OOpenCAL does.
    TempDir dir("oopencal-viewer-history-bin");
    const auto base = dir.base("b");
    const std::vector<unsigned> steps = {0, 7, 9};

    {
        std::ofstream index(base + "0_index.txt");
        std::ofstream data(base + "0.bin", std::ios::binary);
        for (const auto step : steps)
        {
            index << step << " " << data.tellp() << " (3-2-2)\n";
            for (int slice = 0; slice < 2; ++slice)
                for (int row = 0; row < 2; ++row)
                    for (int col = 0; col < 3; ++col)
                    {
                        HistoryCell cell;
                        cell.a = expectedA(step, row, col, slice);
                        cell.b = 1.25 * step;
                        data.write(reinterpret_cast<const char*>(&cell), sizeof(cell));
                    }
        }
    }

    ModelReader<HistoryCell> reader;
    reader.readStepsOffsetsForAllNodesFromFiles(1, 1, 1, base + "");
    const auto settings = makeSettings(base, "binary", 1, 1, 1);

    const auto cells = reader.readCellAcrossSteps(settings, 1, 2, 1, steps);
    ASSERT_EQ(cells.size(), steps.size());
    for (std::size_t i = 0; i < steps.size(); ++i)
    {
        ASSERT_TRUE(cells[i].has_value());
        EXPECT_EQ(cells[i]->a, expectedA(steps[i], 1, 2, 1));
        EXPECT_DOUBLE_EQ(cells[i]->b, 1.25 * steps[i]);
        // The record carries the vtable pointer of the writer: the returned cell must be a proper object
        EXPECT_EQ(cells[i]->stringEncoding("a"), std::to_string(expectedA(steps[i], 1, 2, 1)));
    }
}

// ---------------------------------------------------------------------------------------------
// Failure handling
// ---------------------------------------------------------------------------------------------
TEST(ReadCellAcrossSteps, CellOutsideTheGridAndMissingFilesGiveEmptyEntries)
{
    TempDir dir("oopencal-viewer-history-missing");
    const auto base = dir.base("x");
    {
        std::ofstream index(base + "0_index.txt");
        std::ofstream data(base + "0.txt");
        index << "0 0\n";
        data << "1-1\n1;0 \n";
    }

    ModelReader<HistoryCell> reader;
    reader.readStepsOffsetsForAllNodesFromFiles(1, 1, 1, base);
    auto settings = makeSettings(base, "text", 1, 1, 1);

    const auto outside = reader.readCellAcrossSteps(settings, 5, 5, 0, {0});
    ASSERT_EQ(outside.size(), 1u);
    EXPECT_FALSE(outside[0].has_value());

    const auto unknownStep = reader.readCellAcrossSteps(settings, 0, 0, 0, {0, 99});
    ASSERT_EQ(unknownStep.size(), 2u);
    EXPECT_TRUE(unknownStep[0].has_value());
    EXPECT_FALSE(unknownStep[1].has_value());
}

TEST(ReadCellAcrossSteps, ProgressIsReportedAndCanCancel)
{
    TempDir dir("oopencal-viewer-history-progress");
    const auto base = dir.base("p");
    const std::vector<unsigned> steps = {0, 1, 2, 3};
    {
        std::ofstream index(base + "0_index.txt");
        std::ofstream data(base + "0.txt");
        for (const auto step : steps)
        {
            index << step << " " << data.tellp() << "\n";
            data << "1-1\n" << step << ";0 \n";
        }
    }

    ModelReader<HistoryCell> reader;
    reader.readStepsOffsetsForAllNodesFromFiles(1, 1, 1, base);
    const auto settings = makeSettings(base, "text", 1, 1, 1);

    std::vector<std::size_t> reported;
    const auto all = reader.readCellAcrossSteps(settings, 0, 0, 0, steps, [&](std::size_t done, std::size_t total)
    {
        EXPECT_EQ(total, steps.size());
        reported.push_back(done);
        return true;
    });
    EXPECT_EQ(reported, (std::vector<std::size_t>{0, 1, 2, 3, 4}));
    for (const auto& cell : all)
        EXPECT_TRUE(cell.has_value());

    const auto cancelled = reader.readCellAcrossSteps(settings, 0, 0, 0, steps, [](std::size_t done, std::size_t)
    {
        return done < 2; // cancel before the third step
    });
    ASSERT_EQ(cancelled.size(), steps.size());
    EXPECT_TRUE(cancelled[0].has_value());
    EXPECT_TRUE(cancelled[1].has_value());
    EXPECT_FALSE(cancelled[2].has_value());
    EXPECT_FALSE(cancelled[3].has_value());
}
