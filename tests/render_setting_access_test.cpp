// Render setting name access tests.
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/render_setting_access_test tests/render_setting_access_test.cpp

#include "engine/scene/render_setting_access.h"

#include <cmath>
#include <cstdio>

struct MockSettings {
    bool frustumCullingEnabled = true;
    bool shadowCascadeCullingEnabled = true;
    bool spriteCullingEnabled = true;
    bool lodEnabled = true;
    bool animCullingEnabled = true;
    bool lightSelectionEnabled = true;
    bool billboardCullingEnabled = true;
    bool effectCullingEnabled = true;
    bool lodDebugLabels = false;
    float lodBias = 1.0f;
    float cullDistanceScale = 1.0f;
    float shadowDistance = 0.0f;
    int shadowLodOffset = 0;
    float effectCullRadius = 20.0f;
};

static int g_failed = 0;
static void Expect(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); ++g_failed; }
}
static bool Near(double a, double b) { return std::fabs(a - b) < 1e-4; }

int main() {
    using namespace render_setting_access;
    MockSettings s;
    bool b = false;
    double n = 0.0;

    // Every toggle round-trips.
    const char* bools[] = {"frustumCullingEnabled", "shadowCascadeCullingEnabled",
        "spriteCullingEnabled", "lodEnabled", "animCullingEnabled", "lightSelectionEnabled",
        "billboardCullingEnabled", "effectCullingEnabled", "lodDebugLabels"};
    for (const char* key : bools) {
        Expect(SetBool(s, key, false) && GetBool(s, key, b) && !b, key);
        Expect(SetBool(s, key, true) && GetBool(s, key, b) && b, key);
    }
    Expect(!SetBool(s, "fogEnabled", true), "unknown bool key rejected");
    Expect(!GetBool(s, "lodBias", b), "numeric key is not a bool");
    Expect(!SetBool(s, nullptr, true), "null key rejected");

    // Numbers are clamped like the scene loader.
    Expect(SetNumber(s, "lodBias", 2.5) && Near(s.lodBias, 2.5), "lodBias set");
    Expect(SetNumber(s, "lodBias", 1000) && Near(s.lodBias, lod_select::kMaxBias), "lodBias clamps high");
    Expect(SetNumber(s, "lodBias", 0) && Near(s.lodBias, lod_select::kMinBias), "lodBias clamps low");
    Expect(SetNumber(s, "cullDistanceScale", 3) && Near(s.cullDistanceScale, 3), "cullDistanceScale set");
    Expect(SetNumber(s, "cullDistanceScale", 99) && Near(s.cullDistanceScale, cull_settings::kMaxCullDistanceScale), "cullDistanceScale clamps");
    Expect(SetNumber(s, "shadowDistance", 120) && Near(s.shadowDistance, 120), "shadowDistance set");
    Expect(SetNumber(s, "shadowDistance", 2) && Near(s.shadowDistance, cull_settings::kMinShadowDistance), "shadowDistance clamps to the minimum");
    Expect(SetNumber(s, "shadowDistance", -5) && Near(s.shadowDistance, 0), "negative shadowDistance means preset");
    Expect(SetNumber(s, "shadowLodOffset", 2.4) && s.shadowLodOffset == 2, "shadowLodOffset rounds");
    Expect(SetNumber(s, "shadowLodOffset", 9) && s.shadowLodOffset == lod_select::kMaxShadowLodOffset, "shadowLodOffset clamps");
    Expect(SetNumber(s, "effectCullRadius", 5000) && Near(s.effectCullRadius, cull_settings::kMaxEffectCullRadius), "effectCullRadius clamps");

    // Getters.
    Expect(GetNumber(s, "effectCullRadius", n) && Near(n, cull_settings::kMaxEffectCullRadius), "get effectCullRadius");
    Expect(GetNumber(s, "shadowLodOffset", n) && Near(n, lod_select::kMaxShadowLodOffset), "get shadowLodOffset");
    Expect(!GetNumber(s, "lodEnabled", n), "bool key is not numeric");
    Expect(!GetNumber(s, "nope", n), "unknown numeric key rejected");

    // Bad values change nothing.
    const float before = s.lodBias;
    Expect(!SetNumber(s, "lodBias", NAN) && Near(s.lodBias, before), "NaN rejected");
    Expect(!SetNumber(s, "lodBias", INFINITY) && Near(s.lodBias, before), "infinity rejected");
    Expect(!SetNumber(s, "unknown", 1.0), "unknown numeric key rejected");
    Expect(!SetNumber(s, nullptr, 1.0), "null key rejected");

    if (g_failed == 0)
        std::puts("render_setting_access_test: ok");
    return g_failed == 0 ? 0 : 1;
}
