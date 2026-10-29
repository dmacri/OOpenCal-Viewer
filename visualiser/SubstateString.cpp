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

    if (summary.notNumeric > 0)
    {
        std::string example = summary.firstNotNumericText;
        if (example.size() > 60)
            example = example.substr(0, 57) + "...";
        return "Cell::stringEncoding(\"" + substateName + "\") did not return a number for " +
               std::to_string(summary.notNumeric) + " of " + std::to_string(summary.cellsChecked) +
               " checked cells, e.g. '" + example +
               "'. The model must return a plain number for the name of a substate.";
    }
    return {};
}
} // namespace SubstateString
