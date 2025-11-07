#include "Line.h"
#include "visualiser/Visualizer.hpp"
#include "widgets/ColorSettings.h"


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

    for (size_t i = 0; i < lines.size(); ++i)
    {
        pts->InsertNextPoint(lines[i].x1, nRows - 1 - lines[i].y1, 0.0);
        pts->InsertNextPoint(lines[i].x2, nRows - 1 - lines[i].y2, 0.0);
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
