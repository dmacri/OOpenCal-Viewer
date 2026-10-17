/** @file VolumeZSpacing.h
 *  @brief Qt/VTK-free helpers for the "spacing between Z planes" option of the native 3D volume view.
 *
 *  In a native 3D model (number_of_slices > 1) the volume is drawn with all Z planes glued together,
 *  so the upper planes hide what happens in the lower ones (e.g. the subsurface below the terrain).
 *  The option inserts @p gap *empty* (fully transparent) planes between every two neighbouring data
 *  planes, which pulls the planes apart ("exploded view"):
 *
 *      gap = 0 :  [z0][z1][z2]                 (default, the planes touch each other)
 *      gap = 2 :  [z0] . . [z1] . . [z2]       (". " = empty plane)
 *
 *  Data plane @c z is then stored in the drawn volume at index @c z * (gap + 1).
 *
 *  Keeping this arithmetic here, free of Qt and VTK, makes it unit-testable. */
#pragma once

#include <algorithm>
#include <cstddef>

namespace VolumeZSpacing
{

/// Largest number of empty planes the user can insert between two data planes.
inline constexpr int maxGapPlanes = 20;

/// Clamps a requested gap to the supported range [0, maxGapPlanes].
constexpr int clampGap(int gapPlanes) noexcept
{
    return std::clamp(gapPlanes, 0, maxGapPlanes);
}

/** @brief Number of planes of the drawn volume.
 *  @param dataPlanes number of planes of the model (number_of_slices)
 *  @param gapPlanes  number of empty planes between two neighbouring data planes */
constexpr std::size_t drawnPlaneCount(std::size_t dataPlanes, int gapPlanes) noexcept
{
    if (dataPlanes == 0)
        return 0;
    return dataPlanes + (dataPlanes - 1) * static_cast<std::size_t>(clampGap(gapPlanes));
}

/// Index, in the drawn volume, of the data plane @p dataPlane.
constexpr std::size_t drawnPlaneIndex(std::size_t dataPlane, int gapPlanes) noexcept
{
    return dataPlane * (static_cast<std::size_t>(clampGap(gapPlanes)) + 1);
}
} // namespace VolumeZSpacing
