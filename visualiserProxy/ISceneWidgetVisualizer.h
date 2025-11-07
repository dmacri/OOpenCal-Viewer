/** @file ISceneWidgetVisualizer.h
 * @brief Abstract interface for scene widget visualizers.
 * 
 * This file defines the ISceneWidgetVisualizer interface that serves as a contract
 * for all model visualizers in the system. It enables runtime polymorphism for
 * different cell models (e.g., BallCell, SciddicaTCell) without requiring
 * compile-time template binding. This is a key component in the visualization
 * system that allows for flexible model handling and visualization.
 * 
 * The interface is designed to work with the Model-View-Controller pattern,
 * where different model types can be visualized using a common interface while
 * maintaining their specific behaviors and requirements.
 * 
 * @see SceneWidgetVisualizerAdapter
 * @see SceneWidgetVisualizerFactory
 * @see doc/CHANGELOG_RUNTIME_MODELS.md for architectural details */

#pragma once

#include <vtkRenderer.h>

#include "utilities/types.h"

// Forward declarations
class SettingParameter;
class Line;
class Visualizer;

/** @interface ISceneWidgetVisualizer
 * @brief Abstract interface defining the contract for all scene widget visualizers.
 * 
 * This interface provides a common API for visualizing different types of cell-based
 * models. It abstracts away the specific implementation details of each model type,
 * allowing the visualization system to work with various models through a uniform interface.
 * 
 * Each model type (e.g., Ball, SciddicaT) provides its own implementation of this interface,
 * typically through the SceneWidgetVisualizerAdapter template class. The interface
 * handles the complete visualization lifecycle, from data loading to rendering.
 * 
 * @note Implementations of this interface should be thread-safe if used in a multi-threaded context. */
class ISceneWidgetVisualizer
{
public:
    virtual ~ISceneWidgetVisualizer() = default;

    /// @brief Initialize the matrix with given dimensions.
    virtual void initMatrix(int dimX, int dimY) = 0;

    /// @brief Prepare the stage for reading data.
    virtual void prepareStage(int nNodeX, int nNodeY) = 0;

    /** @brief Clear the stage (remove all loaded step data).
     * 
     * This should be called before reloading data to avoid duplicate entries. */
    virtual void clearStage() = 0;

    /// @brief Read steps offsets for all nodes from files.
    virtual void readStepsOffsetsForAllNodesFromFiles(int nNodeX, int nNodeY, const std::string& filename) = 0;

    /// @brief Read stage state from files for a specific step.
    virtual void readStageStateFromFilesForStep(SettingParameter* sp, Line* lines) = 0;

    /// @brief Draw the visualization using VTK.
    virtual void drawWithVTK(int nRows, int nCols, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkActor> gridActor) = 0;

    /// @brief Refresh the VTK windows.
    virtual void refreshWindowsVTK(int nRows, int nCols, vtkSmartPointer<vtkActor> gridActor) = 0;

    /// @brief Get the visualizer instance.
    virtual Visualizer& getVisualizer() = 0;

    /** @brief Get the model name (e.g., "Ball", "SciddicaT").
     * 
     * This should return the name from the Cell type's static name() method. */
    virtual std::string getModelName() const = 0;

    /// @brief Returns available steps from index file.
    virtual std::vector<StepIndex> availableSteps() const = 0;

    /** @brief Get the string encoding of a cell at given grid coordinates.
     * 
     * This method retrieves the string representation of a cell at the specified
     * grid position. The coordinates are in grid indices (row, col), not world coordinates.
     * 
     * @param row The row index of the cell (0-based, from top)
     * @param col The column index of the cell (0-based, from left)
     * @param details Optional field name to retrieve specific cell property (e.g., "h", "z").
     *                If nullptr or empty, returns the default encoding.
     * @return String representation of the cell via stringEncoding(), or empty string if out of bounds */
    virtual std::string getCellStringEncoding(int row, int col, const char* details = nullptr) const = 0;
};
