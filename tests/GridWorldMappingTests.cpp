#include <gtest/gtest.h>
#include "widgets/GridWorldMapping.h"

/** Test Suite: GridWorldMapping
 *
 * Verifies the world X/Y -> grid cell conversion used by the mouse tooltip and by the
 * "click on a cell" sidebar.
 *
 * Regression for issue #139: with a substate shown as 3D height (e.g. SciddicaT "z", Min=490)
 * the tooltip reported h = 0 above a visible lava flow, because the cell under the cursor was
 * computed against the bounding box of the *cropped* 3D surface instead of the whole grid. */

using namespace GridWorldMapping;

namespace
{
// Where the viewer draws the vertex of cell (row, col): see Visualizer::drawWithVTK.
struct World { double x, y; };
World vertexOf(int row, int col, int nRows)
{
    return {static_cast<double>(col), static_cast<double>(nRows - 1 - row)};
}
} // namespace

TEST(GridWorldMapping, PointGridBoundsAreTheLogicalExtentOfTheWholeGrid)
{
    const auto b = pointGridBounds(496, 610);
    EXPECT_DOUBLE_EQ(b.xMin, 0.0);
    EXPECT_DOUBLE_EQ(b.xMax, 609.0);
    EXPECT_DOUBLE_EQ(b.yMin, 0.0);
    EXPECT_DOUBLE_EQ(b.yMax, 495.0);
}

TEST(GridWorldMappingNearestVertex, EveryVertexMapsBackToItsOwnCell_SciddicaTSize)
{
    constexpr int nRows = 496;
    constexpr int nCols = 610;
    for (int row = 0; row < nRows; ++row)
    {
        for (int col = 0; col < nCols; ++col)
        {
            const auto w = vertexOf(row, col, nRows);
            int r = -1, c = -1;
            ASSERT_TRUE(worldToNearestVertexCell(w.x, w.y, nRows, nCols, r, c));
            ASSERT_EQ(r, row) << "col=" << col;
            ASSERT_EQ(c, col) << "row=" << row;
        }
    }
}

TEST(GridWorldMappingNearestVertex, SmallAndNonSquareGrids)
{
    for (auto [nRows, nCols] : {std::pair{2, 2}, {1, 5}, {5, 1}, {7, 3}, {3, 7}})
    {
        for (int row = 0; row < nRows; ++row)
        {
            for (int col = 0; col < nCols; ++col)
            {
                const auto w = vertexOf(row, col, nRows);
                int r = -1, c = -1;
                ASSERT_TRUE(worldToNearestVertexCell(w.x, w.y, nRows, nCols, r, c));
                EXPECT_EQ(r, row);
                EXPECT_EQ(c, col);
            }
        }
    }
}

TEST(GridWorldMappingNearestVertex, PositionNearAVertexStillMapsToThatCell)
{
    constexpr int nRows = 40;
    constexpr int nCols = 60;
    for (int row = 0; row < nRows; ++row)
    {
        for (int col = 0; col < nCols; ++col)
        {
            const auto w = vertexOf(row, col, nRows);
            for (double dx : {-0.49, 0.0, 0.49})
            {
                for (double dy : {-0.49, 0.0, 0.49})
                {
                    int r = -1, c = -1;
                    ASSERT_TRUE(worldToNearestVertexCell(w.x + dx, w.y + dy, nRows, nCols, r, c));
                    ASSERT_EQ(r, row);
                    ASSERT_EQ(c, col);
                }
            }
        }
    }
}

TEST(GridWorldMappingNearestVertex, ClampsOutsidePositionsAndRejectsDegenerateGrids)
{
    int r = -1, c = -1;
    ASSERT_TRUE(worldToNearestVertexCell(-100.0, 1e6, 10, 20, r, c));
    EXPECT_EQ(r, 0);   // far above the grid -> top row
    EXPECT_EQ(c, 0);

    ASSERT_TRUE(worldToNearestVertexCell(1e6, -100.0, 10, 20, r, c));
    EXPECT_EQ(r, 9);   // far below -> bottom row
    EXPECT_EQ(c, 19);

    EXPECT_FALSE(worldToNearestVertexCell(0.0, 0.0, 0, 20, r, c));
    EXPECT_FALSE(worldToNearestVertexCell(0.0, 0.0, 10, 0, r, c));
}

// The core of issue #139. A 3D surface only has quads where the height substate is inside
// (Min, Max]; here it covers a sub-rectangle of the grid, so its bounding box is much smaller
// than the grid. Converting the cursor position with *that* box lands far away from the cell
// the user points at, while the logical grid extent is exact.
TEST(GridWorldMappingRegression139, CroppedSurfaceBoundsMisplaceTheCursorButLogicalExtentDoesNot)
{
    constexpr int nRows = 496;
    constexpr int nCols = 610;

    // Surface covers rows 200..350 and columns 10..400 only (numbers similar to SciddicaT z>490).
    const int firstRow = 200, lastRow = 350, firstCol = 10, lastCol = 400;
    const Bounds2D croppedBounds{
        static_cast<double>(firstCol), static_cast<double>(lastCol),
        static_cast<double>(nRows - 1 - lastRow), static_cast<double>(nRows - 1 - firstRow)};

    const int row = 265, col = 246; // a cell inside the surface, "above the red flow"
    const auto w = vertexOf(row, col, nRows);

    int r = -1, c = -1;
    ASSERT_TRUE(worldToGridCell(w.x, w.y, croppedBounds, nRows, nCols, r, c));
    EXPECT_FALSE(r == row && c == col) << "cropped bounds were expected to misplace the cursor";
    EXPECT_GT(std::abs(c - col), 5);

    ASSERT_TRUE(worldToNearestVertexCell(w.x, w.y, nRows, nCols, r, c));
    EXPECT_EQ(r, row);
    EXPECT_EQ(c, col);
}

// Flat views keep their previous behaviour: bounds of the rendered grid -> cell.
TEST(GridWorldMappingFlatView, CellRenderingBoundsMapEveryCellCentreBack)
{
    constexpr int nRows = 30;
    constexpr int nCols = 50;
    // useCellRendering: points (col, nRows - row) for row 0..nRows, col 0..nCols
    const Bounds2D bounds{0.0, static_cast<double>(nCols), 0.0, static_cast<double>(nRows)};
    for (int row = 0; row < nRows; ++row)
    {
        for (int col = 0; col < nCols; ++col)
        {
            const double x = col + 0.5;
            const double y = (nRows - row) - 0.5;
            int r = -1, c = -1;
            ASSERT_TRUE(worldToGridCell(x, y, bounds, nRows, nCols, r, c));
            ASSERT_EQ(r, row);
            ASSERT_EQ(c, col);
        }
    }
}

TEST(GridWorldMappingFlatView, RejectsDegenerateInput)
{
    int r = 0, c = 0;
    EXPECT_FALSE(worldToGridCell(1.0, 1.0, Bounds2D{0, 0, 0, 10}, 10, 10, r, c));
    EXPECT_FALSE(worldToGridCell(1.0, 1.0, Bounds2D{0, 10, 0, 0}, 10, 10, r, c));
    EXPECT_FALSE(worldToGridCell(1.0, 1.0, Bounds2D{0, 10, 0, 10}, 0, 10, r, c));
}
