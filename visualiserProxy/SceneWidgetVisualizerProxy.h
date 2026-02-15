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

#include "ISceneWidgetVisualizer.h"
#include "data/ModelReader.hpp"
#include "visualiser/Visualizer.hpp"

class Line;
class SettingParameter;

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

    Visualizer visualiser;            ///< The visualizer instance for rendering the model
    ModelReader<Cell> modelReader;    ///< The reader for loading and managing model data
    std::vector<std::vector<Cell>> p; ///< 2D vector storing the cell data

    /** @brief Initializes the internal matrix with the specified dimensions.
     *
     * This method resizes the internal 2D vector to match the given dimensions,
     * creating a grid of default-constructed Cell objects.
     *
     * @param dimX The width of the grid (number of columns)
     * @param dimY The height of the grid (number of rows)
     *
     * @note The dimensions must be positive integers. The method will create a grid with dimY rows and dimX columns. */
    void initMatrix(int dimX, int dimY) override
    {
        p.resize(dimY);
        for (int i = 0; i < dimY; i++)
        {
            p[i].resize(dimX);
        }
    }

    void prepareStage(int nNodeX, int nNodeY) override
    {
        modelReader.prepareStage(nNodeX, nNodeY);
    }

    void clearStage() override
    {
        modelReader.clearStage();
    }

    void readStepsOffsetsForAllNodesFromFiles(int nNodeX, int nNodeY, const std::string& filename) override
    {
        modelReader.readStepsOffsetsForAllNodesFromFiles(nNodeX, nNodeY, filename);
    }

    void readStageStateFromFilesForStep(SettingParameter* sp, Line* lines) override
    {
        modelReader.readStageStateFromFilesForStep(p, sp, lines);
    }

    void drawWithVTK(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        visualiser.drawWithVTK(p, nRows, nCols, renderer, gridActor, colorSubstateInfos);
    }

    void refreshWindowsVTK(int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        visualiser.refreshWindowsVTK(p, nRows, nCols, gridActor, colorSubstateInfos);
    }

    void drawWithVTK3DSubstate(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue, const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        visualiser.drawWithVTK3DSubstate(p, nRows, nCols, renderer, gridActor, substateFieldName, minValue, maxValue, colorSubstateInfos);
    }

    void refreshWindowsVTK3DSubstate(int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue, const std::vector<const SubstateInfo*>& colorSubstateInfos) override
    {
        visualiser.refreshWindowsVTK3DSubstate(p, nRows, nCols, gridActor, substateFieldName, minValue, maxValue, colorSubstateInfos);
    }

    void drawFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> backgroundActor) override
    {
        visualiser.drawFlatSceneBackground(nRows, nCols, renderer, backgroundActor);
    }

    void refreshFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkActor> backgroundActor) override
    {
        visualiser.refreshFlatSceneBackground(nRows, nCols, backgroundActor);
    }

    void drawGridLinesOn3DSurface(int nRows, int nCols, const std::vector<Line>& lines, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridLinesActor, const std::string& substateFieldName, double minValue, double maxValue) override
    {
        visualiser.drawGridLinesOn3DSurface(p, nRows, nCols, lines, renderer, gridLinesActor, substateFieldName, minValue, maxValue);
    }

    void refreshGridLinesOn3DSurface(int nRows, int nCols, const std::vector<Line>& lines, vtkSmartPointer<vtkActor> gridLinesActor, const std::string& substateFieldName, double minValue, double maxValue) override
    {
        visualiser.refreshGridLinesOn3DSurface(p, nRows, nCols, lines, gridLinesActor, substateFieldName, minValue, maxValue);
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
        if (row < 0 || col < 0 || row >= static_cast<int>(p.size()))
            return {};
        if (col >= static_cast<int>(p[row].size()))
            return {};
        
        return p[row][col].stringEncoding(details);
    }

private:
    const std::string m_modelName;
};
