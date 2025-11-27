/** @file SubstateInfo.h
 * @brief Declaration of the SubstateInfo structure for each substate field including display parameters */

#pragma once

#include <string>
#include <limits>

/** @struct SubstateInfo
 * @brief Information about a single substate field including display parameters. */
struct SubstateInfo
{
    std::string name;                                           ///< Field name (e.g., "h", "z")
    std::string format = "";                                    ///< Printf format string (user-editable, empty if not set)
    double minValue = std::numeric_limits<double>::quiet_NaN(); ///< Minimum value for display (user-editable, NaN if not set)
    double maxValue = std::numeric_limits<double>::quiet_NaN(); ///< Maximum value for display (user-editable, NaN if not set)
    double noValue = std::numeric_limits<double>::quiet_NaN();  ///< Value representing "no data" (NaN if not set)
    std::string minColor = "";                                  ///< Hex color for minimum value (e.g., "#000011", empty if not set)
    std::string maxColor = "";                                  ///< Hex color for maximum value (e.g., "#0011ff", empty if not set)
};
