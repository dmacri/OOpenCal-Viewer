#include "core/ApplicationLocale.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <clocale>
#include <locale>
#include <stdexcept>
#include <system_error>

namespace viz::appLocale
{

std::optional<char> decimalPointOf(const std::string& name)
{
    try
    {
        // std::locale(name) throws when the C library does not know the locale; unlike setlocale() it does not change
        // anything in the process, so it is safe to ask about every locale in a list
        const std::locale locale(name.c_str());
        return std::use_facet<std::numpunct<char>>(locale).decimal_point();
    }
    catch (const std::runtime_error&)
    {
        return std::nullopt;
    }
}

bool apply(const std::string& name, std::string* errorMessage)
{
    if (std::setlocale(LC_NUMERIC, name.c_str()) != nullptr)
        return true;

    if (errorMessage)
    {
        *errorMessage = "The locale \"" + (name.empty() ? std::string("system default") : name)
                        + "\" is not installed on this computer.";
    }
    return false;
}

std::string activeName()
{
    const char* name = std::setlocale(LC_NUMERIC, nullptr);
    return name ? name : defaultLocaleName;
}

double readLikeStrtod(std::string_view text, char decimalPoint)
{
    std::size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])))
        ++first;

    // Translate to the C format, which std::from_chars reads: the decimal separator of the locale becomes '.', and a '.'
    // which is NOT the separator becomes a character which ends the number (that is what strtod does with it)
    std::string cFormat;
    cFormat.reserve(text.size() - first);
    for (std::size_t i = first; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == decimalPoint)
            cFormat.push_back('.');
        else if (c == '.')
            cFormat.push_back('?');
        else
            cFormat.push_back(c);
    }

    std::string_view view = cFormat;
    if (! view.empty() && view.front() == '+') // from_chars does not take a plus sign, strtod does
        view.remove_prefix(1);

    double value = 0.0;
    const auto [end, error] = std::from_chars(view.data(), view.data() + view.size(), value);
    (void)end;
    return error == std::errc{} ? value : 0.0;
}

std::string writeLikePrintf(double value, int digitsAfterPoint, char decimalPoint)
{
    char buffer[128];
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, digitsAfterPoint);
    if (error != std::errc{})
        return {};

    std::string text(buffer, end);
    std::replace(text.begin(), text.end(), '.', decimalPoint);
    return text;
}

} // namespace viz::appLocale
