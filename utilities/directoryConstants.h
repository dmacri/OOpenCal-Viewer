/** @file DirectoryConstants.h
 * @brief Constants for directory and file naming conventions used throughout the application.
 *
 * This file centralizes all directory and file naming constants related to model
 * loading, configuration, and data file organization. */

#pragma once

#include <string_view>

/** @namespace DirectoryConstants
 * @brief Constants for directory and file naming conventions */
namespace DirectoryConstants
{
    /// Directory name for nested structure (legacy format)
    constexpr std::string_view OUTPUT_DIRECTORY = "Output";

    /// File extension suffix for reduction data files
    constexpr std::string_view REDUCTION_FILE_SUFFIX = "-red.txt";

    /// File name for model configuration
    constexpr std::string_view HEADER_FILE_NAME = "Header.txt";

    /// File name for wrapper source code (generated during compilation)
    constexpr std::string_view WRAPPER_FILE_SUFFIX = "_wrapper.cpp";
}
