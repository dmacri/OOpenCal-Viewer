/** @file ActionCommand.h
 * @brief The `--runAction` / `--listActions` mode of the application: runs an action plugin (see plugins/ActionApi.h)
 *        on a simulation directory without any GUI.
 *
 * @code
 * OOpenCal-Viewer --listActions --loadModel plugins/libTrajectoryExport.so
 * OOpenCal-Viewer ~/sim/OutputCrowd --loadModel plugins/libTrajectoryExport.so \
 *     --runAction=export-trajectories-julich --actionParam=output_file=trajectories.txt --actionParam=cell_size_m=0.4
 * @endcode
 *
 * Handy on a cluster without a display and in scripts. As in the `--buildPrecompiledHeader` mode the option has to be
 * recognized before QApplication is created (it needs a display). */

#pragma once

#include <optional>

namespace viz::cli
{
inline constexpr char RUN_ACTION_OPTION[] = "--runAction";
inline constexpr char ACTION_PARAM_OPTION[] = "--actionParam";
inline constexpr char LIST_ACTIONS_OPTION[] = "--listActions";

/// If the command line contains `--runAction` or `--listActions`, performs it (without any GUI) and returns the exit
/// code of the application: 0 on success, 1 if the action failed, 2 if the command line is wrong.
/// Otherwise returns nothing and the application starts normally.
std::optional<int> runActionCommand(int& argc, char* argv[]);
} // namespace viz::cli
