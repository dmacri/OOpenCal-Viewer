#pragma once

#include <concepts> // it requires C++20
#include <string>
#include <OOpenCAL/base/Cell.h>

/// @brief Concept verifying that a type satisfies the Cell interface
template <typename Cell>
concept CellLike = requires(Cell cell, char* str, const char* cstr, int step) {
    // must provide methods:
    { cell.composeElement(str) } -> std::same_as<void>;
    { cell.stringEncoding(cstr) } -> std::convertible_to<std::string>;
    { cell.outputValue(cstr) } -> std::convertible_to<Color>;
    { cell.startStep(step) } -> std::same_as<void>;
};
