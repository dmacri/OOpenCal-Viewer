/** @file TimeFormatting.h
 *  @brief Qt-free helpers for presenting simulated time in the UI.
 *
 *  All functions build the text by hand (no printf / streams), so the result never depends on the process locale. */
#pragma once

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
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
 *  (59.96 s is `1 min 0.0 s`). The split into days, hours, minutes and seconds is done by std::chrono.
 *  @param time Simulated time (any std::chrono duration converts to SimulatedSeconds)
 *  @param secondDecimals Digits kept after the decimal point of the seconds: 0 to 3 (3 = milliseconds) */
inline std::string pretty(SimulatedSeconds time, int secondDecimals = 1)
{
    using namespace std::chrono;

    if (!std::isfinite(time.count()))
        return "n/a";

    const SimulatedSeconds absolute{std::fabs(time.count())};
    if (absolute.count() > 1e12) // not a meaningful simulation time, and too much for the integer durations below
        return plainSeconds(time.count());

    // Round to the resolution that is shown first, so that e.g. 59.96 s (one decimal) becomes a whole minute
    secondDecimals = std::clamp(secondDecimals, 0, 3);
    constexpr double decimalScale[] = {1.0, 10.0, 100.0, 1000.0};
    const double scale = decimalScale[secondDecimals];
    const milliseconds total = round<milliseconds>(SimulatedSeconds{std::round(absolute.count() * scale) / scale});

    std::string text;
    if (total >= days{1})
    {
        // From the first day on fractions of a second are noise
        const seconds whole{std::llround(absolute.count())};
        const days wholeDays = floor<days>(whole);
        const hh_mm_ss<seconds> rest{whole - wholeDays};
        text = std::to_string(wholeDays.count()) + " d " + std::to_string(rest.hours().count()) + " h "
               + std::to_string(rest.minutes().count()) + " min " + std::to_string(rest.seconds().count()) + " s";
    }
    else
    {
        const hh_mm_ss<milliseconds> parts{total};

        std::string secondsText = std::to_string(parts.seconds().count());
        if (secondDecimals > 0)
        {
            std::string fraction = std::to_string(parts.subseconds().count()); // milliseconds: 0..999
            fraction.insert(0, 3 - fraction.size(), '0');
            secondsText += "." + fraction.substr(0, static_cast<std::size_t>(secondDecimals));
        }
        secondsText += " s";

        if (parts.hours().count() > 0)
            text = std::to_string(parts.hours().count()) + " h " + std::to_string(parts.minutes().count()) + " min " + secondsText;
        else if (parts.minutes().count() > 0)
            text = std::to_string(parts.minutes().count()) + " min " + secondsText;
        else
            text = secondsText;
    }

    return time.count() < 0.0 && total.count() != 0 ? "-" + text : text;
}

/// Same as above for a plain number of seconds.
inline std::string pretty(double seconds, int secondDecimals = 1)
{
    return pretty(SimulatedSeconds{seconds}, secondDecimals);
}

/** @brief Formats a time given in the unit used by the model.
 *  @param value Time as written in the time file
 *  @param unit Unit of @p value
 *  @param prettyFormat true: days/hours/minutes/seconds (resolution of the unit, see timeUnitSecondDecimals()),
 *         false: the value with its unit, e.g. `2500 ms` */
inline std::string format(double value, TimeUnit unit, bool prettyFormat)
{
    return prettyFormat ? pretty(toSeconds(value, unit), timeUnitSecondDecimals(unit)) : plain(value, unit);
}

/// Same as above for seconds.
inline std::string format(double seconds, bool prettyFormat)
{
    return format(seconds, TimeUnit::Seconds, prettyFormat);
}
} // namespace TimeFormatting
