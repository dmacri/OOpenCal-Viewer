/** @file BenchmarkStatistics.h
 *  @brief Qt/VTK-free statistics of the times measured by the --benchmark command-line mode.
 *
 *  A step shown in the viewer consists of four phases, which are measured separately, because a faster way of
 *  drawing the grid (the Rendering menu) changes only some of them:
 *
 *  - read       reading the files of the step and parsing them into the cells,
 *  - coloring   CPU time of colouring the cells: calculating the colour of every cell and writing it into the
 *               lookup table (Points, Cells) or into the pixels of the texture (Texture),
 *  - vtkUpdate  the rest of updating the VTK scene, e.g. rebuilding of the pipeline after the colours changed,
 *  - render     drawing the scene, including the wait until the GPU has finished. */

#pragma once

#include <algorithm>
#include <cstddef>
#include <format>
#include <string>
#include <vector>

namespace Benchmark
{

/// @brief Times of one step in milliseconds.
struct StepTimes
{
    double readMs = 0.0;
    double coloringMs = 0.0;
    double vtkUpdateMs = 0.0;
    double renderMs = 0.0;

    double totalMs() const
    {
        return readMs + coloringMs + vtkUpdateMs + renderMs;
    }
};

/// @brief Basic statistics of the times of one phase (all zeros when there were no samples).
struct Summary
{
    double mean = 0.0;
    double median = 0.0;
    double min = 0.0;
    double max = 0.0;
};

/// @brief Mean, median (the mean of the two middle values for an even number of samples), minimum and maximum.
inline Summary summarize(std::vector<double> samples)
{
    if (samples.empty())
        return {};

    std::ranges::sort(samples);
    const std::size_t n = samples.size();

    double sum = 0.0;
    for (const double sample : samples)
        sum += sample;

    const double median = (n % 2 == 1) ? samples[n / 2] : (samples[n / 2 - 1] + samples[n / 2]) / 2.0;
    return {sum / static_cast<double>(n), median, samples.front(), samples.back()};
}

/// @brief Statistics of every phase and of the whole step.
struct PhaseSummaries
{
    std::size_t steps = 0; ///< number of measured steps
    Summary read;
    Summary coloring;
    Summary vtkUpdate;
    Summary render;
    Summary total;
};

inline PhaseSummaries summarize(const std::vector<StepTimes>& steps)
{
    std::vector<double> read, coloring, vtkUpdate, render, total;
    for (const auto& step : steps)
    {
        read.push_back(step.readMs);
        coloring.push_back(step.coloringMs);
        vtkUpdate.push_back(step.vtkUpdateMs);
        render.push_back(step.renderMs);
        total.push_back(step.totalMs());
    }
    return {steps.size(), summarize(read), summarize(coloring), summarize(vtkUpdate), summarize(render), summarize(total)};
}

/// @brief The statistics as a text table: one row per phase, columns median / mean / min / max in milliseconds.
inline std::string formatTable(const PhaseSummaries& summaries)
{
    std::string table = std::format("{:<26}{:>12}{:>12}{:>12}{:>12}\n", "phase [ms]", "median", "mean", "min", "max");
    const auto addRow = [&table](const char* name, const Summary& summary)
    {
        table += std::format("{:<26}{:>12.2f}{:>12.2f}{:>12.2f}{:>12.2f}\n", name, summary.median, summary.mean, summary.min, summary.max);
    };
    addRow("read", summaries.read);
    addRow("coloring (CPU)", summaries.coloring);
    addRow("VTK update", summaries.vtkUpdate);
    addRow("render (GPU finished)", summaries.render);
    addRow("total", summaries.total);
    return table;
}

} // namespace Benchmark
