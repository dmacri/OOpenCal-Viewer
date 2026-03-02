#include "ModelReader.hpp"


ColumnAndRow ReaderHelpers::getColumnAndRowFromLine(const std::string& line)
{
    /// input format: "C-R" for 2D or "C-R-S" for 3D where C, R, and S are numbers
    /// For 3D models, we only extract C and R (columns and rows), ignoring slices
    if (line.empty())
    {
        throw std::invalid_argument("Line is empty, but it should contain columns and row!");
    }

    const auto firstDelimiterPos = line.find('-');
    if (firstDelimiterPos == std::string::npos)
    {
        throw std::runtime_error("No delimiter '-' found in the line: >" + line + "<");
    }

    const auto x = std::stoi(line.substr(0, firstDelimiterPos));
    
    // Find second delimiter to check if this is 3D format (C-R-S)
    const auto secondDelimiterPos = line.find('-', firstDelimiterPos + 1);
    
    int y;
    if (secondDelimiterPos != std::string::npos)
    {
        // 3D format: C-R-S, extract R from between first and second delimiter
        y = std::stoi(line.substr(firstDelimiterPos + 1, secondDelimiterPos - firstDelimiterPos - 1));
        // Note: We ignore the slice count (S) here as it's handled separately
    }
    else
    {
        // 2D format: C-R
        y = std::stoi(line.substr(firstDelimiterPos + 1));
    }
    
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
