// euler_math.h
// Header-only Euler extraction that matches how the engine RENDERS rotations.
// No engine / D3D dependency, so the Linux unit test can compile it.
//
// Model3D::Draw builds its rotation with XMMatrixRotationRollPitchYaw(x, y, z), i.e. in
// DirectXMath's row-vector convention
//     M = Rz(roll) * Rx(pitch) * Ry(yaw)
// with Euler degrees stored as (x = pitch, y = yaw, z = roll). A child object's world
// rotation comes out of the hierarchy as a matrix, so it has to be turned back into those
// three angles in the SAME order, otherwise the mesh is drawn with the wrong rotation.
//
// DirectXMath row-vector matrices (angle a):
//   Rx = [1 0 0; 0 c s; 0 -s c]   Ry = [c 0 -s; 0 1 0; s 0 c]   Rz = [c s 0; -s c 0; 0 0 1]

#pragma once

#include <cmath>

namespace euler_math {

struct EulerDeg {
    float x = 0.0f;   // pitch (about X)
    float y = 0.0f;   // yaw   (about Y)
    float z = 0.0f;   // roll  (about Z)
};

constexpr float kRadToDeg = 180.0f / 3.14159265358979f;

// m[row][col] is a pure rotation matrix in DirectXMath row-vector convention.
// Inverse of XMMatrixRotationRollPitchYaw: M = Rz(roll) * Rx(pitch) * Ry(yaw).
// pitch is in [-90, 90]; yaw and roll cover the full circle, so a pure yaw of any angle
// comes back as (0, yaw, 0).
inline EulerDeg RollPitchYawDegFromMatrix(const float m[3][3]) {
    // M row 2 = (cp*sy, -sp, cp*cy), M[0][1] = sr*cp, M[1][1] = cr*cp.
    const float cosPitch = std::sqrt(m[2][0] * m[2][0] + m[2][2] * m[2][2]);
    const float pitch = std::atan2(-m[2][1], cosPitch);
    float yaw = 0.0f;
    float roll = 0.0f;
    if (cosPitch > 1e-6f) {
        yaw = std::atan2(m[2][0], m[2][2]);
        roll = std::atan2(m[0][1], m[1][1]);
    } else {
        // Gimbal lock (pitch = +/-90): yaw and roll are coupled; fold it all into roll.
        roll = std::atan2(-m[1][0], m[0][0]);
    }
    return { pitch * kRadToDeg, yaw * kRadToDeg, roll * kRadToDeg };
}

} // namespace euler_math
