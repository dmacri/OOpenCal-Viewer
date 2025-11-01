/** @file SettingParameter.h
 * @brief Declaration of the SettingParameter structure for visualization settings. */

#pragma once

#include <iosfwd>
#include <string>

#include "utilities/types.h"

/** @struct SettingParameter
 * @brief Contains parameters for visualization settings and simulation configuration.
 * 
 * This structure holds various parameters that control the visualization and
 * behavior of the simulation. */
struct SettingParameter
{
    StepIndex step;             ///< Current simulation step
    StepIndex nsteps;           ///< Total number of simulation steps
    int numberOfColumnX;        ///< Number of columns in the simulation grid
    int numberOfRowsY;          ///< Number of rows in the simulation grid
    NodeIndex nNodeX;           ///< Number of nodes in X direction
    NodeIndex nNodeY;           ///< Number of nodes in Y direction
    int numberOfLines;          ///< Total number of lines in the visualization
    std::string outputFileName; ///< Name of the output file
    std::string readMode;       ///< File read mode: "text" or "binary"
    std::string substates;      ///< Substates to read (e.g., "h,z")
    std::string reduction;      ///< Reduction operations (e.g., "sum,min,max")

    static constexpr int font_size = 18; ///< Font size for text rendering

    bool changed; ///< Flag indicating if settings have been modified and currently visible state should be redrown

    /// @brief Printing SettingParameter to output stream
    friend std::ostream& operator<<(std::ostream& os, const SettingParameter& sp);
};
