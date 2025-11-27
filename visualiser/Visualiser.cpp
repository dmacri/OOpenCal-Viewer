#include <limits>
#include "Line.h"
#include "visualiser/Visualizer.hpp"
#include "widgets/ColorSettings.h"  // ColorSettings


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
