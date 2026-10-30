/** @file TrajectoryExportPlugin.cpp
 * @brief Plugin with the action exporting trajectories of pedestrians (see JulichTrajectoryAction.h).
 *
 * Build it with the CMakeLists.txt next to this file and load it with `Model -> Load Plugin...`,
 * `--loadModel libTrajectoryExport.so`, or put the library into `./plugins/`. */

#include "JulichTrajectoryAction.h"

VIZ_ACTION_PLUGIN(registry)
{
    registry.registerAction(std::make_unique<viz::examples::JulichTrajectoryAction>());
}

// Optional metadata shown in the plugin list (the same functions as in model plugins; do NOT export getModelName())
extern "C" __attribute__((visibility("default"))) const char* getPluginInfo()
{
    return "Trajectory export: pedestrian trajectories in the Jülich format";
}
