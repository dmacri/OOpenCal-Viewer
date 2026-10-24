/** @file StepTimeMap.h
 *  @brief Optional mapping "simulation step -> simulated time in seconds".
 *
 *  A model may write an extra text file `<output_file_name>-time.txt` next to the other output files
 *  (the same place where `<output_file_name>-red.txt` lives, e.g. `OutputSciddicaT/sciddicaTout-time.txt`). When the file exists the viewer shows the
 *  simulated time next to the step number; when it does not, only the step is shown.
 *
 *  File format (plain text, one saved step per line):
 *  @code
 *  # step  time[s]        <- '#' starts a comment, blank lines are ignored
 *  0       0
 *  500     2500
 *  1000    5000.5
 *  @endcode
 *  - the first column is the step number (as used in the output file names / `*_index.txt`),
 *  - the second column is the simulated time elapsed at that step, in seconds (may be fractional),
 *  - the columns are separated by whitespace, further columns are ignored (room for e.g. delta_t),
 *  - '.' and ',' are both accepted as the decimal separator, so the locale of the model does not matter.
 *
 *  The class is Qt-free (so it can be unit tested without Qt/VTK). */
#pragma once

#include <filesystem>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>

#include "core/types.h"

class StepTimeMap
{
public:
    /// Suffix appended to `output_file_name` from Header.txt to get the name of the time file.
    static constexpr const char* fileSuffix = "-time.txt";

    /** @brief Builds the path of the time file for a model.
     *  @param outputFilePath Output file path prefix as resolved by the viewer (directory + `output_file_name`
     *         from Header.txt, e.g. "/data/OutputSciddicaT/sciddicaTout")
     *  @return `outputFilePath` + fileSuffix, i.e. the time file lies next to the `-red.txt` file */
    static std::filesystem::path pathForOutput(const std::string& outputFilePath)
    {
        return std::filesystem::path(outputFilePath + fileSuffix);
    }

    /// Creates an empty (not available) map.
    StepTimeMap() = default;

    /** @brief Loads the map from a file.
     *  @note A missing file is not an error (the file is optional): the map is just not available and
     *        errorMessage() stays empty. A malformed file makes the map unavailable and sets errorMessage(). */
    explicit StepTimeMap(const std::filesystem::path& file);

    /// Parses the map from a stream. The same rules as for the file constructor apply.
    explicit StepTimeMap(std::istream& stream);

    /// True when the file was read successfully and contains at least one step.
    bool isAvailable() const
    {
        return !timeByStep.empty();
    }

    /// Simulated time (seconds) at the given step, or std::nullopt when the step is not in the file.
    std::optional<double> secondsForStep(StepIndex step) const;

    /// Number of steps for which the time is known.
    std::size_t size() const
    {
        return timeByStep.size();
    }

    /// Description of the problem found while loading (empty when there was none).
    const std::string& errorMessage() const
    {
        return error;
    }

    /// Path of the file the map was loaded from (empty when it was not loaded from a file).
    const std::filesystem::path& filePath() const
    {
        return path;
    }

private:
    void parse(std::istream& stream);

    std::map<StepIndex, double> timeByStep;
    std::string error;
    std::filesystem::path path;
};
