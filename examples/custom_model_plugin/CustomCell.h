#pragma once

#include <algorithm> // std::max, std::min
#include <charconv>  // std::from_chars
#include <cstring>
#include <format>
#include <limits>
#include <stdexcept> // std::invalid_argument

#include "OOpenCAL/base/Cell.h"

/** CustomCell - Example custom model for OOpenCal-Viewer plugin system
 * 
 * This is a simple example showing how to create a custom cell model
 * that can be loaded as a plugin without recompiling the main application.
 * 
 * This model stores a single integer value and displays it with a color
 * gradient from blue (low values) to red (high values). */
class CustomCell : public Cell
{
    int value;

public:
    OPENCALF CustomCell()
        : value(0)
    {
    }

    OPENCALF explicit CustomCell(int val)
        : value(val)
    {
    }

    OPENCALF void setValue(int v)
    {
        value = v;
    }

    OPENCALF int getValue() const
    {
        return value;
    }

    /// Parse a string representation and set the cell value
    void composeElement(char* str) override
    {
        if (! str || str[0] == '\0')
        {
            this->value = 0;
            throw std::invalid_argument("Provided empty input!");
        }

        int result = 0;
        auto [ptr, ec] = std::from_chars(str, str + std::strlen(str), result);

        if (ec == std::errc::invalid_argument)
        {
            throw std::invalid_argument("Provided not number '" + std::string(str) + "'");
            this->value = 0;
        }
        else if (ec == std::errc::result_out_of_range)
        {
            throw std::invalid_argument(std::format("Provided number '{}' is out of int range [{}, {}]",
                                                    str,
                                                    std::numeric_limits<int>::min(),
                                                    std::numeric_limits<int>::max()));
            this->value = (result > 0) ? std::numeric_limits<int>::max() : std::numeric_limits<int>::min();
        }
        else // success
        {
            this->value = result;
        }
    }

    /// Convert cell state to string representation
    std::string stringEncoding(const char*) const override
    {
        return std::to_string(value);
    }

    /** Determine the output color based on the cell value
     * Blue (0) -> Cyan -> Green -> Yellow -> Red (255) */
    Color outputValue(const char* /*str*/, GlobalValueManager* /*gvm*/) const override
    {
        Color outputColor{ 128, 128, 128 };

        // Normalize value to 0-1 range (assuming 0-255 input)
        double normalized = value / 255.0;
        normalized = std::max(0.0, std::min(1.0, normalized));

        if (normalized < 0.25)
        {
            // Blue -> Cyan
            double t = normalized * 4.0;
            outputColor = Color(0, static_cast<int>(255 * t), 255);
        }
        else if (normalized < 0.5)
        {
            // Cyan -> Green
            double t = (normalized - 0.25) * 4.0;
            outputColor = Color(0, 255, static_cast<int>(255 * (1 - t)));
        }
        else if (normalized < 0.75)
        {
            // Green -> Yellow
            double t = (normalized - 0.5) * 4.0;
            outputColor = Color(static_cast<int>(255 * t), 255, 0);
        }
        else
        {
            // Yellow -> Red
            double t = (normalized - 0.75) * 4.0;
            outputColor = Color(255, static_cast<int>(255 * (1 - t)), 0);
        }

        return outputColor;
    }

    /// Called at the start of each simulation step
    void startStep(int /*step*/) override
    {
        // Custom initialization for each step if needed
    }
};
