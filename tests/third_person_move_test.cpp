// Third-person facing-relative movement math tests.
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -Isrc/app/camera -o /tmp/third_person_move_test tests/third_person_move_test.cpp

#include "third_person_move_math.h"

#include <cmath>
#include <cstdio>

using namespace tp_move;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static bool Near(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }

static float Wrap180(float a) {
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

static void ExpectDir(float yaw, float ix, float iz, float ex, float ez, const char* msg) {
    const Vec2 w = CameraRelativeWish(yaw, ix, iz);
    if (!Near(w.x, ex) || !Near(w.z, ez)) {
        std::fprintf(stderr, "FAIL: %s (got %.4f, %.4f expected %.4f, %.4f)\n", msg, w.x, w.z, ex, ez);
        ++g_failed;
    }
}

int main() {
    // Facing +Z (yaw 0): W forward, D right (+X), S back, A left.
    ExpectDir(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, "W at yaw 0 -> +Z");
    ExpectDir(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, "D at yaw 0 -> +X");
    ExpectDir(0.0f, 0.0f, -1.0f, 0.0f, -1.0f, "S at yaw 0 -> -Z");
    ExpectDir(0.0f, -1.0f, 0.0f, -1.0f, 0.0f, "A at yaw 0 -> -X");

    // Facing +X (yaw 90): W -> +X, D -> -Z, S -> -X, A -> +Z.
    ExpectDir(90.0f, 0.0f, 1.0f, 1.0f, 0.0f, "W at yaw 90 -> +X");
    ExpectDir(90.0f, 1.0f, 0.0f, 0.0f, -1.0f, "D at yaw 90 -> -Z");
    ExpectDir(90.0f, 0.0f, -1.0f, -1.0f, 0.0f, "S at yaw 90 -> -X");
    ExpectDir(90.0f, -1.0f, 0.0f, 0.0f, 1.0f, "A at yaw 90 -> +Z");
    ExpectDir(180.0f, 0.0f, 1.0f, 0.0f, -1.0f, "W at yaw 180 -> -Z");
    ExpectDir(270.0f, 0.0f, 1.0f, -1.0f, 0.0f, "W at yaw 270 -> -X");

    // The control scheme: from ANY facing, W is the facing heading, D is +90, S is +180 and
    // A is +270 (clockwise from the front). The character never turns to the move direction.
    for (int yaw = 0; yaw < 360; yaw += 7) {
        const float f = static_cast<float>(yaw);
        Expect(Near(Wrap180(HeadingDeg(CameraRelativeWish(f, 0.0f, 1.0f)) - f), 0.0f, 1e-2f), "W = facing + 0");
        Expect(Near(Wrap180(HeadingDeg(CameraRelativeWish(f, 1.0f, 0.0f)) - f), 90.0f, 1e-2f), "D = facing + 90");
        Expect(Near(std::fabs(Wrap180(HeadingDeg(CameraRelativeWish(f, 0.0f, -1.0f)) - f)), 180.0f, 1e-2f), "S = facing + 180");
        Expect(Near(Wrap180(HeadingDeg(CameraRelativeWish(f, -1.0f, 0.0f)) - f), -90.0f, 1e-2f), "A = facing + 270");
    }

    // Magnitude is preserved (no speed change from yaw).
    {
        const Vec2 w = CameraRelativeWish(37.0f, 0.6f, 0.8f);
        Expect(Near(std::sqrt(w.x * w.x + w.z * w.z), 1.0f), "rotation keeps length");
    }

    // Heading helper: +Z = 0, +X = 90, -Z = 180, -X = -90.
    Expect(Near(HeadingDeg({0.0f, 1.0f}), 0.0f), "heading +Z");
    Expect(Near(HeadingDeg({1.0f, 0.0f}), 90.0f), "heading +X");
    Expect(Near(std::fabs(HeadingDeg({0.0f, -1.0f})), 180.0f), "heading -Z");
    Expect(Near(HeadingDeg({-1.0f, 0.0f}), -90.0f), "heading -X");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("third_person_move_test: PASS");
    return 0;
}
