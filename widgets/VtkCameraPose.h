/** @file VtkCameraPose.h
 * @brief Conversions between a vtkCamera and the VTK-free CameraPivotMath::CameraPose. */

#pragma once

#include "widgets/CameraPivotMath.h"

#include <vtkCamera.h>

namespace VtkCameraPose
{
/** @brief Read position, focal point and view-up vector of a VTK camera. */
inline CameraPivotMath::CameraPose read(vtkCamera& camera)
{
    const double* position = camera.GetPosition();
    const double* focalPoint = camera.GetFocalPoint();
    const double* viewUp = camera.GetViewUp();

    return CameraPivotMath::CameraPose{
        { position[0], position[1], position[2] },
        { focalPoint[0], focalPoint[1], focalPoint[2] },
        { viewUp[0], viewUp[1], viewUp[2] }
    };
}

/** @brief Set position, focal point and view-up vector of a VTK camera. */
inline void write(vtkCamera& camera, const CameraPivotMath::CameraPose& pose)
{
    camera.SetPosition(pose.position[0], pose.position[1], pose.position[2]);
    camera.SetFocalPoint(pose.focalPoint[0], pose.focalPoint[1], pose.focalPoint[2]);
    camera.SetViewUp(pose.viewUp[0], pose.viewUp[1], pose.viewUp[2]);
}
} // namespace VtkCameraPose
