/** @file TimeUnit.h
 *  @brief Unit in which a model writes the simulated time (first line of `<output_file_name>-time.txt`, e.g. `[ms]`). */
#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

/// Units of simulated time understood in the time file. To add a unit, extend the three functions below.
enum class TimeUnit
{
    Seconds,
    Milliseconds,
};

/// Name of the unit as written in the time file (without brackets) and shown in the UI, e.g. "ms".
constexpr const char* timeUnitName(TimeUnit unit)
{
    switch (unit)
    {
    case TimeUnit::Seconds:
        return "s";
    case TimeUnit::Milliseconds:
        return "ms";
    }
    return "s";
}

/// How many seconds the given unit has, e.g. 0.001 for milliseconds.
constexpr double timeUnitInSeconds(TimeUnit unit)
{
    switch (unit)
    {
    case TimeUnit::Seconds:
        return 1.0;
    case TimeUnit::Milliseconds:
        return 0.001;
    }
    return 1.0;
}

/// Digits after the decimal point the "pretty" format keeps for seconds: the resolution of the unit
/// (tenths for seconds, thousandths when the model counts in milliseconds).
constexpr int timeUnitSecondDecimals(TimeUnit unit)
{
    return unit == TimeUnit::Milliseconds ? 3 : 1;
}

/** @brief Parses a unit name such as "ms" or "s" (case insensitive, surrounding spaces ignored; no brackets).
 *  @return The unit or std::nullopt when the name is not known */
inline std::optional<TimeUnit> parseTimeUnit(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);

    std::string name(text);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const TimeUnit unit : {TimeUnit::Seconds, TimeUnit::Milliseconds})
        if (name == timeUnitName(unit))
            return unit;
    return std::nullopt;
}
