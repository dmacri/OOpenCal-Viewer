/** @file PerformanceMetrics.h
 * @brief Performance metrics collection system for model loading and processing.
 *
 * This file provides classes for tracking performance metrics such as timing and cell counts
 * during model loading operations. Designed to be compatible with C++23. */

#pragma once

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <mutex>
#include <format>

/** @class ScopedTimer
 * @brief RAII-based timer for measuring duration of code blocks.
 * 
 * Automatically records the elapsed time when the object goes out of scope. */
class ScopedTimer
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = std::chrono::duration<double, std::milli>; // milliseconds

    explicit ScopedTimer(const std::string& label, std::vector<Duration>* timings = nullptr)
        : m_label(label), m_timings(timings), m_start(Clock::now())
    {
    }

    ~ScopedTimer()
    {
        const auto end = Clock::now();
        const Duration elapsed = end - m_start;
        
        if (m_timings)
        {
            m_timings->push_back(elapsed);
        }
    }

    // Prevent copying and moving
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    std::string m_label;
    std::vector<Duration>* m_timings;
    Clock::time_point m_start;
};

/** @class PerformanceMetrics
 * @brief Singleton class for collecting and reporting performance metrics.
 * 
 * Tracks loading times for different data sizes and cell counts across the application. */
class PerformanceMetrics
{
public:
    using Duration = ScopedTimer::Duration;

    struct LoadingEntry
    {
        std::string description;       ///< Operation description (e.g., "Model Loading", "Module Compilation")
        std::string gridSize;          ///< Grid dimensions (e.g., "100x100") - empty if not applicable
        uint32_t timesteps = 0;        ///< Number of timesteps loaded (0 if not applicable)
        uint32_t stepNumber = 0;       ///< Current step number being loaded (optional, for step loading)
        size_t totalCellsRead = 0;     ///< Total cells read in this loading session
        Duration totalLoadingTime{0};  ///< Total loading time
        uint32_t callCount = 0;        ///< Number of read calls
    };

    static PerformanceMetrics& instance()
    {
        static PerformanceMetrics singleton;
        return singleton;
    }

    // Prevent copying and moving
    PerformanceMetrics(const PerformanceMetrics&) = delete;
    PerformanceMetrics& operator=(const PerformanceMetrics&) = delete;
    PerformanceMetrics(PerformanceMetrics&&) = delete;
    PerformanceMetrics& operator=(PerformanceMetrics&&) = delete;

    ~PerformanceMetrics()
    {
        // Print summary report when singleton is destroyed (at program exit)
        printSummaryReport();
    }

    /** @brief Enable or disable performance metrics collection and reporting.
     *
     * @param enabled true to enable metrics, false to disable */
    static void setEnabled(bool enabled)
    {
        instance().m_enabled = enabled;
    }

    /** @brief Check if performance metrics are enabled.
     *
     * @return true if metrics are enabled */
    static bool isEnabled()
    {
        return instance().m_enabled;
    }

    /** @brief Record a completed metric entry (e.g., from PerformanceSession).
     *
     * @param entry The LoadingEntry with all collected metrics */
    void recordEntry(const LoadingEntry& entry)
    {
        if (!m_enabled)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (entry.callCount > 0 || entry.totalLoadingTime.count() > 0)
        {
            m_sessions.push_back(entry);
        }
    }

    /** @brief Print a comprehensive performance report. */
    void printReport(const LoadingEntry& entry) const
    {
        if (!isEnabled() || (entry.callCount == 0 && entry.totalLoadingTime.count() == 0))
            return;

        const double avgTimePerCall = entry.callCount > 0 ? 
            entry.totalLoadingTime.count() / entry.callCount : 0.0;
        const double throughput = entry.totalLoadingTime.count() > 0 ?
            entry.totalCellsRead / entry.totalLoadingTime.count() : 0.0;

        // Build description with optional step number
        std::string fullDescription = entry.description;
        if (entry.stepNumber > 0)
            fullDescription += std::format(" (Step {})", entry.stepNumber);

        std::cout << "\n[METRIC] " << fullDescription << "\n";
        
        // Only print Grid Size if not empty
        if (!entry.gridSize.empty())
            std::cout << "  Grid Size:          " << entry.gridSize << "\n";
        
        // Only print Timesteps if > 0
        if (entry.timesteps > 0)
            std::cout << "  Timesteps:          " << entry.timesteps << "\n";
        
        // Only print cell info if cells were read
        if (entry.totalCellsRead > 0)
        {
            std::cout << "  Total Cells Read:   " << entry.totalCellsRead << "\n";
            // Only print Function Calls if more than 1 call
            if (entry.callCount > 1)
                std::cout << "  Function Calls:     " << entry.callCount << "\n";
        }
        
        std::cout << "  Total Time:         " << std::fixed << std::setprecision(3) 
                 << entry.totalLoadingTime.count() << " ms\n";
        
        // Only print Avg Time/Call if multiple calls (avoid redundant info)
        if (entry.callCount > 1)
            std::cout << "  Avg Time/Call:      " << std::fixed << std::setprecision(3) 
                     << avgTimePerCall << " ms\n";
        
        // Only print Throughput if cells were read
        if (entry.totalCellsRead > 0)
            std::cout << "  Throughput:         " << std::fixed << std::setprecision(0) 
                     << throughput << " cells/ms\n";
    }

private:
    /** @brief Print overall summary of all sessions at program exit. */
    void printSummaryReport() const
    {
        if (!m_enabled)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_sessions.empty())
            return;

        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "PERFORMANCE METRICS SUMMARY - All Sessions\n";
        std::cout << std::string(80, '=') << std::endl;

        size_t totalCells = 0;
        uint32_t totalCalls = 0;
        Duration totalTime{0};

        for (const auto& entry : m_sessions)
        {
            totalCells += entry.totalCellsRead;
            totalCalls += entry.callCount;
            totalTime += entry.totalLoadingTime;
        }

        std::cout << "\nTotal Sessions:         " << m_sessions.size() << "\n";
        if (totalCells > 0)
            std::cout << "Total Cells Processed:  " << totalCells << "\n";
        if (totalCalls > 0)
            std::cout << "Total Function Calls:   " << totalCalls << "\n";
        std::cout << "Total Execution Time:   " << std::fixed << std::setprecision(3) 
                 << totalTime.count() << " ms\n";

        if (totalCells > 0 && totalTime.count() > 0)
        {
            const double cellsPerMs = totalCells / totalTime.count();
            std::cout << "Overall Throughput:     " << std::fixed << std::setprecision(0) 
                     << cellsPerMs << " cells/ms\n";
        }

        std::cout << std::string(80, '=') << "\n" << std::endl;
    }

    PerformanceMetrics() = default;

    mutable std::mutex m_mutex;
    bool m_enabled = true;  ///< Enable/disable metrics collection
    std::vector<LoadingEntry> m_sessions;
};

/** @class PerformanceSession
 * @brief RAII wrapper for tracking a single performance session.
 * 
 * Automatically records session metrics and prints report when destroyed. */
class PerformanceSession
{
public:
    /// Constructor for simple operations (without grid dimensions)
    explicit PerformanceSession(const std::string& description)
        : m_entry(), m_startTime(ScopedTimer::Clock::now())
    {
        m_entry.description = description;
    }

    /// Constructor for grid-based operations with dimensions
    explicit PerformanceSession(const std::string& description, 
                               uint32_t gridWidth, uint32_t gridHeight, 
                               uint32_t gridDepth = 1, uint32_t timestepCount = 0)
        : m_entry(), m_startTime(ScopedTimer::Clock::now())
    {
        m_entry.description = description;
        if (gridWidth > 0 && gridHeight > 0)
            m_entry.gridSize = std::format("{}x{}x{}", gridWidth, gridHeight, gridDepth);
        m_entry.timesteps = timestepCount;
    }

    ~PerformanceSession()
    {
        // Calculate total time
        const auto endTime = ScopedTimer::Clock::now();
        m_entry.totalLoadingTime = endTime - m_startTime;

        // Print individual session report
        PerformanceMetrics::instance().printReport(m_entry);

        // Record in global metrics
        PerformanceMetrics::instance().recordEntry(m_entry);
    }

    // Prevent copying and moving
    PerformanceSession(const PerformanceSession&) = delete;
    PerformanceSession& operator=(const PerformanceSession&) = delete;
    PerformanceSession(PerformanceSession&&) = delete;
    PerformanceSession& operator=(PerformanceSession&&) = delete;

    /** @brief Record a read operation within this session.
     *
     * @param cellsRead Number of cells read */
    void recordReadCall(size_t cellsRead)
    {
        m_entry.totalCellsRead += cellsRead;
        m_entry.callCount++;
    }

    /** @brief Set the step number for this session (e.g., for timestep loading).
     *
     * @param stepNum The step number being loaded */
    void setStepNumber(uint32_t stepNum)
    {
        m_entry.stepNumber = stepNum;
    }

private:
    PerformanceMetrics::LoadingEntry m_entry;
    ScopedTimer::Clock::time_point m_startTime;
};
