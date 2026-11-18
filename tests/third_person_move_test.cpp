// Third-person camera-relative movement math tests.
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

static void ExpectDir(float yaw, float ix, float iz, float ex, float ez, const char* msg) {
    const Vec2 w = CameraRelativeWish(yaw, ix, iz);
    if (!Near(w.x, ex) || !Near(w.z, ez)) {
        std::fprintf(stderr, "FAIL: %s (got %.4f, %.4f expected %.4f, %.4f)\n", msg, w.x, w.z, ex, ez);
        ++g_failed;
    }
}

int main() {
    // Camera at yaw 0 looks along +Z: W forward, D right (+X), S back, A left.
    ExpectDir(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, "W at yaw 0 -> +Z");
    ExpectDir(0.0f, 0.0f, -1.0f, 0.0f, -1.0f, "S at yaw 0 -> -Z");
    ExpectDir(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, "D at yaw 0 -> +X");
    ExpectDir(0.0f, -1.0f, 0.0f, -1.0f, 0.0f, "A at yaw 0 -> -X");

    // Camera yawed 90 deg (looks along +X): W -> +X, D -> -Z (screen right).
    ExpectDir(90.0f, 0.0f, 1.0f, 1.0f, 0.0f, "W at yaw 90 -> +X");
    ExpectDir(90.0f, 1.0f, 0.0f, 0.0f, -1.0f, "D at yaw 90 -> -Z");
    ExpectDir(180.0f, 0.0f, 1.0f, 0.0f, -1.0f, "W at yaw 180 -> -Z");
    ExpectDir(270.0f, 0.0f, 1.0f, -1.0f, 0.0f, "W at yaw 270 -> -X");

    // Magnitude is preserved (no speed change from yaw).
    {
        const Vec2 w = CameraRelativeWish(37.0f, 0.6f, 0.8f);
        Expect(Near(std::sqrt(w.x * w.x + w.z * w.z), 1.0f), "rotation keeps length");
    }

    // The regression: the move direction depends only on the camera yaw, never on
    // the character heading. Holding S for many frames while the heading turns to
    // face the move direction keeps moving the SAME world direction.
    {
        const float cameraYaw = 25.0f;
        float facing = 25.0f;
        const Vec2 first = CameraRelativeWish(cameraYaw, 0.0f, -1.0f);
        for (int i = 0; i < 120; ++i) {
            const Vec2 w = CameraRelativeWish(cameraYaw, 0.0f, -1.0f);
            facing = TurnToward(facing, HeadingDeg(w), 720.0f / 60.0f);
            Expect(Near(w.x, first.x) && Near(w.z, first.z), "S direction stays constant while turning");
        }
        Expect(Near(WrapDeg180(facing - HeadingDeg(first)), 0.0f), "facing converges to move heading");
        Expect(cameraYaw == 25.0f, "camera yaw is untouched by facing updates");
    }

    // Heading helper: +Z = 0, +X = 90, -Z = 180, -X = -90.
    Expect(Near(HeadingDeg({0.0f, 1.0f}), 0.0f), "heading +Z");
    Expect(Near(HeadingDeg({1.0f, 0.0f}), 90.0f), "heading +X");
    Expect(Near(std::fabs(HeadingDeg({0.0f, -1.0f})), 180.0f), "heading -Z");
    Expect(Near(HeadingDeg({-1.0f, 0.0f}), -90.0f), "heading -X");

    // Turning: shortest way, clamped step, wraps across +/-180.
    Expect(Near(TurnToward(0.0f, 90.0f, 30.0f), 30.0f), "turn clamps to step");
    Expect(Near(TurnToward(0.0f, 20.0f, 30.0f), 20.0f), "turn snaps when within step");
    Expect(Near(TurnToward(170.0f, -170.0f, 5.0f), 175.0f), "turn takes the short way across 180");
    Expect(Near(TurnToward(-170.0f, 170.0f, 5.0f), -175.0f), "turn takes the short way across -180");
    Expect(Near(TurnToward(10.0f, 10.0f, 5.0f), 10.0f), "no turn when aligned");
    Expect(Near(WrapDeg180(540.0f), 180.0f) || Near(WrapDeg180(540.0f), -180.0f), "wrap 540");
    Expect(Near(WrapDeg180(-190.0f), 170.0f), "wrap -190");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("third_person_move_test: PASS");
    return 0;
}
