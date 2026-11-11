/** @file ApplicationLocale.h
 * @brief The number locale (LC_NUMERIC) of the whole Viewer process.
 *
 * The model classes (Cell) are compiled into plugins which run *inside* the Viewer process, so everything they do with
 * printf / scanf / strtod / std::stod / std::to_string follows the LC_NUMERIC category of the process. It decides whether
 * a value is written and read as "1008.5" or "1008,5". The same holds for the values which the Viewer itself reads with
 * std::stod (the substate lines of Header.txt, the text returned by Cell::stringEncoding()).
 *
 * Only LC_NUMERIC is changed, on purpose: the global std::locale of C++ streams is not touched, because the streams of the
 * Viewer would then start to write thousands separators and decimal commas. The functions here do not depend on Qt. */
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace viz::appLocale
{

/// @brief The locale which is used until the user chooses another one: numbers with a decimal point ("1008.5")
inline constexpr const char* defaultLocaleName = "C";

/// @brief The "system" choice: the locale from the environment (LC_ALL, LC_NUMERIC, LANG)
inline constexpr const char* systemLocaleName = "";

/** @brief Decimal separator of a locale.
 *  @param name locale name as known to the C library, e.g. "C", "pl_PL.UTF-8" or "" (= the locale of the environment)
 *  @return the separator, or std::nullopt when the locale is not installed on this computer */
std::optional<char> decimalPointOf(const std::string& name);

/** @brief Sets LC_NUMERIC of the whole process.
 *  @param name locale name, see decimalPointOf()
 *  @param errorMessage receives a readable message when the locale cannot be set (may be nullptr)
 *  @return true when the locale is active now; false when it is not installed (the previous locale stays active) */
bool apply(const std::string& name, std::string* errorMessage = nullptr);

/// @brief Name of the locale which is active now for numbers (e.g. "C" or "pl_PL.UTF-8")
std::string activeName();

/** @brief The number which strtod() returns for the text in a locale with the given decimal separator.
 *
 *  strtod() reads the longest number at the start of the text and ignores the rest, so in a locale with a decimal
 *  comma "1008.5" is read as 1008 and "1008,5" as 1008.5. Locale independent, so it can be used to show the user what
 *  the model code is going to do before the locale is applied.
 *  @return the number, 0 when the text does not start with a number */
double readLikeStrtod(std::string_view text, char decimalPoint);

/// @brief The text which printf("%.<digitsAfterPoint>f") writes for the value in a locale with the given decimal separator
std::string writeLikePrintf(double value, int digitsAfterPoint, char decimalPoint);

} // namespace viz::appLocale
