/** @file StepTimeMap.cpp
 *  @brief Implementation of StepTimeMap. */

#include "StepTimeMap.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

namespace
{
std::vector<std::string> splitOnWhitespace(const std::string& text)
{
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    for (std::string token; iss >> token;)
        tokens.push_back(token);
    return tokens;
}

/// Locale independent (std::from_chars), accepts both '.' and ',' as the decimal separator.
bool parseNumber(std::string token, double& value)
{
    std::replace(token.begin(), token.end(), ',', '.');
    if (!token.empty() && token.front() == '+') // from_chars does not accept a leading '+'
        token.erase(0, 1);

    double parsed = 0.0;
    const auto [end, ec] = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (ec != std::errc{} || end != token.data() + token.size() || !std::isfinite(parsed))
        return false;

    value = parsed;
    return true;
}

bool parseStep(const std::string& token, StepIndex& step)
{
    if (token.empty() || !std::all_of(token.begin(), token.end(), [](char c) { return c >= '0' && c <= '9'; }))
        return false;

    errno = 0;
    const unsigned long long value = std::strtoull(token.c_str(), nullptr, 10);
    if (errno == ERANGE || value > std::numeric_limits<StepIndex>::max())
        return false;

    step = static_cast<StepIndex>(value);
    return true;
}
} // namespace

StepTimeMap::StepTimeMap(const std::filesystem::path& file)
{
    std::ifstream stream(file);
    if (!stream)
        return; // the file is optional: no file simply means "no time information"

    path = file;
    parse(stream);
}

StepTimeMap::StepTimeMap(std::istream& stream)
{
    parse(stream);
}

void StepTimeMap::parse(std::istream& stream)
{
    std::map<StepIndex, double> parsed;
    TimeUnit parsedUnit = TimeUnit::Seconds;
    bool unitLineSeen = false;

    const auto describeFile = [this] { return path.empty() ? std::string{} : " '" + path.string() + "'"; };

    std::string line;
    for (int lineNumber = 1; std::getline(stream, line); ++lineNumber)
    {
        if (const auto comment = line.find('#'); comment != std::string::npos)
            line.erase(comment);

        const auto tokens = splitOnWhitespace(line); // also swallows a trailing '\r' of Windows line endings
        if (tokens.empty())
            continue;

        const std::string trimmed = [&line] {
            const auto first = line.find_first_not_of(" \t\r\n");
            const auto last = line.find_last_not_of(" \t\r\n");
            return line.substr(first, last - first + 1); // the line has a token, so it is not blank
        }();

        if (trimmed.front() == '[') // unit line, e.g. "[ms]"
        {
            const bool isUnitLineAllowed = parsed.empty() && !unitLineSeen && trimmed.size() >= 2 && trimmed.back() == ']';
            if (!isUnitLineAllowed)
            {
                error = "Invalid line " + std::to_string(lineNumber) + " in time file" + describeFile()
                        + " (the unit, e.g. [ms], may be given once, before the first step): " + line;
                return;
            }

            const auto unitName = trimmed.substr(1, trimmed.size() - 2);
            const auto unitFromFile = parseTimeUnit(unitName);
            if (!unitFromFile)
            {
                error = "Unknown time unit [" + unitName + "] in line " + std::to_string(lineNumber) + " of time file" + describeFile()
                        + " (supported: [s], [ms])";
                return;
            }

            parsedUnit = *unitFromFile;
            unitLineSeen = true;
            continue;
        }

        StepIndex step = 0;
        double time = 0.0;
        if (tokens.size() < 2 || !parseStep(tokens[0], step) || !parseNumber(tokens[1], time))
        {
            error = "Invalid line " + std::to_string(lineNumber) + " in time file" + describeFile()
                    + " (expected: <step> <time>): " + line;
            return; // keep the map empty: half of a file would give misleading times
        }

        parsed[step] = time;
    }

    timeByStep = std::move(parsed);
    timeUnit = parsedUnit;
}

std::optional<double> StepTimeMap::timeForStep(StepIndex step) const
{
    const auto it = timeByStep.find(step);
    if (it == timeByStep.end())
        return std::nullopt;
    return it->second;
}

std::optional<double> StepTimeMap::secondsForStep(StepIndex step) const
{
    if (const auto time = timeForStep(step))
        return *time * timeUnitInSeconds(timeUnit);
    return std::nullopt;
}
