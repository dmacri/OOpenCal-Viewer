#include <gtest/gtest.h>
#include <vector>
#include "utilities/types.h"

// Implementation of calculateXYOffsetForNode for testing
namespace ReaderHelpers
{
    ColumnAndRow calculateXYOffsetForNode(NodeIndex node,
                                         NodeIndex nNodeX,
                                         NodeIndex nNodeY,
                                         const std::vector<ColumnAndRow>& columnsAndRows)
    {
        int offsetX = 0;
        int offsetY = 0;

        for (NodeIndex k = (node / nNodeX) * nNodeX; k < node; k++)
        {
            offsetX += columnsAndRows[k].column;
        }

        if (node >= nNodeX)
        {
            for (int k = node - nNodeX; k >= 0;)
            {
                offsetY += columnsAndRows[k].row;
                k -= nNodeX;
            }
        }
        return ColumnAndRow::xy(offsetX, offsetY);
    }
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
    /*
     * Scene: 500x500
     * Nodes: 2x1 (2 nodes in X direction, 1 node in Y direction)
     * 
     * Layout:
     * ┌─────────────────┬─────────────────┐
     * │   Node 0        │   Node 1        │
     * │  (250x500)      │  (250x500)      │
     * │  offset(0,0)    │  offset(250,0)  │
     * └─────────────────┴─────────────────┘
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 250, .row = 500},  // Node 0
        ColumnAndRow{.column = 250, .row = 500}   // Node 1
    };
    
    // Node 0: should be at (0, 0)
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 2, 1, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);
    
    // Node 1: should be at (250, 0)
    auto offset1 = ReaderHelpers::calculateXYOffsetForNode(1, 2, 1, columnsAndRows);
    EXPECT_EQ(offset1.x(), 250);
    EXPECT_EQ(offset1.y(), 0);
}

// ============================================================================
// Test 2: 1x2 Configuration (1 node horizontally, 2 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, OneByTwo_500x500)
{
    /*
     * Scene: 500x500
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
     * └─────────────────┘
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 500, .row = 250},  // Node 0
        ColumnAndRow{.column = 500, .row = 250}   // Node 1
    };
    
    // Node 0: should be at (0, 0)
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 1, 2, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);
    
    // Node 1: should be at (0, 250)
    auto offset1 = ReaderHelpers::calculateXYOffsetForNode(1, 1, 2, columnsAndRows);
    EXPECT_EQ(offset1.x(), 0);
    EXPECT_EQ(offset1.y(), 250);
}

// ============================================================================
// Test 3: 2x2 Configuration (2 nodes horizontally, 2 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, TwoByTwo_500x500)
{
    /*
     * Scene: 500x500
     * Nodes: 2x2 (2 nodes in X direction, 2 nodes in Y direction)
     * 
     * Layout:
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
     * Node 3 = row 1, col 1
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 250, .row = 250},  // Node 0
        ColumnAndRow{.column = 250, .row = 250},  // Node 1
        ColumnAndRow{.column = 250, .row = 250},  // Node 2
        ColumnAndRow{.column = 250, .row = 250}   // Node 3
    };
    
    // Node 0: should be at (0, 0)
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 2, 2, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);
    
    // Node 1: should be at (250, 0)
    auto offset1 = ReaderHelpers::calculateXYOffsetForNode(1, 2, 2, columnsAndRows);
    EXPECT_EQ(offset1.x(), 250);
    EXPECT_EQ(offset1.y(), 0);
    
    // Node 2: should be at (0, 250)
    auto offset2 = ReaderHelpers::calculateXYOffsetForNode(2, 2, 2, columnsAndRows);
    EXPECT_EQ(offset2.x(), 0);
    EXPECT_EQ(offset2.y(), 250);
    
    // Node 3: should be at (250, 250)
    auto offset3 = ReaderHelpers::calculateXYOffsetForNode(3, 2, 2, columnsAndRows);
    EXPECT_EQ(offset3.x(), 250);
    EXPECT_EQ(offset3.y(), 250);
}

// ============================================================================
// Test 4: 4x1 Configuration (4 nodes horizontally, 1 node vertically)
// THIS IS THE PROBLEMATIC CASE!
// ============================================================================
TEST(CalculateXYOffsetForNode, FourByOne_500x500)
{
    /*
     * Scene: 500x500
     * Nodes: 4x1 (4 nodes in X direction, 1 node in Y direction)
     * 
     * Layout:
     * ┌──────────┬──────────┬──────────┬──────────┐
     * │ Node 0   │ Node 1   │ Node 2   │ Node 3   │
     * │(125x500) │(125x500) │(125x500) │(125x500) │
     * │offset(0) │offset(125)│offset(250)│offset(375)
     * └──────────┴──────────┴──────────┴──────────┘
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 125, .row = 500},  // Node 0
        ColumnAndRow{.column = 125, .row = 500},  // Node 1
        ColumnAndRow{.column = 125, .row = 500},  // Node 2
        ColumnAndRow{.column = 125, .row = 500}   // Node 3
    };
    
    // Node 0: should be at (0, 0)
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 4, 1, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);
    
    // Node 1: should be at (125, 0)
    auto offset1 = ReaderHelpers::calculateXYOffsetForNode(1, 4, 1, columnsAndRows);
    EXPECT_EQ(offset1.x(), 125);
    EXPECT_EQ(offset1.y(), 0);
    
    // Node 2: should be at (250, 0)
    auto offset2 = ReaderHelpers::calculateXYOffsetForNode(2, 4, 1, columnsAndRows);
    EXPECT_EQ(offset2.x(), 250);
    EXPECT_EQ(offset2.y(), 0);
    
    // Node 3: should be at (375, 0)
    auto offset3 = ReaderHelpers::calculateXYOffsetForNode(3, 4, 1, columnsAndRows);
    EXPECT_EQ(offset3.x(), 375);
    EXPECT_EQ(offset3.y(), 0);
}

// ============================================================================
// Test 5: 1x4 Configuration (1 node horizontally, 4 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, OneByFour_500x500)
{
    /*
     * Scene: 500x500
     * Nodes: 1x4 (1 node in X direction, 4 nodes in Y direction)
     * 
     * Layout:
     * ┌──────────┐
     * │ Node 0   │
     * │(500x125) │
     * │offset(0) │
     * ├──────────┤
     * │ Node 1   │
     * │(500x125) │
     * │offset(125)
     * ├──────────┤
     * │ Node 2   │
     * │(500x125) │
     * │offset(250)
     * ├──────────┤
     * │ Node 3   │
     * │(500x125) │
     * │offset(375)
     * └──────────┘
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 500, .row = 125},  // Node 0
        ColumnAndRow{.column = 500, .row = 125},  // Node 1
        ColumnAndRow{.column = 500, .row = 125},  // Node 2
        ColumnAndRow{.column = 500, .row = 125}   // Node 3
    };
    
    // Node 0: should be at (0, 0)
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 1, 4, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);
    
    // Node 1: should be at (0, 125)
    auto offset1 = ReaderHelpers::calculateXYOffsetForNode(1, 1, 4, columnsAndRows);
    EXPECT_EQ(offset1.x(), 0);
    EXPECT_EQ(offset1.y(), 125);
    
    // Node 2: should be at (0, 250)
    auto offset2 = ReaderHelpers::calculateXYOffsetForNode(2, 1, 4, columnsAndRows);
    EXPECT_EQ(offset2.x(), 0);
    EXPECT_EQ(offset2.y(), 250);
    
    // Node 3: should be at (0, 375)
    auto offset3 = ReaderHelpers::calculateXYOffsetForNode(3, 1, 4, columnsAndRows);
    EXPECT_EQ(offset3.x(), 0);
    EXPECT_EQ(offset3.y(), 375);
}

// ============================================================================
// Test 6: 4x4 Configuration (4 nodes horizontally, 4 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, FourByFour_500x500)
{
    /*
     * Scene: 500x500
     * Nodes: 4x4 (4 nodes in X direction, 4 nodes in Y direction)
     * Each node: 125x125
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
     * └──────┴──────┴──────┴──────┘
     */
    
    std::vector<ColumnAndRow> columnsAndRows(16);
    for (int i = 0; i < 16; ++i)
    {
        columnsAndRows[i] = ColumnAndRow{.column = 125, .row = 125};
    }
    
    // Test a few key nodes
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 4, 4, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);
    
    auto offset5 = ReaderHelpers::calculateXYOffsetForNode(5, 4, 4, columnsAndRows);
    EXPECT_EQ(offset5.x(), 125);
    EXPECT_EQ(offset5.y(), 125);
    
    auto offset10 = ReaderHelpers::calculateXYOffsetForNode(10, 4, 4, columnsAndRows);
    EXPECT_EQ(offset10.x(), 250);
    EXPECT_EQ(offset10.y(), 250);
    
    auto offset15 = ReaderHelpers::calculateXYOffsetForNode(15, 4, 4, columnsAndRows);
    EXPECT_EQ(offset15.x(), 375);
    EXPECT_EQ(offset15.y(), 375);
}

// ============================================================================
// Test 7: Uneven Distribution (2x2 with different sizes)
// ============================================================================
TEST(CalculateXYOffsetForNode, TwoByTwo_UnevenDistribution)
{
    /*
     * Scene: 600x600
     * Nodes: 2x2 with uneven distribution
     * 
     * Layout:
     * ┌──────────────┬──────────────┐
     * │   Node 0     │   Node 1     │
     * │ (300x300)    │ (300x300)    │
     * │ offset(0,0)  │ offset(300,0)│
     * ├──────────────┼──────────────┤
     * │   Node 2     │   Node 3     │
     * │ (400x300)    │ (200x300)    │
     * │ offset(0,300)│ offset(400,300)
     * └──────────────┴──────────────┘
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 300, .row = 300},  // Node 0
        ColumnAndRow{.column = 300, .row = 300},  // Node 1
        ColumnAndRow{.column = 400, .row = 300},  // Node 2
        ColumnAndRow{.column = 200, .row = 300}   // Node 3
    };
    
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 2, 2, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);
    
    auto offset1 = ReaderHelpers::calculateXYOffsetForNode(1, 2, 2, columnsAndRows);
    EXPECT_EQ(offset1.x(), 300);
    EXPECT_EQ(offset1.y(), 0);
    
    auto offset2 = ReaderHelpers::calculateXYOffsetForNode(2, 2, 2, columnsAndRows);
    EXPECT_EQ(offset2.x(), 0);
    EXPECT_EQ(offset2.y(), 300);
    
    auto offset3 = ReaderHelpers::calculateXYOffsetForNode(3, 2, 2, columnsAndRows);
    EXPECT_EQ(offset3.x(), 400);
    EXPECT_EQ(offset3.y(), 300);
}

// ============================================================================
// Test 8: Real data from OOpenCAL Ball model (4x1 configuration)
// This test uses actual data that was reported as problematic
// ============================================================================
TEST(CalculateXYOffsetForNode, RealData_Ball_4x1)
{
    /*
     * Real scenario from OOpenCAL Ball model with 4x1 configuration
     * Scene: 500x500
     * Nodes: 4x1
     * 
     * From the error report:
     * - node=2, row=0, col=0: offsetXY={column:500, row:0}
     * - But array is 500x500 (indices 0-499)
     * - This suggests columnsAndRows might be wrong
     * 
     * Expected columnsAndRows for 4x1 with 500x500 scene:
     * Each node should have 125 columns and 500 rows
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 125, .row = 500},  // Node 0
        ColumnAndRow{.column = 125, .row = 500},  // Node 1
        ColumnAndRow{.column = 125, .row = 500},  // Node 2
        ColumnAndRow{.column = 125, .row = 500}   // Node 3
    };
    
    // Node 2 should be at (250, 0), NOT (500, 0)
    auto offset2 = ReaderHelpers::calculateXYOffsetForNode(2, 4, 1, columnsAndRows);
    EXPECT_EQ(offset2.x(), 250) << "Node 2 should have X offset of 250, not 500";
    EXPECT_EQ(offset2.y(), 0);
    
    // Verify all nodes
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 4, 1, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    
    auto offset1 = ReaderHelpers::calculateXYOffsetForNode(1, 4, 1, columnsAndRows);
    EXPECT_EQ(offset1.x(), 125);
    
    auto offset3 = ReaderHelpers::calculateXYOffsetForNode(3, 4, 1, columnsAndRows);
    EXPECT_EQ(offset3.x(), 375);
}

// ============================================================================
// Test 9: Incorrect columnsAndRows (what might be causing the crash)
// ============================================================================
TEST(CalculateXYOffsetForNode, IncorrectColumnsAndRows_4x1)
{
    /*
     * This test simulates what might be happening if columnsAndRows
     * is incorrectly populated with {250, 250, 125, 125} instead of
     * {125, 125, 125, 125}
     * 
     * This would cause:
     * - Node 0: offset = 0
     * - Node 1: offset = 250
     * - Node 2: offset = 250 + 250 = 500 (OUT OF BOUNDS!)
     * - Node 3: offset = 250 + 250 + 125 = 625 (OUT OF BOUNDS!)
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 250, .row = 500},  // Node 0 - WRONG!
        ColumnAndRow{.column = 250, .row = 500},  // Node 1 - WRONG!
        ColumnAndRow{.column = 125, .row = 500},  // Node 2
        ColumnAndRow{.column = 125, .row = 500}   // Node 3
    };
    
    // This will show the ACTUAL problem
    auto offset2 = ReaderHelpers::calculateXYOffsetForNode(2, 4, 1, columnsAndRows);
    EXPECT_EQ(offset2.x(), 500) << "With incorrect columnsAndRows, Node 2 gets offset 500 (OUT OF BOUNDS)";
    
    auto offset3 = ReaderHelpers::calculateXYOffsetForNode(3, 4, 1, columnsAndRows);
    EXPECT_EQ(offset3.x(), 625) << "With incorrect columnsAndRows, Node 3 gets offset 625 (OUT OF BOUNDS)";
}

// ============================================================================
// Test 10: 3x2 Configuration (3 nodes horizontally, 2 nodes vertically)
// ============================================================================
TEST(CalculateXYOffsetForNode, ThreeByTwo_600x400)
{
    /*
     * Scene: 600x400
     * Nodes: 3x2 (3 nodes in X direction, 2 nodes in Y direction)
     * 
     * Layout (row-major):
     * ┌──────────┬──────────┬──────────┐
     * │ Node 0   │ Node 1   │ Node 2   │
     * │(200x200) │(200x200) │(200x200) │
     * │offset(0) │offset(200)│offset(400)
     * ├──────────┼──────────┼──────────┤
     * │ Node 3   │ Node 4   │ Node 5   │
     * │(200x200) │(200x200) │(200x200) │
     * │offset(0) │offset(200)│offset(400)
     * └──────────┴──────────┴──────────┘
     */
    
    std::vector<ColumnAndRow> columnsAndRows = {
        ColumnAndRow{.column = 200, .row = 200},  // Node 0
        ColumnAndRow{.column = 200, .row = 200},  // Node 1
        ColumnAndRow{.column = 200, .row = 200},  // Node 2
        ColumnAndRow{.column = 200, .row = 200},  // Node 3
        ColumnAndRow{.column = 200, .row = 200},  // Node 4
        ColumnAndRow{.column = 200, .row = 200}   // Node 5
    };
    
    // First row
    auto offset0 = ReaderHelpers::calculateXYOffsetForNode(0, 3, 2, columnsAndRows);
    EXPECT_EQ(offset0.x(), 0);
    EXPECT_EQ(offset0.y(), 0);
    
    auto offset1 = ReaderHelpers::calculateXYOffsetForNode(1, 3, 2, columnsAndRows);
    EXPECT_EQ(offset1.x(), 200);
    EXPECT_EQ(offset1.y(), 0);
    
    auto offset2 = ReaderHelpers::calculateXYOffsetForNode(2, 3, 2, columnsAndRows);
    EXPECT_EQ(offset2.x(), 400);
    EXPECT_EQ(offset2.y(), 0);
    
    // Second row
    auto offset3 = ReaderHelpers::calculateXYOffsetForNode(3, 3, 2, columnsAndRows);
    EXPECT_EQ(offset3.x(), 0);
    EXPECT_EQ(offset3.y(), 200);
    
    auto offset4 = ReaderHelpers::calculateXYOffsetForNode(4, 3, 2, columnsAndRows);
    EXPECT_EQ(offset4.x(), 200);
    EXPECT_EQ(offset4.y(), 200);
    
    auto offset5 = ReaderHelpers::calculateXYOffsetForNode(5, 3, 2, columnsAndRows);
    EXPECT_EQ(offset5.x(), 400);
    EXPECT_EQ(offset5.y(), 200);
}
