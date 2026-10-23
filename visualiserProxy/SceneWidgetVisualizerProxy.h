/** @file SceneWidgetVisualizerProxy.h
 * @brief Template class for managing visualization of cell-based models.
 *
 * This file contains the SceneWidgetVisualizerTemplate class which provides
 * a bridge between the visualization system and cell-based models. It combines
 * a Visualizer instance with a ModelReader to handle the visualization of
 * grid-based cellular automata or similar models.
 * 
 * @details The Visualizer supports various models for visualization, each requiring
 * specific handling for its element type (Cell, which inherits from Element in OOpenCal).
 * Different models need dedicated file reading methods and specific visualization approaches.
 * This template class serves as a connector between model reading and its visualization,
 * providing a flexible way to handle various model types.
 *
 * For more architectural details, see: doc/CHANGELOG_RUNTIME_MODELS.md */

#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>
#include <vtkAbstractVolumeMapper.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include "ISceneWidgetVisualizer.h"
#include "data/ModelReader.hpp"
#include "visualiserProxy/ContiguousGrid.h"
#include "visualiser/Visualizer.hpp"
#include "visualiser/VolumePicking.h"

struct Line;
struct SettingParameter;

/** @class SceneWidgetVisualizerTemplate
 * @tparam Cell The cell type used in the model (must inherit from Element in OOpenCal)
 * 
 * @brief Template class that manages visualization of a grid-based model.
 * 
 * This class serves as a bridge between the visualization system and the underlying
 * model data. It combines a Visualizer for rendering with a ModelReader to load
 * and manage cell data, providing a complete solution for model visualization.
 * 
 * The template architecture allows for different model types to be visualized
 * using the same infrastructure, with each model providing its own Cell implementation
 * and corresponding ModelReader specialization.
 * 
 * @note The template parameter Cell must be a type that represents a single
 *       cell in the model and must inherit from Element (defined in OOpenCal).
 *       The ModelReader must be specialized for the specific Cell type to handle
 *       the model's file format and data structure. */
template<typename Cell>
class SceneWidgetVisualizerTemplate : public ISceneWidgetVisualizer
{
public:
    explicit SceneWidgetVisualizerTemplate(const std::string& modelName)
        : m_modelName(modelName)
    {
    }

    /** @brief Initializes the internal matrix with the specified dimensions.
     *
     * This method resizes the internal contiguous storage to match the given
     * dimensions, creating a grid of default-constructed Cell objects.
     *
     * @param dimX The width of the grid (number of columns)
     * @param dimY The height of the grid (number of rows)
     *
     * @note The dimensions must be positive integers. The method will create a
     *       grid with dimY rows and dimX columns. */
    void initMatrix(int dimX, int dimY) override
    {
        p.resize(dimY, dimX);
    }

    void prepareStage(int nNodeX, int nNodeY, int nNodeZ = 1) override
    {
        nodeCountZ = nNodeZ;
        modelReader.prepareStage(nNodeX, nNodeY, nNodeZ);
    }

    void clearStage() override
    {
        modelReader.clearStage();
    }

    void readStepsOffsetsForAllNodesFromFiles(int nNodeX, int nNodeY, int nNodeZ, const std::string& filename) override
    {
        modelReader.readStepsOffsetsForAllNodesFromFiles(nNodeX, nNodeY, nNodeZ, filename);
    }

    void readStageStateFromFilesForStep(SettingParameter* sp, Line* lines) override
    {
        if (p.rowCount() != static_cast<std::size_t>(sp->numberOfRowsY) ||
            p.columnCount() != static_cast<std::size_t>(sp->numberOfColumnX) ||
            p.layerCount() != static_cast<std::size_t>(sp->numberOfSlicesZ))
        {
            p.resize(sp->numberOfRowsY, sp->numberOfColumnX, sp->numberOfSlicesZ);
        }
        modelReader.readStageStateFromFilesForStep(p, sp, lines);
    }

    void drawWithVTK(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& colorSubstateInfos, bool useCellRendering = false) override
    {
        if (p.layerCount() > 1)
        {
            if (nativeSliceEnabled)
            {
                if (nativeVolumeActor)
                    renderer->RemoveVolume(nativeVolumeActor);

                const ContiguousGridSliceView<Cell> slice(p, nativeSliceAxis, nativeSliceIndex);
                visualiser.drawWithVTK(slice,
                                       static_cast<int>(slice.rowCount()),
                                       static_cast<int>(slice.columnCount()),
                                       renderer,
                                       gridActor,
                                       colorSubstateInfos,
                                       useCellRendering);
                return;
            }

            if (!nativeVolumeActor)
                nativeVolumeActor = vtkSmartPointer<vtkVolume>::New();
            nativeVolumeRenderer = renderer;
            renderer->RemoveVolume(nativeVolumeActor);
            visualiser.drawWithVTK3DVolume(p,
                                           nRows,
                                           nCols,
                                           static_cast<int>(p.layerCount()),
                                           renderer,
                                           nativeVolumeActor,
                                           colorSubstateInfos,
                                           nativeVolumeZGap,
                                           nativeVolumeLayerPointers());
            return;
        }

        if (nativeVolumeActor)
            renderer->RemoveVolume(nativeVolumeActor);
        visualiser.drawWithVTK(p, nRows, nCols, renderer, gridActor, colorSubstateInfos, useCellRendering);
    }

    void refreshWindowsVTK(int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        if (p.layerCount() > 1)
        {
            if (nativeSliceEnabled)
            {
                const ContiguousGridSliceView<Cell> slice(p, nativeSliceAxis, nativeSliceIndex);
                visualiser.refreshWindowsVTK(slice,
                                             static_cast<int>(slice.rowCount()),
                                             static_cast<int>(slice.columnCount()),
                                             gridActor,
                                             colorSubstateInfos);
                return;
            }

            if (!nativeVolumeActor)
                nativeVolumeActor = vtkSmartPointer<vtkVolume>::New();
            if (nativeVolumeRenderer)
                nativeVolumeRenderer->RemoveVolume(nativeVolumeActor);
            visualiser.drawWithVTK3DVolume(p,
                                           nRows,
                                           nCols,
                                           static_cast<int>(p.layerCount()),
                                           nativeVolumeRenderer,
                                           nativeVolumeActor,
                                           colorSubstateInfos,
                                           nativeVolumeZGap,
                                           nativeVolumeLayerPointers());
            return;
        }
        visualiser.refreshWindowsVTK(p, nRows, nCols, gridActor, colorSubstateInfos);
    }

    void setNative3DSlice(GridSliceAxis axis, int fixedIndex) override
    {
        const auto axisSize = [this, axis]
        {
            switch (axis)
            {
                case GridSliceAxis::X:
                    return p.columnCount();
                case GridSliceAxis::Y:
                    return p.rowCount();
                case GridSliceAxis::Z:
                    return p.layerCount();
            }
            return std::size_t{};
        }();

        if (axisSize == 0)
            return;

        nativeSliceAxis = axis;
        nativeSliceIndex = static_cast<std::size_t>(
            std::clamp(fixedIndex, 0, static_cast<int>(axisSize) - 1));
        nativeSliceEnabled = true;
    }

    void clearNative3DSlice() override
    {
        nativeSliceEnabled = false;
    }

    bool isNative3DSliceEnabled() const override
    {
        return nativeSliceEnabled;
    }

    void setNative3DVolumeZGap(int gapPlanes) override
    {
        nativeVolumeZGap = VolumeZSpacing::clampGap(gapPlanes);
    }

    int native3DVolumeZGap() const override
    {
        return nativeVolumeZGap;
    }

    GridSliceAxis native3DSliceAxis() const override
    {
        return nativeSliceAxis;
    }

    int native3DSliceIndex() const override
    {
        return static_cast<int>(nativeSliceIndex);
    }

    void drawWithVTK3DSubstate(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue, const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        visualiser.drawWithVTK3DSubstate(p, nRows, nCols, renderer, gridActor, substateFieldName, minValue, maxValue, colorSubstateInfos);
    }

    void drawWithVTK3DSubstates(int nRows,
                                int nCols,
                                vtkSmartPointer<vtkRenderer> renderer,
                                vtkSmartPointer<vtkActor> gridActor,
                                const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
                                const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        visualiser.drawWithVTK3DSubstates(p,
                                          nRows,
                                          nCols,
                                          renderer,
                                          gridActor,
                                          heightSubstateInfosBottomToTop,
                                          colorSubstateInfos);
    }

    void refreshWindowsVTK3DSubstate(int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue, const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        visualiser.refreshWindowsVTK3DSubstate(p, nRows, nCols, gridActor, substateFieldName, minValue, maxValue, colorSubstateInfos);
    }

    void refreshWindowsVTK3DSubstates(int nRows,
                                      int nCols,
                                      vtkSmartPointer<vtkActor> gridActor,
                                      const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
                                      const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        visualiser.refreshWindowsVTK3DSubstates(p,
                                                nRows,
                                                nCols,
                                                gridActor,
                                                heightSubstateInfosBottomToTop,
                                                colorSubstateInfos);
    }

    void drawWithVTK3DSubstateSlice(int nRows,
                                    int nCols,
                                    vtkSmartPointer<vtkRenderer> renderer,
                                    vtkSmartPointer<vtkActor> gridActor,
                                    const std::string& substateFieldName,
                                    double minValue,
                                    double maxValue,
                                    const std::vector<const SubstateInfo*>& colorSubstateInfos,
                                    GridSliceAxis fixedAxis,
                                    int fixedIndex) override
    {
        visualiser.drawWithVTK3DSubstateSlice(p,
                                              nRows,
                                              nCols,
                                              renderer,
                                              gridActor,
                                              substateFieldName,
                                              minValue,
                                              maxValue,
                                              colorSubstateInfos,
                                              fixedAxis,
                                              fixedIndex);
    }

    void drawWithVTK3DSubstatesSlice(int nRows,
                                     int nCols,
                                     vtkSmartPointer<vtkRenderer> renderer,
                                     vtkSmartPointer<vtkActor> gridActor,
                                     const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop,
                                     const std::vector<const SubstateInfo*>& colorSubstateInfos,
                                     GridSliceAxis fixedAxis,
                                     int fixedIndex) override
    {
        visualiser.drawWithVTK3DSubstatesSlice(p,
                                               nRows,
                                               nCols,
                                               renderer,
                                               gridActor,
                                               heightSubstateInfosBottomToTop,
                                               colorSubstateInfos,
                                               fixedAxis,
                                               fixedIndex);
    }

    void drawFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> backgroundActor) override
    {
        const double zPosition = p.layerCount() > 1 ? -1.0 : 0.0;
        visualiser.drawFlatSceneBackground(nRows, nCols, renderer, backgroundActor, zPosition);
    }

    void refreshFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkActor> backgroundActor) override
    {
        visualiser.refreshFlatSceneBackground(nRows, nCols, backgroundActor);
    }

    void drawGridLinesOn3DSurface(int nRows, int nCols, const std::vector<Line>& lines, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridLinesActor, const std::string& substateFieldName, double minValue, double maxValue) override
    {
        if (p.layerCount() > 1)
        {
            visualiser.drawGridLinesFor3DVolume(nRows,
                                                nCols,
                                                static_cast<int>(p.layerCount()),
                                                static_cast<int>(nodeCountZ),
                                                lines,
                                                renderer,
                                                gridLinesActor,
                                                nativeVolumeZGap);
            return;
        }
        visualiser.drawGridLinesOn3DSurface(p, nRows, nCols, lines, renderer, gridLinesActor, substateFieldName, minValue, maxValue);
    }

    void drawGridLinesOn3DSubstateStack(int nRows,
                                        int nCols,
                                        const std::vector<Line>& lines,
                                        vtkSmartPointer<vtkRenderer> renderer,
                                        vtkSmartPointer<vtkActor> gridLinesActor,
                                        const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop) override
    {
        if (p.layerCount() > 1)
        {
            visualiser.drawGridLinesFor3DVolume(nRows,
                                                nCols,
                                                static_cast<int>(p.layerCount()),
                                                static_cast<int>(nodeCountZ),
                                                lines,
                                                renderer,
                                                gridLinesActor,
                                                nativeVolumeZGap);
            return;
        }
        visualiser.drawGridLinesOn3DSubstateStack(p,
                                                  nRows,
                                                  nCols,
                                                  lines,
                                                  renderer,
                                                  gridLinesActor,
                                                  heightSubstateInfosBottomToTop);
    }

    void refreshGridLinesOn3DSurface(int nRows, int nCols, const std::vector<Line>& lines, vtkSmartPointer<vtkActor> gridLinesActor, const std::string& substateFieldName, double minValue, double maxValue) override
    {
        if (p.layerCount() > 1)
        {
            visualiser.refreshGridLinesFor3DVolume(nRows,
                                                   nCols,
                                                   static_cast<int>(p.layerCount()),
                                                   static_cast<int>(nodeCountZ),
                                                   lines,
                                                   gridLinesActor,
                                                   nativeVolumeZGap);
            return;
        }
        visualiser.refreshGridLinesOn3DSurface(p, nRows, nCols, lines, gridLinesActor, substateFieldName, minValue, maxValue);
    }

    void refreshGridLinesOn3DSubstateStack(int nRows,
                                           int nCols,
                                           const std::vector<Line>& lines,
                                           vtkSmartPointer<vtkActor> gridLinesActor,
                                           const std::vector<const SubstateInfo*>& heightSubstateInfosBottomToTop) override
    {
        if (p.layerCount() > 1)
        {
            visualiser.refreshGridLinesFor3DVolume(nRows,
                                                   nCols,
                                                   static_cast<int>(p.layerCount()),
                                                   static_cast<int>(nodeCountZ),
                                                   lines,
                                                   gridLinesActor,
                                                   nativeVolumeZGap);
            return;
        }
        visualiser.refreshGridLinesOn3DSubstateStack(p,
                                                     nRows,
                                                     nCols,
                                                     lines,
                                                     gridLinesActor,
                                                     heightSubstateInfosBottomToTop);
    }

    Visualizer& getVisualizer() override
    {
        return visualiser;
    }

    std::string getModelName() const override
    {
        return m_modelName;
    }

    std::vector<StepIndex> availableSteps() const override
    {
        return modelReader.availableSteps();
    }

    CellHistory cellHistory(const SettingParameter& sp,
                            int viewRow,
                            int viewCol,
                            const std::vector<std::string>& fields,
                            const CellHistoryProgress& progress = {}) override
    {
        if (viewRow < 0 || viewCol < 0)
            return emptyHistory(fields);

        // Position of the cell in the whole grid (in a cross-section the view is only one plane of it)
        GridCellIndex index{static_cast<std::size_t>(viewRow), static_cast<std::size_t>(viewCol), 0};
        if (nativeSliceEnabled && p.layerCount() > 1)
        {
            const ContiguousGridSliceView<Cell> slice(p, nativeSliceAxis, nativeSliceIndex);
            if (static_cast<std::size_t>(viewRow) >= slice.rowCount() ||
                static_cast<std::size_t>(viewCol) >= slice.columnCount())
            {
                return emptyHistory(fields);
            }
            index = gridIndexFromSliceView(nativeSliceAxis,
                                           nativeSliceIndex,
                                           p.layerCount(),
                                           static_cast<std::size_t>(viewRow),
                                           static_cast<std::size_t>(viewCol));
        }
        return readCellHistory(sp, index, fields, progress);
    }

    CellHistory cellHistoryOfVolumeCell(const SettingParameter& sp,
                                        const GridCellIndex& cell,
                                        const std::vector<std::string>& fields,
                                        const CellHistoryProgress& progress = {}) override
    {
        if (! isInsideGrid(cell))
            return emptyHistory(fields);
        return readCellHistory(sp, cell, fields, progress);
    }

    void setNative3DVolumeSubstates(const std::vector<const SubstateInfo*>& layersTopToBottom) override
    {
        nativeVolumeLayers.clear();
        for (const SubstateInfo* info : layersTopToBottom)
        {
            if (info)
                nativeVolumeLayers.push_back(*info);
        }
    }

    std::optional<GridCellIndex> pickNative3DVolumeCell(const std::array<double, 3>& rayStart,
                                                        const std::array<double, 3>& rayEnd) const override
    {
        if (p.layerCount() <= 1 || nativeSliceEnabled || ! nativeVolumeActor)
            return std::nullopt;

        // What is on the screen is the image the volume was drawn from, so the pick sees exactly the voxels
        // which are visible: those whose scalar has a non-zero opacity in the transfer function.
        vtkAbstractVolumeMapper* mapper = nativeVolumeActor->GetMapper();
        auto* image = mapper ? vtkImageData::SafeDownCast(mapper->GetDataSetInput()) : nullptr;
        vtkVolumeProperty* property = nativeVolumeActor->GetProperty();
        vtkPiecewiseFunction* opacity = property ? property->GetScalarOpacity() : nullptr;
        vtkDataArray* scalars = image ? image->GetPointData()->GetScalars() : nullptr;
        if (! scalars || ! opacity)
            return std::nullopt;

        int dims[3];
        image->GetDimensions(dims);
        constexpr double minimalPickableOpacity = 0.1;
        const auto isVisible = [&](int x, int y, int z)
        {
            const vtkIdType index = (static_cast<vtkIdType>(z) * dims[1] + y) * dims[0] + x;
            return opacity->GetValue(scalars->GetComponent(index, 0)) >= minimalPickableOpacity;
        };

        const auto voxel = VolumePicking::firstVisibleVoxelOnRay({rayStart[0], rayStart[1], rayStart[2]},
                                                                 {rayEnd[0], rayEnd[1], rayEnd[2]},
                                                                 {dims[0], dims[1], dims[2]},
                                                                 isVisible);
        if (! voxel)
            return std::nullopt;

        const auto cell = VolumePicking::voxelToCell(*voxel, static_cast<int>(p.rowCount()), nativeVolumeZGap);
        if (! cell || ! isInsideGrid(*cell))
            return std::nullopt;
        return cell;
    }

    std::string getVolumeCellStringEncoding(const GridCellIndex& cell, const char* details = nullptr) const override
    {
        if (! isInsideGrid(cell))
            return {};
        return p.get(cell.row, cell.column, cell.layer).stringEncoding(details);
    }

    std::string getCellStringEncoding(int row, int col, const char* details = nullptr) const override
    {
        if (nativeSliceEnabled && p.layerCount() > 1)
        {
            const ContiguousGridSliceView<Cell> slice(p, nativeSliceAxis, nativeSliceIndex);
            if (row < 0 || col < 0 ||
                row >= static_cast<int>(slice.rowCount()) ||
                col >= static_cast<int>(slice.columnCount()))
            {
                return {};
            }
            return slice[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)]
                .stringEncoding(details);
        }

        if (row < 0 || col < 0 || row >= static_cast<int>(p.size()))
            return {};
        if (col >= static_cast<int>(p[row].size()))
            return {};
        
        return p[row][col].stringEncoding(details);
    }

private:
    std::vector<const SubstateInfo*> nativeVolumeLayerPointers() const
    {
        std::vector<const SubstateInfo*> pointers;
        pointers.reserve(nativeVolumeLayers.size());
        for (const SubstateInfo& info : nativeVolumeLayers)
            pointers.push_back(&info);
        return pointers;
    }

    /// Substates shown in the volume, the one on top of the panel first (see setNative3DVolumeSubstates())
    std::vector<SubstateInfo> nativeVolumeLayers;

    static CellHistory emptyHistory(const std::vector<std::string>& fields)
    {
        CellHistory history;
        history.fields = fields;
        history.values.assign(fields.size(), {});
        return history;
    }

    bool isInsideGrid(const GridCellIndex& cell) const
    {
        return cell.row < p.rowCount() && cell.column < p.columnCount() && cell.layer < p.layerCount();
    }

    /// Read the values of the given substates of the cell at @p index of the whole grid at all available steps
    CellHistory readCellHistory(const SettingParameter& sp,
                                const GridCellIndex& index,
                                const std::vector<std::string>& fields,
                                const CellHistoryProgress& progress)
    {
        CellHistory history = emptyHistory(fields);
        history.row = static_cast<int>(index.row);
        history.column = static_cast<int>(index.column);
        history.slice = static_cast<int>(index.layer);
        history.threeDimensional = p.layerCount() > 1;

        history.steps = modelReader.availableSteps();

        // Remember whether the caller cancelled
        const CellHistoryProgress trackingProgress = [&history, &progress](std::size_t done, std::size_t total)
        {
            if (progress && ! progress(done, total))
            {
                history.cancelled = true;
                return false;
            }
            return true;
        };
        const auto cells = modelReader.readCellAcrossSteps(sp,
                                                           history.row,
                                                           history.column,
                                                           history.slice,
                                                           history.steps,
                                                           trackingProgress);

        for (std::size_t f = 0; f < fields.size(); ++f)
        {
            auto& series = history.values[f];
            series.reserve(cells.size());
            for (const auto& cell : cells)
            {
                series.push_back(cell ? parseSubstateValue(cell->stringEncoding(fields[f].c_str()))
                                      : std::numeric_limits<double>::quiet_NaN());
            }
        }

        return history;
    }

    const std::string m_modelName;

    Visualizer visualiser;           ///< The visualizer instance for rendering the model
    ModelReader<Cell> modelReader;   ///< The reader for loading and managing model data
    ContiguousGrid<Cell> p;          ///< Temporary contiguous grid storage with a default single layer until std::mdspan is available
    vtkSmartPointer<vtkVolume> nativeVolumeActor;
    vtkSmartPointer<vtkRenderer> nativeVolumeRenderer;
    NodeIndex nodeCountZ = 1;
    bool nativeSliceEnabled = false;
    GridSliceAxis nativeSliceAxis = GridSliceAxis::Z;
    std::size_t nativeSliceIndex = 0;
    int nativeVolumeZGap = 0;        ///< Empty planes between neighbouring Z planes in the volume view
};
