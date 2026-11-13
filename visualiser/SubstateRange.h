/** @file SubstateRange.h
 *  @brief Minimum and maximum of a substate over a range of simulation steps (Qt/VTK-free). */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <vector>

#include "core/types.h"

/** @struct StepRange
 *  @brief Interval of simulation steps with both ends included. */
struct StepRange
{
    StepIndex first = 0; ///< First step of the interval
    StepIndex last = 0;  ///< Last step of the interval (not lower than first)

    /** @brief Number of step numbers in the interval, the ones which were not saved included. */
    [[nodiscard]] std::size_t length() const noexcept
    {
        return last >= first ? static_cast<std::size_t>(last) - first + 1 : 0;
    }

    /** @brief Whether the step lies in the interval. */
    [[nodiscard]] bool contains(StepIndex step) const noexcept
    {
        return step >= first && step <= last;
    }

    /** @brief Whether both intervals have the same ends. */
    [[nodiscard]] bool operator==(const StepRange& other) const noexcept = default;
};

namespace StepRanges
{
/** @brief Interval from the lowest to the highest of the steps; nothing for an empty list. */
inline std::optional<StepRange> boundsOf(const std::vector<StepIndex>& steps)
{
    if (steps.empty())
        return std::nullopt;

    const auto [lowest, highest] = std::ranges::minmax(steps);
    return StepRange{lowest, highest};
}

/** @brief Puts both ends of the wanted interval into the bounds, the first one never above the last one. */
inline StepRange clampedTo(StepRange wanted, StepRange bounds)
{
    const StepIndex last = std::clamp(wanted.last, bounds.first, bounds.last);
    const StepIndex first = std::min(std::clamp(wanted.first, bounds.first, bounds.last), last);
    return StepRange{first, last};
}

/** @brief The steps of the list which lie in the interval, in ascending order and without repetitions. */
inline std::vector<StepIndex> stepsWithin(const std::vector<StepIndex>& available, StepRange range)
{
    std::vector<StepIndex> inside;
    for (const StepIndex step : available)
    {
        if (range.contains(step))
            inside.push_back(step);
    }

    std::ranges::sort(inside);
    const auto repeated = std::ranges::unique(inside);
    inside.erase(repeated.begin(), repeated.end());
    return inside;
}

/** @brief How many step numbers of the interval are not among the available steps (e.g. a simulation saved every 10th step). */
inline std::size_t missingStepsCount(const std::vector<StepIndex>& available, StepRange range)
{
    return range.length() - stepsWithin(available, range).size();
}
} // namespace StepRanges

/** @brief Where the reading of the steps is: step `step` is the `stepNumber`-th of `stepsTotal`. */
struct SubstateRangeProgress
{
    StepIndex step = 0;            ///< Simulation step being read
    std::size_t stepNumber = 0;    ///< Position of the step among the steps to read, counted from 1
    std::size_t stepsTotal = 0;    ///< Number of steps to read
    double fraction = 0.0;         ///< Part of the whole work which is done, from 0 to 1
};

/** @brief Progress callback of the reading of the steps; return false from it to cancel. */
using SubstateRangeProgressCallback = std::function<bool(const SubstateRangeProgress&)>;

/** @struct SubstateRange
 *  @brief Lowest and highest value of one substate found in the cells of a number of steps. */
struct SubstateRange
{
    std::optional<double> minimum;       ///< Lowest value, nothing if there was no number to look at
    std::optional<double> maximum;       ///< Highest value, nothing if there was no number to look at
    std::size_t valuesCount = 0;         ///< How many numbers were taken into account
    std::size_t stepsRead = 0;           ///< How many steps were read and looked through completely
    std::vector<StepIndex> failedSteps;  ///< Steps which could not be read (they are not in the result)
    bool cancelled = false;              ///< The progress callback stopped the work: the result is partial and should not be used

    /** @brief Takes one value into account; a value which is not a finite number is ignored. */
    void add(double value)
    {
        if (! std::isfinite(value))
            return;

        minimum = minimum ? std::min(*minimum, value) : value;
        maximum = maximum ? std::max(*maximum, value) : value;
        ++valuesCount;
    }

    /** @brief Whether at least one number was found. */
    [[nodiscard]] bool hasValues() const noexcept
    {
        return valuesCount > 0;
    }
};

/** @brief Goes through the steps one by one: reads each into the grid and looks through its cells in portions.
 *
 *  A step which cannot be read (readStep() throws) is put on SubstateRange::failedSteps and skipped. The portions
 *  ("units", e.g. a row of cells) are scanned with scanUnits(range, begin, end) for `[begin, end)`; their number
 *  gives the callback finer progress than the steps alone, and gives a chance to cancel in the middle of a big step.
 *
 *  @param steps         steps to read, in the order in which they are read
 *  @param unitsPerStep  how many portions the cells of one step are split into
 *  @param readStep      `void(StepIndex)`: loads the cells of the step
 *  @param scanUnits     `void(SubstateRange&, std::size_t begin, std::size_t end)`: adds the values of the portions
 *  @param progress      called before every step and after every portion; return false to cancel */
template<class ReadStep, class ScanUnits>
SubstateRange accumulateAcrossSteps(const std::vector<StepIndex>& steps,
                                    std::size_t unitsPerStep,
                                    ReadStep&& readStep,
                                    ScanUnits&& scanUnits,
                                    const SubstateRangeProgressCallback& progress = {});

template<class ReadStep, class ScanUnits>
SubstateRange accumulateAcrossSteps(const std::vector<StepIndex>& steps,
                                    std::size_t unitsPerStep,
                                    ReadStep&& readStep,
                                    ScanUnits&& scanUnits,
                                    const SubstateRangeProgressCallback& progress)
{
    SubstateRange result;
    if (steps.empty() || unitsPerStep == 0)
        return result;

    // Around a hundred reports per step are enough for a smooth bar; a small step is reported unit by unit
    const std::size_t portion = std::max<std::size_t>(1, unitsPerStep / 100);

    const auto report = [&](std::size_t index, std::size_t unitsDone)
    {
        if (! progress)
            return true;

        SubstateRangeProgress state;
        state.step = steps[index];
        state.stepNumber = index + 1;
        state.stepsTotal = steps.size();
        state.fraction = (static_cast<double>(index) + static_cast<double>(unitsDone) / static_cast<double>(unitsPerStep))
                         / static_cast<double>(steps.size());
        return progress(state);
    };

    for (std::size_t index = 0; index < steps.size(); ++index)
    {
        if (! report(index, 0))
        {
            result.cancelled = true;
            return result;
        }

        try
        {
            readStep(steps[index]);
        }
        catch (const std::exception&)
        {
            result.failedSteps.push_back(steps[index]);
            continue;
        }

        for (std::size_t begin = 0; begin < unitsPerStep; begin += portion)
        {
            const std::size_t end = std::min(begin + portion, unitsPerStep);
            scanUnits(result, begin, end);

            if (! report(index, end))
            {
                result.cancelled = true;
                return result;
            }
        }
        ++result.stepsRead;
    }
    return result;
}
