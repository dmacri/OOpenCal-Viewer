/** @file TimeFormatting.h
 *  @brief Qt-free helpers for presenting simulated time in the UI.
 *
 *  All functions build the text by hand (no printf / streams), so the result never depends on the process locale. */
#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>

#include "core/TimeUnit.h"

namespace TimeFormatting
{
/** @brief Time with its unit, as it is written in the time file: `2500 ms`, `0.5 s`, `12345.678 s`.
 *
 *  The value is rounded to three decimal places and printed in the shortest form (no trailing zeros, no exponent). */
inline std::string plain(double value, TimeUnit unit)
{
    if (!std::isfinite(value))
        return "n/a";

    const double rounded = std::round(value * 1000.0) / 1000.0;

    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), rounded == 0.0 ? 0.0 : rounded, std::chars_format::fixed);
    return std::string(buffer, result.ptr) + " " + timeUnitName(unit);
}

/// Plain seconds: `0.5 s`, `3600 s`, `12345.678 s`. See plain().
inline std::string plainSeconds(double seconds)
{
    return plain(seconds, TimeUnit::Seconds);
}

/** @brief Human friendly time: seconds roll over to minutes, minutes to hours, hours to days.
 *
 *  - below a minute:  `5.5 s`
 *  - below an hour:   `1 min 5.5 s`
 *  - below a day:     `1 h 2 min 5.5 s`
 *  - a day and more:  `1 d 1 h 0 min 5 s` (fractions of a second are dropped from the first day on)
 *
 *  The leading units that are zero are omitted, the following ones are always shown (also when zero), so the
 *  text keeps its shape while a simulation is played. The fraction of a second is rounded, not truncated
 *  (59.96 s is `1 min 0.0 s`).
 *  @param seconds Time in seconds
 *  @param secondDecimals Digits kept after the decimal point of the seconds: 1 (tenths), 2 or 3 (milliseconds) */
inline std::string pretty(double seconds, int secondDecimals = 1)
{
    if (!std::isfinite(seconds))
        return "n/a";

    const bool negative = seconds < 0.0;
    const double absolute = std::fabs(seconds);
    if (absolute > 1e12) // would overflow the integer arithmetic below, and is not a meaningful simulation time anyway
        return plainSeconds(seconds);

    secondDecimals = std::clamp(secondDecimals, 0, 3);
    std::int64_t ticksPerSecond = 1; // a "tick" is the smallest fraction of a second that is shown
    for (int i = 0; i < secondDecimals; ++i)
        ticksPerSecond *= 10;

    const std::int64_t ticksPerMinute = 60 * ticksPerSecond;
    const std::int64_t ticksPerHour = 60 * ticksPerMinute;
    const std::int64_t ticksPerDay = 24 * ticksPerHour;

    std::string text;
    const std::int64_t ticks = std::llround(absolute * static_cast<double>(ticksPerSecond));
    if (ticks >= ticksPerDay)
    {
        const std::int64_t whole = std::llround(absolute); // from the first day on fractions of a second are noise
        text = std::to_string(whole / 86400) + " d " + std::to_string(whole / 3600 % 24) + " h "
               + std::to_string(whole / 60 % 60) + " min " + std::to_string(whole % 60) + " s";
    }
    else
    {
        const std::int64_t hours = ticks / ticksPerHour;
        const std::int64_t minutes = ticks / ticksPerMinute % 60;
        const std::int64_t secondsInTicks = ticks % ticksPerMinute;

        std::string secondsText = std::to_string(secondsInTicks / ticksPerSecond);
        if (secondDecimals > 0)
        {
            std::string fraction = std::to_string(secondsInTicks % ticksPerSecond);
            secondsText += "." + std::string(static_cast<std::size_t>(secondDecimals) - fraction.size(), '0') + fraction;
        }
        secondsText += " s";

        if (hours > 0)
            text = std::to_string(hours) + " h " + std::to_string(minutes) + " min " + secondsText;
        else if (minutes > 0)
            text = std::to_string(minutes) + " min " + secondsText;
        else
            text = secondsText;
    }

    return negative && ticks != 0 ? "-" + text : text;
}

/** @brief Formats a time given in the unit used by the model.
 *  @param value Time as written in the time file
 *  @param unit Unit of @p value
 *  @param prettyFormat true: days/hours/minutes/seconds (resolution of the unit, see timeUnitSecondDecimals()),
 *         false: the value with its unit, e.g. `2500 ms` */
inline std::string format(double value, TimeUnit unit, bool prettyFormat)
{
    return prettyFormat ? pretty(value * timeUnitInSeconds(unit), timeUnitSecondDecimals(unit)) : plain(value, unit);
}

/// Same as above for seconds.
inline std::string format(double seconds, bool prettyFormat)
{
    return format(seconds, TimeUnit::Seconds, prettyFormat);
}
} // namespace TimeFormatting
