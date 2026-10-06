#include <vector>

#include <gtest/gtest.h>

#include "widgets/RulerLabelFit.h"

using namespace RulerLabelFit;

namespace
{
/** Horizontal labels: @p count boxes of @p width, their left edges @p spacing apart. */
std::vector<LabelBox> evenLabels(std::size_t count, double width, double spacing)
{
    std::vector<LabelBox> boxes;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double left = static_cast<double>(i) * spacing;
        boxes.push_back({ left, 0.0, left + width, 12.0 });
    }
    return boxes;
}

constexpr double kGap = 4.0;
} // namespace

// ---------------------------------------------------------------------------------------------
// minimumFontPixels / minimumGap
// ---------------------------------------------------------------------------------------------

TEST(RulerLabelFit, FontShrinksToAboutHalfOfItsNormalSize)
{
    EXPECT_DOUBLE_EQ(minimumFontPixels(20.0), 11.0);
    EXPECT_DOUBLE_EQ(minimumFontPixels(40.0), 22.0);
}

TEST(RulerLabelFit, FontNeverShrinksBelowTheAbsoluteFloor)
{
    EXPECT_DOUBLE_EQ(minimumFontPixels(10.0), kAbsoluteMinimumFontPixels);
    EXPECT_DOUBLE_EQ(minimumFontPixels(kAbsoluteMinimumFontPixels), kAbsoluteMinimumFontPixels);
}

TEST(RulerLabelFit, FontFloorNeverExceedsTheNormalSize)
{
    // A small window already starts below the absolute floor: the labels keep their size.
    EXPECT_DOUBLE_EQ(minimumFontPixels(5.0), 5.0);
}

TEST(RulerLabelFit, GapGrowsWithTheFontButHasAFloor)
{
    EXPECT_DOUBLE_EQ(minimumGap(20.0), 7.0);
    EXPECT_DOUBLE_EQ(minimumGap(4.0), 3.0);
}

// ---------------------------------------------------------------------------------------------
// anyOverlap
// ---------------------------------------------------------------------------------------------

TEST(RulerLabelFit, NoBoxesOrOneBoxNeverOverlap)
{
    EXPECT_FALSE(anyOverlap({}, kGap));
    EXPECT_FALSE(anyOverlap(evenLabels(1, 20.0, 0.0), kGap));
}

TEST(RulerLabelFit, SeparatedLabelsDoNotOverlap)
{
    EXPECT_FALSE(anyOverlap(evenLabels(8, 20.0, 30.0), kGap));
}

TEST(RulerLabelFit, IntersectingLabelsOverlap)
{
    EXPECT_TRUE(anyOverlap(evenLabels(8, 20.0, 15.0), kGap));
}

TEST(RulerLabelFit, LabelsCloserThanTheGapCountAsOverlapping)
{
    // 3 px between the labels, 4 px required.
    EXPECT_TRUE(anyOverlap(evenLabels(2, 20.0, 23.0), kGap));
    // Exactly the required gap is enough.
    EXPECT_FALSE(anyOverlap(evenLabels(2, 20.0, 24.0), kGap));
}

TEST(RulerLabelFit, OnlyTheNeighboursNeedToBeCheckedButAllPairsAre)
{
    // The first and the third label overlap although both are clear of the middle one.
    const std::vector<LabelBox> boxes{ { 0, 0, 50, 12 }, { 60, 100, 70, 112 }, { 40, 0, 90, 12 } };
    EXPECT_TRUE(anyOverlap(boxes, kGap));
}

TEST(RulerLabelFit, LabelsOnTopOfEachOtherOverlapOnAVerticalAxis)
{
    // Y ruler: the labels are stacked, their x ranges are identical.
    const std::vector<LabelBox> crowded{ { 0, 0, 30, 12 }, { 0, 10, 30, 22 } };
    const std::vector<LabelBox> roomy{ { 0, 0, 30, 12 }, { 0, 30, 30, 42 } };
    EXPECT_TRUE(anyOverlap(crowded, kGap));
    EXPECT_FALSE(anyOverlap(roomy, kGap));
}

// ---------------------------------------------------------------------------------------------
// isShown / everyNth
// ---------------------------------------------------------------------------------------------

TEST(RulerLabelFit, EveryNthCountsFromTheFirstLabel)
{
    const auto boxes = evenLabels(8, 10.0, 20.0);
    const auto kept = everyNth(boxes, 3);
    ASSERT_EQ(kept.size(), 3u);
    EXPECT_DOUBLE_EQ(kept[0].minX, boxes[0].minX);
    EXPECT_DOUBLE_EQ(kept[1].minX, boxes[3].minX);
    EXPECT_DOUBLE_EQ(kept[2].minX, boxes[6].minX);
}

TEST(RulerLabelFit, EveryNthCanCountFromTheLastLabel)
{
    const auto boxes = evenLabels(6, 10.0, 20.0);
    const auto kept = everyNth(boxes, 2, Anchor::Last);
    ASSERT_EQ(kept.size(), 3u);
    EXPECT_DOUBLE_EQ(kept[0].minX, boxes[1].minX);
    EXPECT_DOUBLE_EQ(kept[1].minX, boxes[3].minX);
    EXPECT_DOUBLE_EQ(kept[2].minX, boxes[5].minX);
}

TEST(RulerLabelFit, StrideOfOneKeepsEverything)
{
    EXPECT_EQ(everyNth(evenLabels(8, 10.0, 20.0), 1).size(), 8u);
    EXPECT_EQ(everyNth(evenLabels(8, 10.0, 20.0), 0).size(), 8u); // 0 is treated as 1
}

TEST(RulerLabelFit, IsShownAgreesWithEveryNth)
{
    for (const Anchor anchor : { Anchor::First, Anchor::Last })
    {
        for (std::size_t stride = 1; stride <= 8; ++stride)
        {
            std::size_t shown = 0;
            for (std::size_t i = 0; i < 8; ++i)
                shown += isShown(i, 8, stride, anchor);
            EXPECT_EQ(shown, everyNth(evenLabels(8, 10.0, 20.0), stride, anchor).size()) << "stride " << stride;
        }
    }
}

TEST(RulerLabelFit, NothingIsShownWithAStrideOfZero)
{
    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_FALSE(isShown(i, 4, 0, Anchor::First));
}

// ---------------------------------------------------------------------------------------------
// smallestFittingStride
// ---------------------------------------------------------------------------------------------

TEST(RulerLabelFit, AllLabelsAreShownWhenTheyFit)
{
    EXPECT_EQ(smallestFittingStride(evenLabels(8, 20.0, 30.0), kGap), 1u);
}

TEST(RulerLabelFit, EverySecondLabelIsShownWhenNeighboursCollide)
{
    // 2 px between neighbours (4 needed), 24 px between every second label.
    EXPECT_EQ(smallestFittingStride(evenLabels(8, 20.0, 22.0), kGap), 2u);
}

TEST(RulerLabelFit, TheStrideGrowsAsTheAxisShrinks)
{
    EXPECT_EQ(smallestFittingStride(evenLabels(8, 20.0, 12.0), kGap), 2u);
    EXPECT_EQ(smallestFittingStride(evenLabels(8, 20.0, 8.0), kGap), 3u);
    EXPECT_EQ(smallestFittingStride(evenLabels(8, 20.0, 5.0), kGap), 5u);
}

TEST(RulerLabelFit, NoStrideFitsWhenEvenTheEndsCollide)
{
    // Three labels 100 px wide on a 20 px axis.
    EXPECT_EQ(smallestFittingStride(evenLabels(3, 100.0, 10.0), kGap), 0u);
}

TEST(RulerLabelFit, ASingleLabelOrNoneNeedsNoThinning)
{
    EXPECT_EQ(smallestFittingStride({}, kGap), 1u);
    EXPECT_EQ(smallestFittingStride(evenLabels(1, 20.0, 0.0), kGap), 1u);
}

TEST(RulerLabelFit, TheAnchorDecidesWhichLabelsMustFitTogether)
{
    // The first label is wide, so it collides with label 2 but not with label 3.
    std::vector<LabelBox> boxes = evenLabels(4, 10.0, 30.0);
    boxes[0].maxX = 70.0;

    EXPECT_EQ(smallestFittingStride(boxes, kGap, Anchor::First), 3u); // keeps 0 and 3
    EXPECT_EQ(smallestFittingStride(boxes, kGap, Anchor::Last), 2u);  // keeps 3 and 1
}

// ---------------------------------------------------------------------------------------------
// spreadStride / fittingStride
// ---------------------------------------------------------------------------------------------

TEST(RulerLabelFit, SpreadStrideReachesTheEndOfTheAxis)
{
    EXPECT_EQ(spreadStride(8, 4), 7u); // 0 and 4  ->  0 and 7
    EXPECT_EQ(spreadStride(8, 5), 7u);
    EXPECT_EQ(spreadStride(8, 7), 7u);
    EXPECT_EQ(spreadStride(6, 3), 5u);
}

TEST(RulerLabelFit, SpreadStrideKeepsTheNumberOfLabels)
{
    for (std::size_t count = 2; count <= 12; ++count)
    {
        for (std::size_t stride = 1; stride < count; ++stride)
        {
            const std::size_t spread = spreadStride(count, stride);
            EXPECT_GE(spread, stride);
            EXPECT_EQ((count + spread - 1) / spread, (count + stride - 1) / stride) << count << " labels, stride " << stride;
        }
    }
}

TEST(RulerLabelFit, SpreadStrideLeavesLayoutsThatAlreadyReachTheEndAlone)
{
    EXPECT_EQ(spreadStride(8, 1), 1u);
    EXPECT_EQ(spreadStride(8, 2), 2u); // 0 2 4 6: any wider stride would show fewer labels
    EXPECT_EQ(spreadStride(5, 2), 2u); // 0 2 4 reaches the end
    EXPECT_EQ(spreadStride(1, 1), 1u);
    EXPECT_EQ(spreadStride(8, 0), 1u); // 0 is treated as 1
}

TEST(RulerLabelFit, FittingStrideSpreadsTheLabelsOverTheAxis)
{
    // 8 labels, 30 px wide; the smallest working stride is 4 (every 3rd collides), which would
    // show labels 0 and 4 and leave the end of the axis without a label.
    const auto boxes = evenLabels(8, 30.0, 10.0);
    ASSERT_EQ(smallestFittingStride(boxes, kGap), 4u);
    EXPECT_EQ(fittingStride(boxes, kGap), 7u);
}

TEST(RulerLabelFit, FittingStrideFallsBackWhenTheSpreadLayoutDoesNotFit)
{
    // 9 labels, minimal stride 3 shows 0, 3, 6; spreading would show 0, 4, 8, but label 4 is wide.
    std::vector<LabelBox> boxes = evenLabels(9, 10.0, 10.0);
    boxes[4].maxX = boxes[4].minX + 60.0;

    ASSERT_EQ(smallestFittingStride(boxes, kGap), 3u);
    ASSERT_EQ(spreadStride(boxes.size(), 3), 4u);
    EXPECT_EQ(fittingStride(boxes, kGap), 3u);
}

TEST(RulerLabelFit, FittingStrideIsZeroWhenNothingFits)
{
    EXPECT_EQ(fittingStride(evenLabels(3, 100.0, 10.0), kGap), 0u);
}

// ---------------------------------------------------------------------------------------------
// largestFittingValue
// ---------------------------------------------------------------------------------------------

TEST(RulerLabelFit, BisectionFindsTheThreshold)
{
    const auto fits = [](double value)
    {
        return value <= 0.37;
    };
    const double found = largestFittingValue(0.0, 1.0, 10, fits);

    EXPECT_LE(found, 0.37);
    EXPECT_GT(found, 0.37 - 1.0 / 1024.0);
}

TEST(RulerLabelFit, BisectionReturnsTheLowerBoundWhenNothingLargerFits)
{
    EXPECT_DOUBLE_EQ(largestFittingValue(0.4,
                                         1.0,
                                         6,
                                         [](double)
                                         {
                                             return false;
                                         }),
                     0.4);
}

TEST(RulerLabelFit, BisectionNeverEvaluatesTheUpperBound)
{
    double largestProbe = 0.0;
    largestFittingValue(0.0,
                        1.0,
                        8,
                        [&](double value)
                        {
                            largestProbe = std::max(largestProbe, value);
                            return true;
                        });
    EXPECT_LT(largestProbe, 1.0);
}

TEST(RulerLabelFit, BisectionCallsThePredicateOncePerStep)
{
    int calls = 0;
    largestFittingValue(0.0,
                        1.0,
                        5,
                        [&](double)
                        {
                            ++calls;
                            return true;
                        });
    EXPECT_EQ(calls, 5);
}
