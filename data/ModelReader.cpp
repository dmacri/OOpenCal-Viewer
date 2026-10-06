#include "ModelReader.hpp"


ColumnAndRow ReaderHelpers::getColumnAndRowFromLine(const std::string& line)
{
    const auto dimensions = getDimensionsFromLine(line);
    return ColumnAndRow::xy(dimensions.column, dimensions.row);
}

ColumnRowSlice ReaderHelpers::getDimensionsFromLine(const std::string& line)
{
    /// Input format: "C-R" for 2D or "C-R-S" for 3D.
    if (line.empty())
    {
        throw std::invalid_argument("Line is empty, but it should contain grid dimensions!");
    }

    const auto firstDelimiterPos = line.find('-');
    if (firstDelimiterPos == std::string::npos)
    {
        throw std::runtime_error("No delimiter '-' found in the line: >" + line + "<");
    }

    const auto columns = std::stoi(line.substr(0, firstDelimiterPos));
    const auto secondDelimiterPos = line.find('-', firstDelimiterPos + 1);

    if (secondDelimiterPos != std::string::npos)
    {
        const auto rows = std::stoi(line.substr(firstDelimiterPos + 1,
                                                secondDelimiterPos - firstDelimiterPos - 1));
        const auto slices = std::stoi(line.substr(secondDelimiterPos + 1));
        return ColumnRowSlice::xyz(columns, rows, slices);
    }

    const auto rows = std::stoi(line.substr(firstDelimiterPos + 1));
    return ColumnRowSlice::xyz(columns, rows, 1);
}

ColumnAndRow ReaderHelpers::calculateXYOffsetForNode(NodeIndex node,
                                                     NodeIndex nNodeX,
                                                     NodeIndex nNodeY,
                                                     const std::vector<ColumnAndRow>& columnsAndRows)
{
    int offsetX = 0; //= //(node % nNodeX)*nLocalCols;//-this->borderSizeX;
    int offsetY = 0; //= //(node / nNodeX)*nLocalRows;//-this->borderSizeY;

    for (NodeIndex k = (node / nNodeX) * nNodeX; k < node; k++)
    {
        offsetX += columnsAndRows[k].column;
    }

    if (node >= nNodeX)
    {
        for (int k = node - nNodeX; k >= 0;)
        {
            offsetY += columnsAndRows[k].row;
            k -= nNodeX;
        }
    }
    return ColumnAndRow::xy(offsetX, offsetY);
}

ColumnRowSlice ReaderHelpers::calculateXYZOffsetForNode(
    NodeIndex node,
    NodeIndex nNodeX,
    NodeIndex nNodeY,
    NodeIndex nNodeZ,
    const std::vector<ColumnRowSlice>& dimensions)
{
    const auto totalNodes = nNodeX * nNodeY * nNodeZ;
    if (nNodeX == 0 || nNodeY == 0 || nNodeZ == 0 ||
        node >= totalNodes || dimensions.size() < totalNodes)
    {
        throw std::invalid_argument("Invalid node grid while calculating a 3D offset");
    }

    const NodeIndex nodeX = node % nNodeX;
    const NodeIndex nodeY = (node / nNodeX) % nNodeY;
    const NodeIndex nodeZ = node / (nNodeX * nNodeY);

    int offsetX = 0;
    int offsetY = 0;
    int offsetZ = 0;

    auto nodeIndex = [=](NodeIndex x, NodeIndex y, NodeIndex z)
    {
        return z * nNodeX * nNodeY + y * nNodeX + x;
    };

    for (NodeIndex x = 0; x < nodeX; ++x)
        offsetX += dimensions[nodeIndex(x, nodeY, nodeZ)].column;

    for (NodeIndex y = 0; y < nodeY; ++y)
        offsetY += dimensions[nodeIndex(nodeX, y, nodeZ)].row;

    for (NodeIndex z = 0; z < nodeZ; ++z)
        offsetZ += dimensions[nodeIndex(nodeX, nodeY, z)].slice;

    return ColumnRowSlice::xyz(offsetX, offsetY, offsetZ);
}
