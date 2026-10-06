#include "widgets/RulerAxisActor2D.h"

#include <algorithm>
#include <cmath>

#include <vtkActor2D.h>
#include <vtkCoordinate.h>
#include <vtkObjectFactory.h>
#include <vtkTextMapper.h>
#include <vtkTextProperty.h>
#include <vtkViewport.h>
#include <vtkWindow.h>

using RulerLabelFit::Anchor;
using RulerLabelFit::anyOverlap;
using RulerLabelFit::everyNth;
using RulerLabelFit::fittingStride;
using RulerLabelFit::isShown;
using RulerLabelFit::LabelBox;
using RulerLabelFit::largestFittingValue;
using RulerLabelFit::minimumFontPixels;
using RulerLabelFit::minimumGap;

vtkStandardNewMacro(RulerAxisActor2D);

namespace
{
/** Bisection steps when looking for the largest font that fits: the font range between the normal
 *  and the smallest size is split into 16 parts, which is finer than one font point. */
constexpr int kFontSearchSteps = 4;

/** VTK picks the largest font whose text still fits a target height, so the font reached with a
 *  scaled LabelFactor can land a point or two below the one asked for. The factor is raised a
 *  few times to reach the smallest acceptable font. */
constexpr int kFloorCorrections = 3;
} // namespace

RulerAxisActor2D::RulerAxisActor2D()
    : preferredLabelFactor(this->LabelFactor)
{
}

RulerAxisActor2D::~RulerAxisActor2D()
{
    if (sizePeer)
        sizePeer->sizePeer = nullptr;
}

void RulerAxisActor2D::LinkLabelSizeWith(RulerAxisActor2D* peer)
{
    if (peer == this || peer == sizePeer)
        return;

    if (sizePeer)
        sizePeer->sizePeer = nullptr;
    sizePeer = peer;

    if (peer)
    {
        if (peer->sizePeer)
            peer->sizePeer->sizePeer = nullptr;
        peer->sizePeer = this;
    }
}

int RulerAxisActor2D::windowDpi(vtkViewport* viewport)
{
    // Qt scales the DPI of the window on high-DPI screens.
    const int dpi = viewport->GetVTKWindow()->GetDPI();
    return dpi > 0 ? dpi : 72;
}

bool RulerAxisActor2D::canFit(vtkViewport* viewport)
{
    return AutoFitLabels && LabelVisibility && LabelTextProperty && viewport->GetVTKWindow();
}

void RulerAxisActor2D::BuildAxis(vtkViewport* viewport)
{
    if (! canFit(viewport))
    {
        vtkAxisActor2D::BuildAxis(viewport);
        return;
    }

    // A peer that is not rendered must not shrink our labels.
    RulerAxisActor2D* peer = sizePeer && sizePeer->GetVisibility() && sizePeer->canFit(viewport) ? sizePeer : nullptr;

    // Fitting costs a few builds, so it is redone only when something that influences the layout
    // changed: the axis ends or the viewport size (zoom, pan of the perspective camera, resize),
    // or a property of the axis itself (range, format, text properties, ...).
    if (stampWith(viewport, peer) == lastStamp)
    {
        vtkAxisActor2D::BuildAxis(viewport);
        return;
    }

    if (peer)
    {
        // The peer is fitted here, whichever of the two axes the renderer asks first, so that
        // both always end up with the same font.
        const double common = std::min(chooseFactor(viewport), peer->chooseFactor(viewport));
        applyFactor(viewport, common);
        peer->applyFactor(viewport, common);
    }
    else
    {
        applyFactor(viewport, chooseFactor(viewport));
    }

    // Taken after the builds above, which touch the modification time of the axes.
    lastStamp = stampWith(viewport, peer);
    if (peer)
        peer->lastStamp = peer->stampWith(viewport, this);
}

RulerAxisActor2D::FitStamp RulerAxisActor2D::stampWith(vtkViewport* viewport, RulerAxisActor2D* peer)
{
    FitStamp stamp;

    const int* start = PositionCoordinate->GetComputedViewportValue(viewport);
    stamp.pixels[0] = start[0];
    stamp.pixels[1] = start[1];

    const int* end = Position2Coordinate->GetComputedViewportValue(viewport);
    stamp.pixels[2] = end[0];
    stamp.pixels[3] = end[1];

    const int* size = viewport->GetSize();
    stamp.pixels[4] = size[0];
    stamp.pixels[5] = size[1];
    stamp.pixels[6] = windowDpi(viewport);

    stamp.modifiedTimes[0] = GetMTime();
    stamp.modifiedTimes[1] = LabelTextProperty->GetMTime();

    if (peer)
    {
        const int* peerStart = peer->PositionCoordinate->GetComputedViewportValue(viewport);
        stamp.peerPixels[0] = peerStart[0];
        stamp.peerPixels[1] = peerStart[1];

        const int* peerEnd = peer->Position2Coordinate->GetComputedViewportValue(viewport);
        stamp.peerPixels[2] = peerEnd[0];
        stamp.peerPixels[3] = peerEnd[1];

        stamp.modifiedTimes[2] = peer->GetMTime();
        stamp.modifiedTimes[3] = peer->LabelTextProperty->GetMTime();
    }
    return stamp;
}

double RulerAxisActor2D::chooseFactor(vtkViewport* viewport)
{
    // buildLayout() leaves its result in `current`, which is what the steps below look at.

    // 1. Normal size. Most of the time (zoomed in) the labels fit and nothing else is needed.
    buildLayout(viewport, preferredLabelFactor);
    if (! anyOverlap(current.boxes, minimumGap(current.fontPixels)))
        return preferredLabelFactor;

    // 2. Smallest acceptable font. Whether the labels fit even then decides how many have to go.
    const double smallestFontPixels = minimumFontPixels(current.fontPixels);
    double smallestFactor = preferredLabelFactor * smallestFontPixels / current.fontPixels;
    buildLayout(viewport, smallestFactor);

    for (int attempt = 0; attempt < kFloorCorrections && current.fontPixels < smallestFontPixels; ++attempt)
    {
        smallestFactor *= smallestFontPixels / std::max(current.fontPixels, 1.0);
        smallestFactor = std::min(smallestFactor, preferredLabelFactor);
        buildLayout(viewport, smallestFactor);
    }

    const std::size_t stride = fittingStride(current.boxes, minimumGap(current.fontPixels), labelAnchor());
    if (stride == 0)
        return preferredLabelFactor; // No labels are shown, so they do not constrain the font.

    // 3. Labels that have to be thinned out keep the smallest font. Making them larger again would
    //    let the font grow while the scene is zoomed further out, i.e. the numbers would pulse.
    if (stride > 1)
        return smallestFactor;

    // 4. All labels fit at the smallest font: they get the largest font at which they still do.
    const auto fitsAt = [&](double factor)
    {
        buildLayout(viewport, factor);
        return ! anyOverlap(current.boxes, minimumGap(current.fontPixels));
    };
    return largestFittingValue(smallestFactor, preferredLabelFactor, kFontSearchSteps, fitsAt);
}

void RulerAxisActor2D::applyFactor(vtkViewport* viewport, double factor)
{
    // After chooseFactor() the axis is still built for its last probe, which is often the answer.
    if (current.factor != factor)
        buildLayout(viewport, factor);

    publish(current, fittingStride(current.boxes, minimumGap(current.fontPixels), labelAnchor()));
}

void RulerAxisActor2D::buildLayout(vtkViewport* viewport, double labelFactor)
{
    SetLabelFactor(labelFactor);

    // Modified() makes vtkAxisActor2D re-adjust the range, set the label texts again (undoing the
    // hiding done by publish()) and recompute the font size, even if LabelFactor did not change.
    Modified();
    vtkAxisActor2D::BuildAxis(viewport);

    current.factor = labelFactor;
    current.fontSize = NumberOfLabelsBuilt > 0 ? LabelMappers[0]->GetTextProperty()->GetFontSize() : 0;
    current.fontPixels = current.fontSize * windowDpi(viewport) / 72.0; // a point is 1/72 inch
    current.boxes = measureLabelBoxes(viewport);
}

std::vector<LabelBox> RulerAxisActor2D::measureLabelBoxes(vtkViewport* viewport)
{
    std::vector<LabelBox> boxes;
    boxes.reserve(static_cast<std::size_t>(NumberOfLabelsBuilt));

    for (int i = 0; i < NumberOfLabelsBuilt; ++i)
    {
        // vtkAxisActor2D positions every label by its lower-left corner.
        const double* corner = LabelActors[i]->GetPosition();
        int size[2] = { 0, 0 };
        LabelMappers[i]->GetSize(viewport, size);

        boxes.push_back({ corner[0], corner[1], corner[0] + size[0], corner[1] + size[1] });
    }
    return boxes;
}

void RulerAxisActor2D::publish(const Layout& layout, std::size_t stride)
{
    // vtkAxisActor2D draws every label that has a text, so a hidden label is an empty one. The
    // ticks stay, which is what makes the remaining labels still readable as a scale.
    const Anchor anchor = labelAnchor();
    const std::size_t count = static_cast<std::size_t>(NumberOfLabelsBuilt);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (! isShown(i, count, stride, anchor))
            LabelMappers[i]->SetInput("");
    }

    fittedFontSize = layout.fontSize;
    labelStride = stride;
    visibleLabelBoxes = stride == 0 ? std::vector<LabelBox>{} : everyNth(layout.boxes, stride, anchor);
}

RulerLabelFit::Anchor RulerAxisActor2D::labelAnchor() const
{
    // The first label belongs to the start of the axis. A reversed axis (e.g. the Y ruler, which
    // counts from the top) has its origin at the other end.
    return std::abs(AdjustedRange[1]) < std::abs(AdjustedRange[0]) ? Anchor::Last : Anchor::First;
}
