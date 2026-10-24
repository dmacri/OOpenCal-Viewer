/** @file TimeFormatting.h
 *  @brief Qt-free helpers for presenting simulated time (given in seconds) in the UI.
 *
 *  All functions build the text by hand (no printf / streams), so the result never depends on the process locale. */
#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>

namespace TimeFormatting
{
/** @brief Plain seconds: `0.5 s`, `3600 s`, `12345.678 s`.
 *
 *  The value is rounded to milliseconds and printed in the shortest form (no trailing zeros, no exponent). */
inline std::string plainSeconds(double seconds)
{
    if (!std::isfinite(seconds))
        return "n/a";

    const double rounded = std::round(seconds * 1000.0) / 1000.0;

    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), rounded == 0.0 ? 0.0 : rounded, std::chars_format::fixed);
    return std::string(buffer, result.ptr) + " s";
}

/** @brief Human friendly time: seconds roll over to minutes, minutes to hours, hours to days.
 *
 *  - below a minute:  `5.5 s`
 *  - below an hour:   `1 min 5.5 s`
 *  - below a day:     `1 h 2 min 5.5 s`
 *  - a day and more:  `1 d 1 h 0 min 5 s` (tenths of a second are dropped from the first day on)
 *
 *  The leading units that are zero are omitted, the following ones are always shown (also when zero), so the
 *  text keeps its shape while a simulation is played. Tenths are rounded, not truncated (59.96 s is `1 min 0.0 s`). */
inline std::string pretty(double seconds)
{
    if (!std::isfinite(seconds))
        return "n/a";

    const bool negative = seconds < 0.0;
    const double absolute = std::fabs(seconds);
    if (absolute > 1e15) // would overflow the integer arithmetic below, and is not a meaningful simulation time anyway
        return plainSeconds(seconds);

    constexpr std::int64_t tenthsPerMinute = 600;
    constexpr std::int64_t tenthsPerHour = 36000;
    constexpr std::int64_t tenthsPerDay = 864000;

    std::string text;
    const std::int64_t tenths = std::llround(absolute * 10.0);
    if (tenths >= tenthsPerDay)
    {
        const std::int64_t whole = std::llround(absolute); // from the first day on tenths of a second are noise
        text = std::to_string(whole / 86400) + " d " + std::to_string(whole / 3600 % 24) + " h "
               + std::to_string(whole / 60 % 60) + " min " + std::to_string(whole % 60) + " s";
    }
    else
    {
        const std::int64_t hours = tenths / tenthsPerHour;
        const std::int64_t minutes = tenths / tenthsPerMinute % 60;
        const std::int64_t secondsTenths = tenths % tenthsPerMinute;
        const std::string secondsText = std::to_string(secondsTenths / 10) + "." + std::to_string(secondsTenths % 10) + " s";

        if (hours > 0)
            text = std::to_string(hours) + " h " + std::to_string(minutes) + " min " + secondsText;
        else if (minutes > 0)
            text = std::to_string(minutes) + " min " + secondsText;
        else
            text = secondsText;
    }

    return negative && tenths != 0 ? "-" + text : text;
}

/// Picks plainSeconds() or pretty() according to the user setting.
inline std::string format(double seconds, bool prettyFormat)
{
    return prettyFormat ? pretty(seconds) : plainSeconds(seconds);
}
} // namespace TimeFormatting
