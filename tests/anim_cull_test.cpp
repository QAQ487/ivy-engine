// Animation off-screen skip decision tests.
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/anim_cull_test tests/anim_cull_test.cpp

#include "engine/rendering/culling/anim_cull_math.h"

#include <cstdio>

using namespace anim_cull;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

int main() {
    Expect(!ShouldSkipPose(true, kNever, kNever, 5), "never considered keeps animating");
    Expect(!ShouldSkipPose(true, kNever, kNever, 0), "never considered at serial 0 keeps animating");
    Expect(ShouldSkipPose(true, 5, 4, 5), "considered but not drawn last frame skips");
    Expect(ShouldSkipPose(true, 5, kNever, 5), "considered, never drawn skips");
    Expect(!ShouldSkipPose(true, 5, 5, 5), "drawn last frame animates");
    Expect(!ShouldSkipPose(false, 5, 4, 5), "disabled never skips");
    Expect(!ShouldSkipPose(true, 4, 4, 5), "stale consideration (not looked at last frame) animates");
    // Shadow-only visibility counts as drawn: caster must keep its pose.
    Expect(!ShouldSkipPose(true, 7, 7, 7), "shadow-only draw still animates");

    if (g_failed == 0)
        std::puts("anim_cull_test: ok");
    return g_failed == 0 ? 0 : 1;
}
