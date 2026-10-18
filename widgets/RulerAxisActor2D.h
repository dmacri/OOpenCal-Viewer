/** @file RulerAxisActor2D.h
 *  @brief vtkAxisActor2D whose tick labels stay readable at every zoom level. */
#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <vtkAxisActor2D.h>

#include "widgets/RulerLabelFit.h"

/** @brief Ruler axis that keeps its tick labels from overlapping when the scene is zoomed out.
 *
 *  A plain vtkAxisActor2D draws the same 5-8 labels whatever the camera distance and sizes their
 *  font from the window, not from the axis. When the scene is zoomed out the axis gets short in
 *  pixels, the labels no longer fit along it and run together ("100200300400").
 *
 *  Every time the axis is (re)built, i.e. on every zoom, resize or export render, this class
 *  measures the labels that VTK has laid out and, if any two collide, fixes the layout:
 *
 *  1. the label font is shrunk, but not below RulerLabelFit::minimumFontPixels(),
 *  2. when the labels still collide at that size only every N-th label is shown, at that smallest
 *     size (the font never grows while the scene is zoomed further out),
 *  3. when even the first and the last label collide, the labels are hidden (ticks and title stay).
 *
 *  The decision is recomputed from the normal size each time, so zooming back in restores the
 *  full-size labels. When the labels fit, nothing differs from vtkAxisActor2D.
 *
 *  The axes of one scene (X and Y) usually need different font sizes. Link them with
 *  LinkLabelSizeWith() and they share the smaller one, so the numbers on both look alike.
 *
 *  While auto-fit is on, LabelFactor is owned by this class: use it as a drop-in replacement for
 *  vtkAxisActor2D, but do not call SetLabelFactor() on it. */
class RulerAxisActor2D : public vtkAxisActor2D
{
public:
    static RulerAxisActor2D* New();
    vtkTypeMacro(RulerAxisActor2D, vtkAxisActor2D);

    ///@{
    /** @brief Turns the label fitting on (default) or off.
     *
     *  When off, the axis behaves exactly like vtkAxisActor2D. */
    vtkSetMacro(AutoFitLabels, vtkTypeBool);
    vtkGetMacro(AutoFitLabels, vtkTypeBool);
    vtkBooleanMacro(AutoFitLabels, vtkTypeBool);
    ///@}

    /** @brief Makes this axis and @p peer use one common label size.
     *
     *  The link is symmetric and joins exactly two axes of the same renderer; a previous link of
     *  either axis is dropped. The peer is not owned: the link is cleared when either axis is
     *  destroyed. The peer only takes part while it is visible.
     *
     *  @param peer  the other axis, or nullptr to remove the link */
    void LinkLabelSizeWith(RulerAxisActor2D* peer);

    /** @brief Font size (points) of the labels after the last build, 0 before the first one. */
    int GetLabelFontSize() const
    {
        return fittedFontSize;
    }

    /** @brief 1 when every label is shown, N when every N-th is, 0 when all labels are hidden. */
    std::size_t GetLabelStride() const
    {
        return labelStride;
    }

    /** @brief Boxes (viewport pixels) of the labels that are shown after the last build. */
    const std::vector<RulerLabelFit::LabelBox>& GetVisibleLabelBoxes() const
    {
        return visibleLabelBoxes;
    }

protected:
    RulerAxisActor2D();
    ~RulerAxisActor2D() override;

    /** @brief Builds the axis like vtkAxisActor2D does, then fits the labels to the axis length. */
    void BuildAxis(vtkViewport* viewport) override;

    vtkTypeBool AutoFitLabels = 1;

private:
    /** Everything the layout depends on, so that it is recomputed only when one of them changes:
     *  both ends of this axis and of its peer plus the viewport size (pixels) and the window DPI,
     *  and the modification times of the axis and of its label text property (range, format,
     *  colours, ...). */
    struct FitStamp
    {
        std::array<int, 7> pixels{};
        std::array<int, 4> peerPixels{};
        std::array<vtkMTimeType, 4> modifiedTimes{};

        bool operator==(const FitStamp&) const = default;
    };

    /** The labels as vtkAxisActor2D laid them out for one LabelFactor. */
    struct Layout
    {
        double factor = 0.0;
        int fontSize = 0;        // points, as VTK sizes fonts
        double fontPixels = 0.0; // the same size on screen, which also depends on the window DPI
        std::vector<RulerLabelFit::LabelBox> boxes;
    };

    static int windowDpi(vtkViewport* viewport);
    bool canFit(vtkViewport* viewport);
    FitStamp stampWith(vtkViewport* viewport, RulerAxisActor2D* peer);

    /** The largest LabelFactor at which this axis can show its labels as well as it can. */
    double chooseFactor(vtkViewport* viewport);

    /** Builds the axis for @p factor and hides the labels that do not fit at that size. */
    void applyFactor(vtkViewport* viewport, double factor);

    /** Builds the axis for @p labelFactor and measures the labels into `current`. */
    void buildLayout(vtkViewport* viewport, double labelFactor);
    std::vector<RulerLabelFit::LabelBox> measureLabelBoxes(vtkViewport* viewport);
    void publish(const Layout& layout, std::size_t stride);

    /** The end of the axis that carries the origin of the scale, i.e. the smaller label value. */
    RulerLabelFit::Anchor labelAnchor() const;

    RulerAxisActor2D(const RulerAxisActor2D&) = delete;
    void operator=(const RulerAxisActor2D&) = delete;

    /** LabelFactor of the unfitted axis (the vtkAxisActor2D default). */
    double preferredLabelFactor;

    RulerAxisActor2D* sizePeer = nullptr;
    FitStamp lastStamp;
    Layout current; // the layout the axis is currently built for

    int fittedFontSize = 0;
    std::size_t labelStride = 1;
    std::vector<RulerLabelFit::LabelBox> visibleLabelBoxes;
};
