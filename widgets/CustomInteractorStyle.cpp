/** @file CustomInteractorStyle.cpp
 * @brief Implementation of CustomInteractorStyle for cursor-based zoom. */

#include "CustomInteractorStyle.h"
#include <vtkCamera.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkObjectFactory.h>
#include <iostream>

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

    // Get the active renderer
    vtkRenderer* renderer = this->Interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
    if (!renderer || !renderer->GetActiveCamera())
        return;

    // Get current mouse position in screen coordinates
    const int* eventPos = this->Interactor->GetEventPosition();
    int screenX = eventPos[0];
    int screenY = eventPos[1];

    vtkCamera* camera = renderer->GetActiveCamera();

    // Get current camera state
    double focalPoint[3];
    double position[3];
    camera->GetFocalPoint(focalPoint);
    camera->GetPosition(position);

    // Convert screen coordinates to world coordinates
    const int* windowSize = this->Interactor->GetRenderWindow()->GetSize();
    double displayPos[3] = {
        static_cast<double>(screenX),
        static_cast<double>(windowSize[1] - screenY),  // Flip Y
        0.0
    };

    renderer->SetDisplayPoint(displayPos);
    renderer->DisplayToWorld();
    
    // SAFE: Copy the world point returned by VTK
    const double* worldPosPtr = renderer->GetWorldPoint();
    const double worldPos[3] = { worldPosPtr[0], worldPosPtr[1], worldPosPtr[2] };

    // Calculate offset from focal point to cursor position (only X and Y matter for 2D)
    double offsetX = worldPos[0] - focalPoint[0];
    double offsetY = worldPos[1] - focalPoint[1];

    // Calculate movement factor
    double movementFactor = zoomFactor - 1.0;  // -0.1 for zoom in, +0.1 for zoom out

    // STRATEGY: 
    // 1. Move focal point towards cursor (panning effect) - only X and Y
    // 2. Scale camera distance from focal point (zoom effect)
    // This keeps the point under cursor stationary on screen
    
    double newFocalPoint[3];
    newFocalPoint[0] = focalPoint[0] - offsetX * movementFactor;
    newFocalPoint[1] = focalPoint[1] - offsetY * movementFactor;
    newFocalPoint[2] = focalPoint[2];  // Keep focal point Z unchanged

    // For camera position: 
    // 1. Follow focal point panning (X and Y)
    // 2. Scale distance in Z by zoom factor
    double cameraDistX = position[0] - focalPoint[0];
    double cameraDistY = position[1] - focalPoint[1];
    double cameraDistZ = position[2] - focalPoint[2];
    
    double newPosition[3];
    newPosition[0] = newFocalPoint[0] + cameraDistX;
    newPosition[1] = newFocalPoint[1] + cameraDistY;
    newPosition[2] = newFocalPoint[2] + cameraDistZ * zoomFactor;  // Scale Z distance

    // Apply changes
    camera->SetFocalPoint(newFocalPoint);
    camera->SetPosition(newPosition);
    
    // Reset clipping range to ensure all geometry is visible
    // This is important when zooming - we need to adjust near/far planes
    renderer->ResetCameraClippingRange();

    // Trigger render
    this->Interactor->GetRenderWindow()->Render();

    // Debug output
    std::cerr << "\n=== ZOOM DEBUG ===" << std::endl;
    std::cerr << "Screen position: (" << screenX << ", " << screenY << ")" << std::endl;
    std::cerr << "World position: (" << worldPos[0] << ", " << worldPos[1] << ", " << worldPos[2] << ")" << std::endl;
    std::cerr << "Zoom factor: " << zoomFactor << std::endl;
    std::cerr << "Offset: (" << offsetX << ", " << offsetY << ")" << std::endl;
    std::cerr << "Old focal point: (" << focalPoint[0] << ", " << focalPoint[1] << ", " << focalPoint[2] << ")" << std::endl;
    std::cerr << "New focal point: (" << newFocalPoint[0] << ", " << newFocalPoint[1] << ", " << newFocalPoint[2] << ")" << std::endl;
    std::cerr << "Old camera Z: " << position[2] << ", New camera Z: " << newPosition[2] << std::endl;
    std::cerr << "==================\n" << std::endl;
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
