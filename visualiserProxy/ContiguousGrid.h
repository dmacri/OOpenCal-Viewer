/** @file ContiguousGrid.h
 * @brief Lightweight contiguous grid storage with row and layer access.
 *
 * This is a temporary in-tree replacement for std::mdspan while the project
 * still needs to build with toolchains that do not provide it yet. */

#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

/** @class ContiguousGrid
 * @brief Stores grid data in a single contiguous std::vector.
 *
 * The container keeps a row-major layout with an optional third dimension.
 * The layer count defaults to 1 so the same type can back the current 2D code
 * and future 3D-oriented access patterns. */
template<typename T>
class ContiguousGrid
{
public:
    using value_type = T;
    using size_type = std::size_t;

    /** @class RowProxy
     * @brief Provides column-based access to a single row of the grid. */
    template<typename ElementType>
    class RowProxy
    {
    public:
        /** @brief Returns the number of columns available in this row. */
        [[nodiscard]] size_type size() const noexcept
        {
            return columnCount_;
        }

        /** @brief Returns the element from the first layer at the given column. */
        [[nodiscard]] decltype(auto) operator[](size_type column) const noexcept
        {
            assert(column < columnCount_);
            return (rowData_[column * layerCount_]);
        }

        /** @brief Returns the element at the given column and layer. */
        [[nodiscard]] decltype(auto) operator[](size_type column, size_type layer) const noexcept
        {
            assert(column < columnCount_);
            assert(layer < layerCount_);
            return (rowData_[column * layerCount_ + layer]);
        }

    private:
        friend class ContiguousGrid;

        /** @brief Creates a row proxy for internal contiguous storage access. */
        RowProxy(ElementType* rowData, size_type columnCount, size_type layerCount) noexcept
            : rowData_(rowData)
            , columnCount_(columnCount)
            , layerCount_(layerCount)
        {
        }

        ElementType* rowData_ = nullptr;
        size_type columnCount_ = 0;
        size_type layerCount_ = 1;
    };

    /** @brief Creates an empty grid. */
    ContiguousGrid() = default;

    /** @brief Creates a grid with the given row, column and layer counts. */
    explicit ContiguousGrid(size_type rowCount, size_type columnCount, size_type layerCount = 1)
    {
        resize(rowCount, columnCount, layerCount);
    }

    /** @brief Resizes the grid and value-initializes the contiguous storage. */
    void resize(size_type rowCount, size_type columnCount, size_type layerCount = 1)
    {
        rowCount_ = rowCount;
        columnCount_ = columnCount;
        layerCount_ = layerCount;
        storage_.resize(rowCount_ * columnCount_ * layerCount_);
    }

    /** @brief Returns the number of rows to preserve `grid[row][column]` usage. */
    [[nodiscard]] size_type size() const noexcept
    {
        return rowCount_;
    }

    /** @brief Returns true when the grid contains no elements. */
    [[nodiscard]] bool empty() const noexcept
    {
        return storage_.empty();
    }

    /** @brief Returns the configured number of rows. */
    [[nodiscard]] size_type rowCount() const noexcept
    {
        return rowCount_;
    }

    /** @brief Returns the configured number of columns. */
    [[nodiscard]] size_type columnCount() const noexcept
    {
        return columnCount_;
    }

    /** @brief Returns the configured number of layers. */
    [[nodiscard]] size_type layerCount() const noexcept
    {
        return layerCount_;
    }

    /** @brief Returns a mutable pointer to the contiguous storage buffer. */
    [[nodiscard]] value_type* data() noexcept
    {
        return storage_.data();
    }

    /** @brief Returns a const pointer to the contiguous storage buffer. */
    [[nodiscard]] const value_type* data() const noexcept
    {
        return storage_.data();
    }

    /** @brief Returns a mutable row proxy for `grid[row][column]` access. */
    [[nodiscard]] RowProxy<value_type> operator[](size_type row) noexcept
    {
        assert(row < rowCount_);
        return RowProxy<value_type>(storage_.data() + row * columnCount_ * layerCount_, columnCount_, layerCount_);
    }

    /** @brief Returns a const row proxy for `grid[row][column]` access. */
    [[nodiscard]] RowProxy<const value_type> operator[](size_type row) const noexcept
    {
        assert(row < rowCount_);
        return RowProxy<const value_type>(storage_.data() + row * columnCount_ * layerCount_, columnCount_, layerCount_);
    }

    /** @brief Returns a mutable element from the first layer using C++23 multidimensional subscript syntax. */
    [[nodiscard]] value_type& operator[](size_type row, size_type column) noexcept
    {
        return storage_[flatIndex(row, column, 0)];
    }

    /** @brief Returns a const element from the first layer using C++23 multidimensional subscript syntax. */
    [[nodiscard]] const value_type& operator[](size_type row, size_type column) const noexcept
    {
        return storage_[flatIndex(row, column, 0)];
    }

    /** @brief Returns a mutable element from the given layer using C++23 multidimensional subscript syntax. */
    [[nodiscard]] value_type& operator[](size_type row, size_type column, size_type layer) noexcept
    {
        return storage_[flatIndex(row, column, layer)];
    }

    /** @brief Returns a const element from the given layer using C++23 multidimensional subscript syntax. */
    [[nodiscard]] const value_type& operator[](size_type row, size_type column, size_type layer) const noexcept
    {
        return storage_[flatIndex(row, column, layer)];
    }

private:
    /** @brief Computes the storage offset for a row, column and layer triplet. */
    [[nodiscard]] size_type flatIndex(size_type row, size_type column, size_type layer) const noexcept
    {
        assert(row < rowCount_);
        assert(column < columnCount_);
        assert(layer < layerCount_);
        return (row * columnCount_ + column) * layerCount_ + layer;
    }

    std::vector<value_type> storage_;
    size_type rowCount_ = 0;
    size_type columnCount_ = 0;
    size_type layerCount_ = 1;
};
