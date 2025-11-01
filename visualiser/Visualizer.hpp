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

#include "utilities/types.h"    // StepIndex

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
    void buildLoadBalanceLine(const std::vector<Line>& lines, int nRows, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor2D> actorBuildLine);
    void refreshBuildLoadBalanceLine(const std::vector<Line> &lines, int nRows, vtkActor2D* lineActor);
    vtkTextProperty* buildStepLine(StepIndex step, vtkSmartPointer<vtkTextMapper> singleLineTextB);
    vtkNew<vtkActor2D> buildStepText(StepIndex step, int font_size, vtkSmartPointer<vtkTextMapper> stepLineTextMapper, vtkSmartPointer<vtkRenderer> renderer);

private:
    template<class Matrix>
    void buidColor(vtkLookupTable* lut, int nCols, int nRows, const Matrix& p);

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
    for (int i = 0; i < numberOfPoints; ++i)
    {
        pointValues->SetValue(i, i);
    }

    vtkNew<vtkLookupTable> lut;
    lut->SetNumberOfTableValues(numberOfPoints);

    vtkNew<vtkPoints> points;
    for (int row = 0; row < nRows; row++)
    {
        for (int col = 0; col < nCols; col++)
        {
            points->InsertNextPoint(col, row, 1);
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
