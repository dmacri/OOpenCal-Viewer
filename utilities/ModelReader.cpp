#include "ModelReader.hpp"


ColumnAndRow ReaderHelpers::getColumnAndRowFromLine(const std::string& line)
{
    /// input format: "C-R" where C and R are numbers
    if (line.empty())
    {
        throw std::invalid_argument("Line is empty, but it should contain columns and row!");
    }

    const auto delimiterPos = line.find('-');
    if (delimiterPos == std::string::npos)
    {
        throw std::runtime_error("No delimiter '-' found in the line: >" + line + "<");
    }

    const auto x = std::stoi(line.substr(0, delimiterPos));
    const auto y = std::stoi(line.substr(delimiterPos + 1));
    return ColumnAndRow::xy(x, y);
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
