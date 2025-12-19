/** @file types.h
 * @brief Common type definitions used throughout the application. */

#pragma once

/** @brief Type alias for step index in simulation/visualization.
 *
 *  Represents a discrete step or frame index in the simulation or visualization pipeline.
 *  Uses unsigned integer to disallow for negative indices. */
using StepIndex = unsigned;

/** @brief Type alias for node index in a graph or tree structure.
 *
 * Used to uniquely identify nodes in various data structures.
 * Uses unsigned integer to disallow for negative indices. */
using NodeIndex = unsigned;

/** @brief Type alias for file position or offset.
 *
 * Used for file I/O operations to represent positions or sizes within files.
 * Uses long long to handle large files.
 * @note: The type should be (according to standard) `std::streampos`, but the type can not be used in streams */
using FilePosition = long long;

/** @struct ColumnAndRow
 * @brief Represents a 2D coordinate system using column and row indices.
 *
 * This structure is used to represent positions in a 2D grid or matrix.
 * It provides convenience methods for coordinate manipulation. */
struct ColumnAndRow
{
    /// @brief Type used for coordinate values.
    using CoordinateType = int;

    CoordinateType column; ///< Zero-based column index (x-coordinate)
    CoordinateType row;    ///< Zero-based row index (y-coordinate)

    /** @brief Creates a ColumnAndRow instance from x and y coordinates.
     * @param x The column/x-coordinate
     * @param y The row/y-coordinate
     * @return A new ColumnAndRow instance */
    static ColumnAndRow xy(CoordinateType x, CoordinateType y)
    {
        return ColumnAndRow{ .column = x, .row = y };
    }

    /// @brief Gets the x-coordinate (same as column)
    auto x() const
    {
        return column;
    }

    /// @brief Gets the y-coordinate (same as row)
    auto y() const
    {
        return row;
    }
};
