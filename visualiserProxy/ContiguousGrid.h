/** @file ContiguousGrid.h
 * @brief Lightweight contiguous N-dimensional storage with chained indexing.
 *
 * This is a temporary in-tree replacement for std::mdspan while the project
 * still needs to build with toolchains that do not provide it yet. */

#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <vector>

/** @class ContiguousGrid
 * @brief Stores N-dimensional data in a single contiguous std::vector.
 *
 * The class offers both chained indexing (`grid[y][x]`, `grid[z][y][x]`) and
 * direct multi-index access (`grid(y, x)`, `grid(z, y, x)`), while keeping the
 * data in a compact row-major layout.
 *
 * @note This helper is intended as a temporary solution until std::mdspan is
 *       reliably available in the compilers used by this project. */
template<typename T, std::size_t Dimensions>
class ContiguousGrid
{
    static_assert(Dimensions >= 2, "ContiguousGrid is intended for 2D or higher-dimensional storage.");

public:
    using value_type = T;
    using size_type = std::size_t;

    template<typename ElementType, std::size_t RemainingDimensions>
    class Slice
    {
    public:
        [[nodiscard]] size_type size() const noexcept
        {
            return extents_[0];
        }

        [[nodiscard]] decltype(auto) operator[](size_type index) const noexcept
        {
            assert(index < size());

            if constexpr (RemainingDimensions == 1)
            {
                return (data_[index]);
            }
            else
            {
                return Slice<ElementType, RemainingDimensions - 1>(data_ + index * strides_[0], extents_ + 1, strides_ + 1);
            }
        }

    private:
        friend class ContiguousGrid;

        Slice(ElementType* data, const size_type* extents, const size_type* strides) noexcept
            : data_(data)
            , extents_(extents)
            , strides_(strides)
        {
        }

        ElementType* data_ = nullptr;
        const size_type* extents_ = nullptr;
        const size_type* strides_ = nullptr;
    };

    ContiguousGrid() = default;

    explicit ContiguousGrid(const std::array<size_type, Dimensions>& extents)
    {
        resize(extents);
    }

    void resize(const std::array<size_type, Dimensions>& extents)
    {
        extents_ = extents;
        recomputeStrides();
        storage_.resize(totalSize());
    }

    template<typename... Sizes>
        requires(sizeof...(Sizes) == Dimensions && (std::integral<Sizes> && ...))
    void resize(Sizes... sizes)
    {
        resize(std::array<size_type, Dimensions>{toSize(sizes)...});
    }

    [[nodiscard]] size_type size() const noexcept
    {
        return extents_[0];
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return storage_.empty();
    }

    [[nodiscard]] const std::array<size_type, Dimensions>& extents() const noexcept
    {
        return extents_;
    }

    [[nodiscard]] value_type* data() noexcept
    {
        return storage_.data();
    }

    [[nodiscard]] const value_type* data() const noexcept
    {
        return storage_.data();
    }

    [[nodiscard]] auto operator[](size_type index) noexcept
    {
        assert(index < size());
        return Slice<value_type, Dimensions - 1>(storage_.data() + index * strides_[0], extents_.data() + 1, strides_.data() + 1);
    }

    [[nodiscard]] auto operator[](size_type index) const noexcept
    {
        assert(index < size());
        return Slice<const value_type, Dimensions - 1>(storage_.data() + index * strides_[0], extents_.data() + 1, strides_.data() + 1);
    }

    template<typename... Indices>
        requires(sizeof...(Indices) == Dimensions && (std::integral<Indices> && ...))
    [[nodiscard]] value_type& operator()(Indices... indices) noexcept
    {
        return storage_[flatten(std::array<size_type, Dimensions>{toSize(indices)...})];
    }

    template<typename... Indices>
        requires(sizeof...(Indices) == Dimensions && (std::integral<Indices> && ...))
    [[nodiscard]] const value_type& operator()(Indices... indices) const noexcept
    {
        return storage_[flatten(std::array<size_type, Dimensions>{toSize(indices)...})];
    }

private:
    [[nodiscard]] static constexpr size_type toSize(std::integral auto value) noexcept
    {
        return static_cast<size_type>(value);
    }

    void recomputeStrides() noexcept
    {
        size_type stride = 1;
        for (size_type i = Dimensions; i-- > 0;)
        {
            strides_[i] = stride;
            stride *= extents_[i];
        }
    }

    [[nodiscard]] size_type totalSize() const noexcept
    {
        size_type size = 1;
        for (const size_type extent : extents_)
            size *= extent;
        return size;
    }

    [[nodiscard]] size_type flatten(const std::array<size_type, Dimensions>& indices) const noexcept
    {
        size_type offset = 0;
        for (size_type i = 0; i < Dimensions; ++i)
        {
            assert(indices[i] < extents_[i]);
            offset += indices[i] * strides_[i];
        }
        return offset;
    }

    std::vector<value_type> storage_;
    std::array<size_type, Dimensions> extents_{};
    std::array<size_type, Dimensions> strides_{};
};
