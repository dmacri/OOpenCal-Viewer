#pragma once

#include <cstdint>
#include <string>

#include "core/types.h"

/** Minimal cell type satisfying the Viewer's cell concept, shared by the reader tests. */
class TestCell
{
public:
    void composeElement(char* text)
    {
        value = std::stoi(text);
    }

    std::string stringEncoding(const char* = nullptr) const
    {
        return std::to_string(value);
    }

    Color outputValue(const char*, GlobalValueManager*) const
    {
        return Color(static_cast<std::uint8_t>(value), 0, 0);
    }

    void startStep(int)
    {
    }

    int value = 0;
};
