#include "widgets/TimeSeriesPlot.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QPalette>

#include <vtkAxis.h>
#include <vtkChartXY.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkDoubleArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkPen.h>
#include <vtkPlot.h>
#include <vtkPlotPoints.h>
#include <vtkRenderer.h>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkTextProperty.h>
#include <vtkTooltipItem.h>

namespace
{
constexpr std::size_t maxPointsWithMarkers = 120; ///< Draw a marker at every point only for short series

void setColumn(vtkTable* table, int column, const std::vector<double>& values)
{
    auto* array = vtkDoubleArray::SafeDownCast(table->GetColumn(column));
    array->SetNumberOfValues(static_cast<vtkIdType>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i)
        array->SetValue(static_cast<vtkIdType>(i), values[i]);
    array->Modified();
}

vtkSmartPointer<vtkTable> makeTable(const char* xName, const char* yName)
{
    auto table = vtkSmartPointer<vtkTable>::New();
    for (const char* name : {xName, yName})
    {
        vtkNew<vtkDoubleArray> column;
        column->SetName(name);
        table->AddColumn(column);
    }
    return table;
}
} // namespace

TimeSeriesPlot::TimeSeriesPlot(QWidget* parent)
    : QVTKOpenGLNativeWidget(parent)
{
    vtkNew<vtkGenericOpenGLRenderWindow> window;
    setRenderWindow(window);

    m_seriesTable = makeTable("Step", "Value");
    m_markerTable = makeTable("MarkerX", "MarkerY");

    m_chart = vtkSmartPointer<vtkChartXY>::New();
    m_chart->SetShowLegend(false);
    m_chart->GetAxis(vtkAxis::BOTTOM)->SetTitle("Step");

    m_view = vtkSmartPointer<vtkContextView>::New();
    m_view->SetRenderWindow(window);
    m_view->SetInteractor(interactor());
    m_view->GetScene()->AddItem(m_chart);

    applyPaletteColors();
    rebuild();
}

TimeSeriesPlot::~TimeSeriesPlot() = default;

QSize TimeSeriesPlot::sizeHint() const
{
    return {560, 340};
}

QSize TimeSeriesPlot::minimumSizeHint() const
{
    return {320, 220};
}

void TimeSeriesPlot::setSeries(std::vector<double> x, std::vector<double> y)
{
    m_x = std::move(x);
    m_y = std::move(y);

    // Both vectors must be equally long
    const auto count = std::min(m_x.size(), m_y.size());
    m_x.resize(count);
    m_y.resize(count);

    rebuild();
}

void TimeSeriesPlot::setAxisLabels(const QString& xLabel, const QString& yLabel)
{
    m_xLabel = xLabel;
    m_yLabel = yLabel;
    rebuild();
}

void TimeSeriesPlot::setMarkerX(std::optional<double> x)
{
    m_markerX = x;
    rebuild();
}

void TimeSeriesPlot::applyPaletteColors()
{
    const auto toRgb = [](const QColor& color, double& r, double& g, double& b)
    {
        r = color.redF();
        g = color.greenF();
        b = color.blueF();
    };

    double r = 0, g = 0, b = 0;
    toRgb(palette().color(QPalette::Base), r, g, b);
    m_view->GetRenderer()->SetBackground(r, g, b);

    toRgb(palette().color(QPalette::Text), r, g, b);
    const auto rgb255 = [](double v) { return static_cast<unsigned char>(std::lround(v * 255.0)); };
    for (const int axisId : {vtkAxis::LEFT, vtkAxis::BOTTOM})
    {
        auto* axis = m_chart->GetAxis(axisId);
        axis->GetTitleProperties()->SetColor(r, g, b);
        axis->GetLabelProperties()->SetColor(r, g, b);
        axis->GetPen()->SetColor(rgb255(r), rgb255(g), rgb255(b));
        axis->GetGridPen()->SetColor(rgb255(r), rgb255(g), rgb255(b), 45);
    }
}

void TimeSeriesPlot::rebuild()
{
    // Remove the plots, they are created again below for the new data
    m_chart->ClearPlots();
    m_chart->GetTooltip()->SetVisible(false); // the tooltip of a removed plot would stay on the screen

    m_chart->GetAxis(vtkAxis::BOTTOM)->SetTitle(m_xLabel.toStdString());
    m_chart->GetAxis(vtkAxis::LEFT)->SetTitle(m_yLabel.toStdString());

    double yMin = std::numeric_limits<double>::infinity();
    double yMax = -std::numeric_limits<double>::infinity();
    for (const double v : m_y)
    {
        if (std::isfinite(v))
        {
            yMin = std::min(yMin, v);
            yMax = std::max(yMax, v);
        }
    }
    const bool hasData = std::isfinite(yMin) && ! m_x.empty();

    // Steps are whole numbers. The chart picks "nice" tick positions, which for a short range would be
    // fractions (0, 0.2, 0.4, ...), so for a short range put a tick at every whole step instead.
    // The range is the one of the points which have a value: the chart ignores the others.
    auto* stepAxis = m_chart->GetAxis(vtkAxis::BOTTOM);
    stepAxis->SetCustomTickPositions(nullptr, nullptr);
    if (hasData)
    {
        double xMin = std::numeric_limits<double>::infinity();
        double xMax = -std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < m_x.size(); ++i)
        {
            if (std::isfinite(m_y[i]))
            {
                xMin = std::min(xMin, m_x[i]);
                xMax = std::max(xMax, m_x[i]);
            }
        }
        if (m_markerX)
        {
            xMin = std::min(xMin, *m_markerX);
            xMax = std::max(xMax, *m_markerX);
        }

        constexpr double maxRangeWithTickPerStep = 12.0;
        if (xMax > xMin && xMax - xMin <= maxRangeWithTickPerStep)
        {
            vtkNew<vtkDoubleArray> positions;
            vtkNew<vtkStringArray> labels;
            for (double tick = std::ceil(xMin); tick <= std::floor(xMax); tick += 1.0)
            {
                positions->InsertNextValue(tick);
                labels->InsertNextValue(std::to_string(static_cast<long long>(tick)));
            }
            stepAxis->SetCustomTickPositions(positions, labels);
        }
    }

    if (hasData)
    {
        setColumn(m_seriesTable, 0, m_x);
        setColumn(m_seriesTable, 1, m_y);
        m_seriesTable->Modified();

        // Marker of the step shown in the scene: a dashed vertical line over the whole range of values.
        // A constant series has no range, so give it some, the chart could not scale it otherwise.
        double lineMin = yMin;
        double lineMax = yMax;
        if (lineMin == lineMax)
        {
            const double margin = lineMin == 0.0 ? 1.0 : std::abs(lineMin) * 0.1;
            lineMin -= margin;
            lineMax += margin;
        }
        if (m_markerX)
        {
            setColumn(m_markerTable, 0, {*m_markerX, *m_markerX});
            setColumn(m_markerTable, 1, {lineMin, lineMax});
            m_markerTable->Modified();

            auto* marker = m_chart->AddPlot(vtkChart::LINE);
            marker->SetInputData(m_markerTable, 0, 1);
            marker->SetColor(214, 39, 40, 255);
            marker->SetWidth(1.5f);
            marker->GetPen()->SetLineType(vtkPen::DASH_LINE);
            marker->SetTooltipLabelFormat(""); // no tooltip for the marker
            if (auto* points = vtkPlotPoints::SafeDownCast(marker))
                points->SetMarkerStyle(vtkPlotPoints::NONE);
        }

        auto* line = m_chart->AddPlot(vtkChart::LINE);
        line->SetInputData(m_seriesTable, 0, 1);
        line->SetColor(31, 119, 180, 255);
        line->SetWidth(2.0f);
        line->SetTooltipLabelFormat("step %x: %y");
        line->SetTooltipPrecision(10);
        if (auto* points = vtkPlotPoints::SafeDownCast(line))
        {
            points->SetMarkerStyle(m_x.size() <= maxPointsWithMarkers ? vtkPlotPoints::CIRCLE : vtkPlotPoints::NONE);
            points->SetMarkerSize(7.0f);
        }

        // The marker was added first, so the data line is drawn over it; show the whole data
        m_chart->RecalculateBounds();
    }

    renderWindow()->Render();
}
