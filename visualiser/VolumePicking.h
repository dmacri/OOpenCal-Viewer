/** @file VolumePicking.h
 *  @brief Qt/VTK-free helpers choosing the cell of a native 3D volume under the mouse cursor.
 *
 *  A native 3D model (number_of_slices > 1) is drawn as a volume (Visualizer::drawWithVTK3DVolume):
 *  a vtkImageData with one voxel per cell and spacing 1, so the voxel with the index (ix, iy, iz) is the
 *  cube [ix - 0.5, ix + 0.5] x [iy - 0.5, iy + 0.5] x [iz - 0.5, iz + 0.5] in world coordinates. The
 *  voxel of the cell at (row, col, plane) is
 *
 *      ix = col,   iy = nRows - 1 - row,   iz = plane * (gap + 1)
 *
 *  where @c gap is the number of empty planes inserted by the "Z spacing" option (see VolumeZSpacing.h).
 *
 *  The volume is not a polygonal actor, so a vtkPropPicker finds nothing on it. Instead the view ray under
 *  the cursor is walked voxel by voxel and the first *visible* voxel (one with a non-transparent value)
 *  is the cell the user pointed at.
 *
 *  Keeping the arithmetic here, free of Qt and VTK, makes it unit-testable. */
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

#include "visualiser/VolumeZSpacing.h"
#include "visualiserProxy/ContiguousGrid.h" // GridCellIndex
#include "widgets/GridWorldMapping.h"       // Point3D

namespace VolumePicking
{

/// Index of a voxel in the drawn volume (x = column, y = inverted row, z = drawn plane).
struct VoxelIndex
{
    int x = 0;
    int y = 0;
    int z = 0;

    friend constexpr bool operator==(const VoxelIndex&, const VoxelIndex&) = default;
};

/** @brief Walks the view ray through a volume of voxels and returns the first visible one.
 *
 *  The ray starts at @p rayStart (the point under the cursor on the near clipping plane) and goes through
 *  @p rayEnd (on the far plane); it is followed from its start onwards, so voxels behind the camera are
 *  never returned. Every voxel the ray passes is tested in the order of passing (Amanatides & Woo), which
 *  works for perspective and parallel projection alike.
 *
 *  @param rayStart  point of the ray nearest to the camera
 *  @param rayEnd    another point of the ray, further from the camera
 *  @param dims      number of voxels along X, Y and Z
 *  @param isVisible callable `bool(int x, int y, int z)`: is the voxel opaque enough to be pointed at
 *  @return the first visible voxel along the ray, nothing if the ray misses the volume or all voxels it
 *          crosses are transparent */
template<class IsVisible>
std::optional<VoxelIndex> firstVisibleVoxelOnRay(const GridWorldMapping::Point3D& rayStart,
                                                 const GridWorldMapping::Point3D& rayEnd,
                                                 const std::array<int, 3>& dims,
                                                 IsVisible&& isVisible)
{
    if (dims[0] <= 0 || dims[1] <= 0 || dims[2] <= 0)
        return std::nullopt;

    const std::array<double, 3> start{rayStart.x, rayStart.y, rayStart.z};
    const std::array<double, 3> direction{rayEnd.x - rayStart.x, rayEnd.y - rayStart.y, rayEnd.z - rayStart.z};

    const double length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    if (! (length > 0.0) || ! std::isfinite(length))
        return std::nullopt;

    // Slab test: the part of the ray [tEnter, tLeave] inside the box of the whole volume
    double tEnter = 0.0; // never look behind the start of the ray
    double tLeave = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis)
    {
        const double low = -0.5;
        const double high = dims[axis] - 0.5;
        if (std::abs(direction[axis]) < 1e-12 * length)
        {
            if (start[axis] < low || start[axis] > high)
                return std::nullopt; // parallel to the slab and outside of it
            continue;
        }
        double t1 = (low - start[axis]) / direction[axis];
        double t2 = (high - start[axis]) / direction[axis];
        if (t1 > t2)
            std::swap(t1, t2);
        tEnter = std::max(tEnter, t1);
        tLeave = std::min(tLeave, t2);
    }
    if (tEnter > tLeave)
        return std::nullopt;

    // The voxel the ray enters the volume in
    std::array<int, 3> index{};
    std::array<int, 3> step{};
    std::array<double, 3> tNext{};
    std::array<double, 3> tDelta{};
    for (int axis = 0; axis < 3; ++axis)
    {
        const double entry = start[axis] + tEnter * direction[axis];
        index[axis] = std::clamp(static_cast<int>(std::floor(entry + 0.5)), 0, dims[axis] - 1);

        if (std::abs(direction[axis]) < 1e-12 * length)
        {
            step[axis] = 0;
            tNext[axis] = std::numeric_limits<double>::infinity();
            tDelta[axis] = std::numeric_limits<double>::infinity();
            continue;
        }

        step[axis] = direction[axis] > 0.0 ? 1 : -1;
        const double boundary = index[axis] + 0.5 * step[axis];
        tNext[axis] = (boundary - start[axis]) / direction[axis];
        tDelta[axis] = std::abs(1.0 / direction[axis]);
    }

    // The ray crosses at most dx + dy + dz voxels
    const int maxSteps = dims[0] + dims[1] + dims[2] + 3;
    for (int i = 0; i < maxSteps; ++i)
    {
        if (isVisible(index[0], index[1], index[2]))
            return VoxelIndex{index[0], index[1], index[2]};

        // Move to the neighbouring voxel behind the nearest boundary
        int axis = 0;
        if (tNext[1] < tNext[axis])
            axis = 1;
        if (tNext[2] < tNext[axis])
            axis = 2;

        if (tNext[axis] > tLeave)
            break; // left the volume

        index[axis] += step[axis];
        if (index[axis] < 0 || index[axis] >= dims[axis])
            break;
        tNext[axis] += tDelta[axis];
    }
    return std::nullopt;
}

/** @brief Converts a voxel of the drawn volume to the cell of the model.
 *  @param voxel      voxel of the drawn volume
 *  @param nRows      number of rows of the model
 *  @param gapPlanes  empty planes between neighbouring Z planes (see VolumeZSpacing.h)
 *  @return the cell, nothing for a voxel of an empty plane inserted by the Z spacing */
inline std::optional<GridCellIndex> voxelToCell(const VoxelIndex& voxel, int nRows, int gapPlanes)
{
    if (voxel.x < 0 || voxel.y < 0 || voxel.z < 0 || voxel.y >= nRows)
        return std::nullopt;

    const int planeStride = VolumeZSpacing::clampGap(gapPlanes) + 1;
    if (voxel.z % planeStride != 0)
        return std::nullopt;

    return GridCellIndex{static_cast<std::size_t>(nRows - 1 - voxel.y),
                         static_cast<std::size_t>(voxel.x),
                         static_cast<std::size_t>(voxel.z / planeStride)};
}

} // namespace VolumePicking
