#include <gtest/gtest.h>
#include <vector>
#include "utilities/types.h"
#include "utilities/ModelReader.hpp"

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
