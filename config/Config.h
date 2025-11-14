/** @file Config.h
 * @brief Main configuration class for managing application settings.
 * 
 * This file defines the Config class which is responsible for loading, storing,
 * and managing configuration settings organized in categories. It supports both
 * OOpenCAL format and standard INI file formats. */

#pragma once

#include "ConfigCategory.h"

/** @class Config
 * @brief Manages application configuration settings organized in categories.
 * 
 * The Config class provides functionality to read and write configuration
 * settings from/to files, with support for different file formats. It organizes
 * settings into ConfigCategory objects, each containing related parameters. */
class Config
{
    std::string configuration_path;
    std::vector<ConfigCategory> configCategories;
    bool printWarnings = true;

public:
    /** @brief Construct a new Config object with the specified configuration file path.
     *
     * @param configuration_path Path to the configuration file to load
     * @param printWarnings when true it will print warnings to std::cerr, otherwise silent mode
     * @throws std::runtime_error If the file cannot be opened or has an invalid format */
    explicit Config(const std::string& configuration_path, bool printWarnings=true);

    /** @brief Sets the path to the configuration file.
     * 
     * @param value The new configuration file path */
    void setConfigurationPath(const std::string& value)
    {
        configuration_path = value;
    }

    /** @brief Writes the current configuration to the configuration file with OOpenCal format
     * 
     * @throws std::runtime_error If the configuration file cannot be written */
    void writeConfigFile() const;

    /** @brief Reads the configuration from the configuration file.
     * 
     * Automatically detects the file format (OOpenCAL or INI) and parses it (detection by file extention).
     * 
     * @throws std::runtime_error If the configuration file cannot be read or parsed */
    void readConfigFile();

    /** @brief Gets a configuration category by name.
     * 
     * @param name The name of the category to retrieve
     * @param ignoreCase If true, performs case-insensitive comparison
     * @return ConfigCategory* Pointer to the category, or nullptr if not found */
    ConfigCategory* getConfigCategory(const std::string& name, bool ignoreCase = false);

    /** @brief Gets a list of all configuration category names.
     * 
     * @return std::vector<std::string> Vector of category names */
    std::vector<std::string> categoryNames() const;

protected:
    void setUpConfigCategories();

    void readConfigFileInOOpenCalFormat();
    void readConfigFileInIniFormat();
};
