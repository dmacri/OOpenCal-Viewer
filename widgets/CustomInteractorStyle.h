/** @file CustomInteractorStyle.h
 * @brief Custom VTK interactor style for cursor-based zoom.
 * 
 * This interactor style extends vtkInteractorStyleImage to provide
 * zoom functionality that keeps the point under the cursor stationary. */

#pragma once

#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkSmartPointer.h>

class vtkCellPicker;

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

    CustomInteractorStyle();

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
     * @param zoomFactor Multiplicative factor (> 1.0 zooms in, < 1.0 zooms out) */
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

    /// @brief Picker used to find world position under cursor
    vtkSmartPointer<vtkCellPicker> m_picker;

    /// @brief Minimum and maximum allowed camera distance to avoid clipping issues
    double m_minDistance = 0.1;
    double m_maxDistance = 1e6;
};

/** @brief Simple interactor style with wait cursor feedback.
 *
 * Extends vtkInteractorStyleTrackballCamera to show wait cursor during zoom operations.
 * This is a minimal overhead alternative to CustomInteractorStyle. */
class SimpleInteractorWithWaitCursor : public vtkInteractorStyleTrackballCamera
{
public:
    static SimpleInteractorWithWaitCursor* New();
    vtkTypeMacro(SimpleInteractorWithWaitCursor, vtkInteractorStyleTrackballCamera);

    /** @brief Handle mouse wheel forward event (zoom in).
     * Shows wait cursor during zoom operation. */
    void OnMouseWheelForward() override;

    /** @brief Handle mouse wheel backward event (zoom out).
     * Shows wait cursor during zoom operation. */
    void OnMouseWheelBackward() override;
};
