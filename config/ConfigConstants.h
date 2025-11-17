/** @file ConfigConstants.h
 * @brief Configuration constants for all hardcoded configuration keys.
 * 
 * This file defines compile-time constants for all configuration category names
 * and parameter names used throughout the application. Using these constants
 * instead of hardcoded strings provides:
 * - Single source of truth for configuration keys
 * - Compile-time type safety
 * - Easy refactoring (change in one place)
 * - Reduced risk of typos
 * 
 * All constants are constexpr strings that can be used at compile time. */

#pragma once


/** @namespace ConfigConstants
 * @brief Contains all configuration category and parameter name constants. */
namespace ConfigConstants
{
    // ========== GENERAL Category ==========
    /** @brief Configuration category name for general simulation settings */
    inline constexpr const char CATEGORY_GENERAL[] = "GENERAL";
    
    /** @brief Number of columns in the simulation grid */
    inline constexpr const char PARAM_NUMBER_OF_COLUMNS[] = "number_of_columns";
    
    /** @brief Number of rows in the simulation grid */
    inline constexpr const char PARAM_NUMBER_OF_ROWS[] = "number_of_rows";
    
    /** @brief Total number of simulation steps */
    inline constexpr const char PARAM_NUMBER_STEPS[] = "number_steps";
    
    /** @brief Output file name prefix for simulation data files */
    inline constexpr const char PARAM_OUTPUT_FILE_NAME[] = "output_file_name";

    // ========== DISTRIBUTED Category ==========
    /** @brief Configuration category name for distributed computing settings */
    inline constexpr const char CATEGORY_DISTRIBUTED[] = "DISTRIBUTED";
    
    /** @brief Number of nodes in X direction (horizontal distribution) */
    inline constexpr const char PARAM_NUMBER_NODE_X[] = "number_node_x";
    
    /** @brief Number of nodes in Y direction (vertical distribution) */
    inline constexpr const char PARAM_NUMBER_NODE_Y[] = "number_node_y";
    
    /** @brief Border size in X direction for distributed computing */
    inline constexpr const char PARAM_BORDER_SIZE_X[] = "border_size_x";
    
    /** @brief Border size in Y direction for distributed computing */
    inline constexpr const char PARAM_BORDER_SIZE_Y[] = "border_size_y";

    // ========== LOAD_BALANCING Category ==========
    /** @brief Configuration category name for load balancing settings */
    inline constexpr const char CATEGORY_LOAD_BALANCING[] = "LOAD_BALANCING";
    
    /** @brief First load balancing step */
    inline constexpr const char PARAM_FIRST_LB[] = "firstLB";
    
    /** @brief Load balancing interval (steps between load balancing operations) */
    inline constexpr const char PARAM_STEP_LB[] = "stepLB";

    // ========== MULTICUDA Category ==========
    /** @brief Configuration category name for CUDA multi-GPU settings */
    inline constexpr const char CATEGORY_MULTICUDA[] = "MULTICUDA";
    
    /** @brief Number of GPUs per node */
    inline constexpr const char PARAM_NUMBER_OF_GPUS_PER_NODE[] = "number_of_gpus_per_node";

    // ========== SHARED Category ==========
    /** @brief Configuration category name for shared memory settings */
    inline constexpr const char CATEGORY_SHARED[] = "SHARED";
    
    /** @brief Chunk size for shared memory operations */
    inline constexpr const char PARAM_CHUNK_SIZE[] = "chunk_size";

    // ========== VISUALIZATION Category ==========
    /** @brief Configuration category name for visualization settings */
    inline constexpr const char CATEGORY_VISUALIZATION[] = "VISUALIZATION";
    
    /** @brief File read mode: "text" or "binary" */
    inline constexpr const char PARAM_MODE[] = "mode";
    
    /** @brief Substates to read from simulation data (e.g., "h,z") */
    inline constexpr const char PARAM_SUBSTATES[] = "substates";
    
    /** @brief Reduction operations to apply (e.g., "sum,min,max") */
    inline constexpr const char PARAM_REDUCTION[] = "reduction";

    // ========== Default Values ==========
    /** @brief Default file read mode */
    inline constexpr const char DEFAULT_MODE[] = "text";
    
    /** @brief Default substates (empty = no filtering) */
    inline constexpr const char DEFAULT_SUBSTATES[] = "";
    
    /** @brief Default reduction (empty = no reduction) */
    inline constexpr const char DEFAULT_REDUCTION[] = "";

} // namespace ConfigConstants
