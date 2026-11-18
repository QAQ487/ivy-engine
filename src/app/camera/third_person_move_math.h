// third_person_move_math.h
// Header-only facing-relative movement helpers for third-person scripts.
// No engine / D3D dependency, so the Linux unit test can compile it.
//
// Convention (matches ThirdPersonCamera): the camera orbits at yaw `cameraYawDeg` behind
// the target and looks along +(sin yaw, cos yaw) in the XZ plane (+Z at yaw 0, +X at 90).
// Input is (x = strafe right, z = forward), each in [-1, 1].

#pragma once

#include <cmath>

namespace tp_move {

constexpr float kDegToRad = 3.14159265358979f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265358979f;

struct Vec2 {
    float x = 0.0f;
    float z = 0.0f;
};

// World-space XZ direction for WASD input relative to the facing (= camera) yaw:
// W = facing, D = facing + 90 deg, S = facing + 180 deg, A = facing + 270 deg (clockwise
// seen from above).
inline Vec2 CameraRelativeWish(float cameraYawDeg, float inputX, float inputZ) {
    const float yaw = cameraYawDeg * kDegToRad;
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const float rx = fz;   // right = (forward.z, -forward.x)
    const float rz = -fx;
    return { rx * inputX + fx * inputZ, rz * inputX + fz * inputZ };
}

// Heading in degrees for a world XZ direction (0 = +Z, 90 = +X).
inline float HeadingDeg(const Vec2& dir) {
    return std::atan2(dir.x, dir.z) * kRadToDeg;
}

} // namespace tp_move
