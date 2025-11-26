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
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkStructuredGrid.h>
#include <vtkTextMapper.h>
#include <vtkTextProperty.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPolyDataNormals.h>
#include <vtkCellData.h>
#include <vtkProperty.h>
#include <cmath>

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

    /// @brief Draw 3D substate visualization as a quad mesh surface (new healed quad approach).
    template<class Matrix>
    void drawWithVTK3DSubstate(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue);
    /// @brief Refresh 3D substate visualization as a quad mesh surface (new healed quad approach).
    template<class Matrix>
    void refreshWindowsVTK3DSubstate(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue);

    /// @brief Draw flat background plane at Z=0 for 3D visualization.
    template<class Matrix>
    void drawFlatSceneBackground(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> backgroundActor);
    /// @brief Refresh flat background plane colors.
    template<class Matrix>
    void refreshFlatSceneBackground(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkActor> backgroundActor);

    void buildLoadBalanceLine(const std::vector<Line>& lines, int nRows, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor2D> actorBuildLine);
    void refreshBuildLoadBalanceLine(const std::vector<Line> &lines, int nRows, vtkActor2D* lineActor);
    vtkTextProperty* buildStepLine(StepIndex step, vtkSmartPointer<vtkTextMapper> singleLineTextB);
    vtkNew<vtkActor2D> buildStepText(StepIndex step, int font_size, vtkSmartPointer<vtkTextMapper> stepLineTextMapper, vtkSmartPointer<vtkRenderer> renderer);

private:
    template<class Matrix>
    void buidColor(vtkLookupTable* lut, int nCols, int nRows, const Matrix& p);

    /** @brief Build 3D quad mesh surface for 3D substate visualization (healed quad approach).
     * 
     * Creates a quad mesh where each grid cell becomes a quadrilateral.
     * If a cell has at least 2 valid corners, missing corners are filled using average height.
     * Returns vtkPolyData with quads and RGB colors.
     * 
     * @param p Matrix of cells containing substate data
     * @param nRows Number of grid rows
     * @param nCols Number of grid columns
     * @param substateFieldName Name of the substate field to extract
     * @param minValue Minimum value for normalization
     * @param maxValue Maximum value for normalization
     * @return vtkSmartPointer<vtkPolyData> with quad mesh and colors */
    template<class Matrix>
    vtkSmartPointer<vtkPolyData> build3DSubstateSurfaceQuadMesh(const Matrix& p, int nRows, int nCols,
                                                                const std::string& substateFieldName,
                                                                double minValue, double maxValue);

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
template<class Matrix>
vtkSmartPointer<vtkPolyData> Visualizer::build3DSubstateSurfaceQuadMesh(const Matrix& p, int nRows, int nCols,
                                                                        const std::string& substateFieldName,
                                                                        double minValue, double maxValue)
{
    // Validate min/max values
    if (std::isnan(minValue) || std::isnan(maxValue) || minValue >= maxValue)
    {
        return vtkSmartPointer<vtkPolyData>::New();
    }

    const double valueRange = std::max(1e-12, maxValue - minValue);
    const double heightScale = std::max(nRows, nCols) / 3.0;
    const double eps = 1e-9;

    // Helper lambda to get cell value
    auto getCellValue = [&](int row, int col) -> double {
        if (row < 0 || row >= nRows || col < 0 || col >= nCols)
            return minValue;
        
        try {
            std::string cellValueStr = p[row][col].stringEncoding(substateFieldName.c_str());
            double cellValue = std::stod(cellValueStr);
            return std::clamp(cellValue, minValue, maxValue);
        } catch (...) {
            return minValue;
        }
    };

    // Helper lambda to get cell color
    auto getCellColor = [&](int row, int col) -> Color {
        if (row < 0 || row >= nRows || col < 0 || col >= nCols)
            return Color(0, 0, 0);
        return p[row][col].outputValue(nullptr);
    };

    // Helper lambda to check if value is valid (not no-data)
    auto isValidValue = [&](double val) -> bool {
        return !std::isnan(val) && std::fabs(val - minValue) > eps;
    };

    // Helper lambda to convert value to Z height
    auto valueToHeight = [&](double val) -> double {
        double normalized = (val - minValue) / valueRange;
        normalized = std::clamp(normalized, 0.0, 1.0);
        return normalized * heightScale;
    };

    // Helper lambda to convert grid coordinates to VTK coordinates
    auto gridToVtk = [&](int row, int col) -> std::pair<double, double> {
        return {static_cast<double>(col), static_cast<double>(nRows - 1 - row)};
    };

    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> cells;
    vtkNew<vtkUnsignedCharArray> cellColors;
    cellColors->SetNumberOfComponents(3);

    // Build base points (one per grid location)
    std::vector<vtkIdType> basePointId(nRows * nCols, -1);
    
    for (int row = 0; row < nRows; row++)
    {
        for (int col = 0; col < nCols; col++)
        {
            double z0 = getCellValue(row, col);
            double height = valueToHeight(z0);
            auto [x, y] = gridToVtk(row, col);
            
            vtkIdType pid = points->InsertNextPoint(x, y, height);
            basePointId[row * nCols + col] = pid;
        }
    }

    // Helper lambda to add virtual point (for healed quads)
    auto addVirtualPoint = [&](int row, int col, double zraw) -> vtkIdType {
        double height = valueToHeight(zraw);
        auto [x, y] = gridToVtk(row, col);
        return points->InsertNextPoint(x, y, height);
    };

    // Build quad cells (healed quad approach)
    for (int row = 0; row + 1 < nRows; row++)
    {
        for (int col = 0; col + 1 < nCols; col++)
        {
            // Get the four corner values
            double z0 = getCellValue(row, col);
            double z1 = getCellValue(row, col + 1);
            double z2 = getCellValue(row + 1, col + 1);
            double z3 = getCellValue(row + 1, col);

            bool v0 = isValidValue(z0);
            bool v1 = isValidValue(z1);
            bool v2 = isValidValue(z2);
            bool v3 = isValidValue(z3);

            int validCount = (int)v0 + (int)v1 + (int)v2 + (int)v3;

            // Healed quad: if at least 2 valid corners, fill missing with average
            if (validCount < 2)
                continue;

            // Calculate average of valid corners
            double sum = 0.0;
            if (v0) sum += z0;
            if (v1) sum += z1;
            if (v2) sum += z2;
            if (v3) sum += z3;
            double avg = sum / std::max(1, validCount);

            // Get or create point IDs
            vtkIdType ids[4];
            ids[0] = v0 ? basePointId[row * nCols + col] : addVirtualPoint(row, col, avg);
            ids[1] = v1 ? basePointId[row * nCols + (col + 1)] : addVirtualPoint(row, col + 1, avg);
            ids[2] = v2 ? basePointId[(row + 1) * nCols + (col + 1)] : addVirtualPoint(row + 1, col + 1, avg);
            ids[3] = v3 ? basePointId[(row + 1) * nCols + col] : addVirtualPoint(row + 1, col, avg);

            // Create quad cell
            cells->InsertNextCell(4);
            cells->InsertCellPoint(ids[0]);
            cells->InsertCellPoint(ids[1]);
            cells->InsertCellPoint(ids[2]);
            cells->InsertCellPoint(ids[3]);

            // Add color (use average of valid corner colors)
            Color c0 = getCellColor(row, col);
            Color c1 = getCellColor(row, col + 1);
            Color c2 = getCellColor(row + 1, col + 1);
            Color c3 = getCellColor(row + 1, col);

            int rSum = 0, gSum = 0, bSum = 0;
            if (v0) { rSum += c0.getRed(); gSum += c0.getGreen(); bSum += c0.getBlue(); }
            if (v1) { rSum += c1.getRed(); gSum += c1.getGreen(); bSum += c1.getBlue(); }
            if (v2) { rSum += c2.getRed(); gSum += c2.getGreen(); bSum += c2.getBlue(); }
            if (v3) { rSum += c3.getRed(); gSum += c3.getGreen(); bSum += c3.getBlue(); }

            unsigned char r = static_cast<unsigned char>(rSum / validCount);
            unsigned char g = static_cast<unsigned char>(gSum / validCount);
            unsigned char b = static_cast<unsigned char>(bSum / validCount);

            cellColors->InsertNextTuple3(r, g, b);
        }
    }

    // Create polydata
    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetPolys(cells);
    polyData->GetCellData()->SetScalars(cellColors);

    // Compute normals for proper shading
    vtkNew<vtkPolyDataNormals> normals;
    normals->SetInputData(polyData);
    normals->AutoOrientNormalsOn();
    normals->ComputePointNormalsOn();
    normals->ComputeCellNormalsOff();
    normals->ConsistencyOn();
    normals->SplittingOff();
    normals->Update();

    return normals->GetOutput();
}

template<class Matrix>
void Visualizer::drawWithVTK3DSubstate(const Matrix& p, int nRows, int nCols,
                                       vtkSmartPointer<vtkRenderer> renderer,
                                       vtkSmartPointer<vtkActor> gridActor,
                                       const std::string& substateFieldName,
                                       double minValue, double maxValue)
{
    // Validate min/max values
    if (std::isnan(minValue) || std::isnan(maxValue) || minValue >= maxValue)
    {
        // Fallback to regular 2D visualization if invalid values
        drawWithVTK(p, nRows, nCols, renderer, gridActor);
        return;
    }

    // Remove only the grid actor from renderer (preserve other actors like background)
    if (gridActor && renderer)
    {
        renderer->RemoveActor(gridActor);
    }

    // Build quad mesh surface
    vtkSmartPointer<vtkPolyData> surfacePolyData = build3DSubstateSurfaceQuadMesh(p, nRows, nCols, substateFieldName, minValue, maxValue);

    // Create mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(surfacePolyData);
    mapper->SetScalarModeToUseCellData();
    mapper->ScalarVisibilityOn();

    // Configure actor
    gridActor->SetMapper(mapper);
    gridActor->GetProperty()->SetInterpolationToPhong();
    gridActor->GetProperty()->SetAmbient(0.3);
    gridActor->GetProperty()->SetDiffuse(0.7);
    gridActor->GetProperty()->SetSpecular(0.1);
    gridActor->GetProperty()->SetSpecularPower(10.0);
    gridActor->GetProperty()->EdgeVisibilityOff();

    // Add actor to renderer
    renderer->AddActor(gridActor);
    renderer->ResetCamera();
}

template<class Matrix>
void Visualizer::refreshWindowsVTK3DSubstate(const Matrix& p, int nRows, int nCols,
                                             vtkSmartPointer<vtkActor> gridActor,
                                             const std::string& substateFieldName,
                                             double minValue, double maxValue)
{
    // Validate min/max values
    if (std::isnan(minValue) || std::isnan(maxValue) || minValue >= maxValue)
        return;

    if (auto mapper = vtkPolyDataMapper::SafeDownCast(gridActor->GetMapper()))
    {
        // Build new quad mesh surface
        vtkSmartPointer<vtkPolyData> surfacePolyData = build3DSubstateSurfaceQuadMesh(p, nRows, nCols, substateFieldName, minValue, maxValue);

        // Update mapper with new data
        mapper->SetInputData(surfacePolyData);
        mapper->Update();
    }
}

// Flat Scene Background Methods

template<class Matrix>
void Visualizer::drawFlatSceneBackground(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> backgroundActor)
{
    // Validate inputs
    if (!backgroundActor || !renderer)
    {
        return;
    }

    const auto numberOfPoints = nRows * nCols;
    vtkNew<vtkDoubleArray> pointValues;
    pointValues->SetNumberOfTuples(numberOfPoints);
    
    // Set scalar values - all same value for uniform color
    for (int row = 0; row < nRows; row++)
    {
        for (int col = 0; col < nCols; col++)
        {
            int pointIndex = row * nCols + col;
            pointValues->SetValue(pointIndex, 0);  // All points have same value for uniform color
        }
    }

    vtkNew<vtkLookupTable> lut;
    lut->SetNumberOfTableValues(1);  // Only one color needed
    
    // Set uniform light gray color for the background plane
    lut->SetTableValue(0, 0.8, 0.8, 0.8, 1.0);  // Light gray, fully opaque

    // Create flat plane at Z=0
    vtkNew<vtkPoints> points;
    for (int row = 0; row < nRows; row++)
    {
        for (int col = 0; col < nCols; col++)
        {
            // Z=0 for flat background plane
            points->InsertNextPoint(/*x=*/col, /*y=*/nRows - 1 - row, /*z=*/0);
        }
    }

    vtkNew<vtkStructuredGrid> structuredGrid;
    structuredGrid->SetDimensions(nCols, nRows, 1);
    structuredGrid->SetPoints(points);
    structuredGrid->GetPointData()->SetScalars(pointValues);

    vtkNew<vtkDataSetMapper> backgroundMapper;
    backgroundMapper->UpdateDataObject();
    backgroundMapper->SetInputData(structuredGrid);
    backgroundMapper->SetLookupTable(lut);
    backgroundMapper->SetScalarRange(0, 0);  // Single color

    backgroundActor->SetMapper(backgroundMapper);
    renderer->AddActor(backgroundActor);
}

template<class Matrix>
void Visualizer::refreshFlatSceneBackground(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkActor> backgroundActor)
{
    // Validate input
    if (!backgroundActor || !backgroundActor->GetMapper())
    {
        return;
    }

    if (vtkLookupTable* lut = dynamic_cast<vtkLookupTable*>(backgroundActor->GetMapper()->GetLookupTable()))
    {
        // Keep uniform light gray color - no need to update from cell data
        lut->SetTableValue(0, 0.8, 0.8, 0.8, 1.0);  // Light gray, fully opaque
        backgroundActor->GetMapper()->SetLookupTable(lut);
        backgroundActor->GetMapper()->Update();
    }
}
