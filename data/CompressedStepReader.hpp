/** @file CompressedStepReader.hpp
 * @brief Reading of one step stored by OOpenCAL in `mode=compressed` (`<name><node>.blosc` files).
 *
 * Kept apart from ModelReader.hpp on purpose: this header is included by plugins that are compiled
 * on the fly, and it must stay free of Blosc2 headers. The implementation (CompressedStepReader.cpp)
 * is part of the Viewer executable, where plugins find it through the dynamic symbol table. */

#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <vector>


namespace ReaderHelpers
{
/** @brief Header in front of every step in a `.blosc` file (native byte order, written by OOpenCAL). */
struct CompressedStepHeader
{
    std::uint64_t originalSize;   ///< bytes after decompression
    std::uint64_t compressedSize; ///< bytes of the Blosc2 chunk that follows the header
};
static_assert(sizeof(CompressedStepHeader) == 16, "must match StepHeader in OOpenCAL (base/lib/BloscStepCodec.h)");
// TODO: Consider using the header from OOpenCAL, but it is blocked until: https://github.com/alessioderango/OOpenCAL/pull/61

/** @brief Reads one step stored as `[CompressedStepHeader][Blosc2 chunk]` and returns the decompressed bytes.
 *
 * The result has the same layout as one step of a `.bin` file: `width * height * slices` raw cells.
 * Defined in CompressedStepReader.cpp (not inline), so that plugins built on the fly do not need
 * the Blosc2 headers or library.
 *
 * @param in            stream positioned at the beginning of the CompressedStepHeader
 * @param expectedBytes size the step must have after decompression (cells * sizeof(Cell))
 * @throws std::runtime_error when the data is truncated, corrupted or has an unexpected size,
 *         or when the Viewer was built without C-Blosc2 (`-DVIEWER_WITH_BLOSC2=OFF`) */
std::vector<char> readCompressedStep(std::istream& in, std::size_t expectedBytes);
} // namespace ReaderHelpers
