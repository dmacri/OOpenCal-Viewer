/** @file VolumeLayers.h
 *  @brief Qt/VTK-free rules for drawing several substates of a native 3D model in one volume.
 *
 *  Every cell of a native 3D model (x, y, z) has its own value of every substate, so a substate is not a
 *  height here (like in a 2D model) - it is simply what a voxel is made of. The user checks the substates
 *  to show ("Show in 3D volume" in the Substate Parameters panel). For every cell:
 *
 *   - the checked substates are looked at in the order of the panel, the one on top first;
 *   - the first of them which has a value for the cell wins, the rest are hidden by it (so a voxel always
 *     shows exactly one substate - the highest one);
 *   - a substate has a value when it is a number in (Min, Max] which is not the "No value": Min is the
 *     background (like in the 2D height surface), Max is still shown;
 *   - the voxel is coloured by the intensity (value - Min) / (Max - Min) between the Min and Max colours of
 *     the substate; a cell with no value in any checked substate is transparent.
 *
 *  The volume is drawn from one scalar per voxel, which a colour/opacity transfer function turns into a
 *  colour. The scalar encodes both the winning substate and the intensity, so each substate has its own
 *  stretch of the transfer function (its own gradient):
 *
 *      scalar = emptyScalar                                     no value anywhere (transparent)
 *      scalar = layer * slotWidth + firstValueOffset + valueSpan * intensity
 *
 *  The same scalar is what the picking of the clicked cell looks at (see VolumePicking.h), so it does not
 *  need to know about the layers. Keeping this arithmetic free of Qt and VTK makes it unit-testable. */
#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace VolumeLayers
{

/// Scalar of a voxel which shows nothing (transparent)
inline constexpr double emptyScalar = -1.0;
/// Width of the stretch of scalars of one layer (substate)
inline constexpr double slotWidth = 10.0;
/// Scalar of the lowest intensity of layer 0, and the scalars' width of the intensity range
inline constexpr double firstValueOffset = 1.0;
inline constexpr double valueSpan = 8.0;

/// What decides whether a substate has a value in a cell
struct LayerRange
{
    double minValue = std::nan("");
    double maxValue = std::nan("");
    bool noValueEnabled = false;
    double noValue = std::nan("");

    /// Min and Max have to be set and Min < Max, otherwise the substate cannot be drawn
    [[nodiscard]] bool isUsable() const noexcept
    {
        return std::isfinite(minValue) && std::isfinite(maxValue) && minValue < maxValue;
    }
};

/** @brief Intensity in (0, 1] of a value, nothing if the substate has no value in the cell.
 *  @param value  value of the substate in the cell
 *  @param range  range of the substate */
inline std::optional<double> intensityOf(double value, const LayerRange& range) noexcept
{
    if (! range.isUsable() || ! std::isfinite(value))
        return std::nullopt;
    if (range.noValueEnabled && std::isfinite(range.noValue) && value == range.noValue)
        return std::nullopt;
    if (value <= range.minValue || value > range.maxValue)
        return std::nullopt;
    return (value - range.minValue) / (range.maxValue - range.minValue);
}

/// Scalar of a voxel showing the layer with the given intensity in (0, 1]
inline double encode(std::size_t layer, double intensity) noexcept
{
    return static_cast<double>(layer) * slotWidth + firstValueOffset + valueSpan * intensity;
}

/// Scalar of the lowest intensity of the layer
inline double layerFirstScalar(std::size_t layer) noexcept
{
    return encode(layer, 0.0);
}

/// Scalar of the highest intensity of the layer
inline double layerLastScalar(std::size_t layer) noexcept
{
    return encode(layer, 1.0);
}

/// Greatest scalar a voxel of any of the layers can have
inline double lastScalar(std::size_t layerCount) noexcept
{
    return layerCount == 0 ? 0.0 : layerLastScalar(layerCount - 1);
}

/// Which layer and intensity a scalar stands for (inverse of encode()); nothing for emptyScalar
struct Decoded
{
    std::size_t layer = 0;
    double intensity = 0.0;
};

inline std::optional<Decoded> decode(double scalar) noexcept
{
    if (! std::isfinite(scalar) || scalar < firstValueOffset)
        return std::nullopt;
    const auto layer = static_cast<std::size_t>(std::floor((scalar - firstValueOffset) / slotWidth));
    const double intensity = (scalar - static_cast<double>(layer) * slotWidth - firstValueOffset) / valueSpan;
    return Decoded{layer, intensity};
}

/** @brief Scalar of a voxel.
 *
 *  @param ranges   ranges of the layers, the layer on top of the panel first
 *  @param valueOf  callable `std::optional<double>(std::size_t layer)` giving the value of the substate of
 *                  the layer in this cell (nothing if it is not a number); called only as long as needed
 *  @return scalar of the first layer which has a value in the cell, emptyScalar if none has */
template<class ValueOf>
double scalarForVoxel(const std::vector<LayerRange>& ranges, ValueOf&& valueOf)
{
    for (std::size_t layer = 0; layer < ranges.size(); ++layer)
    {
        if (! ranges[layer].isUsable())
            continue;
        const auto value = valueOf(layer);
        if (! value)
            continue;
        if (const auto intensity = intensityOf(*value, ranges[layer]))
            return encode(layer, *intensity);
    }
    return emptyScalar;
}

/// Colour with components in [0, 255]
struct Rgb
{
    int red = 0;
    int green = 0;
    int blue = 0;

    friend constexpr bool operator==(const Rgb&, const Rgb&) = default;
};

/** @brief Parses "#RRGGBB".
 *  @return the colour, nothing for any other text */
inline std::optional<Rgb> parseHexColor(const std::string& hex) noexcept
{
    if (hex.size() != 7 || hex[0] != '#')
        return std::nullopt;

    int components[3] = {};
    for (int i = 0; i < 3; ++i)
    {
        int value = 0;
        for (int digit = 0; digit < 2; ++digit)
        {
            const char c = hex[1 + i * 2 + digit];
            int nibble = 0;
            if (c >= '0' && c <= '9')
                nibble = c - '0';
            else if (c >= 'a' && c <= 'f')
                nibble = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                nibble = c - 'A' + 10;
            else
                return std::nullopt;
            value = value * 16 + nibble;
        }
        components[i] = value;
    }
    return Rgb{components[0], components[1], components[2]};
}

/** @brief Colours of the lowest and the highest intensity of a layer which has no colours of its own.
 *
 *  The substates differ in hue, so that several of them in one volume can be told apart: layer 0 is
 *  blue -> red, the next ones yellow -> brown, light -> dark green and light -> dark blue (and then again). */
inline std::pair<Rgb, Rgb> defaultColors(std::size_t layer) noexcept
{
    switch (layer % 4)
    {
        case 0: return {Rgb{43, 131, 186}, Rgb{215, 25, 28}};
        case 1: return {Rgb{255, 237, 160}, Rgb{140, 45, 4}};
        case 2: return {Rgb{199, 233, 192}, Rgb{0, 90, 50}};
        default: return {Rgb{198, 219, 239}, Rgb{8, 48, 107}};
    }
}

} // namespace VolumeLayers
