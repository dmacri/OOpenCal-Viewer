/** @file ReductionManager.h
 * @brief Declaration of the ReductionManager class for managing reduction data.
 */

#pragma once

#include <QString>
#include <map>
#include <vector>

/** @struct ReductionData
 * @brief Holds reduction values for a single step. */
struct ReductionData
{
    std::map<QString, QString> values; ///< Map of reduction type to value (e.g., "sum" -> "12057")
};

/** @class ReductionManager
 * @brief Manages loading and accessing reduction data from files.
 *
 * This class handles reading reduction data from {outputFileName}-red.txt files
 * and provides access to reduction values for specific steps. */
class ReductionManager
{
public:
    /** @brief Constructs a ReductionManager.
     *  @param reductionFilePath Path to the reduction file (e.g., "ball-red.txt")
     *  @param reductionConfig Comma-separated reduction types (e.g., "sum,min,max") */
    ReductionManager(const QString& reductionFilePath, const QString& reductionConfig);

    /** @brief Checks if reduction data is available.
     *  @return True if reduction file was loaded successfully */
    bool isAvailable() const { return dataLoaded; }

    /** @brief Gets reduction data for a specific step.
     *  @param stepNumber The step number
     *  @return ReductionData containing values for this step, or empty if not found */
    ReductionData getReductionForStep(int stepNumber) const;

    /** @brief Gets a formatted string with reduction values for display.
     *  @param stepNumber The step number
     *  @return Formatted string like "sum=12057, min=1, max=1" or empty if not available */
    QString getFormattedReductionString(int stepNumber) const;

    /** @brief Gets error message if loading failed.
     *  @return Error message or empty string if no error */
    QString getErrorMessage() const { return errorMessage; }

private:
    /** @brief Loads reduction data from file.
     *  @param filePath Path to the reduction file
     *  @param reductionConfig Comma-separated reduction types */
    void loadReductionData(const QString& filePath, const QString& reductionConfig);

    /** @brief Parses a single line from the reduction file.
     *  @param line The line to parse (e.g., "0  sum=12057,min=1,max=1")
     *  @param stepNumber Output parameter for the step number
     *  @param values Output parameter for the parsed values
     *  @return True if parsing was successful */
    bool parseLine(const QString& line, int& stepNumber, std::map<QString, QString>& values) const;

    std::map<int, ReductionData> reductionDataByStep; ///< Map of step number to reduction data
    bool dataLoaded = false;                          ///< Flag indicating if data was loaded successfully
    QString errorMessage;                             ///< Error message if loading failed
    std::vector<QString> expectedReductions;          ///< Expected reduction types (e.g., ["sum", "min", "max"])
};
