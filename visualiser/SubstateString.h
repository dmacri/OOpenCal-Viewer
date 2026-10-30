/** @file SubstateString.h
 * @brief Building of the `substates=` entries of Header.txt and checking of the values a model returns for a substate.
 *
 * Plain C++ (no Qt, no VTK), so it can be tested without a display. Used by the "Add substate" dialog. */
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "SubstateInfo.h"

namespace SubstateString
{
/** @brief Entry of the `substates=` line for one substate, in the format read by SettingParameter::parseSubstates().
 *
 * Only what is set is written: `(name,%f)`, `(name,%f,noValue)`, `(name,%f,min,max)`, `(name,%f,min,max,noValue)` or
 * with colors `(name,%f,min,max,noValue,#minColor,#maxColor)`. The parser has fixed positions, so a value that is not
 * set but is followed by one that is set is written as `nan` (a color: `-`), which the parser reads back as "not set".
 * The no-value is written only when SubstateInfo::noValueEnabled is set. */
std::string formatEntry(const SubstateInfo& info);

/// @brief Value of the `substates=` line: the entries joined with commas (without the "substates=" prefix)
std::string formatEntries(const std::vector<SubstateInfo>& infos);

/** @brief Why `name` cannot be the name of a new substate.
 *  @param existingNames names of the substates which already exist
 *  @return text for the user, empty when the name is fine */
std::string validateName(const std::string& name, const std::vector<std::string>& existingNames);

/// @brief What the cells of a model return for one substate name (see summarizeValues())
struct ValueSummary
{
    std::size_t cellsChecked = 0;      ///< How many cells were asked
    std::size_t numeric = 0;           ///< Cells which returned a number (also nan and inf)
    std::size_t notNumeric = 0;        ///< Cells which returned something else (e.g. "[1.0,2.0]" or "")
    std::size_t usedForRange = 0;      ///< Finite numbers which are not the no-value (the min and max come from them)
    std::string firstNotNumericText;   ///< Text returned by the first cell which was not a number
    std::string firstNumericText;      ///< Text returned by the first cell which was a number
    double minValue = 0.0;             ///< Smallest of the used values (meaningful if usedForRange > 0)
    double maxValue = 0.0;             ///< Largest of the used values (meaningful if usedForRange > 0)
};

/** @brief Asks the cells for the value of a substate and reads the answers as numbers.
 *
 * A text is a number if the whole of it (spaces around are ignored) is a number, the same way for every locale.
 * This is stricter than std::stod(), which the Viewer uses for a cell and which accepts "1.5abc" as 1.5.
 *
 * @param cellCount number of cells in the model
 * @param stride 1 asks every cell, n asks every n-th (a quick check of a huge model); 0 is taken as 1
 * @param textOfCell text which Cell::stringEncoding(substateName) returns for the cell of the given index
 * @param noValue value which means "no data": such cells do not take part in the min and max */
ValueSummary summarizeValues(std::size_t cellCount,
                             std::size_t stride,
                             const std::function<std::string(std::size_t)>& textOfCell,
                             std::optional<double> noValue = std::nullopt);

/** @brief Explains in a sentence why the substate cannot be used, from the result of summarizeValues().
 *  @return empty if all is fine (every checked cell returned a number) */
std::string describeProblem(const ValueSummary& summary, const std::string& substateName);

/** @brief Reads a number from the whole text (spaces around are ignored), independent of the locale.
 *  @return the number, or nullopt if the text is not a number (empty, "[1,2]", "1.5abc", ...) */
std::optional<double> parseNumber(const std::string& text);

/// @brief The number as short text which gives the same number when read back (0.1 -> "0.1", -9999 -> "-9999")
std::string numberToText(double value);
} // namespace SubstateString
