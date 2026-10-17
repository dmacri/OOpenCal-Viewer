/** @file CellHistory.h
 *  @brief Values of the substates of one cell over the whole simulation (Qt/VTK-free). */
#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/types.h"

/** @struct CellHistory
 *  @brief Time series of the substates of one cell: one value per available simulation step.
 *
 *  `values[f][i]` is the value of substate `fields[f]` at `steps[i]`. A value is NaN when the cell
 *  could not be read at that step, or when the substate is not a number. */
struct CellHistory
{
    /// Position of the cell in the grid (`slice` is 0 for 2D models)
    int row = 0;
    int column = 0;
    int slice = 0;

    /// @brief True for a model with more than one Z plane (then `slice` is a meaningful coordinate)
    bool threeDimensional = false;

    std::vector<StepIndex> steps;
    std::vector<std::string> fields;
    std::vector<std::vector<double>> values;

    /// @brief True when the reading was cancelled before all steps were read
    bool cancelled = false;

    [[nodiscard]] bool empty() const noexcept
    {
        return steps.empty() || fields.empty();
    }

    /// @brief Number of steps for which the given substate has a numeric value
    [[nodiscard]] std::size_t validCount(std::size_t fieldIndex) const
    {
        if (fieldIndex >= values.size())
            return 0;

        std::size_t count = 0;
        for (const double v : values[fieldIndex])
            count += std::isfinite(v) ? 1 : 0;
        return count;
    }
};

/** @brief Progress callback for reading a history: `done` of `total` steps are read.
 *  Return false to cancel the reading. */
using CellHistoryProgress = std::function<bool(std::size_t done, std::size_t total)>;

/** @brief Parses the text a cell gives for a substate (`Cell::stringEncoding`) as a number.
 *
 *  Independent of the process locale: both '.' and ',' are accepted as the decimal separator
 *  ("1008.000000" and "1008,000000" give the same number), because cells print numbers with the
 *  locale the process runs in.
 *
 *  @return the number, or NaN when the text is empty or is not entirely one number
 *          (e.g. a composite value like "[1,000000,2,000000]") */
[[nodiscard]] inline double parseSubstateValue(std::string_view text) noexcept
{
    constexpr double notANumber = std::numeric_limits<double>::quiet_NaN();

    while (! text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (! text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    if (text.empty())
        return notANumber;

    std::string normalized(text);
    if (normalized.front() == '+')
        normalized.erase(0, 1);

    // A single comma and no dot: the comma is the decimal separator of the locale
    if (normalized.find('.') == std::string::npos &&
        std::count(normalized.begin(), normalized.end(), ',') == 1)
    {
        std::ranges::replace(normalized, ',', '.');
    }

    double value = 0.0;
    const char* const begin = normalized.data();
    const char* const end = begin + normalized.size();
    const auto [stop, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || stop != end)
        return notANumber;
    return value;
}

/** @brief Writes a history as CSV: a `step` column and one column per substate.
 *
 *  Numbers are written in the C locale with full precision (so the file is the same on every
 *  machine); a value that is missing (NaN) is an empty cell. */
[[nodiscard]] inline std::string cellHistoryToCsv(const CellHistory& history)
{
    std::string csv = "step";
    for (const auto& field : history.fields)
        csv += "," + field;
    csv += "\n";

    char buffer[64];
    for (std::size_t i = 0; i < history.steps.size(); ++i)
    {
        csv += std::to_string(history.steps[i]);
        for (std::size_t f = 0; f < history.fields.size(); ++f)
        {
            csv += ',';
            if (f < history.values.size() && i < history.values[f].size() && std::isfinite(history.values[f][i]))
            {
                const auto result = std::to_chars(buffer, buffer + sizeof(buffer), history.values[f][i]);
                csv.append(buffer, result.ptr);
            }
        }
        csv += "\n";
    }
    return csv;
}
