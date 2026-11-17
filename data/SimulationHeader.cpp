#include "SimulationHeader.h"

#include <filesystem>

#include "config/Config.h"
#include "config/ConfigCategory.h"
#include "config/ConfigConstants.h"
#include "core/directoryConstants.h"
#include "visualiser/SettingParameter.h"

namespace
{
namespace fs = std::filesystem;

/** @brief Checks if the given directory already contains data files matching the output name pattern
 *  @param configDir Directory to check
 *  @param outputFileNameFromCfg Base output filename to look for
 *  @return True if the directory contains data files, false otherwise */
bool isDataDirectory(const fs::path& configDir, const std::string& outputFileNameFromCfg)
{
    for (const auto& entry : fs::directory_iterator(configDir))
    {
        if (! entry.is_regular_file())
            continue;

        const std::string filename = entry.path().filename().string();

        // Check if file name starts with the output file name pattern
        if (filename.find(outputFileNameFromCfg) != 0)
            continue;

        // Check if it has a known data file extension
        const std::string ext = entry.path().extension().string();
        if (ext == ".bin" || ext == ".blosc" || ext == ".txt")
            return true;
    }

    return false;
}
} // namespace

namespace SimulationHeader
{
std::string resolveOutputFileName(const std::string& headerFile, const std::string& outputFileNameFromCfg)
{
    const fs::path configDir = fs::path(headerFile).parent_path();
    const fs::path outputDir = isDataDirectory(configDir, outputFileNameFromCfg)
                                   ? configDir
                                   : (configDir / std::string(DirectoryConstants::OUTPUT_DIRECTORY));
    return (outputDir / outputFileNameFromCfg).string();
}

int numberOfBoundaryLines(const SettingParameter& settings)
{
    // Each node has two XY boundary lines. Final edges are tracked for every Z node layer.
    const auto totalNodes = settings.nNodeX * settings.nNodeY * settings.nNodeZ;
    return static_cast<int>(2 * totalNodes + settings.nNodeX * settings.nNodeZ + settings.nNodeY * settings.nNodeZ);
}

void readSettingParameter(const std::string& headerFile, SettingParameter& settings, bool printWarnings)
{
    // Config knows all the sections and parameters, so every one of them exists (with a default value if the file lacks it)
    Config config(headerFile, printWarnings);

    {
        ConfigCategory* general = config.getConfigCategory(ConfigConstants::CATEGORY_GENERAL);
        const std::string outputFileNameFromCfg = general->getConfigParameter(ConfigConstants::PARAM_OUTPUT_FILE_NAME)->getValue<std::string>();
        settings.outputFileName = resolveOutputFileName(headerFile, outputFileNameFromCfg);
        settings.numberOfColumnX = general->getConfigParameter(ConfigConstants::PARAM_NUMBER_OF_COLUMNS)->getValue<int>();
        settings.numberOfRowsY = general->getConfigParameter(ConfigConstants::PARAM_NUMBER_OF_ROWS)->getValue<int>();

        // Read number_of_slices for 3D models (defaults to 1 for 2D models)
        const auto* slicesParam = general->getConfigParameter(ConfigConstants::PARAM_NUMBER_OF_SLICES);
        settings.numberOfSlicesZ = slicesParam ? slicesParam->getValue<int>() : 1;

        settings.nsteps = general->getConfigParameter(ConfigConstants::PARAM_NUMBER_STEPS)->getValue<int>();
    }

    {
        ConfigCategory* execContext = config.getConfigCategory(ConfigConstants::CATEGORY_DISTRIBUTED);
        settings.nNodeX = execContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_X)->getValue<int>();
        settings.nNodeY = execContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_Y)->getValue<int>();

        // Read number_node_z for 3D models (defaults to 1 for 2D models)
        const auto* nodeZParam = execContext->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_Z);
        settings.nNodeZ = nodeZParam ? nodeZParam->getValue<int>() : 1;

        /// Notice: there are much more params, which are not used: e.g. border_size_x, border_size_y, border_size_z
    }

    {
        ConfigCategory* visualizationContext = config.getConfigCategory(ConfigConstants::CATEGORY_VISUALIZATION);

        // Read visualization mode (text, binary or compressed)
        const auto* modeParam = visualizationContext->getConfigParameter(ConfigConstants::PARAM_MODE);
        settings.readMode = modeParam ? modeParam->getValue<std::string>() : ConfigConstants::DEFAULT_MODE;

        // Read substates
        const auto* substatesParam = visualizationContext->getConfigParameter(ConfigConstants::PARAM_SUBSTATES);
        settings.substates = substatesParam ? substatesParam->getValue<std::string>() : ConfigConstants::DEFAULT_SUBSTATES;

        // Read reduction operations
        const auto* reductionParam = visualizationContext->getConfigParameter(ConfigConstants::PARAM_REDUCTION);
        settings.reduction = reductionParam ? reductionParam->getValue<std::string>() : ConfigConstants::DEFAULT_REDUCTION;
    }
}
} // namespace SimulationHeader
