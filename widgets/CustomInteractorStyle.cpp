/** @file CustomInteractorStyle.cpp
 * @brief Implementation of CustomInteractorStyle for cursor-based zoom. */

#include "CustomInteractorStyle.h"
#include <vtkCamera.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkObjectFactory.h>
#include <vtkMath.h>
#include <cmath>

vtkStandardNewMacro(CustomInteractorStyle);

void CustomInteractorStyle::OnMouseWheelForward()
{
    ZoomTowardsCursor(0.9);  // Zoom in by 10%
}

void CustomInteractorStyle::OnMouseWheelBackward()
{
    ZoomTowardsCursor(1.1);  // Zoom out by 10%
}

void CustomInteractorStyle::ZoomTowardsCursor(double zoomFactor)
{
    if (!this->Interactor || !this->Interactor->GetRenderWindow())
        return;

    vtkRenderer* renderer = this->Interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
    if (!renderer || !renderer->GetActiveCamera())
        return;

    vtkCamera* camera = renderer->GetActiveCamera();
    int mousePos[2];
    this->Interactor->GetEventPosition(mousePos);

    //--------------------------------------------------------------------------
    // STEP 1: Compute ray from camera through mouse pixel
    //--------------------------------------------------------------------------
    const int* windowSize = this->Interactor->GetRenderWindow()->GetSize();
    double displayPt[3] = {
        static_cast<double>(mousePos[0]),
        static_cast<double>(windowSize[1] - mousePos[1]),  // Flip Y
        0.0
    };

    // Get world coordinates for z=0 and z=1 in display
    renderer->SetDisplayPoint(displayPt);
    renderer->DisplayToWorld();
    const double* world0Ptr = renderer->GetWorldPoint();
    double world0[3] = { world0Ptr[0], world0Ptr[1], world0Ptr[2] };

    double displayPt1[3] = { displayPt[0], displayPt[1], 1.0 };
    renderer->SetDisplayPoint(displayPt1);
    renderer->DisplayToWorld();
    const double* world1Ptr = renderer->GetWorldPoint();
    const double world1[3] = { world1Ptr[0], world1Ptr[1], world1Ptr[2] };

    // Ray direction
    double rayDir[3] = { world1[0] - world0[0],
                        world1[1] - world0[1],
                        world1[2] - world0[2] };
    vtkMath::Normalize(rayDir);

    //--------------------------------------------------------------------------
    // STEP 2: Compute ray-plane intersection
    //         Plane = perpendicular to view direction, passing through focal point
    //--------------------------------------------------------------------------
    double camPos[3], focal[3], viewUp[3];
    camera->GetPosition(camPos);
    camera->GetFocalPoint(focal);
    camera->GetViewUp(viewUp);

    double viewNormal[3] = { focal[0] - camPos[0],
                            focal[1] - camPos[1],
                            focal[2] - camPos[2] };
    vtkMath::Normalize(viewNormal);

    // Solve intersection:
    // dot( world0 + t*rayDir - focal, viewNormal ) = 0
    double numer = vtkMath::Dot(focal, viewNormal) - vtkMath::Dot(world0, viewNormal);
    double denom = vtkMath::Dot(rayDir, viewNormal);

    double pickWorld[3];
    if (fabs(denom) > 1e-12)
    {
        double t = numer / denom;
        pickWorld[0] = world0[0] + t * rayDir[0];
        pickWorld[1] = world0[1] + t * rayDir[1];
        pickWorld[2] = world0[2] + t * rayDir[2];
    }
    else
    {
        // Ray is parallel to focal plane - use focal point
        pickWorld[0] = focal[0];
        pickWorld[1] = focal[1];
        pickWorld[2] = focal[2];
    }

    //--------------------------------------------------------------------------
    // STEP 3: Compute new camera position so pickWorld stays stationary
    //--------------------------------------------------------------------------
    // Vector from camera to picked point
    double v[3] = {
        pickWorld[0] - camPos[0],
        pickWorld[1] - camPos[1],
        pickWorld[2] - camPos[2]
    };
    double dist = vtkMath::Norm(v);
    if (dist < 1e-12)
        return;

    // New distance after zoom (zoomFactor < 1 = zoom in, > 1 = zoom out)
    double newDist = dist / zoomFactor;

    // Normalized direction
    const double vnorm[3] = { v[0] / dist, v[1] / dist, v[2] / dist };

    // New camera position
    double newCamPos[3] = {
        pickWorld[0] - vnorm[0] * newDist,
        pickWorld[1] - vnorm[1] * newDist,
        pickWorld[2] - vnorm[2] * newDist
    };

    // Translation applied equally to focal point (prevents rotation)
    const double translation[3] = {
        newCamPos[0] - camPos[0],
        newCamPos[1] - camPos[1],
        newCamPos[2] - camPos[2]
    };

    double newFocal[3] = {
        focal[0] + translation[0],
        focal[1] + translation[1],
        focal[2] + translation[2]
    };

    camera->SetPosition(newCamPos);
    camera->SetFocalPoint(newFocal);
    camera->SetViewUp(viewUp);

    renderer->ResetCameraClippingRange();
    this->Interactor->GetRenderWindow()->Render();
}

void CustomInteractorStyle::OnLeftButtonDown()
{
    // Only start panning if Shift key is pressed
    if (this->Interactor->GetShiftKey())
    {
        m_isPanning = true;
        const int* pos = this->Interactor->GetEventPosition();
        m_lastMouseX = pos[0];
        m_lastMouseY = pos[1];
    }
    else
    {
        // No Shift - allow normal click processing
        this->Superclass::OnLeftButtonDown();
    }
}

void CustomInteractorStyle::OnLeftButtonUp()
{
    if (m_isPanning)
    {
        m_isPanning = false;
    }
    else
    {
        // Normal click processing
        this->Superclass::OnLeftButtonUp();
    }
}

void CustomInteractorStyle::OnMouseMove()
{
    if (m_isPanning)
    {
        PanCamera();
    }
}

void CustomInteractorStyle::PanCamera()
{
    if (!this->Interactor || !this->Interactor->GetRenderWindow())
        return;

    vtkRenderer* renderer = this->Interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
    if (!renderer || !renderer->GetActiveCamera())
        return;

    // Get current mouse position
    const int* currentPos = this->Interactor->GetEventPosition();
    int currentX = currentPos[0];
    int currentY = currentPos[1];

    // Calculate delta
    int deltaX = currentX - m_lastMouseX;
    int deltaY = currentY - m_lastMouseY;

    // Update last position
    m_lastMouseX = currentX;
    m_lastMouseY = currentY;

    if (deltaX == 0 && deltaY == 0)
        return;

    vtkCamera* camera = renderer->GetActiveCamera();

    // Get current camera state
    double focalPoint[3];
    double position[3];
    camera->GetFocalPoint(focalPoint);
    camera->GetPosition(position);

    // Get window size
    const int* windowSize = this->Interactor->GetRenderWindow()->GetSize();
    double windowWidth = static_cast<double>(windowSize[0]);
    double windowHeight = static_cast<double>(windowSize[1]);

    // Get camera parameters
    double distance = camera->GetDistance();
    double angle = camera->GetViewAngle();

    // Calculate world space movement
    double halfHeight = distance * std::tan(angle * 3.14159265359 / 360.0);
    double halfWidth = halfHeight * (windowWidth / windowHeight);

    // Get view up vector
    const double* viewUp = camera->GetViewUp();

    // Calculate right vector (cross product: viewUp x direction)
    double dirX = focalPoint[0] - position[0];
    double dirY = focalPoint[1] - position[1];
    double dirZ = focalPoint[2] - position[2];

    double rightX = viewUp[1] * dirZ - viewUp[2] * dirY;
    double rightY = viewUp[2] * dirX - viewUp[0] * dirZ;
    double rightZ = viewUp[0] * dirY - viewUp[1] * dirX;

    // Normalize right vector
    double rightLen = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
    if (rightLen > 1e-10)
    {
        rightX /= rightLen;
        rightY /= rightLen;
        rightZ /= rightLen;
    }

    // Convert screen delta to world delta
    // Horizontal movement (deltaX) uses right vector
    double horizontalScaleFactor = (deltaX / windowWidth) * 2.0 * halfWidth;
    double worldDeltaX = horizontalScaleFactor * rightX;
    double worldDeltaY = horizontalScaleFactor * rightY;
    double worldDeltaZ = horizontalScaleFactor * rightZ;

    // Vertical movement (deltaY) uses up vector
    double verticalScaleFactor = -(deltaY / windowHeight) * 2.0 * halfHeight;
    worldDeltaX += verticalScaleFactor * viewUp[0];
    worldDeltaY += verticalScaleFactor * viewUp[1];
    worldDeltaZ += verticalScaleFactor * viewUp[2];

    // Apply movement to both focal point and camera position
    double newFocalPoint[3];
    newFocalPoint[0] = focalPoint[0] + worldDeltaX;
    newFocalPoint[1] = focalPoint[1] + worldDeltaY;
    newFocalPoint[2] = focalPoint[2] + worldDeltaZ;

    double newPosition[3];
    newPosition[0] = position[0] + worldDeltaX;
    newPosition[1] = position[1] + worldDeltaY;
    newPosition[2] = position[2] + worldDeltaZ;

    // Apply changes
    camera->SetFocalPoint(newFocalPoint);
    camera->SetPosition(newPosition);

    // Trigger render
    this->Interactor->GetRenderWindow()->Render();
}
