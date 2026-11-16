/** @file ActionCommand.h
 * @brief The `--runAction` / `--listActions` mode of the application: runs an action plugin (see plugins/ActionApi.h)
 *        on a simulation directory without any GUI.
 *
 * @code
 * OOpenCal-Viewer --listActions --loadPlugin plugins/libTrajectoryExport.so
 * OOpenCal-Viewer ~/sim/OutputCrowd --loadPlugin plugins/libTrajectoryExport.so \
 *     --runAction=export-trajectories-julich --actionParam=output_file=trajectories.txt --actionParam=cell_size_m=0.4
 * @endcode
 *
 * Handy on a cluster without a display and in scripts. As in the `--buildPrecompiledHeader` mode the option has to be
 * recognized before QApplication is created (it needs a display). */

#pragma once

#include <optional>

namespace viz::cli
{
/** @brief Performs `--runAction` or `--listActions` if the command line asks for it.
 *
 *  Plugins are loaded the same way as by the GUI (standard directories, `--loadModel`, `--loadPlugin`), the command line is
 *  read by CommandLineParser, and the simulation is opened with ModelLoader, so nothing is specific to this mode but the
 *  absence of a window.
 *
 *  @param argc number of arguments
 *  @param argv arguments
 *  @return the exit code of the application: 0 on success, 1 if the action failed, 2 if the command line is wrong;
 *          nothing if the command line asks for neither option - the application starts normally then */
std::optional<int> runActionCommand(int& argc, char* argv[]);
} // namespace viz::cli
