/** @file CameraPivotMath.h
 * @brief Header-only camera math for orbiting a 3D view around a fixed pivot point.
 *
 * Kept free of Qt and VTK so it can be unit-tested without a display.
 * Frame convention (same as the Roll/Pitch/Yaw code in SceneWidget):
 *   backward = unit(position - focalPoint)   (points from the focal point to the camera)
 *   right    = up x backward
 *   up       = view-up vector orthogonalised against backward. */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace CameraPivotMath
{
using Vec3 = std::array<double, 3>;

/** @brief Row-major 3x3 rotation matrix. */
struct Mat3
{
    std::array<Vec3, 3> rows{};
};

/** @brief Camera pose as VTK stores it: position, focal point and view-up vector. */
struct CameraPose
{
    Vec3 position{};
    Vec3 focalPoint{};
    Vec3 viewUp{};
};

/** @brief Orthonormal camera axes in world coordinates. */
struct CameraFrame
{
    Vec3 right{};
    Vec3 up{};
    Vec3 backward{};
};

inline Vec3 add(const Vec3& a, const Vec3& b)
{
    return { a[0] + b[0], a[1] + b[1], a[2] + b[2] };
}

inline Vec3 sub(const Vec3& a, const Vec3& b)
{
    return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}

inline Vec3 scale(const Vec3& a, double factor)
{
    return { a[0] * factor, a[1] * factor, a[2] * factor };
}

inline double dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline Vec3 cross(const Vec3& a, const Vec3& b)
{
    return { a[1] * b[2] - a[2] * b[1],
             a[2] * b[0] - a[0] * b[2],
             a[0] * b[1] - a[1] * b[0] };
}

inline double length(const Vec3& a)
{
    return std::sqrt(dot(a, a));
}

/** @brief Unit vector, or nothing when the input is (numerically) zero or not finite. */
inline std::optional<Vec3> normalized(const Vec3& a)
{
    constexpr double minLength = 1e-12;
    const double len = length(a);
    if (!(len > minLength) || !std::isfinite(len))
        return std::nullopt;
    return scale(a, 1.0 / len);
}

/** @brief Apply a rotation matrix to a vector. */
inline Vec3 applyRotation(const Mat3& m, const Vec3& v)
{
    return { dot(m.rows[0], v), dot(m.rows[1], v), dot(m.rows[2], v) };
}

/** @brief Camera axes of a pose, or nothing for a degenerate pose
 *         (position on the focal point, or view-up parallel to the view direction). */
inline std::optional<CameraFrame> cameraFrameOf(const CameraPose& pose)
{
    const auto backward = normalized(sub(pose.position, pose.focalPoint));
    if (!backward)
        return std::nullopt;

    const auto up = normalized(sub(pose.viewUp, scale(*backward, dot(pose.viewUp, *backward))));
    if (!up)
        return std::nullopt;

    return CameraFrame{ cross(*up, *backward), *up, *backward };
}

/** @brief Rotation R that maps every axis of `from` onto the matching axis of `to`: R = sum_i to_i * from_i^T. */
inline Mat3 rotationBetween(const CameraFrame& from, const CameraFrame& to)
{
    const std::array<Vec3, 3> source{ from.right, from.up, from.backward };
    const std::array<Vec3, 3> target{ to.right, to.up, to.backward };

    Mat3 result;
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t col = 0; col < 3; ++col)
        {
            result.rows[row][col] = target[0][row] * source[0][col]
                                  + target[1][row] * source[1][col]
                                  + target[2][row] * source[2][col];
        }
    }
    return result;
}

/** @brief Translation that turns a rotation about the focal point into a rotation about `pivot`:
 *         (R - I) * (focalPoint - pivot). */
inline Vec3 pivotCompensation(const Mat3& rotation, const Vec3& focalPoint, const Vec3& pivot)
{
    const Vec3 offset = sub(focalPoint, pivot);
    return sub(applyRotation(rotation, offset), offset);
}

/** @brief Rigidly rotate a pose by `rotation` around `pivot`.
 *
 * The pivot keeps its screen position, and the camera-to-focal distance is preserved.
 * Returns the pose unchanged when it is degenerate. */
inline CameraPose rotateAroundPivot(const CameraPose& pose, const Vec3& pivot, const Mat3& rotation)
{
    const auto frame = cameraFrameOf(pose);
    if (!frame)
        return pose;

    CameraPose result;
    result.position = add(pivot, applyRotation(rotation, sub(pose.position, pivot)));
    result.focalPoint = add(pivot, applyRotation(rotation, sub(pose.focalPoint, pivot)));
    result.viewUp = applyRotation(rotation, frame->up);
    return result;
}

/** @brief Turn a pose produced by a rotation about its focal point into the same rotation about `pivot`.
 *
 * `after` is the pose after the rotation, `rotation` is the rotation that produced it,
 * and `focalBefore` is the focal point before it. Use it with VTK's trackball, which
 * always rotates around the focal point. */
inline CameraPose compensatePivot(const CameraPose& after, const Mat3& rotation,
                                  const Vec3& focalBefore, const Vec3& pivot)
{
    const Vec3 delta = pivotCompensation(rotation, focalBefore, pivot);

    CameraPose result = after;
    result.position = add(after.position, delta);
    result.focalPoint = add(after.focalPoint, delta);
    return result;
}
} // namespace CameraPivotMath
