#include <algorithm>
#include <cctype> // isspace()
#include <filesystem>
#include <iostream>
#include <stdexcept> // std::runtime_error
#include <string>
#include <fstream>
#include <ranges>
#include <QtGlobal>
#include <QDebug>
#include <INIReader.h>

#include "Config.h"


namespace
{
/**
 * @brief Removes all whitespace characters from a std::string.
 *
 * This function erases all characters for which std::isspace() returns true,
 * such as spaces, tabs, newlines, and other locale-defined whitespace.
 * It works in-place and automatically selects a ranges-based implementation
 * if supported by the compiler, otherwise falls back to classic erase–remove idiom.
 *
 * @param s Reference to the std::string to be modified in place.
 */
void remove_spaces(std::string& s)
{
    auto removingRange = std::ranges::remove_if(s, [](unsigned char ch) {
        return std::isspace(ch);
    });
    s.erase(removingRange.begin(), removingRange.end());
}
} // namespace


Config::Config(const std::string& configuration_path)
    : configuration_path{configuration_path}
{
    setUpConfigCategories();

    readConfigFile();
}

void Config::setUpConfigCategories()
{
    configCategories.push_back(ConfigCategory{
        "GENERAL",
        {
            {"number_of_columns", "610",  ConfigParameter::int_par},
            {"number_of_rows",    "496",  ConfigParameter::int_par},
            {"number_steps",      "4000", ConfigParameter::int_par},
            {"output_file_name",  "sciddicaTout", ConfigParameter::string_par}
        }
    });
    configCategories.push_back(ConfigCategory{
        "DISTRIBUTED",
        {
            {"border_size_x", "1", ConfigParameter::int_par},
            {"border_size_y", "1", ConfigParameter::int_par},
            {"number_node_x", "4", ConfigParameter::int_par},
            {"number_node_y", "4", ConfigParameter::int_par}
        }
    });

    configCategories.push_back(ConfigCategory{
        "LOAD_BALANCING",
        {
            {"firstLB", "100", ConfigParameter::int_par},
            {"stepLB",  "100", ConfigParameter::int_par}
        }
    });

    // configCategories.push_back(ConfigCategory{
    //     "MPI2DLB2DNaive",
    //     {}
    // });
    // configCategories.push_back(ConfigCategory{
    //     "MPI2DLB2DHierarchical",
    //     {}
    // });
    // configCategories.push_back(ConfigCategory{
    //     "MPI2DSMART",
    //     {}
    // });
    // configCategories.push_back(ConfigCategory{
    //     "CUDA2D",
    //     {}
    // });


    configCategories.push_back(ConfigCategory{
        "MULTICUDA",
        {
            {"number_of_gpus_per_node", "2", ConfigParameter::int_par}
        }
    });

    // configCategories.push_back(ConfigCategory{
    //     "MULTICUDA2DSMART",
    //     {}
    // });

    configCategories.push_back(ConfigCategory{
        "SHARED",
        {
            {"chunk_size", "1", ConfigParameter::int_par}
        }
    });

    configCategories.push_back(ConfigCategory{
        "VISUALIZATION",
        {
            {"substates", "h", ConfigParameter::string_par},
            {"mode", "text", ConfigParameter::string_par},
            {"reduction", "sum,min,max", ConfigParameter::string_par}
        }
    });
}

void Config::writeConfigFile() const
{
    std::ofstream file(configuration_path);
    if (! file.is_open())
    {
        throw std::runtime_error(std::string("Can not open file: '") + configuration_path + "' for writing!");
    }

    for (const auto& configCategory : configCategories)
    {
        if (configCategory.getSize() > 0)
        {
            file << configCategory.getName() << ":\n";
            for (const auto& configParameter: configCategory.getConfigParameters())
            {
                file << '\t'
                     << configParameter.getName() << '='
                     << configParameter.getDefaultValue()
                     << '\n';
            }
        }
    }
}

ConfigCategory* Config::getConfigCategory(const std::string& name, bool ignoreCase)
{
    if (! ignoreCase)
    {
        // Default behavior: case-sensitive
        auto it = std::find(begin(configCategories), end(configCategories), name);
        if (it != end(configCategories))
        {
            return &(*it);
        }
        return nullptr;
    }
    else
    {
        // Case-insensitive comparison using QString
        for (auto& category : configCategories)
        {
            if (QString::fromStdString(category.getName())
                    .compare(QString::fromStdString(name), Qt::CaseInsensitive) == 0)
            {
                return &category;
            }
        }
        return nullptr;
    }
}

std::vector<std::string> Config::categoryNames() const
{
    auto categoryName = [](const auto& category)
    {
        return category.getName();
    };

    auto view = configCategories | std::views::transform(categoryName);

    return { view.begin(), view.end() };
}

void Config::readConfigFile()
{
    if (configuration_path.empty())
    {
        throw std::runtime_error("The path to configuration file is empty!");
    }

    std::cout << std::format("READING '{}'...\n", configuration_path);
    std::filesystem::path configurationFile(configuration_path);
    if (".txt" == configurationFile.extension())
    {
        readConfigFileInOOpenCalFormat();
    }
    else if (".ini" == configurationFile.extension())
    {
        readConfigFileInIniFormat();
    }
    else
    {
        throw std::invalid_argument("Not supported file extention for config files: " + configurationFile.extension().string());
    }
}

void Config::readConfigFileInOOpenCalFormat()
{
    std::ifstream file(configuration_path);
    if (! file.is_open())
    {
        throw std::runtime_error(std::format("Cannot open file '{}' for reading!", configuration_path));
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        if (line.back() != ':')
        {
            std::cerr << "WARNING: Category name must end with ':' character, skipping line: " << line << std::endl;
            continue;
        }

        line.pop_back(); // removing ':' from the end
        remove_spaces(line);

        ConfigCategory* configCategory = getConfigCategory(line);
        if (! configCategory)
        {
            std::cerr << "WARNING: Unknown config category '" << line << "' - skipping" << std::endl;
            // Skip parameters for unknown category
            while (std::getline(file, line))
            {
                if (line.empty() || line.back() == ':')
                {
                    // Put the line back by seeking back (not ideal, but works for this case)
                    // Actually, we can't easily put it back, so we'll just process it in the next iteration
                    if (!line.empty() && line.back() == ':')
                    {
                        // This is a new category, process it in the next iteration
                        file.seekg(-(line.length() + 1), std::ios_base::cur);
                    }
                    break;
                }
            }
            continue;
        }

        for (int i{}; i < configCategory->getSize(); ++i)
        {
            if (! std::getline(file, line))
            {
                std::cerr << "WARNING: Unexpected end of file while reading parameters for category '" 
                          << configCategory->getName() << "'" << std::endl;
                break;
            }

            if (line.empty())
            {
                std::cerr << "WARNING: Empty parameter line in category '" 
                          << configCategory->getName() << "'" << std::endl;
                i--; // Don't count this as a parameter
                continue;
            }

            auto pos = line.find('=');
            if (pos == std::string::npos)
            {
                std::cerr << "WARNING: Invalid parameter line (no '=' found): '" << line << "'" << std::endl;
                i--; // Don't count this as a parameter
                continue;
            }

            std::string parName = line.substr(0, pos);
            std::string valueStr = line.substr(pos + 1);

            remove_spaces(parName);
            remove_spaces(valueStr);

            try
            {
                configCategory->setConfigParameterValue(parName, valueStr);
            }
            catch (const std::exception& e)
            {
                std::cerr << "WARNING: Failed to set parameter '" << parName << "' in category '" 
                          << configCategory->getName() << "': " << e.what() << std::endl;
            }
        }
    }
}

void Config::readConfigFileInIniFormat()
{
    INIReader reader(configuration_path);
    if (reader.ParseError() < 0)
    {
        throw std::runtime_error("Cannot load INI file: " + configuration_path);
    }

    // INIReader automatically converts section names to lowercase.
    // Therefore, we use 'ignoreCase=true' when retrieving ConfigCategory to
    // correctly match the section names regardless of case in the INI file.
    for (const auto& section : reader.Sections())
    {
        ConfigCategory* configCategory = getConfigCategory(section, /*ignoreCase=*/true);
        if (! configCategory)
        {
            throw std::runtime_error(std::format("Unknown config category '{}'", section));
        }

        for (auto const& key : reader.Keys(section))
        {
            // Retrieve value as string (INIReader always returns string internally)
            std::string value = reader.Get(section, key, "");

            // Remove whitespaces around key and value to keep consistency
            std::string cleanKey = key;
            std::string cleanValue = value;
            remove_spaces(cleanKey);
            remove_spaces(cleanValue);

            configCategory->setConfigParameterValue(cleanKey, cleanValue);
        }
    }
}
