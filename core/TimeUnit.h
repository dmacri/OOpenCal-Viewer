/** @file TimeUnit.h
 *  @brief Unit in which a model writes the simulated time (first line of `<output_file_name>-time.txt`, e.g. `[ms]`). */
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

/// Units of simulated time understood in the time file. To add a unit, extend this enum, allTimeUnits and the
/// three functions below (timeUnitName(), timeUnitDuration(), timeUnitSecondDecimals()).
enum class TimeUnit
{
    Seconds,
    Milliseconds,
    Minutes,
    Hours,
};

/// Every unit that can be written in the time file (in the order shown in messages).
inline constexpr std::array allTimeUnits{TimeUnit::Seconds, TimeUnit::Milliseconds, TimeUnit::Minutes, TimeUnit::Hours};

/// Simulated time as a floating point number of seconds. Every std::chrono duration (seconds, milliseconds, minutes,
/// hours, ...) converts to it implicitly and without loss.
using SimulatedSeconds = std::chrono::duration<double>;

/// Name of the unit as written in the time file (without brackets) and shown in the UI, e.g. "ms".
constexpr const char* timeUnitName(TimeUnit unit)
{
    switch (unit)
    {
    case TimeUnit::Seconds:
        return "s";
    case TimeUnit::Milliseconds:
        return "ms";
    case TimeUnit::Minutes:
        return "min";
    case TimeUnit::Hours:
        return "h";
    }
    return "s";
}

/// Length of one unit, e.g. `timeUnitDuration(TimeUnit::Minutes)` is 60 s.
constexpr SimulatedSeconds timeUnitDuration(TimeUnit unit)
{
    switch (unit)
    {
    case TimeUnit::Seconds:
        return std::chrono::seconds{1};
    case TimeUnit::Milliseconds:
        return std::chrono::milliseconds{1};
    case TimeUnit::Minutes:
        return std::chrono::minutes{1};
    case TimeUnit::Hours:
        return std::chrono::hours{1};
    }
    return std::chrono::seconds{1};
}

/// How many seconds the given unit has, e.g. 0.001 for milliseconds and 3600 for hours.
constexpr double timeUnitInSeconds(TimeUnit unit)
{
    return timeUnitDuration(unit).count();
}

/// A time written in the given unit (e.g. 2500 in milliseconds), expressed as std::chrono duration of seconds.
constexpr SimulatedSeconds toSeconds(double value, TimeUnit unit)
{
    return value * timeUnitDuration(unit);
}

/// Digits after the decimal point the "pretty" format keeps for seconds: the resolution of the unit
/// (tenths for seconds, thousandths when the model counts in milliseconds, whole seconds for minutes and hours).
constexpr int timeUnitSecondDecimals(TimeUnit unit)
{
    switch (unit)
    {
    case TimeUnit::Seconds:
        return 1;
    case TimeUnit::Milliseconds:
        return 3;
    case TimeUnit::Minutes:
    case TimeUnit::Hours:
        return 0;
    }
    return 1;
}

/** @brief Parses a unit name such as "ms", "s", "min" or "h" (case insensitive, surrounding spaces ignored; no brackets).
 *  @return The unit or std::nullopt when the name is not known */
inline std::optional<TimeUnit> parseTimeUnit(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);

    std::string name(text);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const TimeUnit unit : allTimeUnits)
        if (name == timeUnitName(unit))
            return unit;
    return std::nullopt;
}

/// The units for an error message: `[s], [ms], [min], [h]`.
inline std::string supportedTimeUnitsText()
{
    std::string text;
    for (const TimeUnit unit : allTimeUnits)
        text += (text.empty() ? "[" : ", [") + std::string(timeUnitName(unit)) + "]";
    return text;
}
