#include <limits>
#include "Line.h"
#include "visualiser/Visualizer.hpp"
#include "widgets/ColorSettings.h" // ColorSettings


namespace
{
vtkColor3d toVtkColor(QColor color)
{
    return vtkColor3d{ color.redF(), color.greenF(), color.blueF() };
}
} // namespace


void Visualizer::buildLoadBalanceLine(const std::vector<Line>& lines,
                                      int nRows,
                                      vtkSmartPointer<vtkRenderer> renderer,
                                      vtkSmartPointer<vtkActor2D> actorBuildLine)
{
    // 1. Build line geometry data
    auto grid = createLinePolyData(lines, nRows);

    // 2. Setup coordinate system
    vtkNew<vtkCoordinate> normCoords;
    normCoords->SetCoordinateSystemToWorld();

    // 3. Create 2D mapper
    vtkNew<vtkPolyDataMapper2D> mapper;
    mapper->SetInputData(grid);
    mapper->SetTransformCoordinate(normCoords);
    mapper->Update();

    // 4. Configure actor
    actorBuildLine->SetMapper(mapper);
    actorBuildLine->GetMapper()->Update();

    const QColor gridColor = ColorSettings::instance().gridColor();
    actorBuildLine->GetProperty()->SetColor(toVtkColor(gridColor).GetData());
    // actorBuildLine->GetProperty()->SetLineWidth(1.5);

    // 5. Add to renderer
    renderer->AddViewProp(actorBuildLine);
}

vtkSmartPointer<vtkPolyData> Visualizer::createLinePolyData(const std::vector<Line>& lines, int nRows)
{
    vtkNew<vtkPoints> pts;
    vtkNew<vtkCellArray> cellLines;

    // Small offset to move grid lines slightly outside the scene to avoid obscuring data at corners
    // This offset is in world coordinates (typically pixels)
    constexpr double GRID_LINE_OFFSET = 0.5;

    // Find the bounds of all lines to determine scene extent
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    for (const auto& line : lines)
    {
        minX = std::min(minX, static_cast<double>(line.x1));
        minX = std::min(minX, static_cast<double>(line.x2));
        maxX = std::max(maxX, static_cast<double>(line.x1));
        maxX = std::max(maxX, static_cast<double>(line.x2));
        minY = std::min(minY, static_cast<double>(line.y1));
        minY = std::min(minY, static_cast<double>(line.y2));
        maxY = std::max(maxY, static_cast<double>(line.y1));
        maxY = std::max(maxY, static_cast<double>(line.y2));
    }

    for (size_t i = 0; i < lines.size(); ++i)
    {
        double x1 = lines[i].x1;
        double y1 = nRows - 1 - lines[i].y1;
        double x2 = lines[i].x2;
        double y2 = nRows - 1 - lines[i].y2;

        // Apply offset based on which edge the line is on
        // Left edge (x == minX)
        if (x1 == minX && x2 == minX)
            x1 = x2 = minX - GRID_LINE_OFFSET;
        // Right edge (x == maxX)
        else if (x1 == maxX && x2 == maxX)
            x1 = x2 = maxX + GRID_LINE_OFFSET;

        // Bottom edge in VTK coords (y == nRows - 1 - maxY) - move down (minus)
        if (y1 == (nRows - 1 - maxY) && y2 == (nRows - 1 - maxY))
            y1 = y2 = (nRows - 1 - maxY) - GRID_LINE_OFFSET;
        // Top edge in VTK coords (y == nRows - 1 - minY) - move up (plus)
        else if (y1 == (nRows - 1 - minY) && y2 == (nRows - 1 - minY))
            y1 = y2 = (nRows - 1 - minY) + GRID_LINE_OFFSET;

        pts->InsertNextPoint(x1, y1, 0.0);
        pts->InsertNextPoint(x2, y2, 0.0);
        cellLines->InsertNextCell(2);
        cellLines->InsertCellPoint(i * 2);
        cellLines->InsertCellPoint(i * 2 + 1);
    }

    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(pts);
    polyData->SetLines(cellLines);
    return polyData;
}

void Visualizer::refreshBuildLoadBalanceLine(const std::vector<Line>& lines, int nRows, vtkActor2D* lineActor)
{
    if (! lineActor)
        return;

    // 1. Rebuild geometry
    auto grid = createLinePolyData(lines, nRows);

    // 2. Get existing mapper (assumes it’s a vtkPolyDataMapper2D)
    auto* mapper = vtkPolyDataMapper2D::SafeDownCast(lineActor->GetMapper());
    if (! mapper)
        return;

    vtkNew<vtkCoordinate> normCoords;
    normCoords->SetCoordinateSystemToWorld();

    // 3. Update mapper input
    mapper->SetInputData(grid);
    mapper->SetTransformCoordinate(normCoords);
    mapper->Update();

    lineActor->SetMapper(mapper);
    lineActor->GetMapper()->Update();
}

vtkTextProperty* Visualizer::buildStepLine(StepIndex step, vtkSmartPointer<vtkTextMapper> singleLineTextB)
{
    std::string stepText = "Step " + std::to_string(step);
    singleLineTextB->SetInput(stepText.c_str());

    vtkTextProperty* singleLineTextProp = singleLineTextB->GetTextProperty();
    singleLineTextProp->SetVerticalJustificationToBottom();

    const QColor textColorFromSettings = ColorSettings::instance().textColor();
    singleLineTextProp->SetColor(toVtkColor(textColorFromSettings).GetData());

    return singleLineTextProp;
}

vtkNew<vtkActor2D> Visualizer::buildStepText(StepIndex step,
                                             int font_size,
                                             vtkSmartPointer<vtkTextMapper> stepLineTextMapper,
                                             vtkSmartPointer<vtkRenderer> renderer)
{
    stepLineTextMapper->SetInput(("Step " + std::to_string(step)).c_str());

    auto textProp = stepLineTextMapper->GetTextProperty();
    textProp->SetFontSize(font_size);
    textProp->SetFontFamilyToArial();
    textProp->BoldOn();
    textProp->ItalicOff();
    textProp->ShadowOff();
    textProp->SetVerticalJustificationToBottom();

    const QColor textColorFromSettings = ColorSettings::instance().textColor();
    textProp->SetColor(toVtkColor(textColorFromSettings).GetData());

    vtkNew<vtkActor2D> stepLineTextActor;
    stepLineTextActor->SetMapper(stepLineTextMapper);
    stepLineTextActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedDisplay();
    stepLineTextActor->GetPositionCoordinate()->SetValue(0.05, 0.85);
    renderer->AddViewProp(stepLineTextActor);
    return stepLineTextActor;
}

void Visualizer::drawFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> backgroundActor)
{
    drawFlatSceneBackground(nRows, nCols, renderer, backgroundActor, 0.0);
}

void Visualizer::drawFlatSceneBackground(int nRows,
                                         int nCols,
                                         vtkSmartPointer<vtkRenderer> renderer,
                                         vtkSmartPointer<vtkActor> backgroundActor,
                                         double zPosition)
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

    // Set uniform color for the background plane from settings
    const QColor sceneColor = ColorSettings::instance().flatSceneBackgroundColor();
    lut->SetTableValue(0, sceneColor.redF(), sceneColor.greenF(), sceneColor.blueF(), 1.0);

    // Create the plane at the requested elevation.
    vtkNew<vtkPoints> points;
    for (int row = 0; row < nRows; row++)
    {
        for (int col = 0; col < nCols; col++)
        {
            points->InsertNextPoint(/*x=*/col, /*y=*/nRows - 1 - row, /*z=*/zPosition);
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

vtkSmartPointer<vtkPolyData> Visualizer::create3DVolumeGridLinePolyData(
    int nRows,
    int nCols,
    int nSlices,
    int nNodeZ,
    const std::vector<Line>& lines)
{
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> cellLines;

    auto addSegment = [&](double x1, double y1, double z1,
                          double x2, double y2, double z2)
    {
        const vtkIdType first = points->InsertNextPoint(x1, y1, z1);
        const vtkIdType second = points->InsertNextPoint(x2, y2, z2);
        cellLines->InsertNextCell(2);
        cellLines->InsertCellPoint(first);
        cellLines->InsertCellPoint(second);
    };

    const double maxX = std::max(0, nCols - 1);
    const double maxY = std::max(0, nRows - 1);
    const double maxZ = std::max(0, nSlices - 1);

    for (const auto& line : lines)
    {
        const double x1 = std::clamp(static_cast<double>(line.x1), 0.0, maxX);
        const double x2 = std::clamp(static_cast<double>(line.x2), 0.0, maxX);
        const double y1 = std::clamp(maxY - line.y1, 0.0, maxY);
        const double y2 = std::clamp(maxY - line.y2, 0.0, maxY);

        // Extruding every XY node edge through Z produces a wireframe for each
        // distributed node partition without covering the volume with a solid plane.
        addSegment(x1, y1, 0.0, x2, y2, 0.0);
        addSegment(x1, y1, maxZ, x2, y2, maxZ);
        addSegment(x1, y1, 0.0, x1, y1, maxZ);
        addSegment(x2, y2, 0.0, x2, y2, maxZ);
    }

    // XY lines carry exact X/Y partition offsets. Z partition offsets are not
    // represented by Line, so derive their regular slice boundaries here.
    for (int nodeZ = 1; nodeZ < nNodeZ; ++nodeZ)
    {
        const double z = std::clamp(
            static_cast<double>(nodeZ * nSlices) / nNodeZ,
            0.0,
            maxZ);
        addSegment(0.0, 0.0, z, maxX, 0.0, z);
        addSegment(maxX, 0.0, z, maxX, maxY, z);
        addSegment(maxX, maxY, z, 0.0, maxY, z);
        addSegment(0.0, maxY, z, 0.0, 0.0, z);
    }

    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(points);
    polyData->SetLines(cellLines);
    return polyData;
}

void Visualizer::drawGridLinesFor3DVolume(int nRows,
                                          int nCols,
                                          int nSlices,
                                          int nNodeZ,
                                          const std::vector<Line>& lines,
                                          vtkSmartPointer<vtkRenderer> renderer,
                                          vtkSmartPointer<vtkActor> gridLinesActor)
{
    if (!renderer || !gridLinesActor)
        return;

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(create3DVolumeGridLinePolyData(nRows, nCols, nSlices, nNodeZ, lines));
    gridLinesActor->SetMapper(mapper);
    gridLinesActor->GetProperty()->SetLineWidth(1.25);
    gridLinesActor->GetProperty()->SetOpacity(0.8);
    applyGridColorTo3DGridLinesActor(gridLinesActor);
    renderer->AddActor(gridLinesActor);
}

void Visualizer::refreshGridLinesFor3DVolume(int nRows,
                                             int nCols,
                                             int nSlices,
                                             int nNodeZ,
                                             const std::vector<Line>& lines,
                                             vtkSmartPointer<vtkActor> gridLinesActor)
{
    if (!gridLinesActor)
        return;

    auto* mapper = vtkPolyDataMapper::SafeDownCast(gridLinesActor->GetMapper());
    if (!mapper)
        return;

    mapper->SetInputData(create3DVolumeGridLinePolyData(nRows, nCols, nSlices, nNodeZ, lines));
    mapper->Update();
    applyGridColorTo3DGridLinesActor(gridLinesActor);
}

void Visualizer::refreshFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkActor> backgroundActor)
{
    // Validate input
    if (! backgroundActor || ! backgroundActor->GetMapper())
    {
        return;
    }

    if (vtkLookupTable* lut = dynamic_cast<vtkLookupTable*>(backgroundActor->GetMapper()->GetLookupTable()))
    {
        // Keep uniform color from settings - no need to update from cell data
        const QColor sceneColor = ColorSettings::instance().flatSceneBackgroundColor();
        lut->SetTableValue(0, sceneColor.redF(), sceneColor.greenF(), sceneColor.blueF(), 1.0);
        backgroundActor->GetMapper()->SetLookupTable(lut);
        backgroundActor->GetMapper()->Update();
    }
}

Color Visualizer::flatSceneBackgroundColor() const
{
    const QColor sceneColor = ColorSettings::instance().flatSceneBackgroundColor();
    return Color(
        static_cast<std::uint8_t>(sceneColor.red()),
        static_cast<std::uint8_t>(sceneColor.green()),
        static_cast<std::uint8_t>(sceneColor.blue()),
        255
    );
}

void Visualizer::applyGridColorTo3DGridLinesActor(vtkSmartPointer<vtkActor> gridLinesActor)
{
    if (!gridLinesActor)
    {
        return;
    }

    const QColor gridColor = ColorSettings::instance().gridColor();
    gridLinesActor->GetProperty()->SetColor(toVtkColor(gridColor).GetData());
}
