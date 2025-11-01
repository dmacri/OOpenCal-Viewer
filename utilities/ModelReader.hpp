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

#include "types.h"
#include "visualiser/Line.h"
#include "visualiser/SettingParameter.h"

/** @class ModelReader
 * @brief Template class for reading and processing model data from files.
 * 
 * The ModelReader class provides functionality to read model data in stages and
 * access it at different time steps. It's designed to work with different cell types
 * through template specialization.
 * 
 * @tparam Cell The cell type used in the model 
 * Note: Cell is derived class from Element (which is header from OOpenCal) */
template<class Cell>
class ModelReader
{
public:
    struct StepOffsetInfo
    {
        FilePosition position;
        std::optional<ColumnAndRow> sceneSize;
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
     * @param nNodeY Number of nodes along the Y axis */
    void prepareStage(NodeIndex nNodeX, NodeIndex nNodeY)
    {
        nodeStepOffsets.resize(nNodeX * nNodeY);
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
     * @param filename Name of the file containing the step offsets
     *
     * @throws std::runtime_error If the file cannot be opened or has an invalid format */
    void readStepsOffsetsForAllNodesFromFiles(NodeIndex nNodeX, NodeIndex nNodeY, const std::string& filename);

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
                                                                               ColumnAndRow& columnAndRow,
                                                                               bool isBinary = false);

    [[nodiscard]] ColumnAndRow readColumnAndRowForStepFromFile(StepIndex step, const std::string& fileName, NodeIndex node, bool isBinary = false);

    std::vector<ColumnAndRow> giveMeLocalColsAndRowsForAllSteps(StepIndex step,
                                                                NodeIndex nNodeX,
                                                                NodeIndex nNodeY,
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
} // namespace ReaderHelpers
/////////////////////////////

template<class Cell>
ColumnAndRow ModelReader<Cell>::readColumnAndRowForStepFromFile(StepIndex step, const std::string& fileName, NodeIndex node, bool isBinary)
{
    ColumnAndRow columnAndRow;
    std::ifstream file [[maybe_unused]] = readColumnAndRowForStepFromFileReturningStream(step, fileName, node, columnAndRow, isBinary);
    return columnAndRow;
}

template<class Cell>
std::ifstream ModelReader<Cell>::readColumnAndRowForStepFromFileReturningStream(StepIndex step,
                                                                                const std::string& fileName,
                                                                                NodeIndex node,
                                                                                ColumnAndRow& columnAndRow,
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
            columnAndRow = it->second.sceneSize.value();
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

        columnAndRow = ReaderHelpers::getColumnAndRowFromLine(line);
    }

    return file;
}

template<class Cell>
template<class Matrix>
void ModelReader<Cell>::readStageStateFromFilesForStep(Matrix& m, SettingParameter* sp, Line* lines)
{
    const auto totalNodes = sp->nNodeX * sp->nNodeY;
    const bool isBinary = (sp->readMode == "binary");
    const auto columnsAndRows = giveMeLocalColsAndRowsForAllSteps(sp->step, sp->nNodeX, sp->nNodeY, sp->outputFileName, isBinary);

    /// Lambda responsible for reading and processing a single node's file
    auto processNode = [&, this](NodeIndex node)
    {
        const auto offsetXY = ReaderHelpers::calculateXYOffsetForNode(node, sp->nNodeX, sp->nNodeY, columnsAndRows);

        ColumnAndRow columnAndRow;
        std::ifstream fp = readColumnAndRowForStepFromFileReturningStream(sp->step, sp->outputFileName, node, columnAndRow, isBinary);
        if (! fp)
            throw std::runtime_error("Cannot open file for node " + std::to_string(node));

        // Define boundary lines for the node (bottom and left edges)
        lines[node * 2] = Line(offsetXY.x(), offsetXY.y(), offsetXY.x() + columnAndRow.column, offsetXY.y());
        lines[node * 2 + 1] = Line(offsetXY.x(), offsetXY.y(), offsetXY.x(), offsetXY.y() + columnAndRow.row);

        // Add top edge line for nodes in the last row (highest y)
        const NodeIndex nodeRow = node / sp->nNodeX;
        if (nodeRow == sp->nNodeY - 1) // Top row
        {
            const int topLineIndex = 2 * totalNodes + (node % sp->nNodeX);
            lines[topLineIndex] = Line(offsetXY.x(),
                                       offsetXY.y() + columnAndRow.row,
                                       offsetXY.x() + columnAndRow.column,
                                       offsetXY.y() + columnAndRow.row);
        }

        // Add right edge line for nodes in the last column (highest x)
        const NodeIndex nodeCol = node % sp->nNodeX;
        if (nodeCol == sp->nNodeX - 1) // Rightmost column
        {
            const int rightLineIndex = 2 * totalNodes + sp->nNodeX + nodeRow;
            lines[rightLineIndex] = Line(offsetXY.x() + columnAndRow.column,
                                         offsetXY.y(),
                                         offsetXY.x() + columnAndRow.column,
                                         offsetXY.y() + columnAndRow.row);
        }

        bool localStartStepDone = false;

        if (isBinary)
        {
            // Binary mode: read raw cell data
            const size_t cellCount = columnAndRow.column * columnAndRow.row;
            const size_t cellSize = sizeof(Cell);
            const size_t totalBytes = cellCount * cellSize;

            std::vector<char> buffer(totalBytes);
            fp.read(buffer.data(), totalBytes);

            if (fp.gcount() != static_cast<std::streamsize>(totalBytes))
            {
                throw std::runtime_error(std::format("Failed to read {} bytes from binary file for node {}", totalBytes, node));
            }

            // Parse binary data and fill matrix
            for (int row = 0; row < columnAndRow.row; ++row)
            {
                const int matrixRow = row + offsetXY.y();
                if (matrixRow >= static_cast<int>(m.size())) // TODO: GB: to fix?
                    break; // Skip rows that are out of bounds
                    
                for (int col = 0; col < columnAndRow.column; ++col)
                {
                    const int matrixCol = col + offsetXY.x();
                    if (matrixCol >= static_cast<int>(m[matrixRow].size())) // TODO: GB: to fix?
                        break; // Skip columns that are out of bounds

                    if (! localStartStepDone) [[unlikely]]
                    {
                        m[matrixRow][matrixCol].Cell::startStep(sp->step);
                        localStartStepDone = true;
                    }

                    const size_t cellIndex = row * columnAndRow.column + col;
                    const char* cellData = buffer.data() + (cellIndex * cellSize);

                    // Create a temporary cell from binary data and copy to matrix
                    Cell tempCell;
                    std::memcpy(&tempCell, cellData, cellSize);
                    m[matrixRow][matrixCol] = tempCell;
                }
            }
        }
        else
        {
            // Text mode: read and parse text data (original behavior)
            // Use a thread-local buffer for faster reading
            static thread_local char fileBuffer[1 << 16];
            fp.rdbuf()->pubsetbuf(fileBuffer, sizeof(fileBuffer));

            // Reserve a large line buffer to minimize reallocations
            constexpr std::size_t numbersPerLine = 10'000;
            const std::size_t lineBufferSize = (std::log10(UINT_MAX) + 2) * numbersPerLine;
            std::string line;
            line.reserve(lineBufferSize);

            // Process each line (row) from the node's file
            for (int row = 0; row < columnAndRow.row; ++row)
            {
                const int matrixRow = row + offsetXY.y();
                if (matrixRow >= static_cast<int>(m.size())) // TODO: GB: to fix?
                    break; // Skip rows that are out of bounds
                    
                if (! std::getline(fp, line))
                {
                    const auto fileNameTmp = ReaderHelpers::giveMeFileName(sp->outputFileName, node, isBinary);
                    throw std::runtime_error("Error reading entire line from " + fileNameTmp);
                }

                // Replace spaces with '\0' to tokenize more efficiently
                std::replace(line.begin(), line.end(), ' ', '\0');

                // Tokenize and fill the corresponding part of the matrix
                char* currentTokenPtr = line.data();
                for (int col = 0; col < columnAndRow.column && *currentTokenPtr; ++col)
                {
                    const int matrixCol = col + offsetXY.x();
                    if (matrixCol >= static_cast<int>(m[matrixRow].size())) // TODO: GB: to fix?
                        break; // Skip columns that are out of bounds

                    if (! localStartStepDone) [[unlikely]]
                    {
                        m[matrixRow][matrixCol].Cell::startStep(sp->step);
                        localStartStepDone = true;
                    }

                    /// composeElement() may add extra '\0', so we need extra variable to jump to next position
                    char* nextTokenPtr = std::find(currentTokenPtr, line.data() + line.size(), '\0');
                    ++nextTokenPtr; // skip '\0'

                    m[matrixRow][matrixCol].Cell::composeElement(currentTokenPtr);

                    currentTokenPtr = nextTokenPtr;
                }
            }
        }
    };

    /// Launch async tasks — one per node
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

    // Wait for all async tasks to complete
    std::ranges::for_each(futures,
                          [](std::future<void>& f)
                          {
                              f.get();
                          });
}

template<class Cell>
std::vector<ColumnAndRow> ModelReader<Cell>::giveMeLocalColsAndRowsForAllSteps(StepIndex step,
                                                                               NodeIndex nNodeX,
                                                                               NodeIndex nNodeY,
                                                                               const std::string& fileName,
                                                                               bool isBinary)
{
    const auto nodesCount = nNodeX * nNodeY;
    std::vector<ColumnAndRow> allColumnsAndRows(nodesCount);
    allColumnsAndRows.resize(nodesCount);

    for (NodeIndex node = 0; node < nodesCount; node++)
    {
        allColumnsAndRows[node] = readColumnAndRowForStepFromFile(step, fileName, node, isBinary);
    }
    return allColumnsAndRows;
}

template<class Cell>
void ModelReader<Cell>::readStepsOffsetsForAllNodesFromFiles(NodeIndex nNodeX, NodeIndex nNodeY, const std::string& filename)
{
    const auto totalNodes = nNodeX * nNodeY;
    prepareStage(nNodeX, nNodeY);

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
            if (line.empty()) continue;

            std::istringstream iss(line);
            StepIndex stepNumber{};
            FilePosition position{};
            if (! (iss >> stepNumber >> position))
                throw std::runtime_error("Invalid line format in file: " + fileNameIndex);

            StepOffsetInfo info{position, std::nullopt};

            // Check if we have the optional "(columns-rows)" part
            std::string rangePart;
            if (iss >> rangePart)
            {
                std::regex rangeRegex(R"(\((\d+)-(\d+)\))");
                std::smatch match;
                if (std::regex_match(rangePart, match, rangeRegex))
                {
                    const int columnCount = std::stoi(match[1].str());
                    const int rowsCount = std::stoi(match[2].str());
                    info.sceneSize = ColumnAndRow{.column = columnCount, .row = rowsCount};
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

template<class Cell>
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

template<class Cell>
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

    throw std::out_of_range(std::format("Step {} not found in node {} (available step indices: {})", step, node, stepMap.size() - 1));
}
