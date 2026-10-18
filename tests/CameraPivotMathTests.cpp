#include "widgets/CameraPivotMath.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>

using namespace CameraPivotMath;

namespace
{
constexpr double kTolerance = 1e-9;
constexpr double kWorldTolerance = 1e-6; // positions are in the order of 1e3

// Scene centre used as the rotation pivot in the tests.
const Vec3 kPivot{ 500.0, 250.0, 700.0 };

void expectVecNear(const Vec3& actual, const Vec3& expected, double tolerance = kTolerance)
{
    for (std::size_t i = 0; i < 3; ++i)
        EXPECT_NEAR(actual[i], expected[i], tolerance) << "component " << i;
}

void expectMatNear(const Mat3& actual, const Mat3& expected, double tolerance = kTolerance)
{
    for (std::size_t row = 0; row < 3; ++row)
        expectVecNear(actual.rows[row], expected.rows[row], tolerance);
}

Mat3 identity()
{
    return Mat3{ { Vec3{ 1.0, 0.0, 0.0 }, Vec3{ 0.0, 1.0, 0.0 }, Vec3{ 0.0, 0.0, 1.0 } } };
}

/// Rotation by `angle` radians around `axis` (Rodrigues' formula).
Mat3 rotationAroundAxis(const Vec3& axis, double angle)
{
    const Vec3 k = *normalized(axis);
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const double t = 1.0 - c;

    return Mat3{ { Vec3{ t * k[0] * k[0] + c,        t * k[0] * k[1] - s * k[2], t * k[0] * k[2] + s * k[1] },
                   Vec3{ t * k[0] * k[1] + s * k[2], t * k[1] * k[1] + c,        t * k[1] * k[2] - s * k[0] },
                   Vec3{ t * k[0] * k[2] - s * k[1], t * k[1] * k[2] + s * k[0], t * k[2] * k[2] + c        } } };
}

/// Camera-space coordinates of a world point: its projections on the camera axes.
/// Equal coordinates before and after a rotation mean the point keeps its place on screen.
Vec3 cameraSpaceOf(const Vec3& point, const CameraPose& pose)
{
    const auto frame = cameraFrameOf(pose);
    const Vec3 offset = sub(point, pose.position);
    return { dot(offset, frame->right), dot(offset, frame->up), dot(offset, frame->backward) };
}

/// Camera zoomed towards a point away from the scene centre: its focal point is off the pivot.
CameraPose zoomedPose()
{
    return CameraPose{ { 120.0, -40.0, 900.0 }, { 10.0, 25.0, 300.0 }, { 0.0, 1.0, 0.0 } };
}

} // namespace

TEST(CameraPivotMathTests, DefaultVtkCameraFrameFollowsConvention)
{
    // VTK's default camera looks down -Z with +Y up, so the right vector is +X.
    const auto frame = cameraFrameOf(CameraPose{ { 0.0, 0.0, 1.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } });
    ASSERT_TRUE(frame.has_value());
    expectVecNear(frame->right, { 1.0, 0.0, 0.0 });
    expectVecNear(frame->up, { 0.0, 1.0, 0.0 });
    expectVecNear(frame->backward, { 0.0, 0.0, 1.0 });
}

TEST(CameraPivotMathTests, FrameIsOrthonormalAndRightHanded)
{
    const auto frame = cameraFrameOf(zoomedPose());
    ASSERT_TRUE(frame.has_value());

    EXPECT_NEAR(length(frame->right), 1.0, kTolerance);
    EXPECT_NEAR(length(frame->up), 1.0, kTolerance);
    EXPECT_NEAR(length(frame->backward), 1.0, kTolerance);
    EXPECT_NEAR(dot(frame->right, frame->up), 0.0, kTolerance);
    EXPECT_NEAR(dot(frame->up, frame->backward), 0.0, kTolerance);
    EXPECT_NEAR(dot(frame->backward, frame->right), 0.0, kTolerance);
    expectVecNear(frame->right, cross(frame->up, frame->backward));
}

TEST(CameraPivotMathTests, DegenerateCamerasHaveNoFrame)
{
    // Position on the focal point.
    EXPECT_FALSE(cameraFrameOf(CameraPose{ { 1.0, 2.0, 3.0 }, { 1.0, 2.0, 3.0 }, { 0.0, 1.0, 0.0 } }).has_value());
    // View-up parallel to the view direction.
    EXPECT_FALSE(cameraFrameOf(CameraPose{ { 0.0, 0.0, 1.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 } }).has_value());
}

TEST(CameraPivotMathTests, RotationBetweenIdenticalFramesIsIdentity)
{
    const auto frame = cameraFrameOf(zoomedPose());
    ASSERT_TRUE(frame.has_value());
    expectMatNear(rotationBetween(*frame, *frame), identity());
}

TEST(CameraPivotMathTests, RotationBetweenFramesMapsEveryAxis)
{
    const auto from = cameraFrameOf(zoomedPose());
    const auto to = cameraFrameOf(CameraPose{ { 0.0, -800.0, 200.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 } });
    ASSERT_TRUE(from.has_value());
    ASSERT_TRUE(to.has_value());

    const Mat3 rotation = rotationBetween(*from, *to);
    expectVecNear(applyRotation(rotation, from->right), to->right);
    expectVecNear(applyRotation(rotation, from->up), to->up);
    expectVecNear(applyRotation(rotation, from->backward), to->backward);
}

TEST(CameraPivotMathTests, RotateAroundPivotKeepsDistanceAndPivotScreenPosition)
{
    const CameraPose pose = zoomedPose();
    const CameraPose rotated = rotateAroundPivot(pose, kPivot, rotationAroundAxis({ 0.3, -0.7, 0.5 }, 0.9));

    EXPECT_NEAR(length(sub(rotated.position, rotated.focalPoint)),
                length(sub(pose.position, pose.focalPoint)), kWorldTolerance);
    expectVecNear(cameraSpaceOf(kPivot, rotated), cameraSpaceOf(kPivot, pose), kWorldTolerance);
}

TEST(CameraPivotMathTests, RotateAroundPivotMovesPositionAndFocalPointRigidly)
{
    const CameraPose pose = zoomedPose();
    const Mat3 rotation = rotationAroundAxis({ 1.0, 2.0, 3.0 }, 0.4);
    const CameraPose rotated = rotateAroundPivot(pose, kPivot, rotation);

    expectVecNear(rotated.focalPoint, add(kPivot, applyRotation(rotation, sub(pose.focalPoint, kPivot))), kWorldTolerance);
    expectVecNear(rotated.position, add(kPivot, applyRotation(rotation, sub(pose.position, kPivot))), kWorldTolerance);

    const auto before = cameraFrameOf(pose);
    const auto after = cameraFrameOf(rotated);
    ASSERT_TRUE(before.has_value());
    ASSERT_TRUE(after.has_value());
    expectVecNear(after->backward, applyRotation(rotation, before->backward));
    expectVecNear(after->up, applyRotation(rotation, before->up));
}

TEST(CameraPivotMathTests, RotationCanBeRecoveredFromFramesAfterRotateAroundPivot)
{
    const CameraPose pose = zoomedPose();
    const Mat3 rotation = rotationAroundAxis({ -0.2, 0.8, 0.4 }, -0.6);
    const CameraPose rotated = rotateAroundPivot(pose, kPivot, rotation);

    const Mat3 recovered = rotationBetween(*cameraFrameOf(pose), *cameraFrameOf(rotated));
    expectMatNear(recovered, rotation);
}

TEST(CameraPivotMathTests, CompensationTurnsRotationAroundFocalPointIntoRotationAroundPivot)
{
    // This is what VTK's trackball does (rotate around the focal point), followed by the correction.
    const CameraPose pose = zoomedPose();
    const Mat3 rotation = rotationAroundAxis({ 0.5, 0.5, -1.0 }, 0.35);

    const CameraPose aroundFocal = rotateAroundPivot(pose, pose.focalPoint, rotation);
    const Mat3 measured = rotationBetween(*cameraFrameOf(pose), *cameraFrameOf(aroundFocal));
    const CameraPose corrected = compensatePivot(aroundFocal, measured, pose.focalPoint, kPivot);
    const CameraPose expected = rotateAroundPivot(pose, kPivot, rotation);

    expectVecNear(corrected.position, expected.position, kWorldTolerance);
    expectVecNear(corrected.focalPoint, expected.focalPoint, kWorldTolerance);
    expectVecNear(corrected.viewUp, expected.viewUp, kTolerance);
}

TEST(CameraPivotMathTests, CompensationKeepsDollyUntouched)
{
    const CameraPose pose = zoomedPose();
    const auto frame = cameraFrameOf(pose);
    ASSERT_TRUE(frame.has_value());

    // Dolly: the camera moves along its view direction and the orientation stays the same.
    CameraPose dollied = pose;
    dollied.position = sub(pose.position, scale(frame->backward, 75.0));

    const CameraPose result = compensatePivot(dollied, identity(), pose.focalPoint, kPivot);
    expectVecNear(result.position, dollied.position);
    expectVecNear(result.focalPoint, dollied.focalPoint);
}

TEST(CameraPivotMathTests, NoCompensationWhenPivotIsTheFocalPoint)
{
    // Un-zoomed camera (focal point on the pivot): the correction must vanish, so behaviour matches VTK.
    const CameraPose pose = zoomedPose();
    const Mat3 rotation = rotationAroundAxis({ 0.0, 0.0, 1.0 }, 0.2);
    const CameraPose rotated = rotateAroundPivot(pose, pose.focalPoint, rotation);

    const CameraPose result = compensatePivot(rotated, rotation, pose.focalPoint, pose.focalPoint);
    expectVecNear(result.position, rotated.position);
    expectVecNear(result.focalPoint, rotated.focalPoint);
}

TEST(CameraPivotMathTests, SliderTargetOrientationIsReachedWithoutRecentring)
{
    // Slider-style change: a zoomed camera is moved to an absolute target orientation.
    const CameraPose pose = zoomedPose();
    const CameraFrame target = *cameraFrameOf(CameraPose{ { 0.0, -800.0, 200.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 } });

    const CameraPose result = rotateAroundPivot(pose, kPivot,
                                                rotationBetween(*cameraFrameOf(pose), target));

    const auto resultFrame = cameraFrameOf(result);
    ASSERT_TRUE(resultFrame.has_value());
    expectVecNear(resultFrame->backward, target.backward);
    expectVecNear(resultFrame->up, target.up);
    EXPECT_NEAR(length(sub(result.position, result.focalPoint)),
                length(sub(pose.position, pose.focalPoint)), kWorldTolerance);
    expectVecNear(cameraSpaceOf(kPivot, result), cameraSpaceOf(kPivot, pose), kWorldTolerance);
}

TEST(CameraPivotMathTests, DegenerateCameraIsReturnedUnchangedByRotateAroundPivot)
{
    const CameraPose degenerate{ { 1.0, 2.0, 3.0 }, { 1.0, 2.0, 3.0 }, { 0.0, 1.0, 0.0 } };
    const CameraPose result = rotateAroundPivot(degenerate, kPivot, rotationAroundAxis({ 0.0, 1.0, 0.0 }, 0.5));

    expectVecNear(result.position, degenerate.position);
    expectVecNear(result.focalPoint, degenerate.focalPoint);
    expectVecNear(result.viewUp, degenerate.viewUp);
}
