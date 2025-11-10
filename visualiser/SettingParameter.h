/** @file SettingParameter.h
 * @brief Declaration of the SettingParameter structure for visualization settings. */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <limits>

#include "utilities/types.h"

/** @struct SubstateInfo
 * @brief Information about a single substate field including display parameters. */
struct SubstateInfo
{
    std::string name;                                           ///< Field name (e.g., "h", "z")
    double minValue = std::numeric_limits<double>::quiet_NaN(); ///< Minimum value for display (user-editable, NaN if not set)
    double maxValue = std::numeric_limits<double>::quiet_NaN(); ///< Maximum value for display (user-editable, NaN if not set)
    std::string format = "";                                    ///< Printf format string (user-editable, empty if not set)
};

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
    
    /// @brief Map of substate information (name -> SubstateInfo) for display parameters
    std::map<std::string, SubstateInfo> substateInfo;

    static constexpr int font_size = 18; ///< Font size for text rendering

    bool changed; ///< Flag indicating if settings have been modified and currently visible state should be redrown

    /** @brief Parse substates string into a vector of field names.
     * 
     * Parses the substates string (e.g., "h,z" or "s") into individual field names.
     * Handles both simple format (comma-separated) and extended format with parameters.
     * 
     * @return Vector of field names (e.g., {"h", "z"}) or empty vector if substates is empty */
    std::vector<std::string> getSubstateFields() const;
    
    /** @brief Initialize substate information from parsed substates.
     *
     * Creates SubstateInfo entries for each field in substates. Should be called after substates string is set. */
    void initializeSubstateInfo();

    /// @brief Printing SettingParameter to output stream
    friend std::ostream& operator<<(std::ostream& os, const SettingParameter& sp);
};
