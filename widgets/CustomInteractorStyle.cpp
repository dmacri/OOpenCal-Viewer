/** @file CustomInteractorStyle.cpp
 * @brief Implementation of CustomInteractorStyle for cursor-based zoom. */

#include "CustomInteractorStyle.h"
#include "widgets/WaitCursorGuard.h"
#include <vtkCamera.h>
#include <vtkCellPicker.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkObjectFactory.h>
#include <vtkMath.h>
#include <algorithm>
#include <cmath>


vtkStandardNewMacro(CustomInteractorStyle);
vtkStandardNewMacro(SimpleInteractorWithWaitCursor);


CustomInteractorStyle::CustomInteractorStyle()
    : m_picker{ vtkSmartPointer<vtkCellPicker>::New() }
{
    if (m_picker)
    {
        m_picker->SetTolerance(0.0005);
    }
}

void CustomInteractorStyle::OnMouseWheelForward()
{
    WaitCursorGuard waitCursor("Zooming in...");

    ZoomTowardsCursor(1.1);  // Zoom in by 10%
}

void CustomInteractorStyle::OnMouseWheelBackward()
{
    WaitCursorGuard waitCursor("Zooming out...");

    ZoomTowardsCursor(1.0 / 1.1);  // Zoom out by ~10%
}

void CustomInteractorStyle::ZoomTowardsCursor(double zoomFactor)
{
    if (!this->Interactor || !this->Interactor->GetRenderWindow())
        return;

    vtkRenderWindowInteractor* interactor = this->Interactor;
    vtkRenderWindow* renderWindow = interactor->GetRenderWindow();
    if (!renderWindow)
        return;

    int mousePos[2];
    interactor->GetEventPosition(mousePos);

    vtkRenderer* renderer = interactor->FindPokedRenderer(mousePos[0], mousePos[1]);
    if (!renderer)
    {
        renderer = renderWindow->GetRenderers()->GetFirstRenderer();
    }
    if (!renderer || !renderer->GetActiveCamera())
        return;

    vtkCamera* camera = renderer->GetActiveCamera();

    double pickWorld[3] = { 0.0, 0.0, 0.0 };
    bool havePick = false;

    if (m_picker && m_picker->Pick(mousePos[0], mousePos[1], 0.0, renderer) > 0 && m_picker->GetDataSet())
    {
        const double* picked = m_picker->GetPickPosition();
        pickWorld[0] = picked[0];
        pickWorld[1] = picked[1];
        pickWorld[2] = picked[2];
        havePick = true;
    }

    if (!havePick)
    {
        double displayPt[3] = {
            static_cast<double>(mousePos[0]),
            static_cast<double>(mousePos[1]),
            0.0
        };

        renderer->SetDisplayPoint(displayPt);
        renderer->DisplayToWorld();
        double world0[4];
        renderer->GetWorldPoint(world0);
        if (std::abs(world0[3]) > 1e-12)
        {
            world0[0] /= world0[3];
            world0[1] /= world0[3];
            world0[2] /= world0[3];
        }

        double displayPt1[3] = { displayPt[0], displayPt[1], 1.0 };
        renderer->SetDisplayPoint(displayPt1);
        renderer->DisplayToWorld();
        double world1[4];
        renderer->GetWorldPoint(world1);
        if (std::abs(world1[3]) > 1e-12)
        {
            world1[0] /= world1[3];
            world1[1] /= world1[3];
            world1[2] /= world1[3];
        }

        double rayDir[3] = {
            world1[0] - world0[0],
            world1[1] - world0[1],
            world1[2] - world0[2]
        };
        vtkMath::Normalize(rayDir);

        double camPos[3];
        double focal[3];
        double viewUp[3];
        camera->GetPosition(camPos);
        camera->GetFocalPoint(focal);
        camera->GetViewUp(viewUp);

        double viewNormal[3] = {
            focal[0] - camPos[0],
            focal[1] - camPos[1],
            focal[2] - camPos[2]
        };
        vtkMath::Normalize(viewNormal);

        double numer = vtkMath::Dot(focal, viewNormal) - vtkMath::Dot(world0, viewNormal);
        double denom = vtkMath::Dot(rayDir, viewNormal);

        if (std::abs(denom) > 1e-12)
        {
            double t = numer / denom;
            pickWorld[0] = world0[0] + t * rayDir[0];
            pickWorld[1] = world0[1] + t * rayDir[1];
            pickWorld[2] = world0[2] + t * rayDir[2];
            havePick = true;
        }
        else
        {
            pickWorld[0] = focal[0];
            pickWorld[1] = focal[1];
            pickWorld[2] = focal[2];
            havePick = true;
        }
    }

    if (!havePick)
    {
        camera->Dolly(zoomFactor);
        renderer->ResetCameraClippingRange();
        renderWindow->Render();
        return;
    }

    double camPos[3];
    double focal[3];
    double viewUp[3];
    camera->GetPosition(camPos);
    camera->GetFocalPoint(focal);
    camera->GetViewUp(viewUp);

    double v[3] = {
        pickWorld[0] - camPos[0],
        pickWorld[1] - camPos[1],
        pickWorld[2] - camPos[2]
    };
    double dist = vtkMath::Norm(v);
    if (dist < 1e-12)
        return;

    double newDist = dist / zoomFactor;
    newDist = std::clamp(newDist, m_minDistance, m_maxDistance);

    double vnorm[3] = { v[0] / dist, v[1] / dist, v[2] / dist };

    double newCamPos[3] = {
        pickWorld[0] - vnorm[0] * newDist,
        pickWorld[1] - vnorm[1] * newDist,
        pickWorld[2] - vnorm[2] * newDist
    };

    double translation[3] = {
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
    renderWindow->Render();
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
        
        // Create wait cursor guard for panning (persists until OnLeftButtonUp)
        WaitCursorGuard::changeIcon(/*waitingIcon=*/true);
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
        
        WaitCursorGuard::changeIcon(/*waitingIcon=*/false);
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


void SimpleInteractorWithWaitCursor::OnMouseWheelForward()
{
    WaitCursorGuard waitCursor("Zooming in...");
    this->Superclass::OnMouseWheelForward();
}

void SimpleInteractorWithWaitCursor::OnMouseWheelBackward()
{
    WaitCursorGuard waitCursor("Zooming out...");
    this->Superclass::OnMouseWheelBackward();
}
