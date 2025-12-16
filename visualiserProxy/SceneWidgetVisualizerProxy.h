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

#include "data/ModelReader.hpp"
#include "visualiser/Visualizer.hpp"

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
struct SceneWidgetVisualizerTemplate
{
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
    void initMatrix(int dimX, int dimY)
    {
        p.resize(dimY);
        for (int i = 0; i < dimY; i++)
        {
            p[i].resize(dimX);
        }
    }
};
