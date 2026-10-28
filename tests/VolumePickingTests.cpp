#include <gtest/gtest.h>

#include <random>
#include <set>

#include "visualiser/VolumePicking.h"

/** Test Suite: VolumePicking
 *
 * Clicking a native 3D model (volume view) has to find the cell under the cursor although the volume
 * is not a polygonal actor. These tests check the walk of the view ray through the voxels and the
 * conversion of a voxel to the cell of the model (inverted Y, planes pulled apart by the Z spacing). */

using GridWorldMapping::Point3D;
using namespace VolumePicking;

namespace
{
constexpr std::array<int, 3> dims{10, 8, 6}; // x (columns), y (rows), z (planes)

auto only(std::set<std::array<int, 3>> visible)
{
    return [visible = std::move(visible)](int x, int y, int z) { return visible.contains({x, y, z}); };
}
}

TEST(VolumePicking, RayFromAboveFindsTheTopmostVisibleVoxelOfTheColumn)
{
    const auto visible = only({{3, 2, 1}, {3, 2, 4}, {3, 2, 2}});
    const auto hit = firstVisibleVoxelOnRay({3.0, 2.0, 20.0}, {3.0, 2.0, -20.0}, dims, visible);
    ASSERT_TRUE(hit);
    EXPECT_EQ(*hit, (VoxelIndex{3, 2, 4}));
}

TEST(VolumePicking, RayFromBelowFindsTheLowestVisibleVoxelOfTheColumn)
{
    const auto visible = only({{3, 2, 1}, {3, 2, 4}});
    const auto hit = firstVisibleVoxelOnRay({3.0, 2.0, -20.0}, {3.0, 2.0, 20.0}, dims, visible);
    ASSERT_TRUE(hit);
    EXPECT_EQ(*hit, (VoxelIndex{3, 2, 1}));
}

TEST(VolumePicking, RayMissingTheVolumeFindsNothing)
{
    const auto everything = [](int, int, int) { return true; };
    EXPECT_FALSE(firstVisibleVoxelOnRay({30.0, 2.0, 20.0}, {30.0, 2.0, -20.0}, dims, everything));
    // goes past the volume diagonally
    EXPECT_FALSE(firstVisibleVoxelOnRay({-5.0, -5.0, 0.0}, {-5.0, 30.0, 0.0}, dims, everything));
}

TEST(VolumePicking, RayThroughOnlyTransparentVoxelsFindsNothing)
{
    const auto nothing = [](int, int, int) { return false; };
    EXPECT_FALSE(firstVisibleVoxelOnRay({3.0, 2.0, 20.0}, {3.0, 2.0, -20.0}, dims, nothing));
}

TEST(VolumePicking, VoxelBehindTheStartOfTheRayIsNotReturned)
{
    // The ray starts inside the volume at z = 3 and goes up: the voxel at z = 1 lies behind it
    const auto visible = only({{3, 2, 1}});
    EXPECT_FALSE(firstVisibleVoxelOnRay({3.0, 2.0, 3.0}, {3.0, 2.0, 30.0}, dims, visible));
}

TEST(VolumePicking, RayStartingInsideTheVolumeStartsInItsVoxel)
{
    const auto everything = [](int, int, int) { return true; };
    const auto hit = firstVisibleVoxelOnRay({4.2, 3.1, 2.9}, {4.2, 3.1, -10.0}, dims, everything);
    ASSERT_TRUE(hit);
    EXPECT_EQ(*hit, (VoxelIndex{4, 3, 3}));
}

TEST(VolumePicking, ObliqueRayWalksVoxelsInOrder)
{
    // 45 degrees in the XZ plane: passes (0,0,5), (1,0,4)... only when the visible voxel lies on that line
    const auto visible = only({{4, 0, 1}, {5, 0, 0}});
    const auto hit = firstVisibleVoxelOnRay({-1.0, 0.0, 6.0}, {9.0, 0.0, -4.0}, dims, visible);
    ASSERT_TRUE(hit);
    EXPECT_EQ(*hit, (VoxelIndex{4, 0, 1})); // x + z = 5 along the ray; (4,0,1) before (5,0,0)
}

TEST(VolumePicking, AgreesWithDenseSamplingOfTheRay)
{
    // A deterministic pseudo random volume and random rays: the first visible voxel found by the walk
    // has to be the first one met by a very fine sampling of the same ray.
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::set<std::array<int, 3>> visible;
    for (int x = 0; x < dims[0]; ++x)
        for (int y = 0; y < dims[1]; ++y)
            for (int z = 0; z < dims[2]; ++z)
                if (unit(rng) < 0.05)
                    visible.insert({x, y, z});
    const auto isVisible = only(visible);

    int compared = 0;
    for (int i = 0; i < 2000; ++i)
    {
        const Point3D start{unit(rng) * 30 - 10, unit(rng) * 30 - 10, unit(rng) * 30 - 10};
        // aimed at a random point of the volume, so that most of the rays cross it
        const Point3D end{unit(rng) * dims[0], unit(rng) * dims[1], unit(rng) * dims[2]};

        std::optional<VoxelIndex> expected;
        constexpr int samples = 20000;
        for (int s = 0; s <= samples && ! expected; ++s)
        {
            const double t = static_cast<double>(s) / samples * 4.0; // ray is followed beyond rayEnd too
            const double px = start.x + t * (end.x - start.x);
            const double py = start.y + t * (end.y - start.y);
            const double pz = start.z + t * (end.z - start.z);
            const int ix = static_cast<int>(std::floor(px + 0.5));
            const int iy = static_cast<int>(std::floor(py + 0.5));
            const int iz = static_cast<int>(std::floor(pz + 0.5));
            if (ix >= 0 && ix < dims[0] && iy >= 0 && iy < dims[1] && iz >= 0 && iz < dims[2] && isVisible(ix, iy, iz))
                expected = VoxelIndex{ix, iy, iz};
        }

        const auto actual = firstVisibleVoxelOnRay(start, end, dims, isVisible);
        if (expected)
        {
            // The sampled ray ends at t = 4 and may stop short of a far voxel; only compare when it found one
            ASSERT_TRUE(actual) << "ray " << i;
            EXPECT_EQ(*actual, *expected) << "ray " << i;
            ++compared;
        }
    }
    EXPECT_GT(compared, 500);
}

TEST(VolumePicking, VoxelOfTheCellHasInvertedYAndPlaneStride)
{
    // 8 rows: row 0 is drawn at the top, i.e. at the highest y
    const auto cell = voxelToCell({3, 7, 0}, 8, 0);
    ASSERT_TRUE(cell);
    EXPECT_EQ(*cell, (GridCellIndex{0, 3, 0}));

    const auto bottom = voxelToCell({0, 0, 5}, 8, 0);
    ASSERT_TRUE(bottom);
    EXPECT_EQ(*bottom, (GridCellIndex{7, 0, 5}));
}

TEST(VolumePicking, ZSpacingMapsDrawnPlanesBackToDataPlanes)
{
    // [z0] . . [z1] . . [z2]
    EXPECT_EQ(voxelToCell({1, 1, 0}, 4, 2)->layer, 0u);
    EXPECT_EQ(voxelToCell({1, 1, 3}, 4, 2)->layer, 1u);
    EXPECT_EQ(voxelToCell({1, 1, 6}, 4, 2)->layer, 2u);
    // empty planes are not cells
    EXPECT_FALSE(voxelToCell({1, 1, 1}, 4, 2));
    EXPECT_FALSE(voxelToCell({1, 1, 5}, 4, 2));
}

TEST(VolumePicking, VoxelToCellInvertsTheDrawingIndex)
{
    // Mirrors Visualizer::drawWithVTK3DVolume: ((plane * stride) * nRows + (nRows - 1 - row)) * nCols + col
    constexpr int nRows = 5;
    constexpr int nCols = 7;
    for (int gap : {0, 1, 3})
    {
        for (std::size_t plane = 0; plane < 4; ++plane)
        {
            for (int row = 0; row < nRows; ++row)
            {
                for (int col = 0; col < nCols; ++col)
                {
                    const VoxelIndex voxel{col, nRows - 1 - row,
                                           static_cast<int>(VolumeZSpacing::drawnPlaneIndex(plane, gap))};
                    const auto cell = voxelToCell(voxel, nRows, gap);
                    ASSERT_TRUE(cell);
                    EXPECT_EQ(*cell, (GridCellIndex{static_cast<std::size_t>(row), static_cast<std::size_t>(col), plane}));
                }
            }
        }
    }
}
