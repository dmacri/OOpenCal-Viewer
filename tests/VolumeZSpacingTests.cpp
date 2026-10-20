#include <gtest/gtest.h>
#include "visualiser/VolumeZSpacing.h"

/** Test Suite: VolumeZSpacing
 *
 * The "Z spacing" option of the native 3D volume view inserts empty planes between the Z planes of
 * the model, so the lower planes (e.g. the subsurface) are not hidden by the upper ones.
 * These tests check the index arithmetic that decides where each data plane lands in the drawn volume. */

using namespace VolumeZSpacing;

TEST(VolumeZSpacing, ZeroGapKeepsPlanesGluedTogether)
{
    EXPECT_EQ(drawnPlaneCount(5, 0), 5u);
    for (std::size_t z = 0; z < 5; ++z)
        EXPECT_EQ(drawnPlaneIndex(z, 0), z);
}

TEST(VolumeZSpacing, GapInsertsEmptyPlanesBetweenNeighbouringPlanesOnly)
{
    // [z0] . . [z1] . . [z2]  ->  7 planes, data planes at 0, 3, 6
    EXPECT_EQ(drawnPlaneCount(3, 2), 7u);
    EXPECT_EQ(drawnPlaneIndex(0, 2), 0u);
    EXPECT_EQ(drawnPlaneIndex(1, 2), 3u);
    EXPECT_EQ(drawnPlaneIndex(2, 2), 6u);
}

TEST(VolumeZSpacing, LastDataPlaneIsTheLastDrawnPlane)
{
    for (int gap = 0; gap <= maxGapPlanes; ++gap)
    {
        for (std::size_t planes : {2u, 3u, 10u, 40u})
            EXPECT_EQ(drawnPlaneIndex(planes - 1, gap) + 1, drawnPlaneCount(planes, gap))
                << "planes=" << planes << " gap=" << gap;
    }
}

TEST(VolumeZSpacing, DataPlanesNeverCollide)
{
    constexpr std::size_t planes = 12;
    for (int gap = 0; gap <= maxGapPlanes; ++gap)
    {
        for (std::size_t z = 1; z < planes; ++z)
            EXPECT_EQ(drawnPlaneIndex(z, gap) - drawnPlaneIndex(z - 1, gap), static_cast<std::size_t>(gap) + 1);
    }
}

TEST(VolumeZSpacing, GapIsClampedToTheSupportedRange)
{
    EXPECT_EQ(clampGap(-3), 0);
    EXPECT_EQ(clampGap(0), 0);
    EXPECT_EQ(clampGap(7), 7);
    EXPECT_EQ(clampGap(maxGapPlanes + 100), maxGapPlanes);

    // Out-of-range requests behave like the nearest valid value
    EXPECT_EQ(drawnPlaneCount(4, -1), drawnPlaneCount(4, 0));
    EXPECT_EQ(drawnPlaneCount(4, 1000), drawnPlaneCount(4, maxGapPlanes));
}

TEST(VolumeZSpacing, EmptyModelHasNoPlanes)
{
    EXPECT_EQ(drawnPlaneCount(0, 0), 0u);
    EXPECT_EQ(drawnPlaneCount(0, 5), 0u);
}

TEST(VolumeZSpacing, SinglePlaneIsNotStretched)
{
    for (int gap = 0; gap <= maxGapPlanes; ++gap)
        EXPECT_EQ(drawnPlaneCount(1, gap), 1u);
}
