/** @file ModelReader.hpp
 * @brief Declaration of the ModelReader template class for reading and processing model data.
 * 
 * This file contains the ModelReader class template which provides functionality to read,
 * parse, and process model data from files. It supports reading data in stages and provides
 * methods to access model data at different time steps. */

#pragma once

#include <algorithm> // std::ranges::sort
#include <climits>   // INT_MAX
#include <cmath>     // log10
#include <cstring>   // std::memcpy
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <ranges>
#include <regex>
#include <unordered_map>
#include <vector>

#include "core/types.h"
#include "visualiser/Line.h"
#include "visualiser/SettingParameter.h"
#include "plugins/CellConcept.hpp"
#include "data/PerformanceMetrics.h"

/** @class ModelReader
 * @brief Template class for reading and processing model data from files.
 * 
 * The ModelReader class provides functionality to read model data in stages and
 * access it at different time steps. It's designed to work with different cell types
 * through template specialization.
 * 
 * @tparam Cell The cell type used in the model 
 * Note: Cell is derived class from Element (which is header from OOpenCal) */
template<CellLike Cell>
class ModelReader
{
public:
    struct StepOffsetInfo
    {
        FilePosition position;
        std::optional<ColumnRowSlice> sceneSize;
    };

private:
    std::vector<std::unordered_map<StepIndex, StepOffsetInfo>> nodeStepOffsets; ///< Maps node indices to their file positions for each step

public:
    /** @brief Prepares the reader for a new stage of data processing.
     * 
     * Initializes the internal data structures to handle a grid of nodes with
     * the specified dimensions.
     * 
     * @param nNodeX Number of nodes along the X axis
     * @param nNodeY Number of nodes along the Y axis
     * @param nNodeZ Number of nodes along the Z axis (defaults to 1 for 2D models) */
    void prepareStage(NodeIndex nNodeX, NodeIndex nNodeY, NodeIndex nNodeZ = 1)
    {
        nodeStepOffsets.resize(nNodeX * nNodeY * nNodeZ);
    }

    /// @brief Clears the current stage and releases associated resources.
    void clearStage()
    {
        nodeStepOffsets.clear();
    }

    /** @brief Reads the stage state from files for a specific step.
     * 
     * This method reads the model state for a specific simulation step and updates
     * the provided matrix and settings accordingly.
     * 
     * @tparam Matrix The matrix type used to store the model state
     * @param m Reference to the matrix that will store the model state
     * @param sp Pointer to the setting parameters
     * @param lines Pointer to the line data structure */
    template<class Matrix>
    void readStageStateFromFilesForStep(Matrix& m, SettingParameter* sp, Line* lines);

    /** @brief Loads step offset data from text files into an internal hash map.
     *
     * Supports two file formats:
     * 1) Legacy format:
     *      <stepNumber:int> <positionInFile:long>
     * 2) Extended format:
     *      <stepNumber:int> <positionInFile:long> (<sceneMin:int>-<sceneMax:int>)
     *
     * Example (extended):
     *      0 0 (250-500)
     *      1 2000000 (250-500)
     *
     * @param nNodeX Number of nodes along the X axis
     * @param nNodeY Number of nodes along the Y axis
     * @param nNodeZ Number of nodes along the Z axis (defaults to 1 for 2D models)
     * @param filename Name of the file containing the step offsets
     *
     * @throws std::runtime_error If the file cannot be opened or has an invalid format */
    void readStepsOffsetsForAllNodesFromFiles(NodeIndex nNodeX, NodeIndex nNodeY, NodeIndex nNodeZ, const std::string& filename);

    /** @brief Returns a sorted list of all available simulation steps.
     *
     * This method inspects the internal `stage` structure, which stores for each node
     * a mapping from step number (`StepIndex`) to byte offset within its corresponding file.
     * It verifies that all nodes contain the same set of step indices. If any mismatch
     * between nodes is detected, it either throws an exception or prints a warning message
     * to `std::cerr`, depending on the value of `throwOnMismatch`.
     *
     * Additionally, if the `stage` data structure is empty (no node data loaded),
     * the function will either throw a `std::runtime_error` or print a warning,
     * depending on the value of `throwOnMismatch`.
     *
     * @param throwOnMismatch If true, throws `std::runtime_error` when:
     *        - a mismatch between node step sets is detected, or
     *        - the `stage` structure is empty.
     *        If false, prints warnings to `std::cerr` instead of throwing.
     *
     * @return A sorted `std::vector<StepIndex>` containing all available simulation steps.
     *
     * @throws std::runtime_error If `throwOnMismatch` is true and:
     *         - the `stage` is empty, or
     *         - the step sets differ between nodes. */
    std::vector<StepIndex> availableSteps(bool throwOnMismatch = false) const;

private:
    FilePosition getStepStartingPositionInFile(StepIndex step, NodeIndex node) const;

    /** @brief Opens the data file for a given simulation step and node.
     *
     * The function locates the correct file for the specified node (e.g. "ball3.txt", where 3 is node number),
     * seeks to the byte position corresponding to the given simulation step in file,
     * reads the first header line containing local grid dimensions (columns and rows),
     * and returns an input file stream positioned right after that header.
     *
     * @param step         Simulation step number.
     * @param fileName     Base file name (without node index or extension).
     * @param node         Node index for which data should be opened.
     * @param columnAndRow Output: number of local columns and rows read from header line.
     *
     * @return std::ifstream Stream ready for reading cell data of this node at given step.
     * @throws std::runtime_error If the file cannot be opened, seek fails, or header is invalid. */
    [[nodiscard]] std::ifstream readColumnAndRowForStepFromFileReturningStream(StepIndex step,
                                                                               const std::string& fileName,
                                                                               NodeIndex node,
                                                                               ColumnRowSlice& dimensions,
                                                                               bool isBinary = false);

    [[nodiscard]] ColumnRowSlice readDimensionsForStepFromFile(StepIndex step, const std::string& fileName, NodeIndex node, bool isBinary = false);

    std::vector<ColumnRowSlice> localDimensionsForAllNodes(StepIndex step,
                                                           NodeIndex nNodeX,
                                                           NodeIndex nNodeY,
                                                           NodeIndex nNodeZ,
                                                           const std::string& fileName,
                                                           bool isBinary = false);
};

/////////////////////////////
namespace ReaderHelpers /// functions which are not templates
{
[[nodiscard]] inline std::string giveMeFileName(const std::string& fileName, NodeIndex node, bool isBinary = false)
{
    const auto extention = isBinary ? "bin" : "txt";
    return std::format("{}{}.{}", fileName, node, extention);
}

[[nodiscard]] inline std::string giveMeFileNameIndex(const std::string& fileName, NodeIndex node)
{
    return std::format("{}{}_index.txt", fileName, node);
}

ColumnAndRow getColumnAndRowFromLine(const std::string& line);

ColumnAndRow calculateXYOffsetForNode(NodeIndex node, NodeIndex nNodeX, NodeIndex nNodeY, const std::vector<ColumnAndRow>& columnsAndRows);

ColumnRowSlice getDimensionsFromLine(const std::string& line);

ColumnRowSlice calculateXYZOffsetForNode(NodeIndex node,
                                         NodeIndex nNodeX,
                                         NodeIndex nNodeY,
                                         NodeIndex nNodeZ,
                                         const std::vector<ColumnRowSlice>& dimensions);
} // namespace ReaderHelpers
/////////////////////////////

template<CellLike Cell>
ColumnRowSlice ModelReader<Cell>::readDimensionsForStepFromFile(StepIndex step, const std::string& fileName, NodeIndex node, bool isBinary)
{
    ColumnRowSlice dimensions;
    std::ifstream file [[maybe_unused]] = readColumnAndRowForStepFromFileReturningStream(step, fileName, node, dimensions, isBinary);
    return dimensions;
}

template<CellLike Cell>
std::ifstream ModelReader<Cell>::readColumnAndRowForStepFromFileReturningStream(StepIndex step,
                                                                                const std::string& fileName,
                                                                                NodeIndex node,
                                                                                ColumnRowSlice& dimensions,
                                                                                bool isBinary)
{
    const auto fileNameTmp = ReaderHelpers::giveMeFileName(fileName, node, isBinary);

    std::ifstream file(fileNameTmp, isBinary ? std::ios::binary : std::ios::in);
    if (! file.is_open())
    {
        throw std::runtime_error(std::format("Can't read '{}' in {} function", fileNameTmp, __func__));
    }

    const auto fPos = getStepStartingPositionInFile(step, node);
    file.seekg(fPos);
    if (! file)
    {
        throw std::runtime_error(std::format("Seek failed in '{}' at position {}", fileNameTmp, fPos));
    }

    if (isBinary)
    {
        // For binary mode, read dimensions from sceneSize in StepOffsetInfo
        if (node >= nodeStepOffsets.size())
            throw std::runtime_error(std::format("Invalid node index {} in binary mode", node));

        const auto& stepMap = nodeStepOffsets[node];
        if (auto it = stepMap.find(step); it != stepMap.end() && it->second.sceneSize.has_value())
        {
            dimensions = it->second.sceneSize.value();
        }
        else
        {
            throw std::runtime_error(std::format("Binary mode requires sceneSize in step offset info for step {} node {}", step, node));
        }
    }
    else
    {
        // For text mode, read header line with dimensions
        std::string line;
        if (! std::getline(file, line))
        {
            throw std::runtime_error(std::format("Failed to read line from '{}' at position {}", fileNameTmp, fPos));
        }

        dimensions = ReaderHelpers::getDimensionsFromLine(line);
    }

    return file;
}

template<CellLike Cell>
template<class Matrix>
void ModelReader<Cell>::readStageStateFromFilesForStep(Matrix& m, SettingParameter* sp, Line* lines)
{
    PerformanceSession perfSession("readStageStateFromFilesForStep", 
                                   static_cast<uint32_t>(sp->numberOfColumnX),
                                   static_cast<uint32_t>(sp->numberOfRowsY),
                                   static_cast<uint32_t>(sp->numberOfSlicesZ),
                                   1);
    perfSession.setStepNumber(sp->step);
    perfSession.setCategory(PerformanceMetrics::MetricsCategory::DataLoading);

    const auto totalNodes = sp->nNodeX * sp->nNodeY * sp->nNodeZ;
    const bool isBinary = (sp->readMode == "binary");
    const auto localDimensions = localDimensionsForAllNodes(sp->step,
                                                            sp->nNodeX,
                                                            sp->nNodeY,
                                                            sp->nNodeZ,
                                                            sp->outputFileName,
                                                            isBinary);

    size_t totalCellsForThisCall = 0;
    for (const auto& dimensions : localDimensions)
    {
        totalCellsForThisCall += static_cast<size_t>(dimensions.column) *
                                 dimensions.row *
                                 dimensions.slice;
    }
    perfSession.recordReadCall(totalCellsForThisCall);

    auto processNode = [&, this](NodeIndex node)
    {
        const auto offset = ReaderHelpers::calculateXYZOffsetForNode(node,
                                                                     sp->nNodeX,
                                                                     sp->nNodeY,
                                                                     sp->nNodeZ,
                                                                     localDimensions);

        ColumnRowSlice dimensions;
        std::ifstream fp = readColumnAndRowForStepFromFileReturningStream(sp->step,
                                                                          sp->outputFileName,
                                                                          node,
                                                                          dimensions,
                                                                          isBinary);
        if (! fp)
            throw std::runtime_error("Cannot open file for node " + std::to_string(node));

        const int maxX = static_cast<int>(m[0].size()) - 1;
        const int maxY = static_cast<int>(m.size()) - 1;

        const int x1 = std::min(offset.x(), maxX);
        const int y1 = std::min(offset.y(), maxY);
        const int x2 = std::min(offset.x() + dimensions.column, maxX + 1);
        const int y2 = std::min(offset.y() + dimensions.row, maxY + 1);

        lines[node * 2] = Line(x1, y1, x2, y1);
        lines[node * 2 + 1] = Line(x1, y1, x1, y2);

        const NodeIndex nodeRow = (node / sp->nNodeX) % sp->nNodeY;
        const NodeIndex nodeSlice = node / (sp->nNodeX * sp->nNodeY);
        if (nodeRow == sp->nNodeY - 1)
        {
            const int topLineIndex = 2 * totalNodes +
                                     nodeSlice * sp->nNodeX +
                                     (node % sp->nNodeX);
            lines[topLineIndex] = Line(x1, y2, x2, y2);
        }

        const NodeIndex nodeCol = node % sp->nNodeX;
        if (nodeCol == sp->nNodeX - 1)
        {
            const int rightLineIndex = 2 * totalNodes +
                                       sp->nNodeX * sp->nNodeZ +
                                       nodeSlice * sp->nNodeY +
                                       nodeRow;
            lines[rightLineIndex] = Line(x2, y1, x2, y2);
        }

        bool localStartStepDone = false;

        if (isBinary)
        {
            const size_t cellCount = static_cast<size_t>(dimensions.column) *
                                     dimensions.row *
                                     dimensions.slice;
            const size_t cellSize = sizeof(Cell);
            const size_t totalBytes = cellCount * cellSize;

            std::vector<char> buffer(totalBytes);
            fp.read(buffer.data(), totalBytes);

            if (fp.gcount() != static_cast<std::streamsize>(totalBytes))
            {
                throw std::runtime_error(std::format("Failed to read {} bytes from binary file for node {}", totalBytes, node));
            }

            for (int slice = 0; slice < dimensions.slice; ++slice)
            {
                const int matrixSlice = slice + offset.z();
                if (matrixSlice >= static_cast<int>(m.layerCount()))
                    continue;

                for (int row = 0; row < dimensions.row; ++row)
                {
                    const int matrixRow = row + offset.y();
                    if (matrixRow >= static_cast<int>(m.size()))
                        continue;

                    for (int col = 0; col < dimensions.column; ++col)
                    {
                        const int matrixCol = col + offset.x();
                        if (matrixCol >= static_cast<int>(m[matrixRow].size()))
                            continue;

                        if (! localStartStepDone) [[unlikely]]
                        {
                            m.get(matrixRow, matrixCol, matrixSlice).startStep(sp->step);
                            localStartStepDone = true;
                        }

                        const size_t cellIndex =
                            (static_cast<size_t>(slice) * dimensions.row + row) *
                            dimensions.column +
                            col;
                        const char* cellData = buffer.data() + (cellIndex * cellSize);

                        Cell tempCell;
                        std::memcpy(&tempCell, cellData, cellSize); /// @note This erases the temporary object's vtable; assignment copies the cell state.
                        m.get(matrixRow, matrixCol, matrixSlice) = tempCell;
                    }
                }
            }
        }
        else
        {
            static thread_local char fileBuffer[1 << 16];
            fp.rdbuf()->pubsetbuf(fileBuffer, sizeof(fileBuffer));

            constexpr std::size_t numbersPerLine = 10'000;
            const std::size_t lineBufferSize = (std::log10(UINT_MAX) + 2) * numbersPerLine;
            std::string line;
            line.reserve(lineBufferSize);

            // Each slice is serialized as `rows` consecutive text lines.
            for (int slice = 0; slice < dimensions.slice; ++slice)
            {
                const int matrixSlice = slice + offset.z();

                for (int row = 0; row < dimensions.row; ++row)
                {
                    if (! std::getline(fp, line))
                    {
                        const auto fileNameTmp = ReaderHelpers::giveMeFileName(sp->outputFileName, node, isBinary);
                        throw std::runtime_error("Error reading entire line from " + fileNameTmp);
                    }

                    const int matrixRow = row + offset.y();
                    if (matrixSlice >= static_cast<int>(m.layerCount()) ||
                        matrixRow >= static_cast<int>(m.size()))
                    {
                        continue;
                    }

                    char* cursor = line.data();
                    char* const end = line.data() + line.size();

                    for (int col = 0; col < dimensions.column; ++col)
                    {
                        while (cursor < end && *cursor == ' ')
                            ++cursor;
                        if (cursor >= end)
                            break;

                        char* const token = cursor;
                        while (cursor < end && *cursor != ' ')
                            ++cursor;
                        if (cursor < end)
                            *cursor++ = '\0';

                        const int matrixCol = col + offset.x();
                        if (matrixCol >= static_cast<int>(m[matrixRow].size()))
                            continue;

                        if (! localStartStepDone) [[unlikely]]
                        {
                            m.get(matrixRow, matrixCol, matrixSlice).startStep(sp->step);
                            localStartStepDone = true;
                        }

                        m.get(matrixRow, matrixCol, matrixSlice).composeElement(token);
                    }
                }
            }
        }
    };

    std::vector<std::future<void>> futures;
    futures.reserve(totalNodes);

    for (NodeIndex node = 0; node < totalNodes; ++node)
    {
        futures.push_back(std::async(std::launch::async,
                                     [&, node]()
                                     {
                                         processNode(node);
                                     }));
    }

    std::ranges::for_each(futures,
                          [](std::future<void>& f)
                          {
                              f.get();
                          });
}

template<CellLike Cell>
std::vector<ColumnRowSlice> ModelReader<Cell>::localDimensionsForAllNodes(StepIndex step,
                                                                         NodeIndex nNodeX,
                                                                         NodeIndex nNodeY,
                                                                         NodeIndex nNodeZ,
                                                                         const std::string& fileName,
                                                                         bool isBinary)
{
    const auto nodesCount = nNodeX * nNodeY * nNodeZ;
    std::vector<ColumnRowSlice> allDimensions(nodesCount);

    for (NodeIndex node = 0; node < nodesCount; node++)
    {
        allDimensions[node] = readDimensionsForStepFromFile(step, fileName, node, isBinary);
    }
    return allDimensions;
}

template<CellLike Cell>
void ModelReader<Cell>::readStepsOffsetsForAllNodesFromFiles(NodeIndex nNodeX, NodeIndex nNodeY, NodeIndex nNodeZ, const std::string& filename)
{
    const auto totalNodes = nNodeX * nNodeY * nNodeZ;
    prepareStage(nNodeX, nNodeY, nNodeZ);

    for (NodeIndex node = 0; node < totalNodes; ++node)
    {
        const auto fileNameIndex = ReaderHelpers::giveMeFileNameIndex(filename, node);
        if (! std::filesystem::exists(fileNameIndex))
            throw std::runtime_error("File not found: " + fileNameIndex);

        std::ifstream file(fileNameIndex);
        if (! file)
            throw std::runtime_error("Cannot open file: " + fileNameIndex);

        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            std::istringstream iss(line);
            StepIndex stepNumber{};
            FilePosition position{};
            if (! (iss >> stepNumber >> position))
                throw std::runtime_error("Invalid line format in file: " + fileNameIndex);

            StepOffsetInfo info{position, std::nullopt};

            // Check if we have the optional "(columns-rows[-slices])" part
            std::string rangePart;
            if (iss >> rangePart)
            {
                std::regex rangeRegex(R"(\((\d+)-(\d+)(?:-(\d+))?\))");
                std::smatch match;
                if (std::regex_match(rangePart, match, rangeRegex))
                {
                    const int columnCount = std::stoi(match[1].str());
                    const int rowsCount = std::stoi(match[2].str());
                    const int slicesCount = match[3].matched ? std::stoi(match[3].str()) : 1;
                    info.sceneSize = ColumnRowSlice::xyz(columnCount, rowsCount, slicesCount);
                }
                else
                {
                    throw std::runtime_error("Invalid range format in file: " + fileNameIndex + " line: " + line);
                }
            }

            const auto [it, inserted] = nodeStepOffsets[node].emplace(stepNumber, info);
            if (! inserted)
            {
                std::cerr << std::format("Duplicate stepNumber {} in file '{}' (node {})", stepNumber, fileNameIndex, node) << std::endl;
            }
        }
    }
}

template<CellLike Cell>
std::vector<StepIndex> ModelReader<Cell>::availableSteps(bool throwOnMismatch) const
{
    if (nodeStepOffsets.empty())
    {
        const auto errorMessage = "Warning: availableSteps() called on an empty stage.";
        if (throwOnMismatch)
        {
            throw std::runtime_error(errorMessage);
        }
        else
        {
            std::cerr << errorMessage << std::endl;
        }
        return {};
    }

    // Helper lambda: extracts all step indices (keys) from a map and returns them sorted.
    auto extractAndSortStepIndices = [](const auto& map) -> std::vector<StepIndex>
    {
        auto steps = std::views::keys(map);
        std::vector<StepIndex> steps2Return(steps.begin(), steps.end());
        std::ranges::sort(steps2Return);
        return steps2Return;
    };

    // Use the first node as the reference
    const auto& fistNodeData = nodeStepOffsets.front();

    // Collect and sort all step indices from the first node
    auto firstNodeSteps = extractAndSortStepIndices(fistNodeData);

    // Compare each node's step list against the reference
    for (NodeIndex node = 1; node < nodeStepOffsets.size(); ++node)
    {
        const auto& nodeMap = nodeStepOffsets[node];
        if (nodeMap.size() != fistNodeData.size())
        {
            const std::string msg = std::format("Step count mismatch for node {} (expected {}, found {})",
                                                node,
                                                fistNodeData.size(),
                                                nodeMap.size());
            if (throwOnMismatch)
                throw std::runtime_error(msg);
            else
                std::cerr << "Warning: " << msg << '\n';
        }

        // Extract steps for comparison
        auto nodeSteps = extractAndSortStepIndices(nodeMap);

        // Compare with reference set
        if (! std::ranges::equal(firstNodeSteps, nodeSteps))
        {
            const std::string msg = std::format("Inconsistent step indices detected in node {}.", node);
            if (throwOnMismatch)
                throw std::runtime_error(msg);
            else
                std::cerr << "[Warning] " << msg << '\n';
        }
    }

    // Return the sorted list of unique steps
    return firstNodeSteps;
}

template<CellLike Cell>
FilePosition ModelReader<Cell>::getStepStartingPositionInFile(StepIndex step, NodeIndex node) const
{
    if (node >= nodeStepOffsets.size())
    {
        throw std::out_of_range(std::format("Invalid node index {} (available nodes: {})", node, nodeStepOffsets.size()));
    }

    const auto& stepMap = nodeStepOffsets[node];
    if (auto it = stepMap.find(step); it != stepMap.end())
    {
        return it->second.position;
    }

    // Step not found - find closest available steps
    if (stepMap.empty())
    {
        throw std::out_of_range(std::format("Step {} not found in node {} (no steps available)", step, node));
    }

    // Find closest previous and next steps
    std::optional<StepIndex> prevStep;
    std::optional<StepIndex> nextStep;

    for (const auto& [availableStep, _] : stepMap) // unordered_map does not have lower/upper_bound
    {
        if (availableStep < step)
        {
            if (!prevStep || availableStep > *prevStep)
            {
                prevStep = availableStep;
            }
        }
        else if (availableStep > step)
        {
            if (!nextStep || availableStep < *nextStep)
            {
                nextStep = availableStep;
            }
        }
    }

    // Build error message with nearest steps
    std::string nearestInfo;
    if (prevStep && nextStep)
    {
        nearestInfo = std::format("Nearest steps: {} (previous), {} (next)", *prevStep, *nextStep);
    }
    else if (prevStep)
    {
        nearestInfo = std::format("Nearest step: {} (previous)", *prevStep);
    }
    else if (nextStep)
    {
        nearestInfo = std::format("Nearest step: {} (next)", *nextStep);
    }

    throw std::out_of_range(std::format("Step {} not found in node {}. {}", step, node, nearestInfo));
}
