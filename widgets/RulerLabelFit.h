/** @file RulerLabelFit.h
 *  @brief VTK-free helpers that decide how to keep ruler tick labels from overlapping.
 *
 *  The ruler axes keep a fixed number of tick labels while the camera zooms, so once the scene is
 *  zoomed out far enough the labels run into each other ("100200300"). The strategy implemented
 *  with these helpers is:
 *
 *  1. keep the normal label size when the labels fit,
 *  2. otherwise shrink the font, but never below minimumFontPixels(),
 *  3. if the labels still collide at that size, show only every N-th label (fittingStride()),
 *     still at that smallest size, so the font never grows while the scene is zoomed out further,
 *  4. if even the first and the last label collide, show no labels at all.
 *
 *  Everything here works on plain numbers (pixel boxes), so it is unit-tested without a render
 *  window; RulerAxisActor2D feeds it with the boxes measured from the real VTK labels. */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace RulerLabelFit
{
/** @brief Axis-aligned bounding box of one rendered tick label, in viewport pixels. */
struct LabelBox
{
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
};

/** @brief Which end of the axis the thinned-out labels are counted from.
 *
 *  Showing every N-th label keeps the one at the anchor, so the anchor should be the end that
 *  carries the origin of the scale (the "0"): First for an axis that grows from its origin, Last
 *  for one that is reversed. */
enum class Anchor
{
    First,
    Last
};

/** @brief Whether the label at @p index is shown when every @p stride-th label is (0 = none). */
inline bool isShown(std::size_t index, std::size_t labelCount, std::size_t stride, Anchor anchor)
{
    if (stride == 0)
        return false;
    const std::size_t distanceFromAnchor = anchor == Anchor::First ? index : labelCount - 1 - index;
    return distanceFromAnchor % stride == 0;
}

/** @brief Labels are never shrunk below this many pixels of font size, however small the axis gets. */
inline constexpr double kAbsoluteMinimumFontPixels = 7.0;

/** @brief Labels are never shrunk below this fraction of their normal size. */
inline constexpr double kMinimumFontScale = 0.55;

/** @brief Smallest font size (pixels) the labels may be shrunk to.
 *
 *  Half-size text is still legible, anything smaller is just noise, so the floor is the larger of
 *  kAbsoluteMinimumFontPixels and kMinimumFontScale of the normal size. It never exceeds the normal
 *  size itself: a tiny window that already starts below the absolute floor keeps its size.
 *
 *  Pixels, not points: VTK sizes fonts in points, but Qt scales the window DPI on high-DPI
 *  screens, so a point is not a pixel there. */
inline double minimumFontPixels(double preferredFontPixels)
{
    return std::min(preferredFontPixels, std::max(kAbsoluteMinimumFontPixels, kMinimumFontScale * preferredFontPixels));
}

/** @brief Free space (pixels) that must remain between two neighbouring labels. */
inline double minimumGap(double fontPixels)
{
    return std::max(3.0, 0.35 * fontPixels);
}

/** @brief True when any two boxes intersect or come closer to each other than @p gap pixels. */
inline bool anyOverlap(std::span<const LabelBox> boxes, double gap)
{
    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < boxes.size(); ++j)
        {
            const LabelBox& a = boxes[i];
            const LabelBox& b = boxes[j];
            const bool apartHorizontally = a.maxX + gap <= b.minX || b.maxX + gap <= a.minX;
            const bool apartVertically = a.maxY + gap <= b.minY || b.maxY + gap <= a.minY;
            if (! apartHorizontally && ! apartVertically)
                return true;
        }
    }
    return false;
}

/** @brief The boxes that are shown when every @p stride-th label is (a stride of 0 counts as 1). */
inline std::vector<LabelBox> everyNth(std::span<const LabelBox> boxes, std::size_t stride, Anchor anchor = Anchor::First)
{
    stride = std::max<std::size_t>(stride, 1);
    std::vector<LabelBox> kept;
    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        if (isShown(i, boxes.size(), stride, anchor))
            kept.push_back(boxes[i]);
    }
    return kept;
}

/** @brief Smallest stride for which showing every stride-th label leaves no two labels overlapping.
 *
 *  A stride of 1 means "show all labels". Strides that would leave a single label (stride >= number
 *  of labels) are not considered, because one lonely number says nothing about the scale.
 *
 *  @param boxes  boxes of all labels in axis order, measured at the font size that will be used
 *  @param gap    required free space between neighbours, in pixels
 *  @param anchor the end of the axis whose label is always among the shown ones
 *  @return the stride, or 0 when no stride works (even the first and the last label collide) */
inline std::size_t smallestFittingStride(std::span<const LabelBox> boxes, double gap, Anchor anchor = Anchor::First)
{
    if (boxes.size() < 2)
        return 1;

    for (std::size_t stride = 1; stride < boxes.size(); ++stride)
    {
        if (! anyOverlap(everyNth(boxes, stride, anchor), gap))
            return stride;
    }
    return 0;
}

/** @brief The widest stride that still shows as many labels as @p stride does.
 *
 *  With 8 labels a stride of 4 shows labels 0 and 4, whereas a stride of 7 shows 0 and 7, i.e. both
 *  ends of the axis. Same number of labels, but spread over the whole axis instead of leaving its
 *  far end unlabelled. Wider spacing does not guarantee that the labels still fit (their widths
 *  differ), so use fittingStride(), which checks it.
 *
 *  @param labelCount  number of labels on the axis
 *  @param stride      a stride >= 1 (as returned by smallestFittingStride())
 *  @return @p stride itself when fewer than two labels are shown */
inline std::size_t spreadStride(std::size_t labelCount, std::size_t stride)
{
    stride = std::max<std::size_t>(stride, 1);
    const std::size_t shown = (labelCount + stride - 1) / stride;
    if (shown < 2)
        return stride;
    return (labelCount - 1) / (shown - 1);
}

/** @brief The stride to display the labels with: as many labels as fit, spread over the axis.
 *
 *  That is smallestFittingStride(), widened by spreadStride() when the widened layout still has no
 *  overlap.
 *
 *  @return the stride, or 0 when the labels do not fit at any stride */
inline std::size_t fittingStride(std::span<const LabelBox> boxes, double gap, Anchor anchor = Anchor::First)
{
    const std::size_t smallest = smallestFittingStride(boxes, gap, anchor);
    if (smallest == 0)
        return 0;

    const std::size_t spread = spreadStride(boxes.size(), smallest);
    return anyOverlap(everyNth(boxes, spread, anchor), gap) ? smallest : spread;
}

/** @brief Largest value in [@p low, @p high] for which @p fits is true, found by bisection.
 *
 *  Used to find the largest font size (as a LabelFactor) at which all labels still fit.
 *
 *  Assumes @p fits is monotonic (true up to some threshold, false above it), that it holds for
 *  @p low, and that it does not hold for @p high, which is therefore never evaluated.
 *
 *  @param steps  number of bisection steps; the result is within (high - low) / 2^steps of the
 *                true threshold
 *  @return the largest value that was verified to fit (@p low when nothing larger fits) */
template<typename FitsPredicate> double largestFittingValue(double low, double high, int steps, FitsPredicate&& fits)
{
    for (int step = 0; step < steps; ++step)
    {
        const double middle = 0.5 * (low + high);
        if (fits(middle))
            low = middle;
        else
            high = middle;
    }
    return low;
}
} // namespace RulerLabelFit
