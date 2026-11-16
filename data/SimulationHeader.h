/** @file SimulationHeader.h
 * @brief Reading of `Header.txt` of a simulation into SettingParameter - the part needed to read the data files.
 *
 * Shared by everything that opens a simulation: the scene widget of the main window and ActionRunner (actions of plugins
 * run without any widget). Qt-free. */

#pragma once

#include <string>

struct SettingParameter;

/** @namespace SimulationHeader
 *  @brief Functions reading the header file (`Header.txt`) of a simulation. */
namespace SimulationHeader
{
/** @brief Reads the parameters of the header that describe the data files into @p settings.
 *
 *  Sets: outputFileName (resolved with resolveOutputFileName()), numberOfColumnX, numberOfRowsY, numberOfSlicesZ, nsteps,
 *  nNodeX/Y/Z, readMode, substates, reduction. Other members are left untouched.
 *
 *  @param headerFile    path to `Header.txt`
 *  @param settings      filled in
 *  @param printWarnings if true, the Config reader reports suspicious entries of the file on std::cerr
 *  @throws std::runtime_error if the header cannot be read (Config); parameters missing in the file get the defaults of Config */
void readSettingParameter(const std::string& headerFile, SettingParameter& settings, bool printWarnings = true);

/** @brief Full path (without extension) of the data files of a simulation.
 *
 *  The data files lie next to the header (when the simulation was moved out of its `Output` directory) or in the
 *  `Output` subdirectory of the directory of the header. Nothing is created.
 *
 *  @param headerFile            path to `Header.txt`
 *  @param outputFileNameFromCfg `output_file_name` of the header
 *  @return directory of the files + @p outputFileNameFromCfg */
std::string resolveOutputFileName(const std::string& headerFile, const std::string& outputFileNameFromCfg);

/** @return number of boundary lines exchanged between nodes: SettingParameter::numberOfLines (2 XY lines per node, plus
 *          the final edges of every Z layer of nodes) */
int numberOfBoundaryLines(const SettingParameter& settings);
} // namespace SimulationHeader
