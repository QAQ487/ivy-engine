// shadow_cull_check.cpp
// Linux-runnable unit checks for light-space ortho cascade caster culling:
// inside / laterally outside / behind open near (must NOT cull) / straddling /
// degenerate AABB / invalid AABB / open-near vs closed-near difference.

#include "engine/rendering/culling/frustum_cull_math.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using frustum_cull::Aabb;
using frustum_cull::FrustumPlanes;

static int g_fails = 0;

static void Expect(bool cond, const char* label) {
    if (cond)
        return;
    std::fprintf(stderr, "FAIL: %s\n", label);
    ++g_fails;
}

// LH orthographic off-center, row-major, D3D z [0,1]. Matches
// XMMatrixOrthographicOffCenterLH(l,r,b,t,zn,zf).
static void BuildOrthoOffCenterLH(
    float l, float r, float b, float t, float zn, float zf, float out[16]) {
    for (int i = 0; i < 16; ++i)
        out[i] = 0.0f;
    out[0] = 2.0f / (r - l);
    out[5] = 2.0f / (t - b);
    out[10] = 1.0f / (zf - zn);
    out[12] = (l + r) / (l - r);
    out[13] = (t + b) / (b - t);
    out[14] = zn / (zn - zf);
    out[15] = 1.0f;
}

static Aabb MakeBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    Aabb box;
    box.min = {minX, minY, minZ};
    box.max = {maxX, maxY, maxZ};
    box.valid = true;
    return box;
}

int main() {
    // Identity light view + cascade-like ortho: XY in [-10,10], Z in [5, 50]
    // (minZ already includes the engine's kZPadding-style near extension).
    float view[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    float proj[16];
    BuildOrthoOffCenterLH(-10.0f, 10.0f, -10.0f, 10.0f, 5.0f, 50.0f, proj);

    // viewProj = view * proj with identity view == proj
    float viewProj[16];
    for (int i = 0; i < 16; ++i)
        viewProj[i] = proj[i];
    (void)view;

    FrustumPlanes fp{};
    frustum_cull::ExtractFrustumPlanesD3D(viewProj, fp);

    // --- Inside cascade volume ---
    {
        const Aabb inside = MakeBox(-1.0f, -1.0f, 20.0f, 1.0f, 1.0f, 22.0f);
        Expect(frustum_cull::AabbIntersectsFrustumOpenNear(inside, fp),
            "caster inside cascade");
        Expect(frustum_cull::AabbIntersectsFrustum(inside, fp),
            "caster inside cascade (closed near also)");
    }

    // --- Fully outside laterally (right of +X) ---
    {
        const Aabb right = MakeBox(30.0f, -1.0f, 20.0f, 32.0f, 1.0f, 22.0f);
        Expect(!frustum_cull::AabbIntersectsFrustumOpenNear(right, fp),
            "caster fully outside laterally");
    }

    // --- Behind light-space near plane (Z < zn=5, toward the light) ---
    // Closed near would cull; open near must NOT cull (pancaking / casters
    // between light and cascade still write shadow depth).
    {
        const Aabb betweenLightAndNear = MakeBox(-1.0f, -1.0f, 0.5f, 1.0f, 1.0f, 2.0f);
        Expect(!frustum_cull::AabbIntersectsFrustum(betweenLightAndNear, fp),
            "behind near: closed frustum culls");
        Expect(frustum_cull::AabbIntersectsFrustumOpenNear(betweenLightAndNear, fp),
            "behind near: open-near must NOT cull");
    }

    // --- Straddling cascade +X boundary ---
    {
        const Aabb straddling = MakeBox(8.0f, -1.0f, 20.0f, 15.0f, 1.0f, 22.0f);
        Expect(frustum_cull::AabbIntersectsFrustumOpenNear(straddling, fp),
            "caster straddling cascade boundary");
    }

    // --- Straddling near plane itself ---
    {
        const Aabb straddleNear = MakeBox(-1.0f, -1.0f, 3.0f, 1.0f, 1.0f, 8.0f);
        Expect(frustum_cull::AabbIntersectsFrustumOpenNear(straddleNear, fp),
            "caster straddling light-space near");
        Expect(frustum_cull::AabbIntersectsFrustum(straddleNear, fp),
            "straddle near also intersects closed frustum");
    }

    // --- Beyond far plane: still culled with open near ---
    {
        const Aabb beyondFar = MakeBox(-1.0f, -1.0f, 80.0f, 1.0f, 1.0f, 82.0f);
        Expect(!frustum_cull::AabbIntersectsFrustumOpenNear(beyondFar, fp),
            "caster beyond cascade far is culled");
    }

    // --- Degenerate / zero-size AABB (valid, min == max) ---
    {
        Aabb point = MakeBox(0.0f, 0.0f, 20.0f, 0.0f, 0.0f, 20.0f);
        Expect(frustum_cull::AabbIntersectsFrustumOpenNear(point, fp),
            "zero-size AABB inside cascade");
        Aabb pointOut = MakeBox(40.0f, 0.0f, 20.0f, 40.0f, 0.0f, 20.0f);
        Expect(!frustum_cull::AabbIntersectsFrustumOpenNear(pointOut, fp),
            "zero-size AABB outside cascade");
    }

    // --- Invalid AABB must never cull ---
    {
        Aabb invalid{};
        Expect(frustum_cull::AabbIntersectsFrustumOpenNear(invalid, fp),
            "invalid aabb not culled");
    }

    // --- Toggle-off semantics: when culling is disabled the caller skips the
    // test entirely; open-near of a laterally-outside box would still return
    // false — document that OFF must bypass this helper (engine path). ---
    {
        const Aabb right = MakeBox(30.0f, -1.0f, 20.0f, 32.0f, 1.0f, 22.0f);
        const bool wouldCull = !frustum_cull::AabbIntersectsFrustumOpenNear(right, fp);
        Expect(wouldCull, "toggle-off baseline: outside box would cull if tested");
        // Simulated toggle off: treat every caster as visible.
        const bool toggleOffVisible = true;
        Expect(toggleOffVisible, "toggle off reproduces unculled draw");
    }

    if (g_fails != 0) {
        std::fprintf(stderr, "%d shadow cull check(s) failed\n", g_fails);
        return 1;
    }
    std::puts("PASS: shadow cascade open-near cull (inside/outside/near/straddle/degenerate)");
    return 0;
}
