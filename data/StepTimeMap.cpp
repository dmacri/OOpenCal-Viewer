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
bool parseSeconds(std::string token, double& value)
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

    std::string line;
    for (int lineNumber = 1; std::getline(stream, line); ++lineNumber)
    {
        if (const auto comment = line.find('#'); comment != std::string::npos)
            line.erase(comment);

        const auto tokens = splitOnWhitespace(line); // also swallows a trailing '\r' of Windows line endings
        if (tokens.empty())
            continue;

        StepIndex step = 0;
        double seconds = 0.0;
        if (tokens.size() < 2 || !parseStep(tokens[0], step) || !parseSeconds(tokens[1], seconds))
        {
            error = "Invalid line " + std::to_string(lineNumber) + " in time file"
                    + (path.empty() ? std::string{} : " '" + path.string() + "'")
                    + " (expected: <step> <time in seconds>): " + line;
            return; // keep the map empty: half of a file would give misleading times
        }

        parsed[step] = seconds;
    }

    timeByStep = std::move(parsed);
}

std::optional<double> StepTimeMap::secondsForStep(StepIndex step) const
{
    const auto it = timeByStep.find(step);
    if (it == timeByStep.end())
        return std::nullopt;
    return it->second;
}
