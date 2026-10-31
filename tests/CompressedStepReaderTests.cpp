#include <gtest/gtest.h>
#include <blosc2.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "core/types.h"
#include "data/CompressedStepReader.hpp"
#include "data/ModelReader.hpp"
#include "tests/TestCell.hpp"
#include "visualiserProxy/ContiguousGrid.h"

// Tests of reading `mode=compressed` output (C-Blosc2): the step reader on its own and,
// at the end, ModelReader reading the same grid from `.bin` and `.blosc` files.

namespace
{
/** Writes `[CompressedStepHeader][Blosc2 chunk]` exactly like OOpenCAL's BloscStepCodec (zstd, level 3, shuffle). */
std::string makeCompressedStep(const void* data, std::size_t bytes, int typesize)
{
    blosc2_init();
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.compcode = BLOSC_ZSTD;
    cparams.clevel = 3;
    cparams.typesize = typesize;
    cparams.nthreads = 1;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
    blosc2_context* context = blosc2_create_cctx(cparams);

    std::vector<char> chunk(bytes + BLOSC2_MAX_OVERHEAD);
    const int compressed = blosc2_compress_ctx(context, data, static_cast<int32_t>(bytes), chunk.data(), static_cast<int32_t>(chunk.size()));
    blosc2_free_ctx(context);
    EXPECT_GT(compressed, 0);

    const ReaderHelpers::CompressedStepHeader header{bytes, static_cast<std::uint64_t>(compressed)};
    std::string block(reinterpret_cast<const char*>(&header), sizeof header);
    block.append(chunk.data(), static_cast<std::size_t>(compressed));
    return block;
}
} // namespace

TEST(CompressedStepReader, ReturnsTheOriginalBytes)
{
    std::vector<int> original(1000);
    for (std::size_t i = 0; i < original.size(); ++i)
        original[i] = static_cast<int>(i % 7);
    const std::size_t bytes = original.size() * sizeof(int);

    std::istringstream stream(makeCompressedStep(original.data(), bytes, sizeof(int)));
    const auto result = ReaderHelpers::readCompressedStep(stream, bytes);

    ASSERT_EQ(result.size(), bytes);
    EXPECT_EQ(std::memcmp(result.data(), original.data(), bytes), 0);
}

TEST(CompressedStepReader, ReadsConsecutiveStepsAfterSeeking)
{
    // Steps are stored back to back; the index file points at the beginning of each header.
    const std::vector<int> a(100, 1), b(100, 2);
    const std::string first = makeCompressedStep(a.data(), a.size() * sizeof(int), sizeof(int));
    const std::string second = makeCompressedStep(b.data(), b.size() * sizeof(int), sizeof(int));

    std::istringstream stream(first + second);
    stream.seekg(static_cast<std::streamoff>(first.size()));
    const auto result = ReaderHelpers::readCompressedStep(stream, b.size() * sizeof(int));
    EXPECT_EQ(std::memcmp(result.data(), b.data(), result.size()), 0);
}

TEST(CompressedStepReader, RejectsWrongExpectedSize)
{
    const std::vector<int> original(100, 5);
    std::istringstream stream(makeCompressedStep(original.data(), original.size() * sizeof(int), sizeof(int)));
    EXPECT_THROW(ReaderHelpers::readCompressedStep(stream, original.size() * sizeof(int) + 4), std::runtime_error);
}

TEST(CompressedStepReader, RejectsTruncatedHeaderAndPayload)
{
    const std::vector<int> original(100, 5);
    const std::size_t bytes = original.size() * sizeof(int);
    const std::string block = makeCompressedStep(original.data(), bytes, sizeof(int));

    std::istringstream onlyPartOfHeader(block.substr(0, 8));
    EXPECT_THROW(ReaderHelpers::readCompressedStep(onlyPartOfHeader, bytes), std::runtime_error);

    std::istringstream missingPayload(block.substr(0, block.size() - 5));
    EXPECT_THROW(ReaderHelpers::readCompressedStep(missingPayload, bytes), std::runtime_error);
}

TEST(CompressedStepReader, RejectsCorruptedChunk)
{
    const std::vector<int> original(100, 5);
    const std::size_t bytes = original.size() * sizeof(int);
    std::string block = makeCompressedStep(original.data(), bytes, sizeof(int));
    for (std::size_t i = sizeof(ReaderHelpers::CompressedStepHeader); i < block.size(); ++i)
        block[i] = static_cast<char>(0xFF);

    std::istringstream stream(block);
    EXPECT_THROW(ReaderHelpers::readCompressedStep(stream, bytes), std::runtime_error);
}

namespace
{
/** Value stored in cell `i` of `node` at `step` by writeTwoNodeOutput(). */
int twoNodeValue(int node, int step, std::size_t i)
{
    return static_cast<int>(100 * step + 10 * node + i);
}

/** Writes steps 0 and 5 of a grid split into two nodes side by side (nNodeX = 2): node 0 is 2x2 cells,
 * node 1 is 1x2 cells (3 columns x 2 rows in total), in `<directory>/<mode>/grid<node>.{bin,blosc}`.
 * @param mode "binary" or "compressed"; returns the base name of the files */
std::string writeTwoNodeOutput(const std::filesystem::path& directory, const std::string& mode)
{
    namespace fs = std::filesystem;
    const std::string baseName = (directory / mode / "grid").string();
    fs::create_directories(directory / mode);
    const bool compressed = (mode == "compressed");

    for (int node = 0; node < 2; ++node)
    {
        std::ofstream index(baseName + std::to_string(node) + "_index.txt");
        std::ofstream data(baseName + std::to_string(node) + (compressed ? ".blosc" : ".bin"), std::ios::binary);
        const int cols = node == 0 ? 2 : 1;
        for (int step : {0, 5})
        {
            index << step << ' ' << static_cast<long long>(data.tellp()) << " (" << cols << "-2)\n";

            std::vector<TestCell> cells(static_cast<std::size_t>(cols) * 2);
            for (std::size_t i = 0; i < cells.size(); ++i)
                cells[i].value = twoNodeValue(node, step, i);

            const std::size_t bytes = cells.size() * sizeof(TestCell);
            if (compressed)
            {
                const std::string block = makeCompressedStep(cells.data(), bytes, sizeof(TestCell));
                data.write(block.data(), static_cast<std::streamsize>(block.size()));
            }
            else
            {
                data.write(reinterpret_cast<const char*>(cells.data()), static_cast<std::streamsize>(bytes));
            }
        }
    }
    return baseName;
}
} // namespace

TEST(CompressedModeReading, GivesTheSameGridAsBinaryMode)
{
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() / "oopencal-viewer-reader-compressed-test";
    fs::remove_all(directory);
    fs::create_directories(directory);

    for (const char* mode : {"binary", "compressed"})
        writeTwoNodeOutput(directory, mode);

    auto readGrid = [&](const std::string& mode, StepIndex step)
    {
        const std::string baseName = (directory / mode / "grid").string();
        ModelReader<TestCell> reader;
        reader.readStepsOffsetsForAllNodesFromFiles(2, 1, 1, baseName);

        SettingParameter settings{};
        settings.step = step;
        settings.numberOfColumnX = 3;
        settings.numberOfRowsY = 2;
        settings.numberOfSlicesZ = 1;
        settings.nNodeX = 2;
        settings.nNodeY = 1;
        settings.nNodeZ = 1;
        settings.outputFileName = baseName;
        settings.readMode = mode;

        ContiguousGrid<TestCell> grid(2, 3, 1);  // rows, columns, layers
        std::vector<Line> lines(7);
        reader.readStageStateFromFilesForStep(grid, &settings, lines.data());

        std::vector<int> values;
        for (std::size_t row = 0; row < 2; ++row)
            for (std::size_t column = 0; column < 3; ++column)
                values.push_back((grid[row, column, 0].value));
        return values;
    };

    // The second step is read after a seek; both modes must agree with each other and with the data written.
    for (StepIndex step : {0u, 5u})
    {
        const auto fromBinary = readGrid("binary", step);
        const auto fromCompressed = readGrid("compressed", step);
        EXPECT_EQ(fromCompressed, fromBinary) << "step " << step;
    }

    const auto step5 = readGrid("compressed", 5);
    ASSERT_EQ(step5.size(), 6u);
    EXPECT_EQ(step5[0], 500);  // node 0, cell 0 (row 0, col 0)
    EXPECT_EQ(step5[1], 501);  // node 0, cell 1 (row 0, col 1)
    EXPECT_EQ(step5[2], 510);  // node 1, cell 0 (row 0, col 2)
    EXPECT_EQ(step5[3], 502);  // node 0, cell 2 (row 1, col 0)
    EXPECT_EQ(step5[4], 503);  // node 0, cell 3 (row 1, col 1)
    EXPECT_EQ(step5[5], 511);  // node 1, cell 1 (row 1, col 2)

    fs::remove_all(directory);
}

TEST(CompressedModeReading, ReadCellAcrossStepsDecompressesOnlyTheNodeHoldingTheCell)
{
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() / "oopencal-viewer-reader-compressed-history-test";
    fs::remove_all(directory);
    const std::string baseName = writeTwoNodeOutput(directory, "compressed");

    ModelReader<TestCell> reader;
    reader.readStepsOffsetsForAllNodesFromFiles(2, 1, 1, baseName);

    SettingParameter settings{};
    settings.numberOfColumnX = 3;
    settings.numberOfRowsY = 2;
    settings.numberOfSlicesZ = 1;
    settings.nNodeX = 2;
    settings.nNodeY = 1;
    settings.nNodeZ = 1;
    settings.outputFileName = baseName;
    settings.readMode = "compressed";

    const std::vector<StepIndex> steps = {0, 5};

    // row 1, column 1 lies in node 0 (its 4th cell); row 1, column 2 lies in node 1 (its 2nd cell)
    const auto inNode0 = reader.readCellAcrossSteps(settings, 1, 1, 0, steps);
    const auto inNode1 = reader.readCellAcrossSteps(settings, 1, 2, 0, steps);

    ASSERT_EQ(inNode0.size(), steps.size());
    ASSERT_EQ(inNode1.size(), steps.size());
    for (std::size_t i = 0; i < steps.size(); ++i)
    {
        ASSERT_TRUE(inNode0[i].has_value());
        ASSERT_TRUE(inNode1[i].has_value());
        EXPECT_EQ(inNode0[i]->value, twoNodeValue(0, static_cast<int>(steps[i]), 3));
        EXPECT_EQ(inNode1[i]->value, twoNodeValue(1, static_cast<int>(steps[i]), 1));
    }

    // A step that is not in the index stays empty instead of aborting the whole history
    const auto withUnknownStep = reader.readCellAcrossSteps(settings, 0, 0, 0, {0, 99});
    ASSERT_EQ(withUnknownStep.size(), 2u);
    EXPECT_TRUE(withUnknownStep[0].has_value());
    EXPECT_FALSE(withUnknownStep[1].has_value());

    fs::remove_all(directory);
}
