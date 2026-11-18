// Billboard / particle / effect culling math tests.
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -Itools/linux_win32_shim -o /tmp/fx_cull_test tests/fx_cull_test.cpp

#include "engine/rendering/culling/fx_cull_math.h"

#include <cmath>
#include <cstdio>

using frustum_cull::FrustumPlanes;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

// Camera at the origin looking +Z, LH perspective (row-major, D3D z in [0,1]).
static void MakePlanes(FrustumPlanes& planes, float fovYDeg = 60.0f, float aspect = 1.0f,
                       float zn = 0.1f, float zf = 100.0f) {
    const float f = 1.0f / std::tan(fovYDeg * 0.017453292519943295f * 0.5f);
    float m[16] = {};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = zf / (zf - zn);
    m[11] = 1.0f;
    m[14] = (-zn * zf) / (zf - zn);
    frustum_cull::ExtractFrustumPlanesD3D(m, planes);
}

int main() {
    FrustumPlanes pl;
    MakePlanes(pl);

    // Radii.
    Expect(Near(fx_cull::BillboardRadius(3.0f, 4.0f), 2.5f), "billboard radius is half the diagonal");
    Expect(Near(fx_cull::BillboardRadius(-3.0f, 4.0f), 2.5f), "negative size uses magnitude");
    Expect(fx_cull::BillboardRadius(NAN, 4.0f) == 2.0f, "NaN width counts as 0");
    Expect(Near(fx_cull::ParticleRadius(2.0f), 1.5f), "particle radius covers the half diagonal");
    Expect(fx_cull::ParticleRadius(2.0f) >= 0.70711f * 2.0f, "particle radius is conservative");
    Expect(Near(fx_cull::EffectRadius(20.0f, 1, 1, 1), 20.0f), "effect radius at unit scale");
    Expect(Near(fx_cull::EffectRadius(20.0f, 1, 3, 2), 60.0f), "effect radius uses the largest scale");
    Expect(Near(fx_cull::EffectRadius(20.0f, -2, 1, 1), 40.0f), "negative scale uses magnitude");

    // Inside / outside.
    Expect(fx_cull::SphereIntersectsFrustum(0, 0, 10, 0.5f, pl), "centre of view is visible");
    Expect(!fx_cull::SphereIntersectsFrustum(0, 0, -10, 0.5f, pl), "behind the camera is culled");
    Expect(!fx_cull::SphereIntersectsFrustum(0, 0, 200, 0.5f, pl), "beyond far is culled");
    Expect(!fx_cull::SphereIntersectsFrustum(100, 0, 10, 0.5f, pl), "far to the right is culled");
    Expect(!fx_cull::SphereIntersectsFrustum(-100, 0, 10, 0.5f, pl), "far to the left is culled");
    Expect(!fx_cull::SphereIntersectsFrustum(0, 100, 10, 0.5f, pl), "far above is culled");
    Expect(!fx_cull::SphereIntersectsFrustum(0, -100, 10, 0.5f, pl), "far below is culled");

    // Partly inside: a big radius reaches back into view.
    // At z=10 the half-width of the frustum is about 5.77; centre at x=8.
    Expect(!fx_cull::SphereIntersectsFrustum(8, 0, 10, 0.5f, pl), "just outside with a small radius");
    Expect(fx_cull::SphereIntersectsFrustum(8, 0, 10, 4.0f, pl), "overlapping the edge with a large radius");
    // Straddling the near plane keeps the sphere.
    Expect(fx_cull::SphereIntersectsFrustum(0, 0, 0.0f, 1.0f, pl), "straddling near is kept");

    // Bad numbers never cull.
    Expect(fx_cull::SphereIntersectsFrustum(NAN, 0, 10, 1.0f, pl), "NaN centre is drawn");
    Expect(fx_cull::SphereIntersectsFrustum(0, 0, 10, INFINITY, pl), "inf radius is drawn");
    Expect(!fx_cull::SphereIntersectsFrustum(0, 0, -10, -5.0f, pl), "negative radius treated as 0");

    if (g_failed == 0)
        std::puts("fx_cull_test: ok");
    return g_failed == 0 ? 0 : 1;
}
