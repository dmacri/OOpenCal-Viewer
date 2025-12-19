/** @file DirectoryConstants.h
 * @brief Constants for directory and file naming conventions used throughout the application.
 *
 * This file centralizes all directory and file naming constants related to model
 * loading, configuration, and data file organization. */

#pragma once

/** @namespace DirectoryConstants
 * @brief Constants for directory and file naming conventions */
namespace DirectoryConstants
{
    /// Directory name for nested structure (legacy format)
    constexpr const char OUTPUT_DIRECTORY[] = "Output";

    /// File extension suffix for reduction data files
    constexpr const char REDUCTION_FILE_SUFFIX[] = "-red.txt";

    /// File name for model configuration
    constexpr const char HEADER_FILE_NAME[] = "Header.txt";

    /// File name for wrapper source code (generated during compilation)
    constexpr const char WRAPPER_FILE_SUFFIX[] = "_wrapper.cpp";
}
