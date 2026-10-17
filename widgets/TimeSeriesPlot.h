/** @file TimeSeriesPlot.h
 *  @brief Line plot of one value over the simulation steps, drawn with VTK charts. */
#pragma once

#include <optional>
#include <vector>

#include <QString>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>

class vtkChartXY;
class vtkContextView;
class vtkTable;

/** @class TimeSeriesPlot
 *  @brief Line plot "value versus step" built on `vtkChartXY` (the project already depends on VTK).
 *
 *  Features: axes with tick marks and grid, gaps where a value is missing (NaN), a dashed vertical
 *  marker for the step currently shown in the scene, a tooltip with the point under the mouse, and
 *  the mouse interaction of the VTK chart (wheel zooms, left button drags the plot). */
class TimeSeriesPlot : public QVTKOpenGLNativeWidget
{
    Q_OBJECT

public:
    explicit TimeSeriesPlot(QWidget* parent = nullptr);
    ~TimeSeriesPlot() override;

    /** @brief Set the plotted data.
     *  @param x x values (simulation steps), ascending
     *  @param y y values; NaN means "no value" and leaves a gap in the line */
    void setSeries(std::vector<double> x, std::vector<double> y);

    /// @brief Set the labels of the axes
    void setAxisLabels(const QString& xLabel, const QString& yLabel);

    /// @brief Mark the x position of the step currently shown in the scene (nothing when empty)
    void setMarkerX(std::optional<double> x);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    /// Rebuild the VTK tables and plots from the data and render
    void rebuild();
    /// Take the colors of the chart from the application palette (works for dark themes too)
    void applyPaletteColors();

    std::vector<double> m_x;
    std::vector<double> m_y;
    QString m_xLabel = "Step";
    QString m_yLabel;
    std::optional<double> m_markerX;

    vtkSmartPointer<vtkContextView> m_view;
    vtkSmartPointer<vtkChartXY> m_chart;
    vtkSmartPointer<vtkTable> m_seriesTable;
    vtkSmartPointer<vtkTable> m_markerTable;
};
