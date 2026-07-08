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

#include <string>
#include <vector>
#include <vtkVolume.h>
#include "ISceneWidgetVisualizer.h"
#include "data/ModelReader.hpp"
#include "visualiserProxy/ContiguousGrid.h"
#include "visualiser/Visualizer.hpp"

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
                                           colorSubstateInfos);
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
                                           colorSubstateInfos);
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
                                                gridLinesActor);
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
                                                gridLinesActor);
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
                                                   gridLinesActor);
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
                                                   gridLinesActor);
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
};
