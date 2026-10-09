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

/** Test Suite: GridWorldMappingBasePlane
 *
 * Regression for issue #135: with a substate shown as 3D height the surface has quads only where
 * the substate is inside (Min, Max], so the picker misses over the rest of the grid (the flat
 * "chessboard") and the tooltip said "(Outside the grid)". There the world position comes from
 * the view ray meeting the base plane under the surface. */

namespace
{
// Two points of the view ray through `ground`: one close to the camera (near clipping plane)
// and one behind the plane (far clipping plane), as vtkRenderer::DisplayToWorld gives them.
struct Ray
{
    Point3D start, end;
};
Ray rayThroughGroundPoint(const Point3D& camera, const Point3D& ground, double nearT = 0.02, double farT = 3.0)
{
    const auto at = [&](double t)
    {
        return Point3D{ camera.x + t * (ground.x - camera.x),
                        camera.y + t * (ground.y - camera.y),
                        camera.z + t * (ground.z - camera.z) };
    };
    return { at(nearT), at(farT) };
}
} // namespace

TEST(GridWorldMappingBasePlane, VerticalRayHitsThePointRightBelowTheCursor)
{
    double x = 0.0, y = 0.0;
    ASSERT_TRUE(intersectRayWithHorizontalPlane({ 12.5, 7.25, 100.0 }, { 12.5, 7.25, -100.0 }, heightSurfaceBaseZ, x, y));
    EXPECT_DOUBLE_EQ(x, 12.5);
    EXPECT_DOUBLE_EQ(y, 7.25);
}

TEST(GridWorldMappingBasePlane, ObliqueRayIsShiftedAlongItsDirection)
{
    double x = 0.0, y = 0.0;
    ASSERT_TRUE(intersectRayWithHorizontalPlane({ 0.0, 0.0, 10.0 }, { 20.0, 10.0, 0.0 }, 0.0, x, y));
    EXPECT_DOUBLE_EQ(x, 20.0);
    EXPECT_DOUBLE_EQ(y, 10.0);

    // the same line, the second point taken further along it
    ASSERT_TRUE(intersectRayWithHorizontalPlane({ 0.0, 0.0, 10.0 }, { 60.0, 30.0, -20.0 }, 0.0, x, y));
    EXPECT_DOUBLE_EQ(x, 20.0);
    EXPECT_DOUBLE_EQ(y, 10.0);
}

TEST(GridWorldMappingBasePlane, FindsTheGroundPointSeenThroughAPerspectiveCamera)
{
    // Camera in front of the grid, looking across it (like a tilted 3D view)
    const Point3D camera{ -40.0, -90.0, 260.0 };
    const Point3D ground{ 123.0, 45.0, heightSurfaceBaseZ };
    const Ray ray = rayThroughGroundPoint(camera, ground);

    double x = 0.0, y = 0.0;
    ASSERT_TRUE(intersectRayWithHorizontalPlane(ray.start, ray.end, heightSurfaceBaseZ, x, y));
    EXPECT_NEAR(x, ground.x, 1e-9);
    EXPECT_NEAR(y, ground.y, 1e-9);
}

TEST(GridWorldMappingBasePlane, RayParallelToThePlaneNeverHitsIt)
{
    double x = -1.0, y = -1.0;
    EXPECT_FALSE(intersectRayWithHorizontalPlane({ 0.0, 0.0, 5.0 }, { 10.0, 0.0, 5.0 }, 0.0, x, y));
    EXPECT_DOUBLE_EQ(x, -1.0); // untouched on failure
    EXPECT_DOUBLE_EQ(y, -1.0);
}

TEST(GridWorldMappingBasePlane, PlaneBehindTheRayStartIsNotHit)
{
    double x = 0.0, y = 0.0;
    // the ray climbs away from the plane
    EXPECT_FALSE(intersectRayWithHorizontalPlane({ 0.0, 0.0, 5.0 }, { 10.0, 0.0, 10.0 }, 0.0, x, y));
    // the ray starts below the plane and goes further down
    EXPECT_FALSE(intersectRayWithHorizontalPlane({ 0.0, 0.0, -5.0 }, { 10.0, 0.0, -10.0 }, 0.0, x, y));
}

TEST(GridWorldMappingBasePlane, DegenerateRayIsRejected)
{
    double x = 0.0, y = 0.0;
    EXPECT_FALSE(intersectRayWithHorizontalPlane({ 1.0, 2.0, 3.0 }, { 1.0, 2.0, 3.0 }, 0.0, x, y));
}

TEST(GridWorldMappingBasePlane, HitAboveAGapOfTheSurfaceMapsToTheCellUnderTheCursor)
{
    // SciddicaT: 496 rows x 610 columns. Cell (row 300, col 120) lies outside of the lava flow,
    // so the surface has no quad there and the picker misses it.
    constexpr int nRows = 496;
    constexpr int nCols = 610;
    const World cell = vertexOf(300, 120, nRows);
    const Point3D camera{ cell.x - 80.0, cell.y - 250.0, 400.0 };
    const Ray ray = rayThroughGroundPoint(camera, { cell.x + 0.2, cell.y - 0.3, heightSurfaceBaseZ });

    double x = 0.0, y = 0.0;
    ASSERT_TRUE(intersectRayWithHorizontalPlane(ray.start, ray.end, heightSurfaceBaseZ, x, y));

    const Bounds2D grid = pointGridBounds(nRows, nCols);
    EXPECT_TRUE(x >= grid.xMin && x <= grid.xMax && y >= grid.yMin && y <= grid.yMax);

    int row = -1, col = -1;
    ASSERT_TRUE(worldToNearestVertexCell(x, y, nRows, nCols, row, col));
    EXPECT_EQ(row, 300);
    EXPECT_EQ(col, 120);
}

TEST(GridWorldMappingBasePlane, HitBesideTheGridIsOutsideOfTheLogicalExtent)
{
    constexpr int nRows = 496;
    constexpr int nCols = 610;
    const Point3D camera{ 300.0, -300.0, 500.0 };
    // a point of the plane left of the grid
    const Ray ray = rayThroughGroundPoint(camera, { -60.0, 200.0, heightSurfaceBaseZ });

    double x = 0.0, y = 0.0;
    ASSERT_TRUE(intersectRayWithHorizontalPlane(ray.start, ray.end, heightSurfaceBaseZ, x, y));
    EXPECT_LT(x, pointGridBounds(nRows, nCols).xMin);
}
