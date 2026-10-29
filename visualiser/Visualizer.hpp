/** @file Visualizer.hpp
 * @brief Declaration of the Visualizer class for VTK-based visualization. */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <vtkActor2D.h>
#include <vtkCellArray.h>
#include <vtkCoordinate.h>
#include <vtkDataSetMapper.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkLookupTable.h>
#include <vtkNamedColors.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
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
#include <vtkDataArray.h>
#include <vtkProperty.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include "core/types.h"    // StepIndex
#include "OOpenCAL/base/Cell.h" // Color
#include "visualiser/SettingParameter.h" // SubstateInfo
#include "visualiser/Line.h"
#include "visualiser/VolumeLayers.h"
#include "visualiser/VolumeZSpacing.h"
#include "widgets/GridWorldMapping.h"


/** @brief Converts a color channel (red, green, blue or alpha) from the OOpenCAL range [0, 255] to the VTK range [0, 1].
 *
 * OOpenCAL stores every component of Color as std::uint8_t in the range [0, 255], while VTK expects normalized
 * values in the range [0, 1]. The conversion is always a division by 255 - there is no second (already normalized)
 * format, so a channel with value 1 means 1/255 (almost black), not 100% intensity.
 *
 * @param channel The color component in the range [0, 255].
 * @return Normalized value in the range [0, 1]. */
inline double toUnitColor(std::uint8_t channel)
{
    return static_cast<double>(channel) / 255.0;
}

/** @brief Returns the opacity (0 = fully transparent, 1 = fully opaque) of a color returned by Cell::outputValue().
 *
 * The alpha channel of Color is in the range [0, 255], where 0 means transparent and 255 fully opaque.
 * Color(r, g, b) created without alpha is fully opaque (the default alpha in OOpenCAL's Cell.h is 255),
 * so only a color created with an explicit alpha, e.g. Color(255, 255, 255, 0), is transparent. */
inline double toOpacity(const Color& color)
{
    return toUnitColor(color.getAlpha());
}

/** @brief Tells whether the per-cell RGBA colors of a poly data mapper contain a (partially) transparent cell.
 *
 * VTK renders such geometry correctly only in the translucent pass, so the actor has to be told about it. */
inline bool hasTranslucentCellColors(vtkPolyDataMapper* mapper)
{
    if (!mapper || !mapper->GetInput())
        return false;

    vtkDataArray* colors = mapper->GetInput()->GetCellData()->GetScalars();
    if (!colors || colors->GetNumberOfComponents() < 4 || colors->GetNumberOfTuples() == 0)
        return false;

    double alphaRange[2];
    colors->GetRange(alphaRange, 3);
    return alphaRange[0] < 255.0;
}


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
    void drawWithVTK(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& colorSubstateInfos={}, bool useCellRendering=false);
    template<class Matrix>
    void refreshWindowsVTK(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& colorSubstateInfos);

    /** @brief Render a native 3D cellular grid as a volume.
     *
     * This path is separate from the 2D "substate as altitude" surface.
     *
     * @param zGapPlanes number of empty (fully transparent) planes inserted between every two
     *        neighbouring Z planes of the model, so that the lower planes (e.g. the subsurface)
     *        are not hidden by the upper ones. 0 keeps the planes glued together (see VolumeZSpacing.h).
     * @param volumeLayersTopToBottom substates the user chose to show in the volume ("Show in 3D volume"),
     *        the one on top of the Substate Parameters panel first. When there is at least one with a usable
     *        Min/Max, every voxel shows the highest of them which has a value in the cell, coloured by the
     *        intensity of the value (see VolumeLayers.h) and @p colorSubstateInfos is not used.
     *        Otherwise the volume is coloured by the first of @p colorSubstateInfos (or the default value). */
    template<class Volume>
    void drawWithVTK3DVolume(const Volume& p,
                             int nRows,
                             int nCols,
                             int nSlices,
                             vtkSmartPointer<vtkRenderer> renderer,
                             vtkSmartPointer<vtkVolume> volumeActor,
                             const std::vector<const SubstateInfo*>& colorSubstateInfos,
                             int zGapPlanes = 0,
                             const std::vector<const SubstateInfo*>& volumeLayersTopToBottom = {});

private:
    /** @brief Warns (once per Visualizer) that the native 3D volume has no value range.
     *
     * Called when Cell::stringEncoding() could not be parsed as a number for any cell. Every value is then 0,
     * so min == max == 0 and the volume is drawn fully transparent - without this message the scene just looks empty.
     * @param fieldName substate name passed to stringEncoding() (nullptr when no substate is selected)
     * @param sample text returned by stringEncoding() for the first cell
     * @param cellCount number of cells in the volume */
    void warnVolumeValuesNotNumeric(const char* fieldName, const std::string& sample, std::size_t cellCount)
    {
        if (warnedVolumeValuesNotNumeric)
            return;
        warnedVolumeValuesNotNumeric = true;
        std::cerr << "WARNING: native 3D volume is drawn fully transparent: Cell::stringEncoding("
                  << (fieldName ? std::string("\"") + fieldName + "\"" : std::string("nullptr"))
                  << ") returned text that is not a number for all " << cellCount << " cells (first cell: '" << sample
                  << "'). The model must return a plain number from stringEncoding() for the substate it is coloured by."
                  << std::endl;
    }

    bool warnedVolumeValuesNotNumeric = false; ///< see warnVolumeValuesNotNumeric()

    /// @brief Draws the volume from the chosen substates (see the last parameter of drawWithVTK3DVolume())
    /// @return false when none of the layers can be drawn (no usable Min/Max): nothing was drawn then
    template<class Volume>
    bool drawWithVTK3DVolumeLayers(const Volume& p,
                                   int nRows,
                                   int nCols,
                                   int nSlices,
                                   vtkSmartPointer<vtkRenderer> renderer,
                                   vtkSmartPointer<vtkVolume> volumeActor,
                                   const std::vector<const SubstateInfo*>& layersTopToBottom,
                                   int zGapPlanes);

public:

    /// @brief Draw 3D substate visualization as a quad mesh surface (new healed quad approach).
    template<class Matrix>
    void drawWithVTK3DSubstate(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue, const std::vector<const SubstateInfo*>& colorSubstateInfos);
    /// @brief Draw stacked 3D substate altitude layers as one quad mesh surface collection.
    template<class Matrix>
    void drawWithVTK3DSubstates(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop, const std::vector<const SubstateInfo*>& colorSubstateInfos);
    /// @brief Refresh 3D substate visualization as a quad mesh surface (new healed quad approach).
    template<class Matrix>
    void refreshWindowsVTK3DSubstate(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue, const std::vector<const SubstateInfo*>& colorSubstateInfos);
    /// @brief Refresh stacked 3D substate altitude layers.
    template<class Matrix>
    void refreshWindowsVTK3DSubstates(const Matrix& p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop, const std::vector<const SubstateInfo*>& colorSubstateInfos);

    /// @brief Draw a vertical XZ or YZ profile through a 2D height-field substate.
    template<class Matrix>
    void drawWithVTK3DSubstateSlice(const Matrix& p,
                                    int nRows,
                                    int nCols,
                                    vtkSmartPointer<vtkRenderer> renderer,
                                    vtkSmartPointer<vtkActor> gridActor,
                                    const std::string& substateFieldName,
                                    double minValue,
                                    double maxValue,
                                    const std::vector<const SubstateInfo*>& colorSubstateInfos,
                                    GridSliceAxis fixedAxis,
                                    int fixedIndex);

    /// @brief Draw a vertical XZ or YZ profile through stacked 2D height-field substates.
    template<class Matrix>
    void drawWithVTK3DSubstatesSlice(const Matrix& p,
                                     int nRows,
                                     int nCols,
                                     vtkSmartPointer<vtkRenderer> renderer,
                                     vtkSmartPointer<vtkActor> gridActor,
                                     const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
                                     const std::vector<const SubstateInfo*>& colorSubstateInfos,
                                     GridSliceAxis fixedAxis,
                                     int fixedIndex);

    /// @brief Draw node grid lines projected onto the 3D substate surface.
    template<class Matrix>
    void drawGridLinesOn3DSurface(const Matrix& p,
                                  int nRows,
                                  int nCols,
                                  const std::vector<Line>& lines,
                                  vtkSmartPointer<vtkRenderer> renderer,
                                  vtkSmartPointer<vtkActor> gridLinesActor,
                                  const std::string& substateFieldName,
                                  double minValue,
                                  double maxValue);

    /// @brief Draw node grid lines projected onto stacked 3D substate altitude layers.
    template<class Matrix>
    void drawGridLinesOn3DSubstateStack(const Matrix& p,
                                        int nRows,
                                        int nCols,
                                        const std::vector<Line>& lines,
                                        vtkSmartPointer<vtkRenderer> renderer,
                                        vtkSmartPointer<vtkActor> gridLinesActor,
                                        const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop);

    /// @brief Refresh node grid lines projected onto the 3D substate surface.
    template<class Matrix>
    void refreshGridLinesOn3DSurface(const Matrix& p,
                                     int nRows,
                                     int nCols,
                                     const std::vector<Line>& lines,
                                     vtkSmartPointer<vtkActor> gridLinesActor,
                                     const std::string& substateFieldName,
                                     double minValue,
                                     double maxValue);

    /// @brief Refresh node grid lines projected onto stacked 3D substate altitude layers.
    template<class Matrix>
    void refreshGridLinesOn3DSubstateStack(const Matrix& p,
                                           int nRows,
                                           int nCols,
                                           const std::vector<Line>& lines,
                                           vtkSmartPointer<vtkActor> gridLinesActor,
                                           const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop);

    /// @brief Draw flat background plane at Z=0 for 3D visualization.
    void drawFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> backgroundActor);
    /// @brief Draw a flat background plane at a selected Z coordinate.
    void drawFlatSceneBackground(int nRows,
                                 int nCols,
                                 vtkSmartPointer<vtkRenderer> renderer,
                                 vtkSmartPointer<vtkActor> backgroundActor,
                                 double zPosition);
    /// @brief Refresh flat background plane colors.
    void refreshFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkActor> backgroundActor);

    /** @brief Draw node-boundary wireframes around a native 3D volume.
     *  @param zGapPlanes empty planes between neighbouring Z planes (see VolumeZSpacing.h), so the
     *         wireframe covers the whole stretched volume */
    void drawGridLinesFor3DVolume(int nRows,
                                  int nCols,
                                  int nSlices,
                                  int nNodeZ,
                                  const std::vector<Line>& lines,
                                  vtkSmartPointer<vtkRenderer> renderer,
                                  vtkSmartPointer<vtkActor> gridLinesActor,
                                  int zGapPlanes = 0);

    /// @brief Refresh node-boundary wireframes around a native 3D volume.
    void refreshGridLinesFor3DVolume(int nRows,
                                     int nCols,
                                     int nSlices,
                                     int nNodeZ,
                                     const std::vector<Line>& lines,
                                     vtkSmartPointer<vtkActor> gridLinesActor,
                                     int zGapPlanes = 0);

    /** @brief Builds the lines between the computational nodes in the flat view.
     *  @param nRows Number of grid rows
     *  @param sceneBounds World extent of the drawn scene; the outer lines are drawn exactly on its edges */
    void buildLoadBalanceLine(const std::vector<Line>& lines,
                              int nRows,
                              const GridWorldMapping::Bounds2D& sceneBounds,
                              vtkSmartPointer<vtkRenderer> renderer,
                              vtkSmartPointer<vtkActor2D> actorBuildLine);
    void refreshBuildLoadBalanceLine(const std::vector<Line>& lines,
                                     int nRows,
                                     const GridWorldMapping::Bounds2D& sceneBounds,
                                     vtkActor2D* lineActor);
    vtkTextProperty* buildStepLine(StepIndex step, vtkSmartPointer<vtkTextMapper> singleLineTextB);
    vtkNew<vtkActor2D> buildStepText(StepIndex step, int font_size, vtkSmartPointer<vtkTextMapper> stepLineTextMapper, vtkSmartPointer<vtkRenderer> renderer);

    /** @brief Builds the line with the simulated time, placed directly under the step text.
     *
     *  The text is a bit smaller than the step text (see timeFontSize()). It follows the position of @p stepLineTextActor,
     *  so it stays under it when the window is resized.
     *  @param stepFontSize Font size of the step text
     *  @param timeLineTextMapper Mapper of the time text (its input is set later by the caller)
     *  @param stepLineTextActor Actor of the step text, as returned by buildStepText()
     *  @param renderer Renderer to which the new actor is added
     *  @return The new actor (it is already added to the renderer) */
    vtkNew<vtkActor2D> buildTimeText(int stepFontSize,
                                     vtkSmartPointer<vtkTextMapper> timeLineTextMapper,
                                     vtkActor2D* stepLineTextActor,
                                     vtkSmartPointer<vtkRenderer> renderer);

    /// @brief Font size of the time text for the given font size of the step text (about 80%, never below 8)
    static int timeFontSize(int stepFontSize)
    {
        return std::max(8, static_cast<int>(std::lround(stepFontSize * 0.8)));
    }

private:
    /// @brief Apply grid color settings to 3D grid lines actor.
    /// @note This function is to decrease dependencies with Qt (Visualiser.hpp is used in module compilation, so we don't want Qt)
    void applyGridColorTo3DGridLinesActor(vtkSmartPointer<vtkActor> gridLinesActor);

    /// @brief Fills the lookup table with the cell colors (including alpha).
    /// @return true if at least one cell is (partially) transparent, so the actor must be rendered as translucent geometry
    template<class Matrix>
    bool buidColor(vtkLookupTable* lut, int nCols, int nRows, const Matrix& p, const std::vector<const SubstateInfo*>& colorSubstateInfos);

    /// @brief The method is calculating color for specific cell with substateInfo considered
    template<class Matrix>
    Color calculateCellColor(int row, int column, const Matrix &p, const std::vector<const SubstateInfo*>& colorSubstateInfos);

    /// @brief Calculate color for a specific cell, returning std::optional<Color>.
    /// Returns nullopt if the value is out of range, equals noValue, or parsing fails.
    /// Otherwise returns the calculated color.
    /// This method is useful for aggregating colors from multiple substates.
    template<class Matrix>
    std::optional<Color> calculateCellColorOptional(int row, int column, const Matrix &p, const SubstateInfo* substateInfo=nullptr);

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
     * @param substateInfo Substate info for custom coloring (optional)
     * @return vtkSmartPointer<vtkPolyData> with quad mesh and colors */
    template<class Matrix>
    vtkSmartPointer<vtkPolyData> build3DSubstateSurfaceQuadMesh(const Matrix& p, int nRows, int nCols,
                                                                const std::string& substateFieldName,
                                                                double minValue, double maxValue,
                                                                const std::vector<const SubstateInfo*>& colorSubstateInfos);

    template<class Matrix>
    vtkSmartPointer<vtkPolyData> build3DSubstateStackSurfaceQuadMesh(const Matrix& p,
                                                                     int nRows,
                                                                     int nCols,
                                                                     const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
                                                                     const std::vector<const SubstateInfo*>& colorSubstateInfos);

    template<class Matrix>
    vtkSmartPointer<vtkPolyData> buildGridLinesOnSurfacePolyData(const Matrix& p,
                                                                 int nRows,
                                                                 int nCols,
                                                                 const std::vector<Line>& lines,
                                                                 const std::string& substateFieldName,
                                                                 double minValue,
                                                                 double maxValue);

    template<class Matrix>
    vtkSmartPointer<vtkPolyData> buildGridLinesOnSubstateStackPolyData(const Matrix& p,
                                                                       int nRows,
                                                                       int nCols,
                                                                       const std::vector<Line>& lines,
                                                                       const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop);

    /** @brief Creates a vtkPolyData representing a set of 2D lines.
      * @param lines Vector of Line objects (each defines a line segment, see GridWorldMapping::lineYToWorldY())
      * @param nRows Number of grid rows (used to invert Y coordinates)
      * @param sceneBounds World extent of the drawn scene; the outer lines are drawn exactly on its edges
      * @return vtkSmartPointer<vtkPolyData> with points and lines set */
    vtkSmartPointer<vtkPolyData> createLinePolyData(const std::vector<Line>& lines,
                                                    int nRows,
                                                    const GridWorldMapping::Bounds2D& sceneBounds);

    vtkSmartPointer<vtkPolyData> create3DVolumeGridLinePolyData(int nRows,
                                                                int nCols,
                                                                int nSlices,
                                                                int nNodeZ,
                                                                const std::vector<Line>& lines,
                                                                int zGapPlanes = 0);

    /// @brief This function is to decrease dependencies with Qt (Visualiser.hpp is used in module compilation, so we don't want Qt)
    Color flatSceneBackgroundColor() const;
    GlobalValueManager* gvm = nullptr;
};

////////////////////////////////////////////////////////////////////

template <class Matrix>
void Visualizer::drawWithVTK(const Matrix &p, int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& colorSubstateInfos, bool useCellRendering)
{
    // Runtime switch between high-quality cell rendering (true)
    // and faster point-based rendering (false).
    // This parameter is passed from the GUI to allow user control.

    if (useCellRendering)
    {
        const auto numberOfCells = nRows * nCols;
        vtkNew<vtkDoubleArray> cellValues;
        cellValues->SetNumberOfTuples(numberOfCells);

        // Set scalar values for cells (one color per cell, no interpolation)
        for (int row = 0; row < nRows; row++)
        {
            for (int col = 0; col < nCols; col++)
            {
                const int cellId = row * nCols + col;
                const int colorIndex = (nRows - 1 - row) * nCols + col; // Color index from buidColor()
                cellValues->SetValue(cellId, colorIndex);
            }
        }

        vtkNew<vtkLookupTable> lut;
        lut->SetNumberOfTableValues(numberOfCells);

        vtkNew<vtkPoints> points;
        for (int row = 0; row <= nRows; row++)
        {
            for (int col = 0; col <= nCols; col++)
            {
                // Insert points with Y inverted to match buidColor() indexing
                points->InsertNextPoint(/*x=*/col, /*y=*/nRows - row, /*z=*/GridWorldMapping::flatSceneZ);
            }
        }

        vtkNew<vtkStructuredGrid> structuredGrid;
        structuredGrid->SetDimensions(nCols + 1, nRows + 1, 1);
        structuredGrid->SetPoints(points);
        structuredGrid->GetCellData()->SetScalars(cellValues);

        const bool hasTransparentCells = buidColor(lut, nCols, nRows, p, colorSubstateInfos);

        vtkNew<vtkDataSetMapper> gridMapper;
        gridMapper->UpdateDataObject();
        gridMapper->SetInputData(structuredGrid);
        gridMapper->SetLookupTable(lut);
        gridMapper->SetScalarRange(0, numberOfCells - 1);
        gridMapper->SetScalarModeToUseCellData();
        gridMapper->InterpolateScalarsBeforeMappingOff(); // Keep sharp cell boundaries for small grids

        gridActor->SetMapper(gridMapper);
        // Cells with alpha < 255 must go through the translucent rendering pass, otherwise VTK ignores the alpha.
        gridActor->SetForceTranslucent(hasTransparentCells);
        renderer->AddActor(gridActor);
    }
    else
    {
        // Original point-based rendering: faster and suitable for large grids.
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
                points->InsertNextPoint(/*x=*/col, /*y=*/nRows - 1 - row, /*z=*/GridWorldMapping::flatSceneZ);
            }
        }

        vtkNew<vtkStructuredGrid> structuredGrid;
        structuredGrid->SetDimensions(nCols, nRows, 1);
        structuredGrid->SetPoints(points);
        structuredGrid->GetPointData()->SetScalars(pointValues);

        const bool hasTransparentCells = buidColor(lut, nCols, nRows, p, colorSubstateInfos);

        vtkNew<vtkDataSetMapper> gridMapper;
        gridMapper->UpdateDataObject();
        gridMapper->SetInputData(structuredGrid);
        gridMapper->SetLookupTable(lut);
        gridMapper->SetScalarRange(0, numberOfPoints - 1);
        gridMapper->InterpolateScalarsBeforeMappingOff();

        gridActor->SetMapper(gridMapper);
        // Cells with alpha < 255 must go through the translucent rendering pass, otherwise VTK ignores the alpha.
        gridActor->SetForceTranslucent(hasTransparentCells);
        renderer->AddActor(gridActor);
    }
}

template<class Matrix>
void Visualizer::refreshWindowsVTK(const Matrix &p, int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& colorSubstateInfos)
{
    // The actor gets its mapper only when the scene is drawn (drawWithVTK). After clearing the scene
    // there is nothing to refresh yet, so return instead of dereferencing a null mapper.
    if (!gridActor || !gridActor->GetMapper())
        return;

    if (vtkLookupTable* lut = dynamic_cast<vtkLookupTable*>(gridActor->GetMapper()->GetLookupTable()))
    {
        const bool hasTransparentCells = buidColor(lut, nCols, nRows, p, colorSubstateInfos);
        gridActor->SetForceTranslucent(hasTransparentCells);
        gridActor->GetMapper()->SetLookupTable(lut);
        gridActor->GetMapper()->Update();
    }
    else
        throw std::runtime_error("Invalid dynamic cast!");
}

template<class Volume>
void Visualizer::drawWithVTK3DVolume(const Volume& p,
                                     int nRows,
                                     int nCols,
                                     int nSlices,
                                     vtkSmartPointer<vtkRenderer> renderer,
                                     vtkSmartPointer<vtkVolume> volumeActor,
                                     const std::vector<const SubstateInfo*>& colorSubstateInfos,
                                     int zGapPlanes,
                                     const std::vector<const SubstateInfo*>& volumeLayersTopToBottom)
{
    if (!renderer || !volumeActor || nRows <= 0 || nCols <= 0 || nSlices <= 1)
        return;

    if (!volumeLayersTopToBottom.empty() &&
        drawWithVTK3DVolumeLayers(p, nRows, nCols, nSlices, renderer, volumeActor, volumeLayersTopToBottom, zGapPlanes))
    {
        return;
    }

    const int gapPlanes = VolumeZSpacing::clampGap(zGapPlanes);
    const int drawnSlices = static_cast<int>(VolumeZSpacing::drawnPlaneCount(static_cast<std::size_t>(nSlices), gapPlanes));

    const char* fieldName = nullptr;
    if (!colorSubstateInfos.empty() && colorSubstateInfos.front() &&
        !colorSubstateInfos.front()->name.empty())
    {
        fieldName = colorSubstateInfos.front()->name.c_str();
    }

    const vtkIdType valueCount =
        static_cast<vtkIdType>(nRows) * nCols * drawnSlices;
    vtkNew<vtkFloatArray> scalars;
    scalars->SetName("cell-value");
    scalars->SetNumberOfValues(valueCount);
    // The planes inserted by the Z spacing are not filled yet: they are set to a transparent value
    // once the value range is known (see below).

    double minValue = std::numeric_limits<double>::infinity();
    double maxValue = -std::numeric_limits<double>::infinity();
    Color minColor(255, 255, 255);
    Color maxColor(255, 255, 255);

    std::size_t unparsableCells = 0; // cells whose stringEncoding() is not a number (their value is taken as 0)
    std::string firstUnparsableText;

    for (int slice = 0; slice < nSlices; ++slice)
    {
        for (int row = 0; row < nRows; ++row)
        {
            for (int col = 0; col < nCols; ++col)
            {
                double value = 0.0;
                std::string encoded;
                try
                {
                    encoded = p.get(row, col, slice).stringEncoding(fieldName);
                    value = std::stod(encoded);
                }
                catch (...)
                {
                    value = 0.0;
                    if (unparsableCells++ == 0)
                        firstUnparsableText = encoded;
                }

                const Color color = p.get(row, col, slice).outputValue(fieldName, gvm);
                if (value < minValue)
                {
                    minValue = value;
                    minColor = color;
                }
                if (value > maxValue)
                {
                    maxValue = value;
                    maxColor = color;
                }

                // VTK expects X to vary fastest. Invert Y just like the existing 2D renderer.
                // With a Z spacing the data planes are pulled apart: plane `slice` is drawn at
                // index slice * (gap + 1), the planes in between stay empty.
                const vtkIdType drawnSlice =
                    static_cast<vtkIdType>(VolumeZSpacing::drawnPlaneIndex(static_cast<std::size_t>(slice), gapPlanes));
                const vtkIdType index =
                    (drawnSlice * nRows + (nRows - 1 - row)) *
                    nCols +
                    col;
                scalars->SetValue(index, static_cast<float>(value));
            }
        }
    }

    if (!std::isfinite(minValue) || !std::isfinite(maxValue))
        return;

    const std::size_t cellCount = static_cast<std::size_t>(nSlices) * nRows * nCols;
    if (unparsableCells == cellCount)
    {
        warnVolumeValuesNotNumeric(fieldName, firstUnparsableText, cellCount);
    }

    // Empty planes between the data planes must be fully transparent. The opacity transfer function
    // below maps `minValue` to opacity 0 (and, when all values are equal, `minValue - 1`).
    if (gapPlanes > 0)
    {
        const float transparentValue = static_cast<float>(minValue < maxValue ? minValue : minValue - 1.0);
        for (int drawnSlice = 0; drawnSlice < drawnSlices; ++drawnSlice)
        {
            if (drawnSlice % (gapPlanes + 1) == 0)
                continue; // a data plane

            const vtkIdType begin = static_cast<vtkIdType>(drawnSlice) * nRows * nCols;
            for (vtkIdType i = 0; i < static_cast<vtkIdType>(nRows) * nCols; ++i)
                scalars->SetValue(begin + i, transparentValue);
        }
    }

    // A model may intentionally use pure black for occupied cells (Ball3D does).
    // Pure black has no diffuse lighting response and therefore looks like a flat
    // disk from every angle. Lift only near-black foreground colors to charcoal
    // so the original hue remains intact while surface shading becomes visible.
    auto makeShadeable = [](const Color& color)
    {
        const int brightness = color.getRed() + color.getGreen() + color.getBlue();
        if (brightness < 48)
            return Color(70, 82, 100, color.getAlpha());
        return color;
    };
    maxColor = makeShadeable(maxColor);

    vtkNew<vtkImageData> image;
    image->SetDimensions(nCols, nRows, drawnSlices);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->GetPointData()->SetScalars(scalars);

    vtkNew<vtkColorTransferFunction> colors;
    vtkNew<vtkPiecewiseFunction> opacity;

    auto addColor = [&](double value, const Color& color)
    {
        colors->AddRGBPoint(value,
                            toUnitColor(color.getRed()),
                            toUnitColor(color.getGreen()),
                            toUnitColor(color.getBlue()));
    };

    if (minValue < maxValue)
    {
        addColor(minValue, minColor);
        addColor(maxValue, maxColor);
        opacity->AddPoint(minValue, 0.0);
        opacity->AddPoint(maxValue, 0.9 * toOpacity(maxColor));
    }
    else
    {
        addColor(minValue - 1.0, minColor);
        addColor(minValue, minColor);
        opacity->AddPoint(minValue - 1.0, 0.0);
        opacity->AddPoint(minValue, minValue == 0.0 ? 0.0 : 0.9 * toOpacity(minColor));
    }

    vtkNew<vtkVolumeProperty> property;
    property->SetColor(colors);
    property->SetScalarOpacity(opacity);
    property->SetInterpolationTypeToNearest();
    property->ShadeOn();
    property->SetAmbient(0.22);
    property->SetDiffuse(0.78);
    property->SetSpecular(0.35);
    property->SetSpecularPower(18.0);
    property->SetScalarOpacityUnitDistance(0.8);

    vtkNew<vtkSmartVolumeMapper> mapper;
    mapper->SetInputData(image);
    mapper->SetBlendModeToComposite();
    mapper->SetAutoAdjustSampleDistances(true);

    volumeActor->SetMapper(mapper);
    volumeActor->SetProperty(property);
    renderer->AddVolume(volumeActor);
}

template<class Volume>
bool Visualizer::drawWithVTK3DVolumeLayers(const Volume& p,
                                           int nRows,
                                           int nCols,
                                           int nSlices,
                                           vtkSmartPointer<vtkRenderer> renderer,
                                           vtkSmartPointer<vtkVolume> volumeActor,
                                           const std::vector<const SubstateInfo*>& layersTopToBottom,
                                           int zGapPlanes)
{
    // Layers which cannot be drawn (Min/Max not set) are skipped; the number of a layer is its position
    // among those which are drawn, because it decides about its stretch of the transfer function
    std::vector<const SubstateInfo*> layers;
    std::vector<VolumeLayers::LayerRange> ranges;
    for (const SubstateInfo* info : layersTopToBottom)
    {
        if (!info)
            continue;
        VolumeLayers::LayerRange range;
        range.minValue = info->minValue;
        range.maxValue = info->maxValue;
        range.noValueEnabled = info->noValueEnabled;
        range.noValue = info->noValue;
        if (!range.isUsable())
            continue;
        layers.push_back(info);
        ranges.push_back(range);
    }
    if (layers.empty())
        return false;

    const int gapPlanes = VolumeZSpacing::clampGap(zGapPlanes);
    const int drawnSlices = static_cast<int>(VolumeZSpacing::drawnPlaneCount(static_cast<std::size_t>(nSlices), gapPlanes));

    const vtkIdType planeSize = static_cast<vtkIdType>(nRows) * nCols;
    vtkNew<vtkFloatArray> scalars;
    scalars->SetName("cell-layer-value");
    scalars->SetNumberOfValues(planeSize * drawnSlices);
    // Planes inserted by the Z spacing stay empty (transparent)
    scalars->FillValue(static_cast<float>(VolumeLayers::emptyScalar));

    // A layer without colours of its own is coloured like the model colours its lowest and its highest value
    // (like the volume always was); when the model does not tell them apart, by a default gradient.
    struct Extreme
    {
        double intensity = 0.0;
        std::optional<Color> color;
    };
    std::vector<Extreme> lowest(layers.size());
    std::vector<Extreme> highest(layers.size());
    std::vector<bool> hasCustomColors(layers.size());
    for (std::size_t k = 0; k < layers.size(); ++k)
    {
        hasCustomColors[k] = VolumeLayers::parseHexColor(layers[k]->minColor).has_value() &&
                             VolumeLayers::parseHexColor(layers[k]->maxColor).has_value();
        lowest[k].intensity = std::numeric_limits<double>::infinity();
        highest[k].intensity = -std::numeric_limits<double>::infinity();
    }

    std::vector<std::optional<double>> fieldValues(layers.size());
    for (int slice = 0; slice < nSlices; ++slice)
    {
        const vtkIdType drawnSlice =
            static_cast<vtkIdType>(VolumeZSpacing::drawnPlaneIndex(static_cast<std::size_t>(slice), gapPlanes));
        for (int row = 0; row < nRows; ++row)
        {
            for (int col = 0; col < nCols; ++col)
            {
                const auto& cell = p.get(row, col, slice);
                const double scalar = VolumeLayers::scalarForVoxel(
                    ranges,
                    [&](std::size_t layer) -> std::optional<double>
                    {
                        try
                        {
                            return std::stod(cell.stringEncoding(layers[layer]->name.c_str()));
                        }
                        catch (...)
                        {
                            return std::nullopt;
                        }
                    });

                if (scalar == VolumeLayers::emptyScalar)
                    continue;

                if (const auto decoded = VolumeLayers::decode(scalar); decoded && !hasCustomColors[decoded->layer])
                {
                    const std::size_t k = decoded->layer;
                    if (decoded->intensity < lowest[k].intensity)
                    {
                        lowest[k] = {decoded->intensity, cell.outputValue(layers[k]->name.c_str(), gvm)};
                    }
                    if (decoded->intensity > highest[k].intensity)
                    {
                        highest[k] = {decoded->intensity, cell.outputValue(layers[k]->name.c_str(), gvm)};
                    }
                }

                // VTK expects X to vary fastest. Invert Y just like the existing 2D renderer.
                const vtkIdType index = (drawnSlice * nRows + (nRows - 1 - row)) * nCols + col;
                scalars->SetValue(index, static_cast<float>(scalar));
            }
        }
    }

    // Near-black colours have no diffuse lighting response and would look like a flat disk from every angle
    auto makeShadeable = [](VolumeLayers::Rgb color)
    {
        if (color.red + color.green + color.blue < 48)
            return VolumeLayers::Rgb{70, 82, 100};
        return color;
    };
    auto toRgb = [](const Color& color) { return VolumeLayers::Rgb{color.getRed(), color.getGreen(), color.getBlue()}; };

    vtkNew<vtkColorTransferFunction> colors;
    vtkNew<vtkPiecewiseFunction> opacity;
    // No value: fully transparent (the scalars below 1)
    opacity->AddPoint(VolumeLayers::emptyScalar, 0.0);
    opacity->AddPoint(0.0, 0.0);

    constexpr double fullOpacity = 0.9; // like in the volume coloured by one substate

    for (std::size_t k = 0; k < layers.size(); ++k)
    {
        auto lowColor = VolumeLayers::parseHexColor(layers[k]->minColor);
        auto highColor = VolumeLayers::parseHexColor(layers[k]->maxColor);
        // The colours of the model can be (partially) transparent, those chosen in the panel cannot
        double lowOpacity = 1.0;
        double highOpacity = 1.0;
        if (!lowColor || !highColor)
        {
            // The colours the model gives; when they are the same the model does not show the intensity
            if (lowest[k].color && highest[k].color && toRgb(*lowest[k].color) != toRgb(*highest[k].color))
            {
                lowColor = toRgb(*lowest[k].color);
                highColor = toRgb(*highest[k].color);
                lowOpacity = toOpacity(*lowest[k].color);
                highOpacity = toOpacity(*highest[k].color);
            }
            else
            {
                const auto [defaultLow, defaultHigh] = VolumeLayers::defaultColors(k);
                lowColor = defaultLow;
                highColor = defaultHigh;
            }
        }
        const auto low = makeShadeable(*lowColor);
        const auto high = makeShadeable(*highColor);
        const auto unit = [](int channel) { return toUnitColor(static_cast<std::uint8_t>(channel)); };
        colors->AddRGBPoint(VolumeLayers::layerFirstScalar(k), unit(low.red), unit(low.green), unit(low.blue));
        colors->AddRGBPoint(VolumeLayers::layerLastScalar(k), unit(high.red), unit(high.green), unit(high.blue));

        // Right below the first scalar of the layer the opacity is already the one of the layer, so that
        // the voxels of the layer get exactly their opacity and the "no value" scalars stay transparent
        opacity->AddPoint(VolumeLayers::layerFirstScalar(k) - 0.1, fullOpacity * lowOpacity);
        opacity->AddPoint(VolumeLayers::layerFirstScalar(k), fullOpacity * lowOpacity);
        opacity->AddPoint(VolumeLayers::layerLastScalar(k), fullOpacity * highOpacity);
    }

    vtkNew<vtkImageData> image;
    image->SetDimensions(nCols, nRows, drawnSlices);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->GetPointData()->SetScalars(scalars);

    vtkNew<vtkVolumeProperty> property;
    property->SetColor(colors);
    property->SetScalarOpacity(opacity);
    property->SetInterpolationTypeToNearest();
    property->ShadeOn();
    property->SetAmbient(0.22);
    property->SetDiffuse(0.78);
    property->SetSpecular(0.35);
    property->SetSpecularPower(18.0);
    property->SetScalarOpacityUnitDistance(0.8);

    vtkNew<vtkSmartVolumeMapper> mapper;
    mapper->SetInputData(image);
    mapper->SetBlendModeToComposite();
    mapper->SetAutoAdjustSampleDistances(true);

    volumeActor->SetMapper(mapper);
    volumeActor->SetProperty(property);
    renderer->AddVolume(volumeActor);
    return true;
}

template<class Matrix>
bool Visualizer::buidColor(vtkLookupTable* lut, int nCols, int nRows, const Matrix &p, const std::vector<const SubstateInfo*>& colorSubstateInfos)
{
    bool anyTransparent = false;
    for (int r = 0; r < nRows; ++r)
    {
        for (int c = 0; c < nCols; ++c)
        {
            const auto color = calculateCellColor(r, c, p, colorSubstateInfos);
            const double opacity = toOpacity(color);
            anyTransparent = anyTransparent || opacity < 1.0;
            lut->SetTableValue(
                (nRows - 1 - r) * nCols + c,
                toUnitColor(color.getRed()),
                toUnitColor(color.getGreen()),
                toUnitColor(color.getBlue()),
                opacity
            );
        }
    }
    return anyTransparent;
}

template<class Matrix>
void Visualizer::drawGridLinesOn3DSurface(const Matrix& p,
                                          int nRows,
                                          int nCols,
                                          const std::vector<Line>& lines,
                                          vtkSmartPointer<vtkRenderer> renderer,
                                          vtkSmartPointer<vtkActor> gridLinesActor,
                                          const std::string& substateFieldName,
                                          double minValue,
                                          double maxValue)
{
    if (!renderer || !gridLinesActor)
    {
        return;
    }

    vtkSmartPointer<vtkPolyData> polyData = buildGridLinesOnSurfacePolyData(p,
                                                                            nRows,
                                                                            nCols,
                                                                            lines,
                                                                            substateFieldName,
                                                                            minValue,
                                                                            maxValue);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);
    gridLinesActor->SetMapper(mapper);
    gridLinesActor->GetProperty()->SetLineWidth(1.0);

    // Apply grid color from settings
    applyGridColorTo3DGridLinesActor(gridLinesActor);

    renderer->AddActor(gridLinesActor);
}

template<class Matrix>
void Visualizer::refreshGridLinesOn3DSurface(const Matrix& p,
                                             int nRows,
                                             int nCols,
                                             const std::vector<Line>& lines,
                                             vtkSmartPointer<vtkActor> gridLinesActor,
                                             const std::string& substateFieldName,
                                             double minValue,
                                             double maxValue)
{
    if (!gridLinesActor)
    {
        return;
    }

    auto* mapper = vtkPolyDataMapper::SafeDownCast(gridLinesActor->GetMapper());
    if (!mapper)
    {
        return;
    }

    vtkSmartPointer<vtkPolyData> polyData = buildGridLinesOnSurfacePolyData(p,
                                                                            nRows,
                                                                            nCols,
                                                                            lines,
                                                                            substateFieldName,
                                                                            minValue,
                                                                            maxValue);
    mapper->SetInputData(polyData);
    mapper->Update();

    // Update grid color from settings
    applyGridColorTo3DGridLinesActor(gridLinesActor);
}

template<class Matrix>
void Visualizer::drawGridLinesOn3DSubstateStack(const Matrix& p,
                                                int nRows,
                                                int nCols,
                                                const std::vector<Line>& lines,
                                                vtkSmartPointer<vtkRenderer> renderer,
                                                vtkSmartPointer<vtkActor> gridLinesActor,
                                                const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop)
{
    if (!renderer || !gridLinesActor)
        return;

    vtkSmartPointer<vtkPolyData> polyData =
        buildGridLinesOnSubstateStackPolyData(p, nRows, nCols, lines, heightSubstateInfosBottomToTop);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);
    gridLinesActor->SetMapper(mapper);
    gridLinesActor->GetProperty()->SetLineWidth(1.0);

    applyGridColorTo3DGridLinesActor(gridLinesActor);
    renderer->AddActor(gridLinesActor);
}

template<class Matrix>
void Visualizer::refreshGridLinesOn3DSubstateStack(const Matrix& p,
                                                   int nRows,
                                                   int nCols,
                                                   const std::vector<Line>& lines,
                                                   vtkSmartPointer<vtkActor> gridLinesActor,
                                                   const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop)
{
    if (!gridLinesActor)
        return;

    auto* mapper = vtkPolyDataMapper::SafeDownCast(gridLinesActor->GetMapper());
    if (!mapper)
        return;

    vtkSmartPointer<vtkPolyData> polyData =
        buildGridLinesOnSubstateStackPolyData(p, nRows, nCols, lines, heightSubstateInfosBottomToTop);

    mapper->SetInputData(polyData);
    mapper->Update();

    applyGridColorTo3DGridLinesActor(gridLinesActor);
}

template<class Matrix>
Color Visualizer::calculateCellColor(int row, int column, const Matrix &p, const std::vector<const SubstateInfo*>& colorSubstateInfos)
{
    if (colorSubstateInfos.empty())
    {
        auto optionalColor = calculateCellColorOptional(row, column, p);
        if (optionalColor.has_value())
        {
            return optionalColor.value();
        }
        else
        {
            // Return flat scene background color for noValue or out of range
            return flatSceneBackgroundColor();
        }
    }

    auto orderCompare = [](const SubstateInfo* lhs, const SubstateInfo* rhs)
    {
        return lhs->order > rhs->order;
    };
    auto colorSubstateInfosSorted = colorSubstateInfos; // TODO: GB: It is better to sort once, not for each cell
    std::ranges::sort(colorSubstateInfosSorted);

    for (const auto colorSubstateInfo : colorSubstateInfosSorted)
    {
        auto optionalColor = calculateCellColorOptional(row, column, p, colorSubstateInfo);
        if (optionalColor.has_value())
        {
            return optionalColor.value();
        }
    }

    // Return flat scene background color for noValue or out of range
    return flatSceneBackgroundColor();
}

template<class Matrix>
std::optional<Color> Visualizer::calculateCellColorOptional(int row, int column, const Matrix &p, const SubstateInfo* substateInfo)
{
    // If substateInfo with colors is provided, use gradient coloring
    if (substateInfo && ! substateInfo->minColor.empty() && ! substateInfo->maxColor.empty())
    {
        // Get substate value for this cell
        const char* fieldNamePtr = substateInfo->name.empty() ? nullptr : substateInfo->name.c_str();
        std::string cellValue = p[row][column].stringEncoding(fieldNamePtr);

        try
        {
            double value = std::stod(cellValue);
            double minVal = substateInfo->minValue;
            double maxVal = substateInfo->maxValue;

            // Check if value is noValue (out of range or equals noValue)
            if (std::isnan(minVal) || std::isnan(maxVal))
            {
                // Min/max not set - return nullopt
                return std::nullopt;
            }
            else if (substateInfo->noValueEnabled && !std::isnan(substateInfo->noValue) && value == substateInfo->noValue)
            {
                // Value equals noValue and noValue filtering is enabled
                return std::nullopt;
            }
            else if (value <= minVal || value >= maxVal)
            {
                // Value out of range
                return std::nullopt;
            }

            // Normalize value to [0, 1]
            double normalized = (value - minVal) / (maxVal - minVal);

            // Parse min and max colors (hex format: #RRGGBB)
            auto parseHexColor = [](const std::string& hex) -> std::tuple<int, int, int> {
                if (hex.length() != 7 || hex[0] != '#')
                    return {0, 0, 0};
                int r = std::stoi(hex.substr(1, 2), nullptr, 16);
                int g = std::stoi(hex.substr(3, 2), nullptr, 16);
                int b = std::stoi(hex.substr(5, 2), nullptr, 16);
                return {r, g, b};
            };

            auto [minR, minG, minB] = parseHexColor(substateInfo->minColor);
            auto [maxR, maxG, maxB] = parseHexColor(substateInfo->maxColor);

            // Interpolate between min and max colors
            int r = static_cast<int>(minR + (maxR - minR) * normalized);
            int g = static_cast<int>(minG + (maxG - minG) * normalized);
            int b = static_cast<int>(minB + (maxB - minB) * normalized);

            return Color(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), 255);
        }
        catch (const std::exception&)
        {
            // Failed to parse value - return nullopt
            return std::nullopt;
        }
    }
    else
    {
        // No custom colors configured - use outputValue coloring
        // If substateInfo is provided but no custom colors, use the substate field name
        // Otherwise use nullptr for default coloring
        const char* fieldNamePtr = (substateInfo && !substateInfo->name.empty())
                                       ? substateInfo->name.c_str()
                                       : nullptr;
        return p[row][column].outputValue(fieldNamePtr, gvm);
    }
}

////////////////////////////////////////////////////////////////////
// 3D Substate Visualization Methods
template<class Matrix>
vtkSmartPointer<vtkPolyData> Visualizer::build3DSubstateSurfaceQuadMesh(const Matrix& p, int nRows, int nCols,
                                                                        const std::string& substateFieldName,
                                                                        double minValue, double maxValue,
                                                                        const std::vector<const SubstateInfo*>& colorSubstateInfos)
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

    // Helper lambda to get cell color using calculateCellColor for proper custom color handling
    auto getCellColor = [&](int row, int col) -> Color {
        if (row < 0 || row >= nRows || col < 0 || col >= nCols)
            return Color(0, 0, 0);
        return calculateCellColor(row, col, p, colorSubstateInfos);
    };

    // Helper lambda to check if value is valid (not no-data)
    auto isValidValue = [&](double val) -> bool {
        // Values equal to minValue typically represent "no data" (background) points.
        // Treat them as invalid so the 3D surface remains suspended instead of forming
        // vertical walls down to the base plane.
        return !std::isnan(val) && (val - minValue) > eps && val <= maxValue;
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
    cellColors->SetNumberOfComponents(4); // RGBA, the alpha channel makes transparent cells see-through

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
            double opacitySum = 0.0;
            if (v0) { rSum += c0.getRed(); gSum += c0.getGreen(); bSum += c0.getBlue(); opacitySum += toOpacity(c0); }
            if (v1) { rSum += c1.getRed(); gSum += c1.getGreen(); bSum += c1.getBlue(); opacitySum += toOpacity(c1); }
            if (v2) { rSum += c2.getRed(); gSum += c2.getGreen(); bSum += c2.getBlue(); opacitySum += toOpacity(c2); }
            if (v3) { rSum += c3.getRed(); gSum += c3.getGreen(); bSum += c3.getBlue(); opacitySum += toOpacity(c3); }

            unsigned char r = static_cast<unsigned char>(rSum / validCount);
            unsigned char g = static_cast<unsigned char>(gSum / validCount);
            unsigned char b = static_cast<unsigned char>(bSum / validCount);
            unsigned char a = static_cast<unsigned char>(std::lround(255.0 * opacitySum / validCount));

            cellColors->InsertNextTuple4(r, g, b, a);
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
vtkSmartPointer<vtkPolyData> Visualizer::build3DSubstateStackSurfaceQuadMesh(
    const Matrix& p,
    int nRows,
    int nCols,
    const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
    const std::vector<const SubstateInfo*>& colorSubstateInfos)
{
    std::vector<const SubstateInfo*> layers;
    for (const auto* info : heightSubstateInfosBottomToTop)
    {
        if (info &&
            !std::isnan(info->minValue) &&
            !std::isnan(info->maxValue) &&
            info->minValue < info->maxValue)
        {
            layers.push_back(info);
        }
    }

    if (layers.empty())
        return vtkSmartPointer<vtkPolyData>::New();

    const double heightScale = std::max(nRows, nCols) / 3.0;
    const double eps = 1e-9;

    auto layerBaseline = [&](std::size_t layerIndex) -> double {
        const auto* info = layers[layerIndex];

        // The bottom layer is usually a terrain/elevation field. Draw it
        // relative to its displayed minimum so the scene is not lifted by
        // an arbitrary absolute datum such as z=490.
        if (layerIndex == 0)
            return info->minValue;

        // Upper layers are usually thickness/depth fields. If a noValue/zero
        // datum is configured below the display minimum, use it as the additive
        // baseline so e.g. h=8 contributes 8 units, not 7 or a full normalized
        // layer.
        if (info->noValueEnabled && std::isfinite(info->noValue) && info->noValue < info->minValue)
            return info->noValue;

        return info->minValue;
    };

    auto layerAltitudeScale = [&](std::size_t layerIndex) -> double {
        const auto scale = layers[layerIndex]->altitudeScale;
        return std::isfinite(scale) && scale > 0.0 ? scale : 1.0;
    };

    double rawStackRange = 0.0;
    for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
    {
        rawStackRange += std::max(1e-12, layers[layerIndex]->maxValue - layerBaseline(layerIndex));
    }
    const double heightPerUnit = heightScale / std::max(1e-12, rawStackRange);

    auto readLayerContribution = [&](int row, int col, std::size_t layerIndex) -> std::optional<double> {
        const auto* info = layers[layerIndex];
        if (!info || row < 0 || row >= nRows || col < 0 || col >= nCols)
            return std::nullopt;

        try
        {
            const double value = std::stod(p[row][col].stringEncoding(info->name.c_str()));

            if (!std::isfinite(value))
                return std::nullopt;
            if (info->noValueEnabled && !std::isnan(info->noValue) && value == info->noValue)
                return std::nullopt;
            if (value + eps < info->minValue)
                return std::nullopt;

            const double contribution =
                (std::clamp(value, info->minValue, info->maxValue) - layerBaseline(layerIndex)) *
                heightPerUnit *
                layerAltitudeScale(layerIndex);
            return std::max(0.0, contribution);
        }
        catch (...)
        {
            return std::nullopt;
        }
    };

    auto cumulativeHeight = [&](int row, int col, std::size_t topLayerIndex) -> double {
        double height = 0.0;
        for (std::size_t layerIndex = 0; layerIndex <= topLayerIndex && layerIndex < layers.size(); ++layerIndex)
        {
            if (const auto contribution = readLayerContribution(row, col, layerIndex))
                height += *contribution;
        }
        return height;
    };

    auto currentLayerHasValue = [&](int row, int col, std::size_t layerIndex) -> bool {
        return readLayerContribution(row, col, layerIndex).has_value();
    };

    auto gridToVtk = [&](int row, int col) -> std::pair<double, double> {
        return {static_cast<double>(col), static_cast<double>(nRows - 1 - row)};
    };

    auto getLayerColor = [&](int row, int col, const SubstateInfo* layerInfo) -> Color {
        if (row < 0 || row >= nRows || col < 0 || col >= nCols)
            return Color(0, 0, 0);

        if (!colorSubstateInfos.empty())
            return calculateCellColor(row, col, p, colorSubstateInfos);

        return calculateCellColor(row, col, p, std::vector<const SubstateInfo*>{layerInfo});
    };

    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> cells;
    vtkNew<vtkUnsignedCharArray> cellColors;
    cellColors->SetNumberOfComponents(4); // RGBA, the alpha channel makes transparent cells see-through

    std::vector<std::vector<vtkIdType>> basePointIds(layers.size(),
                                                     std::vector<vtkIdType>(static_cast<std::size_t>(nRows * nCols), -1));

    for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
    {
        for (int row = 0; row < nRows; ++row)
        {
            for (int col = 0; col < nCols; ++col)
            {
                const double height = cumulativeHeight(row, col, layerIndex);
                auto [x, y] = gridToVtk(row, col);
                const vtkIdType pid = points->InsertNextPoint(x, y, height);
                basePointIds[layerIndex][static_cast<std::size_t>(row * nCols + col)] = pid;
            }
        }
    }

    auto addVirtualPoint = [&](int row, int col, double height) -> vtkIdType {
        auto [x, y] = gridToVtk(row, col);
        return points->InsertNextPoint(x, y, height);
    };

    for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
    {
        const auto* layerInfo = layers[layerIndex];

        for (int row = 0; row + 1 < nRows; ++row)
        {
            for (int col = 0; col + 1 < nCols; ++col)
            {
                const double z0 = cumulativeHeight(row, col, layerIndex);
                const double z1 = cumulativeHeight(row, col + 1, layerIndex);
                const double z2 = cumulativeHeight(row + 1, col + 1, layerIndex);
                const double z3 = cumulativeHeight(row + 1, col, layerIndex);

                const bool v0 = currentLayerHasValue(row, col, layerIndex);
                const bool v1 = currentLayerHasValue(row, col + 1, layerIndex);
                const bool v2 = currentLayerHasValue(row + 1, col + 1, layerIndex);
                const bool v3 = currentLayerHasValue(row + 1, col, layerIndex);

                const int validCount = (int)v0 + (int)v1 + (int)v2 + (int)v3;
                if (validCount < 2)
                    continue;

                double sum = 0.0;
                if (v0) sum += z0;
                if (v1) sum += z1;
                if (v2) sum += z2;
                if (v3) sum += z3;
                const double avgHeight = sum / std::max(1, validCount);

                vtkIdType ids[4];
                ids[0] = v0 ? basePointIds[layerIndex][static_cast<std::size_t>(row * nCols + col)] : addVirtualPoint(row, col, avgHeight);
                ids[1] = v1 ? basePointIds[layerIndex][static_cast<std::size_t>(row * nCols + (col + 1))] : addVirtualPoint(row, col + 1, avgHeight);
                ids[2] = v2 ? basePointIds[layerIndex][static_cast<std::size_t>((row + 1) * nCols + (col + 1))] : addVirtualPoint(row + 1, col + 1, avgHeight);
                ids[3] = v3 ? basePointIds[layerIndex][static_cast<std::size_t>((row + 1) * nCols + col)] : addVirtualPoint(row + 1, col, avgHeight);

                cells->InsertNextCell(4);
                cells->InsertCellPoint(ids[0]);
                cells->InsertCellPoint(ids[1]);
                cells->InsertCellPoint(ids[2]);
                cells->InsertCellPoint(ids[3]);

                const Color c0 = getLayerColor(row, col, layerInfo);
                const Color c1 = getLayerColor(row, col + 1, layerInfo);
                const Color c2 = getLayerColor(row + 1, col + 1, layerInfo);
                const Color c3 = getLayerColor(row + 1, col, layerInfo);

                int rSum = 0, gSum = 0, bSum = 0;
                double opacitySum = 0.0;
                if (v0) { rSum += c0.getRed(); gSum += c0.getGreen(); bSum += c0.getBlue(); opacitySum += toOpacity(c0); }
                if (v1) { rSum += c1.getRed(); gSum += c1.getGreen(); bSum += c1.getBlue(); opacitySum += toOpacity(c1); }
                if (v2) { rSum += c2.getRed(); gSum += c2.getGreen(); bSum += c2.getBlue(); opacitySum += toOpacity(c2); }
                if (v3) { rSum += c3.getRed(); gSum += c3.getGreen(); bSum += c3.getBlue(); opacitySum += toOpacity(c3); }

                cellColors->InsertNextTuple4(static_cast<unsigned char>(rSum / validCount),
                                             static_cast<unsigned char>(gSum / validCount),
                                             static_cast<unsigned char>(bSum / validCount),
                                             static_cast<unsigned char>(std::lround(255.0 * opacitySum / validCount)));
            }
        }
    }

    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetPolys(cells);
    polyData->GetCellData()->SetScalars(cellColors);

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
vtkSmartPointer<vtkPolyData> Visualizer::buildGridLinesOnSurfacePolyData(const Matrix& p,
                                                                         int nRows,
                                                                         int nCols,
                                                                         const std::vector<Line>& lines,
                                                                         const std::string& substateFieldName,
                                                                         double minValue,
                                                                         double maxValue)
{
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> polyLines;
    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();

    if (nRows <= 0 || nCols <= 0 || lines.empty() || substateFieldName.empty() ||
        std::isnan(minValue) || std::isnan(maxValue) || minValue >= maxValue)
    {
        polyData->SetPoints(points);
        polyData->SetLines(polyLines);
        return polyData;
    }

    const double valueRange = std::max(1e-12, maxValue - minValue);
    const double heightScale = std::max(nRows, nCols) / 3.0;
    const char* fieldNamePtr = substateFieldName.c_str();

    auto getCellValue = [&](int row, int col) -> double {
        if (row < 0 || row >= nRows || col < 0 || col >= nCols)
        {
            return minValue;
        }

        try
        {
            std::string valueStr = p[row][col].stringEncoding(fieldNamePtr);
            double value = std::stod(valueStr);
            return std::clamp(value, minValue, maxValue);
        }
        catch (...)
        {
            return minValue;
        }
    };

    auto valueToHeight = [&](double val) -> double {
        double normalized = (val - minValue) / valueRange;
        normalized = std::clamp(normalized, 0.0, 1.0);
        return normalized * heightScale;
    };

    auto sampleHeight = [&](double gridX, double gridY) -> double {
        const double clampedX = std::clamp(gridX, 0.0, static_cast<double>(nCols - 1));
        const double clampedY = std::clamp(gridY, 0.0, static_cast<double>(nRows - 1));

        const double baseX = std::floor(clampedX);
        const double baseY = std::floor(clampedY);

        const int col0 = static_cast<int>(baseX);
        const int row0 = static_cast<int>(baseY);
        const int col1 = std::min(col0 + 1, nCols - 1);
        const int row1 = std::min(row0 + 1, nRows - 1);

        const double fracX = clampedX - baseX;
        const double fracY = clampedY - baseY;

        const double h00 = valueToHeight(getCellValue(row0, col0));
        const double h10 = valueToHeight(getCellValue(row0, col1));
        const double h01 = valueToHeight(getCellValue(row1, col0));
        const double h11 = valueToHeight(getCellValue(row1, col1));

        const double h0 = h00 + (h10 - h00) * fracX;
        const double h1 = h01 + (h11 - h01) * fracX;
        constexpr double HEIGHT_EPSILON = 1e-2;
        return h0 + (h1 - h0) * fracY + HEIGHT_EPSILON;
    };

    for (const auto& line : lines)
    {
        const double dx = line.x2 - line.x1;
        const double dy = line.y2 - line.y1;
        const int steps = std::max(1, static_cast<int>(std::max(std::fabs(dx), std::fabs(dy))));

        std::vector<vtkIdType> linePointIds;
        linePointIds.reserve(static_cast<std::size_t>(steps) + 1);

        for (int step = 0; step <= steps; ++step)
        {
            const double t = static_cast<double>(step) / static_cast<double>(steps);
            const double gridX = line.x1 + dx * t;
            const double gridY = line.y1 + dy * t;

            const double vtkX = gridX;
            const double vtkY = (nRows - 1) - gridY;
            const double vtkZ = sampleHeight(gridX, gridY);

            const vtkIdType pid = points->InsertNextPoint(vtkX, vtkY, vtkZ);
            linePointIds.push_back(pid);
        }

        if (linePointIds.size() >= 2)
        {
            polyLines->InsertNextCell(static_cast<vtkIdType>(linePointIds.size()));
            for (vtkIdType pid : linePointIds)
            {
                polyLines->InsertCellPoint(pid);
            }
        }
    }

    polyData->SetPoints(points);
    polyData->SetLines(polyLines);
    return polyData;
}

template<class Matrix>
vtkSmartPointer<vtkPolyData> Visualizer::buildGridLinesOnSubstateStackPolyData(
    const Matrix& p,
    int nRows,
    int nCols,
    const std::vector<Line>& lines,
    const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop)
{
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> polyLines;
    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();

    std::vector<const SubstateInfo*> layers;
    for (const auto* info : heightSubstateInfosBottomToTop)
    {
        if (info &&
            !std::isnan(info->minValue) &&
            !std::isnan(info->maxValue) &&
            info->minValue < info->maxValue)
        {
            layers.push_back(info);
        }
    }

    if (nRows <= 0 || nCols <= 0 || lines.empty() || layers.empty())
    {
        polyData->SetPoints(points);
        polyData->SetLines(polyLines);
        return polyData;
    }

    const double heightScale = std::max(nRows, nCols) / 3.0;
    const double eps = 1e-9;

    auto layerBaseline = [&](std::size_t layerIndex) -> double {
        const auto* info = layers[layerIndex];
        if (layerIndex == 0)
            return info->minValue;

        if (info->noValueEnabled && std::isfinite(info->noValue) && info->noValue < info->minValue)
            return info->noValue;

        return info->minValue;
    };

    auto layerAltitudeScale = [&](std::size_t layerIndex) -> double {
        const auto scale = layers[layerIndex]->altitudeScale;
        return std::isfinite(scale) && scale > 0.0 ? scale : 1.0;
    };

    double rawStackRange = 0.0;
    for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
    {
        rawStackRange += std::max(1e-12, layers[layerIndex]->maxValue - layerBaseline(layerIndex));
    }
    const double heightPerUnit = heightScale / std::max(1e-12, rawStackRange);

    auto readLayerContribution = [&](int row, int col, std::size_t layerIndex) -> double {
        const auto* info = layers[layerIndex];
        if (!info || row < 0 || row >= nRows || col < 0 || col >= nCols)
            return 0.0;

        try
        {
            const double value = std::stod(p[row][col].stringEncoding(info->name.c_str()));
            if (!std::isfinite(value))
                return 0.0;
            if (info->noValueEnabled && !std::isnan(info->noValue) && value == info->noValue)
                return 0.0;
            if (value + eps < info->minValue)
                return 0.0;

            const double contribution =
                (std::clamp(value, info->minValue, info->maxValue) - layerBaseline(layerIndex)) *
                heightPerUnit *
                layerAltitudeScale(layerIndex);
            return std::max(0.0, contribution);
        }
        catch (...)
        {
            return 0.0;
        }
    };

    auto cellHeight = [&](int row, int col) -> double {
        double height = 0.0;
        for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
            height += readLayerContribution(row, col, layerIndex);
        return height;
    };

    auto sampleHeight = [&](double gridX, double gridY) -> double {
        const double clampedX = std::clamp(gridX, 0.0, static_cast<double>(nCols - 1));
        const double clampedY = std::clamp(gridY, 0.0, static_cast<double>(nRows - 1));

        const double baseX = std::floor(clampedX);
        const double baseY = std::floor(clampedY);

        const int col0 = static_cast<int>(baseX);
        const int row0 = static_cast<int>(baseY);
        const int col1 = std::min(col0 + 1, nCols - 1);
        const int row1 = std::min(row0 + 1, nRows - 1);

        const double fracX = clampedX - baseX;
        const double fracY = clampedY - baseY;

        const double h00 = cellHeight(row0, col0);
        const double h10 = cellHeight(row0, col1);
        const double h01 = cellHeight(row1, col0);
        const double h11 = cellHeight(row1, col1);

        const double h0 = h00 + (h10 - h00) * fracX;
        const double h1 = h01 + (h11 - h01) * fracX;
        constexpr double HEIGHT_EPSILON = 1e-2;
        return h0 + (h1 - h0) * fracY + HEIGHT_EPSILON;
    };

    for (const auto& line : lines)
    {
        const double dx = line.x2 - line.x1;
        const double dy = line.y2 - line.y1;
        const int steps = std::max(1, static_cast<int>(std::max(std::fabs(dx), std::fabs(dy))));

        std::vector<vtkIdType> linePointIds;
        linePointIds.reserve(static_cast<std::size_t>(steps) + 1);

        for (int step = 0; step <= steps; ++step)
        {
            const double t = static_cast<double>(step) / static_cast<double>(steps);
            const double gridX = line.x1 + dx * t;
            const double gridY = line.y1 + dy * t;

            const double vtkX = gridX;
            const double vtkY = (nRows - 1) - gridY;
            const double vtkZ = sampleHeight(gridX, gridY);

            const vtkIdType pid = points->InsertNextPoint(vtkX, vtkY, vtkZ);
            linePointIds.push_back(pid);
        }

        if (linePointIds.size() >= 2)
        {
            polyLines->InsertNextCell(static_cast<vtkIdType>(linePointIds.size()));
            for (vtkIdType pid : linePointIds)
                polyLines->InsertCellPoint(pid);
        }
    }

    polyData->SetPoints(points);
    polyData->SetLines(polyLines);
    return polyData;
}

template<class Matrix>
void Visualizer::drawWithVTK3DSubstate(const Matrix& p, int nRows, int nCols,
                                       vtkSmartPointer<vtkRenderer> renderer,
                                       vtkSmartPointer<vtkActor> gridActor,
                                       const std::string& substateFieldName,
                                       double minValue, double maxValue,
                                       const std::vector<const SubstateInfo*>& colorSubstateInfos)
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
    vtkSmartPointer<vtkPolyData> surfacePolyData = build3DSubstateSurfaceQuadMesh(p, nRows, nCols, substateFieldName, minValue, maxValue, colorSubstateInfos);

    // Create mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(surfacePolyData);
    mapper->SetScalarModeToUseCellData();
    mapper->ScalarVisibilityOn();

    // Configure actor
    gridActor->SetMapper(mapper);
    gridActor->SetForceTranslucent(hasTranslucentCellColors(mapper));
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
void Visualizer::drawWithVTK3DSubstates(
    const Matrix& p,
    int nRows,
    int nCols,
    vtkSmartPointer<vtkRenderer> renderer,
    vtkSmartPointer<vtkActor> gridActor,
    const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
    const std::vector<const SubstateInfo*>& colorSubstateInfos)
{
    if (!gridActor || !renderer || heightSubstateInfosBottomToTop.empty())
    {
        drawWithVTK(p, nRows, nCols, renderer, gridActor);
        return;
    }

    renderer->RemoveActor(gridActor);

    vtkSmartPointer<vtkPolyData> surfacePolyData =
        build3DSubstateStackSurfaceQuadMesh(p, nRows, nCols, heightSubstateInfosBottomToTop, colorSubstateInfos);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(surfacePolyData);
    mapper->SetScalarModeToUseCellData();
    mapper->ScalarVisibilityOn();

    gridActor->SetMapper(mapper);
    gridActor->SetForceTranslucent(hasTranslucentCellColors(mapper));
    gridActor->GetProperty()->SetInterpolationToPhong();
    gridActor->GetProperty()->SetAmbient(0.3);
    gridActor->GetProperty()->SetDiffuse(0.7);
    gridActor->GetProperty()->SetSpecular(0.1);
    gridActor->GetProperty()->SetSpecularPower(10.0);
    gridActor->GetProperty()->EdgeVisibilityOff();

    renderer->AddActor(gridActor);
    renderer->ResetCamera();
}

template<class Matrix>
void Visualizer::refreshWindowsVTK3DSubstate(const Matrix& p, int nRows, int nCols,
                                             vtkSmartPointer<vtkActor> gridActor,
                                             const std::string& substateFieldName,
                                             double minValue, double maxValue,
                                             const std::vector<const SubstateInfo*>& colorSubstateInfos)
{
    if (! gridActor)
    {
        return;
    }

    auto* mapper = vtkPolyDataMapper::SafeDownCast(gridActor->GetMapper());
    if (!mapper)
    {
        return;
    }

    vtkSmartPointer<vtkPolyData> surfacePolyData = build3DSubstateSurfaceQuadMesh(p,
                                                                                  nRows,
                                                                                  nCols,
                                                                                  substateFieldName,
                                                                                  minValue,
                                                                                  maxValue,
                                                                                  colorSubstateInfos);

    mapper->SetInputData(surfacePolyData);
    mapper->Update();
}

template<class Matrix>
void Visualizer::refreshWindowsVTK3DSubstates(
    const Matrix& p,
    int nRows,
    int nCols,
    vtkSmartPointer<vtkActor> gridActor,
    const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
    const std::vector<const SubstateInfo*>& colorSubstateInfos)
{
    if (!gridActor)
        return;

    auto* mapper = vtkPolyDataMapper::SafeDownCast(gridActor->GetMapper());
    if (!mapper)
        return;

    vtkSmartPointer<vtkPolyData> surfacePolyData =
        build3DSubstateStackSurfaceQuadMesh(p, nRows, nCols, heightSubstateInfosBottomToTop, colorSubstateInfos);

    mapper->SetInputData(surfacePolyData);
    mapper->Update();
}

template<class Matrix>
void Visualizer::drawWithVTK3DSubstateSlice(
    const Matrix& p,
    int nRows,
    int nCols,
    vtkSmartPointer<vtkRenderer> renderer,
    vtkSmartPointer<vtkActor> gridActor,
    const std::string& substateFieldName,
    double minValue,
    double maxValue,
    const std::vector<const SubstateInfo*>& colorSubstateInfos,
    GridSliceAxis fixedAxis,
    int fixedIndex)
{
    if (!renderer || !gridActor ||
        (fixedAxis != GridSliceAxis::X && fixedAxis != GridSliceAxis::Y) ||
        std::isnan(minValue) || std::isnan(maxValue) || minValue >= maxValue)
    {
        return;
    }

    const int sampleCount = fixedAxis == GridSliceAxis::Y ? nCols : nRows;
    const int clampedFixedIndex = fixedAxis == GridSliceAxis::Y
        ? std::clamp(fixedIndex, 0, nRows - 1)
        : std::clamp(fixedIndex, 0, nCols - 1);

    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> cells;
    vtkNew<vtkUnsignedCharArray> cellColors;
    cellColors->SetNumberOfComponents(4); // RGBA, the alpha channel makes transparent cells see-through

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const int row = fixedAxis == GridSliceAxis::Y ? clampedFixedIndex : sample;
        const int col = fixedAxis == GridSliceAxis::Y ? sample : clampedFixedIndex;

        double value = minValue;
        try
        {
            value = std::stod(p[row][col].stringEncoding(substateFieldName.c_str()));
        }
        catch (...)
        {
            continue;
        }

        if (!std::isfinite(value) || value <= minValue)
            continue;

        value = std::clamp(value, minValue, maxValue);
        const double x0 = static_cast<double>(sample);
        const double x1 = static_cast<double>(sample + 1);
        const vtkIdType ids[4] = {
            points->InsertNextPoint(x0, minValue, 1.0),
            points->InsertNextPoint(x1, minValue, 1.0),
            points->InsertNextPoint(x1, value, 1.0),
            points->InsertNextPoint(x0, value, 1.0)
        };

        cells->InsertNextCell(4);
        for (const vtkIdType id : ids)
            cells->InsertCellPoint(id);

        const Color color = calculateCellColor(row, col, p, colorSubstateInfos);
        cellColors->InsertNextTuple4(color.getRed(), color.getGreen(), color.getBlue(), static_cast<unsigned char>(std::lround(255.0 * toOpacity(color))));
    }

    vtkNew<vtkPolyData> profile;
    profile->SetPoints(points);
    profile->SetPolys(cells);
    profile->GetCellData()->SetScalars(cellColors);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(profile);
    mapper->SetScalarModeToUseCellData();
    mapper->ScalarVisibilityOn();

    renderer->RemoveActor(gridActor);
    gridActor->SetMapper(mapper);
    gridActor->SetForceTranslucent(hasTranslucentCellColors(mapper));
    gridActor->GetProperty()->SetInterpolationToFlat();
    gridActor->GetProperty()->SetAmbient(1.0);
    gridActor->GetProperty()->SetDiffuse(0.0);
    gridActor->GetProperty()->SetSpecular(0.0);
    gridActor->GetProperty()->EdgeVisibilityOff();
    renderer->AddActor(gridActor);
}

template<class Matrix>
void Visualizer::drawWithVTK3DSubstatesSlice(
    const Matrix& p,
    int nRows,
    int nCols,
    vtkSmartPointer<vtkRenderer> renderer,
    vtkSmartPointer<vtkActor> gridActor,
    const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
    const std::vector<const SubstateInfo*>& colorSubstateInfos,
    GridSliceAxis fixedAxis,
    int fixedIndex)
{
    std::vector<const SubstateInfo*> layers;
    for (const auto* info : heightSubstateInfosBottomToTop)
    {
        if (info &&
            !std::isnan(info->minValue) &&
            !std::isnan(info->maxValue) &&
            info->minValue < info->maxValue)
        {
            layers.push_back(info);
        }
    }

    if (!renderer || !gridActor ||
        (fixedAxis != GridSliceAxis::X && fixedAxis != GridSliceAxis::Y) ||
        layers.empty())
    {
        return;
    }

    const int sampleCount = fixedAxis == GridSliceAxis::Y ? nCols : nRows;
    const int clampedFixedIndex = fixedAxis == GridSliceAxis::Y
        ? std::clamp(fixedIndex, 0, nRows - 1)
        : std::clamp(fixedIndex, 0, nCols - 1);
    const double heightScale = std::max(nRows, nCols) / 3.0;
    const double eps = 1e-9;

    auto layerBaseline = [&](std::size_t layerIndex) -> double {
        const auto* info = layers[layerIndex];
        if (layerIndex == 0)
            return info->minValue;

        if (info->noValueEnabled && std::isfinite(info->noValue) && info->noValue < info->minValue)
            return info->noValue;

        return info->minValue;
    };

    auto layerAltitudeScale = [&](std::size_t layerIndex) -> double {
        const auto scale = layers[layerIndex]->altitudeScale;
        return std::isfinite(scale) && scale > 0.0 ? scale : 1.0;
    };

    double rawStackRange = 0.0;
    for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
    {
        rawStackRange += std::max(1e-12, layers[layerIndex]->maxValue - layerBaseline(layerIndex));
    }
    const double heightPerUnit = heightScale / std::max(1e-12, rawStackRange);

    auto readLayerContribution = [&](int row, int col, std::size_t layerIndex) -> std::optional<double> {
        const auto* info = layers[layerIndex];
        if (!info || row < 0 || row >= nRows || col < 0 || col >= nCols)
            return std::nullopt;

        try
        {
            const double value = std::stod(p[row][col].stringEncoding(info->name.c_str()));
            if (!std::isfinite(value))
                return std::nullopt;
            if (info->noValueEnabled && !std::isnan(info->noValue) && value == info->noValue)
                return std::nullopt;
            if (value + eps < info->minValue)
                return std::nullopt;

            const double contribution =
                (std::clamp(value, info->minValue, info->maxValue) - layerBaseline(layerIndex)) *
                heightPerUnit *
                layerAltitudeScale(layerIndex);
            return std::max(0.0, contribution);
        }
        catch (...)
        {
            return std::nullopt;
        }
    };

    auto getLayerColor = [&](int row, int col, const SubstateInfo* layerInfo) -> Color {
        if (!colorSubstateInfos.empty())
            return calculateCellColor(row, col, p, colorSubstateInfos);

        return calculateCellColor(row, col, p, std::vector<const SubstateInfo*>{layerInfo});
    };

    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> cells;
    vtkNew<vtkUnsignedCharArray> cellColors;
    cellColors->SetNumberOfComponents(4); // RGBA, the alpha channel makes transparent cells see-through

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const int row = fixedAxis == GridSliceAxis::Y ? clampedFixedIndex : sample;
        const int col = fixedAxis == GridSliceAxis::Y ? sample : clampedFixedIndex;

        double baseHeight = 0.0;
        for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
        {
            const auto* layerInfo = layers[layerIndex];
            const auto contribution = readLayerContribution(row, col, layerIndex);
            if (!contribution)
                continue;

            const double topHeight = baseHeight + *contribution;
            const double x0 = static_cast<double>(sample);
            const double x1 = static_cast<double>(sample + 1);
            const vtkIdType ids[4] = {
                points->InsertNextPoint(x0, baseHeight, 1.0),
                points->InsertNextPoint(x1, baseHeight, 1.0),
                points->InsertNextPoint(x1, topHeight, 1.0),
                points->InsertNextPoint(x0, topHeight, 1.0)
            };

            cells->InsertNextCell(4);
            for (const vtkIdType id : ids)
                cells->InsertCellPoint(id);

            const Color color = getLayerColor(row, col, layerInfo);
            cellColors->InsertNextTuple4(color.getRed(), color.getGreen(), color.getBlue(), static_cast<unsigned char>(std::lround(255.0 * toOpacity(color))));
            baseHeight = topHeight;
        }
    }

    vtkNew<vtkPolyData> profile;
    profile->SetPoints(points);
    profile->SetPolys(cells);
    profile->GetCellData()->SetScalars(cellColors);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(profile);
    mapper->SetScalarModeToUseCellData();
    mapper->ScalarVisibilityOn();

    renderer->RemoveActor(gridActor);
    gridActor->SetMapper(mapper);
    gridActor->SetForceTranslucent(hasTranslucentCellColors(mapper));
    gridActor->GetProperty()->SetInterpolationToFlat();
    gridActor->GetProperty()->SetAmbient(1.0);
    gridActor->GetProperty()->SetDiffuse(0.0);
    gridActor->GetProperty()->SetSpecular(0.0);
    gridActor->GetProperty()->EdgeVisibilityOff();
    renderer->AddActor(gridActor);
}
