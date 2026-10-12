/** @file CustomInteractorStyle.h
 * @brief Custom VTK interactor style for cursor-based zoom.
 * 
 * This interactor style extends vtkInteractorStyleImage to provide
 * zoom functionality that keeps the point under the cursor stationary. */

#pragma once

#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkSmartPointer.h>
#include <array>
#include <functional>

class vtkCellPicker;

/** @brief Custom interactor style for cursor-based zoom and 3D rotation.
 *
 * Extends vtkInteractorStyleTrackballCamera to provide enhanced interaction:
 *
 * **Inherited from vtkInteractorStyleTrackballCamera:**
 * - Left mouse button: 3D trackball rotation (orbits the rotation pivot, see SetRotationPivotProvider)
 * - Right mouse button: Zoom (dolly - move camera closer/farther)
 * - Middle mouse button: Pan (move focal point in view plane)
 *
 * **Custom enhancements:**
 * - Mouse wheel: Zoom towards cursor position (instead of screen center)
 * - Shift + Left drag: Pan (works in both 2D and 3D modes)
 * - Accumulated zoom: Multiple wheel events are batched for smooth zooming
 *
 * This provides intuitive 3D navigation with cursor-aware zoom behavior,
 * and enables panning in 2D mode via Shift + Left mouse button. */
class CustomInteractorStyle : public vtkInteractorStyleTrackballCamera
{
public:
    static CustomInteractorStyle* New();
    vtkTypeMacro(CustomInteractorStyle, vtkInteractorStyleTrackballCamera);

    CustomInteractorStyle();

    /** @brief Enable full trackball navigation.
     *
     * When disabled (2D mode), only the custom cursor-centered mouse-wheel
     * zoom remains active. Rotation, pan and right-button dolly are blocked. */
    void Set3DInteractionEnabled(bool enabled) noexcept
    {
        m_3dInteractionEnabled = enabled;
    }

    [[nodiscard]] bool Get3DInteractionEnabled() const noexcept
    {
        return m_3dInteractionEnabled;
    }

    /** @brief Register a callback notified when a camera-manipulating mouse gesture starts/ends.
     *
     * The callback receives `true` when the first mouse button of a rotate/pan/dolly
     * gesture is pressed and `false` when the last pressed button is released.
     * It is NOT called in 2D mode (where rotation/pan are disabled).
     * Used to pause the step-by-step playback while the camera is being moved. */
    void SetInteractionStateCallback(std::function<void(bool)> callback)
    {
        m_interactionStateCallback = std::move(callback);
    }

    /** @brief Set the point that 3D rotations orbit around, in world coordinates.
     *
     * Zoom towards the cursor and panning move the camera's focal point, so VTK's
     * trackball would otherwise rotate around a point away from the scene centre.
     * With a provider set, rotation keeps this point fixed on screen instead.
     * Without a provider, rotation behaves like plain VTK (around the focal point). */
    void SetRotationPivotProvider(std::function<std::array<double, 3>()> provider)
    {
        m_rotationPivotProvider = std::move(provider);
    }

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
     * Starts panning when Shift is pressed (works in both 2D and 3D modes). */
    void OnLeftButtonDown() override;

    /** @brief Handle left mouse button release event.
     *
     * Stops panning when left button is released. */
    void OnLeftButtonUp() override;

    void OnMiddleButtonDown() override;
    void OnMiddleButtonUp() override;
    void OnRightButtonDown() override;
    void OnRightButtonUp() override;

    /** @brief Handle mouse move event during panning.
     *
     * Pans the view when Shift+Left or Shift+Right is held down. */
    void OnMouseMove() override;

private:
    enum ButtonBit : unsigned
    {
        LeftButtonBit = 1u << 0,
        MiddleButtonBit = 1u << 1,
        RightButtonBit = 1u << 2,
    };

    /// @brief Track pressed buttons and fire the interaction-state callback on 0 <-> 1+ transitions.
    void UpdatePressedButton(ButtonBit button, bool pressed);

    /** @brief Perform zoom towards cursor.
     *
     * @param zoomFactor Multiplicative factor (> 1.0 zooms in, < 1.0 zooms out) */
    void ZoomTowardsCursor(double zoomFactor);

    /** @brief Perform panning based on mouse movement.
     * 
     * Moves the focal point based on the delta between last and current mouse position. */
    void PanCamera();

    /** @brief Trackball rotation around the rotation pivot.
     *
     * Runs VTK's own rotation step (same sensitivity), then moves the camera so the
     * rotation is about the pivot rather than the focal point. */
    void RotateAroundPivot();

    /// @brief Last mouse position for drag calculation
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;

    /// @brief Flag indicating if panning is active
    bool m_isPanning = false;

    /// @brief False in 2D mode, where only cursor-centered wheel zoom is allowed.
    bool m_3dInteractionEnabled = true;

    /// @brief Bit mask of mouse buttons currently held for a camera gesture
    unsigned m_pressedButtons = 0;

    /// @brief Notified when a camera gesture (rotate/pan/dolly) starts or ends
    std::function<void(bool)> m_interactionStateCallback;

    /// @brief World-space point rotations orbit around (scene centre); empty = VTK's focal-point rotation
    std::function<std::array<double, 3>()> m_rotationPivotProvider;

    /// @brief Picker used to find world position under cursor
    vtkSmartPointer<vtkCellPicker> m_picker;

    /// @brief Minimum and maximum allowed camera distance to avoid clipping issues
    double m_minDistance = 0.1;
    double m_maxDistance = 1e6;

    /// @brief Accumulated zoom factor from multiple wheel events
    double m_accumulatedZoomFactor = 1.0;

    /// @brief Timer ID for deferred zoom application (0 = no timer active)
    int m_zoomTimerId = 0;

    /// @brief Handle timer events for deferred zoom
    void OnTimer() override;
};
