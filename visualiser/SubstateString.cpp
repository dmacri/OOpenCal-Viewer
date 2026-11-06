#include "SubstateString.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <limits>
#include <string_view>
#include <system_error>

namespace SubstateString
{
namespace
{
bool isSet(double value)
{
    return ! std::isnan(value);
}

std::string_view trimmed(std::string_view text)
{
    while (! text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (! text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return text;
}

/** Shorter text of a long example from a cell, so that it fits in the message (the whole text is not needed to see what is wrong) */
std::string shortened(const std::string& text)
{
    constexpr std::size_t maxLength = 60;
    if (text.size() <= maxLength)
        return text;
    return text.substr(0, maxLength - 3) + "...";
}

/** Percentage of `part` in `total` as text, written by hand so it does not depend on the locale.
 *  It is "100%" only when `part` is all of `total`, and never "0%" for a non-zero `part`, so the number is never misleading. */
std::string formatPercent(std::size_t part, std::size_t total)
{
    if (total == 0)
        return "0%";
    if (part >= total)
        return "100%";
    if (part == 0)
        return "0%";

    // Tenths of a percent, rounded; a part which is not all of the total is shown as at most 99.9%
    auto tenths = static_cast<std::size_t>(std::llround(1000.0 * static_cast<double>(part) / static_cast<double>(total)));
    tenths = std::min<std::size_t>(tenths, 999);
    if (tenths == 0)
        return "<0.1%";

    if (tenths >= 100) // 10% and more: whole percents are enough
    {
        const std::size_t whole = std::min<std::size_t>((tenths + 5) / 10, 99);
        return std::to_string(whole) + "%";
    }
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "%";
}
} // namespace

std::string numberToText(double value)
{
    if (std::isnan(value))
        return "nan";

    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value); // shortest text which reads back as the same number
    if (result.ec != std::errc())
        return "nan";
    return std::string(buffer, result.ptr);
}

std::optional<double> parseNumber(const std::string& text)
{
    std::string_view view = trimmed(text);
    if (! view.empty() && view.front() == '+') // from_chars does not take a plus sign, std::stod() does
        view.remove_prefix(1);
    if (view.empty())
        return std::nullopt;

    double value = 0.0;
    const auto result = std::from_chars(view.data(), view.data() + view.size(), value);
    if (result.ec == std::errc::result_out_of_range) // "1e999": a number, too big for double
        return std::numeric_limits<double>::infinity();
    if (result.ec != std::errc() || result.ptr != view.data() + view.size())
        return std::nullopt;
    return value;
}

std::string formatEntry(const SubstateInfo& info)
{
    const std::string format = info.format.empty() ? "%f" : info.format;
    const bool hasMin = isSet(info.minValue);
    const bool hasMax = isSet(info.maxValue);
    const bool hasNoValue = info.noValueEnabled && isSet(info.noValue);
    const bool hasColors = ! info.minColor.empty() || ! info.maxColor.empty();

    std::string entry = "(" + info.name + "," + format;

    if (hasMin || hasMax || hasColors)
    {
        // Four or more parameters: the parser reads them by position (name, format, min, max, noValue, minColor, maxColor)
        entry += "," + numberToText(info.minValue) + "," + numberToText(info.maxValue);
        if (hasNoValue || hasColors)
            entry += "," + (hasNoValue ? numberToText(info.noValue) : std::string("nan"));
        if (hasColors)
        {
            entry += "," + (info.minColor.empty() ? std::string("-") : info.minColor);
            entry += "," + (info.maxColor.empty() ? std::string("-") : info.maxColor);
        }
    }
    else if (hasNoValue)
    {
        entry += "," + numberToText(info.noValue); // three parameters: the third is the no-value
    }

    entry += ")";
    return entry;
}

std::string formatEntries(const std::vector<SubstateInfo>& infos)
{
    std::string line;
    for (const auto& info : infos)
    {
        if (! line.empty())
            line += ",";
        line += formatEntry(info);
    }
    return line;
}

std::string validateName(const std::string& name, const std::vector<std::string>& existingNames)
{
    const std::string_view view = trimmed(name);
    if (view.empty())
        return "Enter the name of the substate.";

    for (const char c : view)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isspace(u) || std::iscntrl(u))
            return "The name cannot contain spaces.";
        if (c == ',' || c == '(' || c == ')' || c == '=')
            return std::string("The name cannot contain the character '") + c + "' (it separates the parts of the substates line).";
    }

    if (std::find(existingNames.begin(), existingNames.end(), std::string(view)) != existingNames.end())
        return "A substate named '" + std::string(view) + "' already exists.";

    return {};
}

ValueSummary summarizeValues(std::size_t cellCount,
                             std::size_t stride,
                             const std::function<std::string(std::size_t)>& textOfCell,
                             std::optional<double> noValue)
{
    ValueSummary summary;
    if (stride == 0)
        stride = 1;

    for (std::size_t index = 0; index < cellCount; index += stride)
    {
        const std::string text = textOfCell(index);
        ++summary.cellsChecked;

        const auto number = parseNumber(text);
        if (! number)
        {
            if (summary.notNumeric++ == 0)
                summary.firstNotNumericText = text;
            continue;
        }

        if (summary.numeric++ == 0)
            summary.firstNumericText = text;

        const double value = *number;
        if (! std::isfinite(value))
            continue;
        if (noValue && value == *noValue)
            continue;

        if (summary.usedForRange++ == 0)
        {
            summary.minValue = summary.maxValue = value;
        }
        else
        {
            summary.minValue = std::min(summary.minValue, value);
            summary.maxValue = std::max(summary.maxValue, value);
        }
    }
    return summary;
}

std::string describeProblem(const ValueSummary& summary, const std::string& substateName)
{
    if (summary.cellsChecked == 0)
        return "The model has no cells to check.";

    if (summary.notNumeric == 0)
        return {};

    const bool noCellGaveData = summary.notNumeric >= summary.cellsChecked;
    const std::string quotedName = "\"" + substateName + "\"";

    std::string first; // the first sentence: for how many cells the data could not be got
    if (noCellGaveData)
    {
        first = "Substate " + quotedName + " seems to be invalid: could not get data for " +
                (summary.cellsChecked == 1 ? std::string("the only checked cell.")
                                           : "any of the " + std::to_string(summary.cellsChecked) + " checked cells.");
    }
    else
    {
        first = "Could not get data of substate " + quotedName + " for " + std::to_string(summary.notNumeric) + " of " +
                std::to_string(summary.cellsChecked) + " checked cells (" +
                formatPercent(summary.notNumeric, summary.cellsChecked) + ").";
    }

    const std::string returned = summary.firstNotNumericText.empty() ? std::string("an empty text")
                                                                      : "'" + shortened(summary.firstNotNumericText) + "'";
    const std::string second = std::string(noCellGaveData ? "" : "For example, ") + "Cell::stringEncoding(" + quotedName +
                               ") returned " + returned + " instead of a number.";

    return first + "\n" + second + "\nThe model must return a plain number for the name of a substate.";
}
} // namespace SubstateString
