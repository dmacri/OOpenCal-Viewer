/** @file NumberFormatting.h
 *  @brief Qt-free helpers for presenting numbers produced by model code in the UI.
 *
 *  Cell classes print substate values with printf-style formats such as "%0.6f", and the
 *  decimal separator follows the process locale (Qt sets it from the environment), so the
 *  viewer sees both "1008.000000" and "1008,000000". */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace NumberFormatting
{
/** @brief Drops a fractional part that consists only of zeros.
 *
 *  "1008,000000" -> "1008", "-3.0" -> "-3", "-0,000000" -> "0".
 *  Anything else is returned unchanged: numbers with a significant fractional digit
 *  ("0,500000", "12.250000"), integers, exponent notation, and any text that is not a single
 *  plain decimal number (e.g. a composite value like "[1,000000,2,000000]").
 *
 *  Both '.' and ',' are accepted as the decimal separator, so the result does not depend on
 *  the locale the process runs in. */
inline std::string withoutZeroFraction(std::string_view text)
{
    std::size_t pos = 0;
    if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
        ++pos;

    const std::size_t integerStart = pos;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9')
        ++pos;
    if (pos == integerStart)
        return std::string{text};                     // no integer digits

    const std::size_t integerEnd = pos;
    if (pos >= text.size() || (text[pos] != '.' && text[pos] != ','))
        return std::string{text};                     // already an integer (or not a number)

    ++pos;
    const std::size_t fractionStart = pos;
    while (pos < text.size() && text[pos] == '0')
        ++pos;
    if (pos == fractionStart || pos != text.size())
        return std::string{text};                     // no fraction digits, or a non-zero/other char

    std::string result{text.substr(0, integerEnd)};
    if (result == "-0" || result == "+0")             // "-0,000000" would otherwise show as "-0"
        result.erase(0, 1);
    return result;
}

namespace detail
{
/// True for "[+-]digits[.digits]" without leading zeros in the integer part ("000000" is rejected).
inline bool isPlainNumberWithDotOrNone(std::string_view token)
{
    std::size_t pos = 0;
    if (pos < token.size() && (token[pos] == '+' || token[pos] == '-'))
        ++pos;
    const std::size_t intStart = pos;
    while (pos < token.size() && token[pos] >= '0' && token[pos] <= '9')
        ++pos;
    const std::size_t intLen = pos - intStart;
    if (intLen == 0)
        return false;
    if (intLen > 1 && token[intStart] == '0')
        return false;                                 // e.g. "000000": a fraction cut off by a decimal comma
    if (pos == token.size())
        return true;
    if (token[pos] != '.')
        return false;
    ++pos;
    const std::size_t fracStart = pos;
    while (pos < token.size() && token[pos] >= '0' && token[pos] <= '9')
        ++pos;
    return pos == token.size() && pos > fracStart;
}
} // namespace detail

/** @brief Tidies up a bracketed list of numbers: "[1008.000000,0.500000]" -> "[1008, 0.500000]".
 *
 *  Every element loses a zero-only fractional part (see withoutZeroFraction()) and the elements
 *  are separated by ", ". This is what Cell::stringEncoding() returns when no substate name is given.
 *
 *  The text is returned unchanged unless it is exactly "[n1,n2,...]" where every element is a plain
 *  number using '.' as decimal separator. The ',' is also the list separator, so a list printed with a
 *  decimal comma ("[1008,5,0,25]") cannot be split reliably; callers should only use this function when
 *  the process locale uses '.' as decimal point. */
inline std::string withoutZeroFractionsInList(std::string_view text)
{
    if (text.size() < 3 || text.front() != '[' || text.back() != ']')
        return std::string{text};

    const std::string_view inner = text.substr(1, text.size() - 2);
    std::vector<std::string_view> elements;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t comma = inner.find(',', start);
        const std::string_view element = inner.substr(start, comma == std::string_view::npos ? comma : comma - start);
        if (!detail::isPlainNumberWithDotOrNone(element))
            return std::string{text};
        elements.push_back(element);
        if (comma == std::string_view::npos)
            break;
        start = comma + 1;
    }

    std::string result{"["};
    for (std::size_t i = 0; i < elements.size(); ++i)
    {
        if (i != 0)
            result += ", ";
        result += withoutZeroFraction(elements[i]);
    }
    result += ']';
    return result;
}
} // namespace NumberFormatting
