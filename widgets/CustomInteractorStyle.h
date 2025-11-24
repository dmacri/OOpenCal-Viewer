/** @file CustomInteractorStyle.h
 * @brief Custom VTK interactor style for cursor-based zoom.
 * 
 * This interactor style extends vtkInteractorStyleImage to provide
 * zoom functionality that keeps the point under the cursor stationary. */

#pragma once

#include <vtkInteractorStyleTrackballCamera.h>

/** @brief Custom interactor style for cursor-based zoom and 3D rotation.
 * 
 * Extends vtkInteractorStyleTrackballCamera to override mouse wheel events
 * and zoom towards the cursor position instead of the screen center.
 * Also supports 3D rotation with trackball camera. */
class CustomInteractorStyle : public vtkInteractorStyleTrackballCamera
{
public:
    static CustomInteractorStyle* New();
    vtkTypeMacro(CustomInteractorStyle, vtkInteractorStyleTrackballCamera);

    /** @brief Handle mouse wheel forward event (zoom in).
     * 
     * Zooms in towards the cursor position. */
    void OnMouseWheelForward() override;

    /** @brief Handle mouse wheel backward event (zoom out).
     * 
     * Zooms out away from the cursor position. */
    void OnMouseWheelBackward() override;

    /** @brief Handle left mouse button press event.
     * 
     * Starts panning when left button is pressed. */
    void OnLeftButtonDown() override;

    /** @brief Handle left mouse button release event.
     * 
     * Stops panning when left button is released. */
    void OnLeftButtonUp() override;

    /** @brief Handle mouse move event during panning.
     * 
     * Pans the view when left button is held down. */
    void OnMouseMove() override;

private:
    /** @brief Perform zoom towards cursor.
     * 
     * @param zoomFactor Factor to zoom by (< 1.0 for zoom in, > 1.0 for zoom out) */
    void ZoomTowardsCursor(double zoomFactor);

    /** @brief Perform panning based on mouse movement.
     * 
     * Moves the focal point based on the delta between last and current mouse position. */
    void PanCamera();

    /// @brief Last mouse position for drag calculation
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;

    /// @brief Flag indicating if panning is active
    bool m_isPanning = false;
};
