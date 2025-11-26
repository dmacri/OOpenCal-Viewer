/** @file SceneWidgetVisualizerAdapter.h
 * @brief Adapter for template-based visualizers to implement ISceneWidgetVisualizer.
 * 
 * This file contains the SceneWidgetVisualizerAdapter class template that bridges the gap
 * between the template-based SceneWidgetVisualizerTemplate and the polymorphic
 * ISceneWidgetVisualizer interface. It's a key component in the visualization system
 * that enables runtime selection and use of different model types.
 * 
 * The adapter pattern is used here to allow template-based implementations to be used
 * through a common interface, enabling dynamic model loading and visualization without
 * sacrificing the type safety and performance benefits of templates.
 * 
 * @see ISceneWidgetVisualizer
 * @see SceneWidgetVisualizerTemplate
 * @see SceneWidgetVisualizerFactory
 * @see doc/CHANGELOG_RUNTIME_MODELS.md for architectural details */

#pragma once

#include "ISceneWidgetVisualizer.h"
#include "SceneWidgetVisualizerProxy.h"

class Line;
class SettingParameter;

/** @class SceneWidgetVisualizerAdapter
 * @brief Adapter that makes template-based visualizers work with the ISceneWidgetVisualizer interface.
 * 
 * This class template implements the ISceneWidgetVisualizer interface by delegating calls
 * to an instance of SceneWidgetVisualizerTemplate<Cell>. It serves as an adapter between
 * the polymorphic interface required by the visualization system and the template-based
 * implementation of specific model visualizers.
 * 
 * The adapter is responsible for:
 * - Wrapping template-based visualizers in a polymorphic interface
 * - Forwarding method calls to the appropriate template instance
 * - Managing the lifetime of the underlying visualizer
 * - Providing model-specific information to the visualization system
 * 
 * @tparam Cell The cell type that this adapter works with (e.g., BallCell, SciddicaTCell).
 *              Must be compatible with the ModelReader and Visualizer being used. */
template<typename Cell>
class SceneWidgetVisualizerAdapter : public ISceneWidgetVisualizer
{
public:
    explicit SceneWidgetVisualizerAdapter(const std::string& modelName)
        : m_modelName(modelName)
    {
    }

    void initMatrix(int dimX, int dimY) override
    {
        m_impl.initMatrix(dimX, dimY);
    }

    void prepareStage(int nNodeX, int nNodeY) override
    {
        m_impl.modelReader.prepareStage(nNodeX, nNodeY);
    }

    void clearStage() override
    {
        m_impl.modelReader.clearStage();
    }

    void readStepsOffsetsForAllNodesFromFiles(int nNodeX, int nNodeY, const std::string& filename) override
    {
        m_impl.modelReader.readStepsOffsetsForAllNodesFromFiles(nNodeX, nNodeY, filename);
    }

    void readStageStateFromFilesForStep(SettingParameter* sp, Line* lines) override
    {
        m_impl.modelReader.readStageStateFromFilesForStep(m_impl.p, sp, lines);
    }

    void drawWithVTK(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor) override
    {
        m_impl.visualiser.drawWithVTK(m_impl.p, nRows, nCols, renderer, gridActor);
    }

    void refreshWindowsVTK(int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor) override
    {
        m_impl.visualiser.refreshWindowsVTK(m_impl.p, nRows, nCols, gridActor);
    }

    void drawWithVTK3DSubstate(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue) override
    {
        m_impl.visualiser.drawWithVTK3DSubstate(m_impl.p, nRows, nCols, renderer, gridActor, substateFieldName, minValue, maxValue);
    }

    void refreshWindowsVTK3DSubstate(int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue) override
    {
        m_impl.visualiser.refreshWindowsVTK3DSubstate(m_impl.p, nRows, nCols, gridActor, substateFieldName, minValue, maxValue);
    }

    void drawWithVTK3DSubstateQuadMesh(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue) override
    {
        m_impl.visualiser.drawWithVTK3DSubstateQuadMesh(m_impl.p, nRows, nCols, renderer, gridActor, substateFieldName, minValue, maxValue);
    }

    void refreshWindowsVTK3DSubstateQuadMesh(int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor, const std::string& substateFieldName, double minValue, double maxValue) override
    {
        m_impl.visualiser.refreshWindowsVTK3DSubstateQuadMesh(m_impl.p, nRows, nCols, gridActor, substateFieldName, minValue, maxValue);
    }

    void drawFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> backgroundActor) override
    {
        m_impl.visualiser.drawFlatSceneBackground(m_impl.p, nRows, nCols, renderer, backgroundActor);
    }

    void refreshFlatSceneBackground(int nRows, int nCols, vtkSmartPointer<vtkActor> backgroundActor) override
    {
        m_impl.visualiser.refreshFlatSceneBackground(m_impl.p, nRows, nCols, backgroundActor);
    }

    Visualizer& getVisualizer() override
    {
        return m_impl.visualiser;
    }

    std::string getModelName() const override
    {
        return m_modelName;
    }

    std::vector<StepIndex> availableSteps() const override
    {
        return m_impl.modelReader.availableSteps();
    }

    std::string getCellStringEncoding(int row, int col, const char* details = nullptr) const override
    {
        // Check bounds
        if (row < 0 || col < 0 || row >= static_cast<int>(m_impl.p.size()))
            return {};
        if (col >= static_cast<int>(m_impl.p[row].size()))
            return {};
        
        // Get the cell and call its stringEncoding method
        return m_impl.p[row][col].stringEncoding(details);
    }

private:
    SceneWidgetVisualizerTemplate<Cell> m_impl;
    const std::string m_modelName;
};
// TODO: GB: IMO the class can be removed, but model name moved to SceneWidgetVisualizerTemplate
