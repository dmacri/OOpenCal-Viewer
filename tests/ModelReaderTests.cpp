#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "core/types.h"
#include "data/ModelReader.hpp"
#include "visualiserProxy/ContiguousGrid.h"

namespace
{
class TestCell
{
public:
    void composeElement(char* text)
    {
        value = std::stoi(text);
    }

    std::string stringEncoding(const char* = nullptr) const
    {
        return std::to_string(value);
    }

    Color outputValue(const char*, GlobalValueManager*) const
    {
        return Color(static_cast<std::uint8_t>(value), 0, 0);
    }

    void startStep(int)
    {
    }

    int value = 0;
};
}

TEST(ParseGridDimensions, Supports2DAnd3DHeaders)
{
    const auto dimensions2D = ReaderHelpers::getDimensionsFromLine("250-500");
    EXPECT_EQ(dimensions2D.column, 250);
    EXPECT_EQ(dimensions2D.row, 500);
    EXPECT_EQ(dimensions2D.slice, 1);

    const auto dimensions3D = ReaderHelpers::getDimensionsFromLine("50-99-99 ");
    EXPECT_EQ(dimensions3D.column, 50);
    EXPECT_EQ(dimensions3D.row, 99);
    EXPECT_EQ(dimensions3D.slice, 99);
}

TEST(ContiguousGridSliceView, MapsAllOrthogonalPlanesWithoutCopying)
{
    ContiguousGrid<int> grid(2, 3, 4);
    for (std::size_t row = 0; row < grid.rowCount(); ++row)
    {
        for (std::size_t column = 0; column < grid.columnCount(); ++column)
        {
            for (std::size_t layer = 0; layer < grid.layerCount(); ++layer)
                grid[row, column, layer] = static_cast<int>(100 * row + 10 * column + layer);
        }
    }

    const ContiguousGridSliceView<int> xy(grid, GridSliceAxis::Z, 2);
    EXPECT_EQ(xy.rowCount(), 2);
    EXPECT_EQ(xy.columnCount(), 3);
    EXPECT_EQ(xy[1][2], 122);

    const ContiguousGridSliceView<int> xz(grid, GridSliceAxis::Y, 1);
    EXPECT_EQ(xz.rowCount(), 4);
    EXPECT_EQ(xz.columnCount(), 3);
    EXPECT_EQ(xz[0][2], 123); // top row is the highest Z
    EXPECT_EQ(xz[3][2], 120); // bottom row is Z=0

    const ContiguousGridSliceView<int> yz(grid, GridSliceAxis::X, 2);
    EXPECT_EQ(yz.rowCount(), 4);
    EXPECT_EQ(yz.columnCount(), 2);
    EXPECT_EQ(yz[0][1], 123);
    EXPECT_EQ(yz[3][0], 20);
}

TEST(CalculateXYZOffsetForNode, TwoByOneByTwo)
{
    const std::vector<ColumnRowSlice> dimensions = {
        ColumnRowSlice::xyz(50, 99, 40),
        ColumnRowSlice::xyz(49, 99, 40),
        ColumnRowSlice::xyz(50, 99, 59),
        ColumnRowSlice::xyz(49, 99, 59)
    };

    const auto node0 = ReaderHelpers::calculateXYZOffsetForNode(0, 2, 1, 2, dimensions);
    const auto node1 = ReaderHelpers::calculateXYZOffsetForNode(1, 2, 1, 2, dimensions);
    const auto node2 = ReaderHelpers::calculateXYZOffsetForNode(2, 2, 1, 2, dimensions);
    const auto node3 = ReaderHelpers::calculateXYZOffsetForNode(3, 2, 1, 2, dimensions);

    EXPECT_EQ(node0.x(), 0);
    EXPECT_EQ(node0.z(), 0);
    EXPECT_EQ(node1.x(), 50);
    EXPECT_EQ(node1.z(), 0);
    EXPECT_EQ(node2.x(), 0);
    EXPECT_EQ(node2.z(), 40);
    EXPECT_EQ(node3.x(), 50);
    EXPECT_EQ(node3.z(), 40);
}

TEST(ReadStageState, StitchesAllSlicesFromMultipleTextNodes)
{
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() / "oopencal-viewer-reader-3d-test";
    fs::remove_all(directory);
    fs::create_directories(directory);
    const std::string baseName = (directory / "volume").string();

    {
        std::ofstream index(baseName + "0_index.txt");
        index << "0 0\n";
        std::ofstream data(baseName + "0.txt");
        data << "1-2-2\n"
             << "1 \n"
             << "2 \n"
             << "3 \n"
             << "4 \n";
    }
    {
        std::ofstream index(baseName + "1_index.txt");
        index << "0 0\n";
        std::ofstream data(baseName + "1.txt");
        data << "1-2-2\n"
             << "5 \n"
             << "6 \n"
             << "7 \n"
             << "8 \n";
    }

    ModelReader<TestCell> reader;
    reader.readStepsOffsetsForAllNodesFromFiles(2, 1, 1, baseName);

    SettingParameter settings{};
    settings.step = 0;
    settings.numberOfColumnX = 2;
    settings.numberOfRowsY = 2;
    settings.numberOfSlicesZ = 2;
    settings.nNodeX = 2;
    settings.nNodeY = 1;
    settings.nNodeZ = 1;
    settings.outputFileName = baseName;
    settings.readMode = "text";

    ContiguousGrid<TestCell> volume(2, 2, 2);
    std::vector<Line> lines(7);
    reader.readStageStateFromFilesForStep(volume, &settings, lines.data());

    EXPECT_EQ((volume[0, 0, 0].value), 1);
    EXPECT_EQ((volume[1, 0, 0].value), 2);
    EXPECT_EQ((volume[0, 0, 1].value), 3);
    EXPECT_EQ((volume[1, 0, 1].value), 4);
    EXPECT_EQ((volume[0, 1, 0].value), 5);
    EXPECT_EQ((volume[1, 1, 0].value), 6);
    EXPECT_EQ((volume[0, 1, 1].value), 7);
    EXPECT_EQ((volume[1, 1, 1].value), 8);

    fs::remove_all(directory);
}

/**
 * Test Suite: calculateXYOffsetForNode
 *
 * This test suite verifies the correct calculation of offsets for distributed nodes
 * in a multi-node simulation grid.
 *
 * KEY FINDING: The function itself is CORRECT!
 * The problem is likely in how columnsAndRows data is populated from index files.
 *
 * When nNodeX != nNodeY (asymmetric configurations like 4x1 or 1x4),
 * the data might be read incorrectly, leading to wrong dimensions per node.
 */

// ============================================================================
// Test 1: 2x1 Configuration (2 nodes horizontally, 1 node vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, TwoByOne_500x500)
{
    /* Scene: 500x500
     * Nodes: 2x1 (2 nodes in X direction, 1 node in Y direction)
     *
     * Layout:
     * ┌─────────────────┬─────────────────┐
     * │   Node 0        │   Node 1        │
     * │  (250x500)      │  (250x500)      │
     * │  offset(0,0)    │  offset(250,0)  │
     * └─────────────────┴─────────────────┘ */

    const std::vector<ColumnAndRow> columnsAndRows =
    {
        ColumnAndRow{.column = 250, .row = 500},  // Node 0
        ColumnAndRow{.column = 250, .row = 500}   // Node 1
    };
    
    // Node 0: should be at (0, 0)
    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/2, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);

    // Node 1: should be at (250, 0)
    const auto offset1 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/1, /*nNodeX=*/2, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset1.x(), columnsAndRows.front().column);
    EXPECT_EQ(offset1.y(), 0);
}

// ============================================================================
// Test 2: 1x2 Configuration (1 node horizontally, 2 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, OneByTwo_500x500)
{
    /* Scene: 500x500
     * Nodes: 1x2 (1 node in X direction, 2 nodes in Y direction)
     * 
     * Layout:
     * ┌─────────────────┐
     * │   Node 0        │
     * │  (500x250)      │
     * │  offset(0,0)    │
     * ├─────────────────┤
     * │   Node 1        │
     * │  (500x250)      │
     * │  offset(0,250)  │
     * └─────────────────┘ */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 500, .row = 250},  // Node 0
        ColumnAndRow{.column = 500, .row = 250}   // Node 1
    };

    // Node 0: should be at (0, 0)
    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/1, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);

    // Node 1: should be at (0, 250)
    const auto offset1 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/1, /*nNodeX=*/1, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset1.x(), 0);
    EXPECT_EQ(offset1.y(), 250);
}

// ============================================================================
// Test 3: 2x2 Configuration (2 nodes horizontally, 2 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, TwoByTwo_500x500)
{
    /* Scene: 500x500
     * Nodes: 2x2 (2 nodes in X direction, 2 nodes in Y direction)
     *
     * Layout (row-major indexing):
     * ┌──────────────┬──────────────┐
     * │   Node 0     │   Node 1     │
     * │ (250x250)    │ (250x250)    │
     * │ offset(0,0)  │ offset(250,0)│
     * ├──────────────┼──────────────┤
     * │   Node 2     │   Node 3     │
     * │ (250x250)    │ (250x250)    │
     * │ offset(0,250)│ offset(250,250)
     * └──────────────┴──────────────┘
     * 
     * Node indexing (row-major):
     * Node 0 = row 0, col 0
     * Node 1 = row 0, col 1
     * Node 2 = row 1, col 0
     * Node 3 = row 1, col 1 */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 250, .row = 250},  // Node 0
        ColumnAndRow{.column = 250, .row = 250},  // Node 1
        ColumnAndRow{.column = 250, .row = 250},  // Node 2
        ColumnAndRow{.column = 250, .row = 250}   // Node 3
    };

    // Node 0: should be at (0, 0)
    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/2, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);

    // Node 1: should be at (250, 0)
    const auto offset1 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/1, /*nNodeX=*/2, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset1.x(), 250);
    EXPECT_EQ(offset1.y(), 0);

    // Node 2: should be at (0, 250)
    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/2, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset2.x(), 0);
    EXPECT_EQ(offset2.y(), 250);

    // Node 3: should be at (250, 250)
    const auto offset3 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/3, /*nNodeX=*/2, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset3.x(), 250);
    EXPECT_EQ(offset3.y(), 250);
}

// ============================================================================
// Test 4: 4x1 Configuration (4 nodes horizontally, 1 node vertically)
// THIS IS THE PROBLEMATIC CASE!
// ============================================================================
TEST(CalculateXYOffsetForNode, FourByOne_500x500)
{
    /* Scene: 500x500, Nodes: 4x1 (4 nodes in X direction, 1 node in Y direction)
     *
     * Layout:
     * ┌───────────┬─────────────┬─────────────┬─────────────┐
     * │ Node 0    │ Node 1      │ Node 2      │ Node 3      │
     * │(125x500)  │(125x500)    │(125x500)    │(125x500)    │
     * │offset(0,0)│offset(125,0)│offset(250,0)│offset(375,0)│
     * └───────────┴─────────────┴─────────────┴─────────────┘ */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 125, .row = 500},  // Node 0
        ColumnAndRow{.column = 125, .row = 500},  // Node 1
        ColumnAndRow{.column = 125, .row = 500},  // Node 2
        ColumnAndRow{.column = 125, .row = 500}   // Node 3
    };

    // Node 0: should be at (0, 0)
    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);

    // Node 1: should be at (125, 0)
    const auto offset1 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/1, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset1.x(), 125);
    EXPECT_EQ(offset1.y(), 0);

    // Node 2: should be at (250, 0)
    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset2.x(), 250);
    EXPECT_EQ(offset2.y(), 0);

    // Node 3: should be at (375, 0)
    const auto offset3 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/3, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset3.x(), 375);
    EXPECT_EQ(offset3.y(), 0);
}

// ============================================================================
// Test 5: 1x4 Configuration (1 node horizontally, 4 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, OneByFour_500x500)
{
    /* Scene: 500x500, Nodes: 1x4 (1 node in X direction, 4 nodes in Y direction)
     *
     * Layout:
     * ┌──────────────┐
     * │   Node 0     │
     * │  (500x125)   │
     * │ offset(0,0)  │
     * ├──────────────┤
     * │   Node 1     │
     * │  (500x125)   │
     * │ offset(0,125)│
     * ├──────────────┤
     * │   Node 2     │
     * │  (500x125)   │
     * │ offset(0,250)│
     * ├──────────────┤
     * │   Node 3     │
     * │  (500x125)   │
     * │ offset(0,375)│
     * └──────────────┘ */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 500, .row = 125},  // Node 0
        ColumnAndRow{.column = 500, .row = 125},  // Node 1
        ColumnAndRow{.column = 500, .row = 125},  // Node 2
        ColumnAndRow{.column = 500, .row = 125}   // Node 3
    };

    // Node 0: should be at (0, 0)
    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/1, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);

    // Node 1: should be at (0, 125)
    const auto offset1 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/1, /*nNodeX=*/1, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset1.x(), 0);
    EXPECT_EQ(offset1.y(), 125);

    // Node 2: should be at (0, 250)
    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/1, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset2.x(), 0);
    EXPECT_EQ(offset2.y(), 250);

    // Node 3: should be at (0, 375)
    const auto offset3 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/3, /*nNodeX=*/1, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset3.x(), 0);
    EXPECT_EQ(offset3.y(), 375);
}

// ============================================================================
// Test 6: 4x4 Configuration (4 nodes horizontally, 4 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, FourByFour_500x500)
{
    /* Scene: 500x500, Nodes: 4x4  (4 nodes in X direction, 4 nodes in Y direction), Each node: 125x125
     *
     * Layout (row-major indexing):
     * ┌──────┬──────┬──────┬──────┐
     * │  0   │  1   │  2   │  3   │
     * ├──────┼──────┼──────┼──────┤
     * │  4   │  5   │  6   │  7   │
     * ├──────┼──────┼──────┼──────┤
     * │  8   │  9   │ 10   │ 11   │
     * ├──────┼──────┼──────┼──────┤
     * │ 12   │ 13   │ 14   │ 15   │
     * └──────┴──────┴──────┴──────┘ */

    std::vector<ColumnAndRow> columnsAndRows(16);
    for (int i = 0; i < 16; ++i)
    {
        columnsAndRows[i] = ColumnAndRow{.column = 125, .row = 125};
    }

    // Test a few key nodes
    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/4, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);

    const auto offset5 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/5, /*nNodeX=*/4, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset5.x(), 125);
    EXPECT_EQ(offset5.y(), 125);

    const auto offset10 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/10, /*nNodeX=*/4, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset10.x(), 250);
    EXPECT_EQ(offset10.y(), 250);

    const auto offset15 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/15, /*nNodeX=*/4, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset15.x(), 375);
    EXPECT_EQ(offset15.y(), 375);
}

// ============================================================================
// Test 7: Uneven Distribution (2x2 with different sizes)
// ============================================================================
TEST(CalculateXYOffsetForNode, TwoByTwo_UnevenDistribution)
{
    /* Scene: 600x600, Nodes: 2x2 with uneven distribution
     *
     * Layout:
     * ┌──────────────┬────────────────┐
     * │   Node 0     │   Node 1       │
     * │ (300x300)    │ (300x300)      │
     * │ offset(0,0)  │ offset(300,0)  │
     * ├──────────────┼────────────────┤
     * │   Node 2     │   Node 3       │
     * │ (400x300)    │ (200x300)      │
     * │ offset(0,300)│ offset(400,300)│
     * └──────────────┴────────────────┘ */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 300, .row = 300},  // Node 0
        ColumnAndRow{.column = 300, .row = 300},  // Node 1
        ColumnAndRow{.column = 400, .row = 300},  // Node 2
        ColumnAndRow{.column = 200, .row = 300}   // Node 3
    };

    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/2, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);

    const auto offset1 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/1, /*nNodeX=*/2, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset1.x(), 300);
    EXPECT_EQ(offset1.y(), 0);

    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/2, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset2.x(), 0);
    EXPECT_EQ(offset2.y(), 300);

    const auto offset3 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/3, /*nNodeX=*/2, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset3.x(), 400);
    EXPECT_EQ(offset3.y(), 300);
}

// ============================================================================
// Test 8: Real data from OOpenCAL Ball model (4x1 configuration)
// This test uses actual data that was reported as problematic
// ============================================================================
TEST(CalculateXYOffsetForNode, RealData_Ball_4x1)
{
    /* Real scenario from OOpenCAL Ball model with 4x1 configuration
     * Scene: 500x500, Nodes: 4x1
     *
     * From the error report:
     * - node=2, row=0, col=0: offsetXY={column:500, row:0}
     * - But array is 500x500 (indices 0-499)
     * - This suggests columnsAndRows might be wrong
     *
     * Expected columnsAndRows for 4x1 with 500x500 scene:
     * Each node should have 125 columns and 500 rows */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 125, .row = 500},  // Node 0
        ColumnAndRow{.column = 125, .row = 500},  // Node 1
        ColumnAndRow{.column = 125, .row = 500},  // Node 2
        ColumnAndRow{.column = 125, .row = 500}   // Node 3
    };

    // Node 2 should be at (250, 0), NOT (500, 0)
    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset2.x(), 250) << "Node 2 should have X offset of 250, not 500";
    EXPECT_EQ(offset2.y(), 0);

    // Verify all nodes
    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);

    const auto offset1 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/1, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset1.x(), 125);

    const auto offset3 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/3, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset3.x(), 375);
}

// ============================================================================
// Test 9: Incorrect columnsAndRows (what might be causing the crash)
// ============================================================================
TEST(CalculateXYOffsetForNode, IncorrectColumnsAndRows_4x1)
{
    /* This test simulates what might be happening if columnsAndRows
     * is incorrectly populated with {250, 250, 125, 125} instead of {125, 125, 125, 125}
     *
     * This would cause:
     * - Node 0: offset = 0
     * - Node 1: offset = 250
     * - Node 2: offset = 250 + 250 = 500 (OUT OF BOUNDS!)
     * - Node 3: offset = 250 + 250 + 125 = 625 (OUT OF BOUNDS!) */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 250, .row = 500},  // Node 0 - WRONG!
        ColumnAndRow{.column = 250, .row = 500},  // Node 1 - WRONG!
        ColumnAndRow{.column = 125, .row = 500},  // Node 2
        ColumnAndRow{.column = 125, .row = 500}   // Node 3
    };

    // This will show the ACTUAL problem
    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset2.x(), 500) << "With incorrect columnsAndRows, Node 2 gets offset 500 (OUT OF BOUNDS)";

    const auto offset3 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/3, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset3.x(), 625) << "With incorrect columnsAndRows, Node 3 gets offset 625 (OUT OF BOUNDS)";
}

// ============================================================================
// Test 10: Debug test - trace through 4x1 calculation step by step
// ============================================================================
TEST(CalculateXYOffsetForNode, Debug_4x1_StepByStep)
{
    /* This test traces through the calculation for 4x1 configuration
     *
     * For node=2:
     * offsetX loop: k = (2 / 4) * 4 = 0; k < 2
     *   k=0: offsetX += 125
     *   k=1: offsetX += 125
     *   Result: offsetX = 250
     * offsetY: node (2) >= nNodeX (4)? NO
     *   offsetY = 0 */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 125, .row = 500},  // Node 0
        ColumnAndRow{.column = 125, .row = 500},  // Node 1
        ColumnAndRow{.column = 125, .row = 500},  // Node 2
        ColumnAndRow{.column = 125, .row = 500}   // Node 3
    };

    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/4, /*nNodeY=*/1, columnsAndRows);
    EXPECT_EQ(offset2.x(), 250) << "Node 2 X offset should be 250";
    EXPECT_EQ(offset2.y(), 0) << "Node 2 Y offset should be 0";
}

// ============================================================================
// Test 11: Debug test - trace through 1x4 calculation step by step
// ============================================================================
TEST(CalculateXYOffsetForNode, Debug_1x4_StepByStep)
{
    /* This test traces through the calculation for 1x4 configuration
     *
     * For node=2:
     * offsetX loop: k = (2 / 1) * 1 = 2; k < 2? NO
     *   offsetX = 0
     * offsetY: node (2) >= nNodeX (1)? YES
     *   k = 2 - 1 = 1; k >= 0? YES
     *     offsetY += 125
     *     k = 1 - 1 = 0; k >= 0? YES
     *       offsetY += 125
     *       k = 0 - 1 = -1; k >= 0? NO
     *   Result: offsetY = 250 */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 500, .row = 125},  // Node 0
        ColumnAndRow{.column = 500, .row = 125},  // Node 1
        ColumnAndRow{.column = 500, .row = 125},  // Node 2
        ColumnAndRow{.column = 500, .row = 125}   // Node 3
    };

    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/1, /*nNodeY=*/4, columnsAndRows);
    EXPECT_EQ(offset2.x(), 0) << "Node 2 X offset should be 0";
    EXPECT_EQ(offset2.y(), 250) << "Node 2 Y offset should be 250";
}

// ============================================================================
// Test 12: 3x2 Configuration (3 nodes horizontally, 2 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, ThreeByTwo_600x400)
{
    /* Scene: 600x400, Nodes: 3x2 (3 nodes in X direction, 2 nodes in Y direction)
     *
     * Layout (row-major indexing):
     * ┌────────────────┬────────────────┬────────────────┐
     * │    Node 0      │    Node 1      │    Node 2      │
     * │   (200x200)    │   (200x200)    │   (200x200)    │
     * │ offset(0,0)    │ offset(200,0)  │ offset(400,0)  │
     * ├────────────────┼────────────────┼────────────────┤
     * │    Node 3      │    Node 4      │    Node 5      │
     * │   (200x200)    │   (200x200)    │   (200x200)    │
     * │ offset(0,200)  │ offset(200,200)│ offset(400,200)│
     * └────────────────┴────────────────┴────────────────┘ */

    const std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 200, .row = 200},  // Node 0
        ColumnAndRow{.column = 200, .row = 200},  // Node 1
        ColumnAndRow{.column = 200, .row = 200},  // Node 2
        ColumnAndRow{.column = 200, .row = 200},  // Node 3
        ColumnAndRow{.column = 200, .row = 200},  // Node 4
        ColumnAndRow{.column = 200, .row = 200}   // Node 5
    };

    // First row
    const auto offset0 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/0, /*nNodeX=*/3, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);

    const auto offset1 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/1, /*nNodeX=*/3, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset1.x(), 200);
    EXPECT_EQ(offset1.y(), 0);

    const auto offset2 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/2, /*nNodeX=*/3, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset2.x(), 400);
    EXPECT_EQ(offset2.y(), 0);

    // Second row
    const auto offset3 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/3, /*nNodeX=*/3, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset3.x(), 0);
    EXPECT_EQ(offset3.y(), 200);

    const auto offset4 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/4, /*nNodeX=*/3, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset4.x(), 200);
    EXPECT_EQ(offset4.y(), 200);

    const auto offset5 = ReaderHelpers::calculateXYOffsetForNode(/*node=*/5, /*nNodeX=*/3, /*nNodeY=*/2, columnsAndRows);
    EXPECT_EQ(offset5.x(), 400);
    EXPECT_EQ(offset5.y(), 200);
}
