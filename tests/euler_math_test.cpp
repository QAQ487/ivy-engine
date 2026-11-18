// Euler extraction tests (engine/ecs/euler_math.h).
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/euler_math_test tests/euler_math_test.cpp

#include "engine/ecs/euler_math.h"

#include <array>
#include <cmath>
#include <cstdio>

using namespace euler_math;

using Mat3 = std::array<std::array<float, 3>, 3>;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

constexpr float kDegToRad = 3.14159265358979f / 180.0f;

// DirectXMath row-vector rotation matrices.
static Mat3 Rx(float deg) { const float a = deg * kDegToRad, c = std::cos(a), s = std::sin(a);
    return {{{1, 0, 0}, {0, c, s}, {0, -s, c}}}; }
static Mat3 Ry(float deg) { const float a = deg * kDegToRad, c = std::cos(a), s = std::sin(a);
    return {{{c, 0, -s}, {0, 1, 0}, {s, 0, c}}}; }
static Mat3 Rz(float deg) { const float a = deg * kDegToRad, c = std::cos(a), s = std::sin(a);
    return {{{c, s, 0}, {-s, c, 0}, {0, 0, 1}}}; }

static Mat3 Mul(const Mat3& a, const Mat3& b) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                r[i][j] += a[i][k] * b[k][j];
    return r;
}

// XMMatrixRotationRollPitchYaw(x, y, z): roll about Z, then pitch about X, then yaw about Y.
static Mat3 RollPitchYaw(const EulerDeg& e) { return Mul(Mul(Rz(e.z), Rx(e.x)), Ry(e.y)); }

static EulerDeg Extract(const Mat3& m) {
    const float a[3][3] = {{m[0][0], m[0][1], m[0][2]}, {m[1][0], m[1][1], m[1][2]}, {m[2][0], m[2][1], m[2][2]}};
    return RollPitchYawDegFromMatrix(a);
}

static bool SameMatrix(const Mat3& a, const Mat3& b, float tol = 1e-4f) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (std::fabs(a[i][j] - b[i][j]) > tol)
                return false;
    return true;
}

static bool Near(float a, float b, float tol = 1e-3f) { return std::fabs(a - b) <= tol; }

static float WrapDeg180(float a) {
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

int main() {
    // Pure yaw: every angle round-trips as (0, yaw, 0). This is the regression: the old
    // extraction negated yaw inside (-90, 90) and returned (180, .., 180) beyond it.
    for (int deg = -179; deg <= 180; deg += 1) {
        const EulerDeg e = Extract(Ry(static_cast<float>(deg)));
        Expect(Near(e.x, 0.0f) && Near(e.z, 0.0f), "pure yaw keeps pitch and roll at 0");
        Expect(Near(WrapDeg180(e.y - static_cast<float>(deg)), 0.0f), "pure yaw returns the same yaw");
    }

    // Pure pitch (within +/-90) and pure roll.
    for (int deg = -90; deg <= 90; deg += 5) {
        const EulerDeg e = Extract(Rx(static_cast<float>(deg)));
        Expect(Near(e.x, static_cast<float>(deg)) && Near(e.y, 0.0f) && Near(e.z, 0.0f), "pure pitch");
    }
    for (int deg = -179; deg <= 180; deg += 5) {
        const EulerDeg e = Extract(Rz(static_cast<float>(deg)));
        Expect(Near(e.x, 0.0f) && Near(e.y, 0.0f), "pure roll keeps pitch and yaw at 0");
        Expect(Near(WrapDeg180(e.z - static_cast<float>(deg)), 0.0f), "pure roll returns the same roll");
    }

    // General rotations: the extracted angles rebuild the exact same matrix when fed to
    // RollPitchYaw, which is what Model3D::Draw does with them.
    for (int p = -85; p <= 85; p += 17)
        for (int y = -175; y <= 175; y += 25)
            for (int r = -175; r <= 175; r += 35) {
                const EulerDeg in{static_cast<float>(p), static_cast<float>(y), static_cast<float>(r)};
                const Mat3 m = RollPitchYaw(in);
                const EulerDeg out = Extract(m);
                Expect(SameMatrix(m, RollPitchYaw(out)), "extracted Euler reproduces the rotation");
                Expect(Near(out.x, in.x, 0.01f), "pitch round trip");
            }

    // Gimbal lock (pitch = +/-90) still reproduces the rotation.
    for (int sign = -1; sign <= 1; sign += 2)
        for (int r = -150; r <= 150; r += 50) {
            const EulerDeg in{90.0f * static_cast<float>(sign), 0.0f, static_cast<float>(r)};
            const Mat3 m = RollPitchYaw(in);
            Expect(SameMatrix(m, RollPitchYaw(Extract(m)), 1e-3f), "gimbal lock reproduces the rotation");
        }

    // The scenario that was broken in the editor: a child with identity local rotation under
    // a yawed parent must render facing exactly where the parent faces, at every yaw.
    for (int deg = 0; deg < 360; deg += 15) {
        const Mat3 world = Mul(Ry(0.0f), Ry(static_cast<float>(deg)));   // child * parent (row-vector)
        const Mat3 drawn = RollPitchYaw(Extract(world));
        const float forwardX = std::sin(static_cast<float>(deg) * kDegToRad);
        const float forwardZ = std::cos(static_cast<float>(deg) * kDegToRad);
        // (0, 0, 1) * M is the third row.
        Expect(Near(drawn[2][0], forwardX, 1e-4f) && Near(drawn[2][2], forwardZ, 1e-4f),
               "child faces its parent's yaw direction");
    }

    // Why a controller must not reuse GetRotation()'s x / z. Rigidbody::Integrate rewrites a
    // body's Euler in canonical ZYX form (yaw within +/-90): yaw 135 -> (180, 45, 180), the
    // same rotation. Overriding only y on that tuple ((180, 135, 180)) is a DIFFERENT rotation,
    // yaw 180 - 135 = 45, which flips the facing every other frame past 90 degrees.
    {
        auto zyx = [](float x, float y, float z) { return Mul(Mul(Rz(z), Ry(y)), Rx(x)); };  // Transform order
        Expect(SameMatrix(zyx(180.0f, 45.0f, 180.0f), Ry(135.0f)), "(180,45,180) is yaw 135");
        Expect(SameMatrix(zyx(180.0f, 135.0f, 180.0f), Ry(45.0f)), "(180,135,180) is yaw 45, not 135");
        Expect(!SameMatrix(zyx(180.0f, 135.0f, 180.0f), Ry(135.0f)), "overriding only y breaks the facing");
        for (int deg = 0; deg < 360; deg += 15)
            Expect(SameMatrix(zyx(0.0f, static_cast<float>(deg), 0.0f), Ry(static_cast<float>(deg))),
                   "an upright (0, yaw, 0) rotation is always exactly that yaw");
    }

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("euler_math_test: PASS");
    return 0;
}
