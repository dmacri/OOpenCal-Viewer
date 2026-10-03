/** @file GridWorldMapping.h
 *  @brief Qt/VTK-free helpers translating VTK world X/Y coordinates into grid cells.
 *
 *  The viewer places the vertex of the cell at (row, col) of an nRows x nCols grid at
 *  world position
 *
 *      x = col,   y = nRows - 1 - row
 *
 *  (see Visualizer::drawWithVTK and Visualizer::build3DSubstateSurfaceQuadMesh).
 *  Converting a world position back to a cell therefore has to use the *logical*
 *  extent of the grid, [0, nCols - 1] x [0, nRows - 1].
 *
 *  It must not use the bounding box of the rendered actor: the 3D substate surface
 *  contains quads only where the height substate is inside (Min, Max], so its bounding
 *  box can be much smaller than the grid (issue #139: the tooltip reported h = 0 above
 *  a visible lava flow because the lookup was computed against a cropped surface).
 *
 *  Keeping the arithmetic here, free of Qt and VTK, makes it unit-testable. */
#pragma once

#include <algorithm>
#include <cmath>

namespace GridWorldMapping
{

/// Axis-aligned rectangle in world X/Y coordinates.
struct Bounds2D
{
    double xMin{};
    double xMax{};
    double yMin{};
    double yMax{};
};

/** @brief Logical world extent of a point-based grid (one vertex per cell).
 *  @param nRows number of grid rows (world Y direction)
 *  @param nCols number of grid columns (world X direction) */
constexpr Bounds2D pointGridBounds(int nRows, int nCols) noexcept
{
    return Bounds2D{0.0, static_cast<double>(nCols - 1), 0.0, static_cast<double>(nRows - 1)};
}

/** @brief Converts a world X/Y position to the (row, column) of the grid cell.
 *
 *  Intended for flat (2D) views, where @p bounds is the bounding box of the rendered grid.
 *  Row 0 is the top of the grid, so the world Y axis (growing upwards) is inverted.
 *  The result is clamped to the valid cell range.
 *
 *  @param worldX  world X coordinate
 *  @param worldY  world Y coordinate
 *  @param bounds  world extent that the whole nRows x nCols grid occupies
 *  @param nRows   number of grid rows
 *  @param nCols   number of grid columns
 *  @param outRow  output row index
 *  @param outCol  output column index
 *  @return false if the grid size or @p bounds are degenerate, true otherwise */
constexpr bool worldToGridCell(double worldX, double worldY,
                               const Bounds2D& bounds,
                               int nRows, int nCols,
                               int& outRow, int& outCol) noexcept
{
    const double width = bounds.xMax - bounds.xMin;
    const double height = bounds.yMax - bounds.yMin;
    if (nRows <= 0 || nCols <= 0 || width <= 0.0 || height <= 0.0)
        return false;

    const double cellWidth = width / nCols;
    const double cellHeight = height / nRows;

    const int col = static_cast<int>((worldX - bounds.xMin) / cellWidth);
    const int rowFromBottom = static_cast<int>((worldY - bounds.yMin) / cellHeight);

    // World Y grows upwards, grid rows grow downwards.
    outCol = std::clamp(col, 0, nCols - 1);
    outRow = std::clamp(nRows - 1 - rowFromBottom, 0, nRows - 1);
    return true;
}

/** @brief Converts a world X/Y position to the cell whose vertex is nearest to it.
 *
 *  This is the exact inverse of the vertex placement described in the file comment
 *  (x = col, y = nRows - 1 - row) and is what the 3D substate surface needs: the surface is a
 *  mesh over those vertices, so hovering anywhere over a quad should report the closest
 *  corner, and hovering exactly over a vertex must report exactly that cell.
 *  The result is clamped to the valid cell range.
 *
 *  @return false if the grid size is degenerate, true otherwise */
inline bool worldToNearestVertexCell(double worldX, double worldY,
                                     int nRows, int nCols,
                                     int& outRow, int& outCol) noexcept
{
    if (nRows <= 0 || nCols <= 0)
        return false;

    const int col = static_cast<int>(std::floor(worldX + 0.5));
    const int rowFromBottom = static_cast<int>(std::floor(worldY + 0.5));

    outCol = std::clamp(col, 0, nCols - 1);
    outRow = std::clamp(nRows - 1 - rowFromBottom, 0, nRows - 1);
    return true;
}
} // namespace GridWorldMapping
