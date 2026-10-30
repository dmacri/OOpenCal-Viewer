#include <gtest/gtest.h>

#include <cmath>
#include <map>

#include "visualiser/VolumeLayers.h"

/** Test Suite: VolumeLayers
 *
 * In a native 3D model every cell has its own value of every substate. The user checks the substates to
 * show in the volume; on a cell the highest checked substate which has a value wins, and the voxel is
 * coloured by the intensity of that value. These tests check the rules deciding that. */

using namespace VolumeLayers;

namespace
{
LayerRange range(double min, double max)
{
    LayerRange r;
    r.minValue = min;
    r.maxValue = max;
    return r;
}
}

TEST(VolumeLayers, ValueBetweenMinAndMaxHasAnIntensity)
{
    const auto r = range(10.0, 20.0);
    EXPECT_DOUBLE_EQ(*intensityOf(15.0, r), 0.5);
    EXPECT_DOUBLE_EQ(*intensityOf(12.5, r), 0.25);
}

TEST(VolumeLayers, MinIsTheBackgroundAndMaxIsStillShown)
{
    const auto r = range(0.0, 1.0);
    EXPECT_FALSE(intensityOf(0.0, r));
    EXPECT_DOUBLE_EQ(*intensityOf(1.0, r), 1.0);
    EXPECT_FALSE(intensityOf(-3.0, r));
    EXPECT_FALSE(intensityOf(1.0001, r));
}

TEST(VolumeLayers, NoValueIsNotDrawnOnlyWhenEnabled)
{
    auto r = range(0.0, 100.0);
    r.noValue = 50.0;
    EXPECT_TRUE(intensityOf(50.0, r)); // not enabled: an ordinary value
    r.noValueEnabled = true;
    EXPECT_FALSE(intensityOf(50.0, r));
    EXPECT_TRUE(intensityOf(51.0, r));
}

TEST(VolumeLayers, UnusableRangeAndNonNumbersGiveNothing)
{
    EXPECT_FALSE(intensityOf(1.0, LayerRange{})); // Min and Max not set
    EXPECT_FALSE(intensityOf(1.0, range(5.0, 5.0)));
    EXPECT_FALSE(intensityOf(1.0, range(5.0, 1.0)));
    EXPECT_FALSE(intensityOf(std::nan(""), range(0.0, 2.0)));
    EXPECT_FALSE(intensityOf(INFINITY, range(0.0, 2.0)));
}

TEST(VolumeLayers, ScalarEncodesTheLayerAndTheIntensity)
{
    for (std::size_t layer : {0u, 1u, 3u, 7u})
    {
        for (double intensity : {0.001, 0.25, 0.5, 1.0})
        {
            const auto decoded = decode(encode(layer, intensity));
            ASSERT_TRUE(decoded);
            EXPECT_EQ(decoded->layer, layer);
            EXPECT_NEAR(decoded->intensity, intensity, 1e-12);
        }
    }
}

TEST(VolumeLayers, StretchesOfDifferentLayersDoNotOverlap)
{
    for (std::size_t layer = 0; layer < 10; ++layer)
    {
        EXPECT_LT(layerLastScalar(layer), layerFirstScalar(layer + 1));
        EXPECT_GT(layerFirstScalar(layer), emptyScalar);
    }
}

TEST(VolumeLayers, EmptyScalarDecodesToNothing)
{
    EXPECT_FALSE(decode(emptyScalar));
    EXPECT_FALSE(decode(0.5));
}

TEST(VolumeLayers, HighestLayerWithAValueWinsOnAVoxel)
{
    // Layer 0 is on top of the panel. Its value is out of range here, so layer 1 shows through.
    const std::vector<LayerRange> ranges{range(0.0, 10.0), range(0.0, 10.0)};
    const std::map<std::size_t, double> values{{0, 0.0}, {1, 5.0}};
    const double scalar = scalarForVoxel(ranges, [&](std::size_t layer) { return std::optional<double>(values.at(layer)); });
    const auto decoded = decode(scalar);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->layer, 1u);
    EXPECT_NEAR(decoded->intensity, 0.5, 1e-12);
}

TEST(VolumeLayers, WhenSeveralLayersHaveAValueOnlyTheTopOneIsDrawn)
{
    const std::vector<LayerRange> ranges{range(0.0, 10.0), range(0.0, 10.0), range(0.0, 10.0)};
    int asked = 0;
    const double scalar = scalarForVoxel(ranges, [&](std::size_t layer)
                                         {
                                             ++asked;
                                             return std::optional<double>(layer + 1.0);
                                         });
    EXPECT_EQ(decode(scalar)->layer, 0u);
    EXPECT_EQ(asked, 1) << "the layers below the winner need not be read";
}

TEST(VolumeLayers, VoxelWithNoValueInAnyLayerIsEmpty)
{
    const std::vector<LayerRange> ranges{range(0.0, 10.0), range(100.0, 200.0)};
    EXPECT_EQ(scalarForVoxel(ranges, [](std::size_t) { return std::optional<double>(0.0); }), emptyScalar);
    EXPECT_EQ(scalarForVoxel(ranges, [](std::size_t) { return std::optional<double>(); }), emptyScalar);
    EXPECT_EQ(scalarForVoxel({}, [](std::size_t) { return std::optional<double>(1.0); }), emptyScalar);
}

TEST(VolumeLayers, LayerWithoutUsableRangeIsSkipped)
{
    const std::vector<LayerRange> ranges{LayerRange{}, range(0.0, 10.0)};
    const double scalar = scalarForVoxel(ranges, [](std::size_t) { return std::optional<double>(5.0); });
    EXPECT_EQ(decode(scalar)->layer, 1u);
}

TEST(VolumeLayers, HexColorIsParsed)
{
    EXPECT_EQ(*parseHexColor("#000011"), (Rgb{0, 0, 17}));
    EXPECT_EQ(*parseHexColor("#0011FF"), (Rgb{0, 17, 255}));
    EXPECT_EQ(*parseHexColor("#aBcDeF"), (Rgb{171, 205, 239}));
}

TEST(VolumeLayers, MalformedHexColorGivesNothing)
{
    EXPECT_FALSE(parseHexColor(""));
    EXPECT_FALSE(parseHexColor("000011"));
    EXPECT_FALSE(parseHexColor("#00001"));
    EXPECT_FALSE(parseHexColor("#00001g"));
    EXPECT_FALSE(parseHexColor("#0000111"));
}

TEST(VolumeLayers, DefaultColorsDifferBetweenNeighbouringLayers)
{
    for (std::size_t layer = 0; layer < 8; ++layer)
    {
        EXPECT_NE(defaultColors(layer), defaultColors(layer + 1));
        EXPECT_NE(defaultColors(layer).first, defaultColors(layer).second);
    }
}
