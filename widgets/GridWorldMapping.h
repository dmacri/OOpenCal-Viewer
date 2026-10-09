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

/// World Z of the flat plane (the "chessboard") under the 3D height surface. Heights are never negative,
/// so the surface lies on or above this plane (see Visualizer::drawFlatSceneBackground).
inline constexpr double heightSurfaceBaseZ = 0.0;

/// Point in world coordinates.
struct Point3D
{
    double x{};
    double y{};
    double z{};
};

/** @brief Intersects a view ray with the horizontal plane z = planeZ.
 *
 *  The 3D height surface has quads only where its substate is inside (Min, Max], so a picker
 *  restricted to it misses everywhere else, although the cursor can still be above the grid: above
 *  the flat base plane under the surface (issue #135). There the position has to come from the ray.
 *
 *  The ray is given by two points under the cursor: on the near and on the far clipping plane
 *  (vtkRenderer::DisplayToWorld with display Z = 0 and 1). The line through them is the view ray
 *  for perspective and for parallel projection alike.
 *
 *  @param rayStart point under the cursor on the near clipping plane
 *  @param rayEnd   another point of the ray, further from the camera
 *  @param planeZ   world Z of the plane
 *  @param outX     world X of the intersection (written only on success)
 *  @param outY     world Y of the intersection (written only on success)
 *  @return false if the ray is parallel to the plane or the plane lies behind rayStart, true otherwise */
inline bool intersectRayWithHorizontalPlane(const Point3D& rayStart,
                                            const Point3D& rayEnd,
                                            double planeZ,
                                            double& outX,
                                            double& outY) noexcept
{
    const double dx = rayEnd.x - rayStart.x;
    const double dy = rayEnd.y - rayStart.y;
    const double dz = rayEnd.z - rayStart.z;

    // A ray that is (almost) parallel to the plane never reaches it, or only very far away
    const double rayLength = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (! (rayLength > 0.0) || std::abs(dz) < 1e-9 * rayLength)
        return false;

    const double t = (planeZ - rayStart.z) / dz;
    if (t < 0.0) // the plane is behind the point the ray starts from
        return false;

    const double x = rayStart.x + t * dx;
    const double y = rayStart.y + t * dy;
    if (! std::isfinite(x) || ! std::isfinite(y))
        return false;

    outX = x;
    outY = y;
    return true;
}
} // namespace GridWorldMapping
