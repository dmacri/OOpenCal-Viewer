/** @file Visualizer.hpp
 * @brief Declaration of the Visualizer class for VTK-based visualization. */

#pragma once

#include <vtkActor2D.h>
#include <vtkCellArray.h>
#include <vtkCoordinate.h>
#include <vtkDataSetMapper.h>
#include <vtkDoubleArray.h>
#include <vtkLookupTable.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkStructuredGrid.h>
#include <vtkTextMapper.h>
#include <vtkTextProperty.h>
#include <vtkUnsignedCharArray.h>

#include "utilities/types.h"    // StepIndex
#include "OOpenCAL/base/Cell.h"  // Color

class Line;

/** @class Visualizer
 * @brief Handles VTK-based visualization of simulation data.
 * 
 * This class provides methods to render and update 2D visualizations
 * of simulation data using the VTK library. It supports drawing grids,
 * color mapping, and text annotations. */
class Visualizer
{
public:
    template<class Matrix>
    void drawWithVTK(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor);
    template<class Matrix>
    void refreshWindowsVTK(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor);

    /// @brief Draw 3D substate visualization using cell colors from outputValue().
    template<class Matrix>
    void drawWithVTK3DSubstate(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue);
    /// @brief Refresh 3D substate visualization with cell colors from outputValue().
    template<class Matrix>
    void refreshWindowsVTK3DSubstate(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue);

    void buildLoadBalanceLine(const std::vector<Line>& lines, int nRows, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor2D> actorBuildLine);
    void refreshBuildLoadBalanceLine(const std::vector<Line> &lines, int nRows, vtkActor2D* lineActor);
    vtkTextProperty* buildStepLine(StepIndex step, vtkSmartPointer<vtkTextMapper> singleLineTextB);
    vtkNew<vtkActor2D> buildStepText(StepIndex step, int font_size, vtkSmartPointer<vtkTextMapper> stepLineTextMapper, vtkSmartPointer<vtkRenderer> renderer);

private:
    template<class Matrix>
    void buidColor(vtkLookupTable* lut, int nCols, int nRows, const Matrix& p);

    /** @brief Build 3D points and colors for 3D substate visualization using cell colors.
     * 
     * This helper method extracts substate values and colors from cells using outputValue().
     * Builds points with height-mapped Z coordinates and RGB color values.
     * 
     * @param p Matrix of cells containing substate data
     * @param nRows Number of grid rows
     * @param nCols Number of grid columns
     * @param substateFieldName Name of the substate field to extract
     * @param minValue Minimum value for normalization
     * @param maxValue Maximum value for normalization
     * @param heightScale Scale factor for Z height
     * @param points Output vtkPoints to populate
     * @param pointValues Output vtkUnsignedCharArray with RGB color values */
    template<class Matrix>
    void build3DSubstatePoints(const Matrix& p, int nRows, int nCols, const std::string& substateFieldName,
                               double minValue, double maxValue, double heightScale,
                               vtkPoints* points, vtkUnsignedCharArray* pointColors);

    /** @brief Creates a vtkPolyData representing a set of 2D lines.
      * @param lines Vector of Line objects (each defines a line segment)
      * @param nRows Number of grid rows (used to invert Y coordinates)
      * @return vtkSmartPointer<vtkPolyData> with points and lines set */
    vtkSmartPointer<vtkPolyData> createLinePolyData(const std::vector<Line>& lines, int nRows);
};

////////////////////////////////////////////////////////////////////

template <class Matrix>
void Visualizer::drawWithVTK(const Matrix &p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor)
{
    const auto numberOfPoints = nRows * nCols;
    vtkNew<vtkDoubleArray> pointValues;
    pointValues->SetNumberOfTuples(numberOfPoints);
    
    // Set scalar values: points are inserted in order (row, col), but colors are indexed by (nRows - 1 - row) * nCols + col
    // So we need to map point index to color index
    for (int row = 0; row < nRows; row++)
    {
        for (int col = 0; col < nCols; col++)
        {
            int pointIndex = row * nCols + col;  // Sequential point index
            int colorIndex = (nRows - 1 - row) * nCols + col;  // Color index from buidColor()
            pointValues->SetValue(pointIndex, colorIndex);
        }
    }

    vtkNew<vtkLookupTable> lut;
    lut->SetNumberOfTableValues(numberOfPoints);

    vtkNew<vtkPoints> points;
    for (int row = 0; row < nRows; row++)
    {
        for (int col = 0; col < nCols; col++)
        {
            // Insert points with Y inverted to match buidColor() indexing
            // buidColor() uses (nRows - 1 - row) so points must be positioned accordingly
            points->InsertNextPoint(/*x=*/col, /*y=*/nRows - 1 - row, /*z=*/1); /// z is not used
        }
    }

    vtkNew<vtkStructuredGrid> structuredGrid;
    structuredGrid->SetDimensions(nCols, nRows, 1);
    structuredGrid->SetPoints(points);
    structuredGrid->GetPointData()->SetScalars(pointValues);

    buidColor(lut, nCols, nRows, p);

    vtkNew<vtkDataSetMapper> gridMapper;
    gridMapper->UpdateDataObject();
    gridMapper->SetInputData(structuredGrid);
    gridMapper->SetLookupTable(lut);
    gridMapper->SetScalarRange(0, numberOfPoints - 1);

    gridActor->SetMapper(gridMapper);
    renderer->AddActor(gridActor);
}

template<class Matrix>
void Visualizer::refreshWindowsVTK(const Matrix &p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor)
{
    if (vtkLookupTable* lut = dynamic_cast<vtkLookupTable*>(gridActor->GetMapper()->GetLookupTable()))
    {
        buidColor(lut, nCols, nRows, p);
        gridActor->GetMapper()->SetLookupTable(lut);
        gridActor->GetMapper()->Update();
    }
    else
        throw std::runtime_error("Invalid dynamic cast!");
}

/** @brief Converts a color channel value to a normalized range [0, 1].
 *
 * This function is designed to be forward-compatible with upcoming changes in OOpenCal.
 * Currently, OOpenCal provides color values in the 0–255 integer range. VTK, however,
 * expects normalized double precision values in the 0–1 range.
 *
 * If the input value is greater than 1, it is assumed to be in the 0–255 range
 * and will be scaled down to [0, 1]. If the value is already in the [0, 1] range
 * (future OOpenCal format), it will be returned unchanged.
 *
 * @param channel The input color component (either in 0–255 or already in 0–1).
 * @return Normalized color value in the range [0, 1]. */
inline double toUnitColor(double channel)
{
    // Backward compatibility: if value is > 1, assume 0–255 format and scale.
    if (channel > 1.0)
        return channel / 255.0;

    // Already normalized (future format) — return as-is.
    return channel;
}

template<class Matrix>
void Visualizer::buidColor(vtkLookupTable* lut, int nCols, int nRows, const Matrix &p)
{
    for (int r = 0; r < nRows; ++r)
    {
        for (int c = 0; c < nCols; ++c)
        {
            const auto color = p[r][c].outputValue(nullptr);
            lut->SetTableValue(
                (nRows - 1 - r) * nCols + c,
                toUnitColor(color.getRed()),
                toUnitColor(color.getGreen()),
                toUnitColor(color.getBlue()),
                1.0 // alpha channel – keep as 1.0 for full opacity (optional scaling)
            );
        }
    }
}

////////////////////////////////////////////////////////////////////
// 3D Substate Visualization Methods

template <class Matrix>
void Visualizer::drawWithVTK3DSubstate(const Matrix &p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue)
{
    vtkNew<vtkUnsignedCharArray> pointColors;

    vtkNew<vtkPoints> points;

    // Validate min/max values
    if (std::isnan(minValue) || std::isnan(maxValue) || minValue >= maxValue)
    {
        // Fallback to regular 2D visualization if invalid values
        drawWithVTK(p, nRows, nCols, renderer, gridActor);
        return;
    }

    // Calculate height scale factor based on grid size
    // Scale to approximately 1/3 of the grid size for good visibility
    double heightScale = std::max(nRows, nCols) / 3.0;

    // Build 3D points with height based on substate value and colors using helper method
    build3DSubstatePoints(p, nRows, nCols, substateFieldName, minValue, maxValue, heightScale, points, pointColors);

    vtkNew<vtkStructuredGrid> structuredGrid;
    structuredGrid->SetDimensions(nCols, nRows, 1);
    structuredGrid->SetPoints(points);
    structuredGrid->GetPointData()->SetScalars(pointColors);

    vtkNew<vtkDataSetMapper> gridMapper;
    gridMapper->UpdateDataObject();
    gridMapper->SetInputData(structuredGrid);
    gridMapper->SetScalarModeToUsePointData();

    gridActor->SetMapper(gridMapper);
    renderer->AddActor(gridActor);
}

template<class Matrix>
void Visualizer::refreshWindowsVTK3DSubstate(const Matrix &p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue)
{
    if (auto mapper = gridActor->GetMapper())
    {
        // Validate min/max values
        if (std::isnan(minValue) || std::isnan(maxValue) || minValue >= maxValue)
            return;

        // Calculate height scale factor based on grid size
        // Scale to approximately 1/3 of the grid size for good visibility
        double heightScale = std::max(nRows, nCols) / 3.0;

        vtkNew<vtkUnsignedCharArray> pointColors;

        vtkNew<vtkPoints> points;

        // Rebuild points with new substate values and colors using helper method
        build3DSubstatePoints(p, nRows, nCols, substateFieldName, minValue, maxValue, heightScale, points, pointColors);

        // Update the structured grid with new points and colors
        if (auto structuredGrid = vtkStructuredGrid::SafeDownCast(mapper->GetInput()))
        {
            structuredGrid->SetPoints(points);
            structuredGrid->GetPointData()->SetScalars(pointColors);
            mapper->Update();
        }
    }
}

template<class Matrix>
void Visualizer::build3DSubstatePoints(const Matrix& p, int nRows, int nCols, const std::string& substateFieldName,
                                                double minValue, double maxValue, double heightScale,
                                                vtkPoints* points, vtkUnsignedCharArray* pointColors)
{
    double valueRange = maxValue - minValue;
    if (valueRange < 1e-10)
        valueRange = 1.0;

    // Set up color array for RGB values (3 components per point)
    pointColors->SetNumberOfComponents(3);
    pointColors->SetNumberOfTuples(nRows * nCols);

    // Build 3D points with height based on substate value and colors from cell's main data
    for (int row = 0; row < nRows; row++)
    {
        for (int col = 0; col < nCols; col++)
        {
            // Get the substate value for this cell (for height)
            std::string cellValueStr = p[row][col].stringEncoding(substateFieldName.c_str());
            double cellValue = 0.0;
            
            try
            {
                cellValue = std::stod(cellValueStr);
            }
            catch (const std::exception& e)
            {
                cellValue = minValue; // Default to min if parsing fails
                cout << "\t! Conversion error:" << cellValue << ", " << e.what() << '\n';
            }

            cellValue = std::clamp(cellValue, minValue, maxValue);

            // Calculate normalized height (0.0 to 1.0) then scale to grid size
            double normalizedHeight = (cellValue - minValue) / valueRange;
            double scaledHeight = normalizedHeight * heightScale;

            // Insert point with Z coordinate as height
            // Y is inverted to match buidColor() indexing: (nRows - 1 - row)
            points->InsertNextPoint(col, nRows - 1 - row, scaledHeight);

            // Get color from cell's main data (outputValue with nullptr), not from substate
            // This ensures we use the cell's actual color representation
            Color cellColor = p[row][col].outputValue(nullptr);
            
            // Extract RGB values from Color (already in 0-255 range)
            unsigned char r = cellColor.getRed();
            unsigned char g = cellColor.getGreen();
            unsigned char b = cellColor.getBlue();
            
            // Store RGB color for this point
            // Map sequential point index to the color index
            int pointIndex = row * nCols + col;  // Sequential point index
            pointColors->SetTuple3(pointIndex, r, g, b);
        }
    }
}
