// frustum_cull_check.cpp
// Linux-runnable unit checks for frustum plane extraction, AABB transform,
// and AABB-vs-frustum (inside / outside / intersecting / behind / near-far).

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

static void ExpectNear(float a, float b, float eps, const char* label) {
    if (std::fabs(a - b) <= eps)
        return;
    std::fprintf(stderr, "FAIL: %s (got %g expected %g)\n", label, a, b);
    ++g_fails;
}

// LH perspective: fovY degrees, aspect, zn, zf. Row-major, D3D z [0,1].
static void BuildPerspectiveLH(float fovYDeg, float aspect, float zn, float zf, float out[16]) {
    const float f = 1.0f / std::tan(fovYDeg * 0.017453292519943295f * 0.5f);
    for (int i = 0; i < 16; ++i)
        out[i] = 0.0f;
    out[0] = f / aspect;
    out[5] = f;
    out[10] = zf / (zf - zn);
    out[11] = 1.0f;
    out[14] = (-zn * zf) / (zf - zn);
}

// Look-at LH from eye looking toward +Z with up +Y, at origin offset.
static void BuildLookAtIdentity(float eyeZ, float out[16]) {
    // Eye at (0,0,-eyeZ) looking at origin → view translates by +eyeZ on Z.
    // Simplified: identity rotation, translation (0,0,eyeZ) in view space.
    for (int i = 0; i < 16; ++i)
        out[i] = 0.0f;
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[14] = eyeZ; // view = T(0,0,+eyeZ) when eye is at (0,0,-eyeZ)
    out[15] = 1.0f;
}

static void MulMatrix(const float a[16], const float b[16], float out[16]) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[r * 4 + c] =
                a[r * 4 + 0] * b[0 * 4 + c] +
                a[r * 4 + 1] * b[1 * 4 + c] +
                a[r * 4 + 2] * b[2 * 4 + c] +
                a[r * 4 + 3] * b[3 * 4 + c];
        }
    }
}

static Aabb UnitBox() {
    Aabb box;
    box.min = {-0.5f, -0.5f, -0.5f};
    box.max = {0.5f, 0.5f, 0.5f};
    box.valid = true;
    return box;
}

int main() {
    // --- Plane extraction: identity VP should give unit clip frustum ---
    {
        float identity[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        FrustumPlanes fp{};
        frustum_cull::ExtractFrustumPlanesD3D(identity, fp);
        // Left plane of identity: (1,0,0,1) normalized → normal length 1
        ExpectNear(fp.planes[0].x, 1.0f, 1e-5f, "identity left.nx");
        ExpectNear(fp.planes[0].w, 1.0f, 1e-5f, "identity left.d");
        ExpectNear(fp.planes[1].x, -1.0f, 1e-5f, "identity right.nx");
        ExpectNear(fp.planes[4].z, 1.0f, 1e-5f, "identity near.nz (D3D z-row)");
        ExpectNear(fp.planes[5].z, -1.0f, 1e-5f, "identity far.nz");
    }

    // --- AABB transform with scale / rotation / translation ---
    {
        const Aabb local = UnitBox();
        const Aabb world = frustum_cull::TransformAabbTRS(
            local, {10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f});
        Expect(world.valid, "scaled translated aabb valid");
        ExpectNear(world.min.x, 9.0f, 1e-4f, "scaled aabb min.x");
        ExpectNear(world.max.x, 11.0f, 1e-4f, "scaled aabb max.x");
        ExpectNear(world.min.y, -0.5f, 1e-4f, "scaled aabb min.y");
        ExpectNear(world.max.y, 0.5f, 1e-4f, "scaled aabb max.y");

        // 90° yaw: local X extent becomes world Z extent (conservative AABB).
        const Aabb spun = frustum_cull::TransformAabbTRS(
            local, {0.0f, 0.0f, 0.0f}, {0.0f, 90.0f, 0.0f}, {2.0f, 1.0f, 1.0f});
        Expect(spun.valid, "rotated aabb valid");
        // After 90° Y: scale(2,1,1) then rotate → X maps toward -Z / Z toward X.
        const float xExtent = spun.max.x - spun.min.x;
        const float zExtent = spun.max.z - spun.min.z;
        Expect(xExtent > 0.9f && xExtent < 1.1f, "rotated aabb x extent ~1");
        Expect(zExtent > 1.9f && zExtent < 2.1f, "rotated aabb z extent ~2");
    }

    // --- Frustum tests with a camera at origin looking +Z ---
    {
        float view[16], proj[16], viewProj[16];
        BuildLookAtIdentity(0.0f, view); // eye at origin looking +Z (identity view)
        // Actually identity view with eye at origin: use pure identity view.
        for (int i = 0; i < 16; ++i)
            view[i] = 0.0f;
        view[0] = view[5] = view[10] = view[15] = 1.0f;

        BuildPerspectiveLH(90.0f, 1.0f, 1.0f, 100.0f, proj);
        MulMatrix(view, proj, viewProj);

        FrustumPlanes fp{};
        frustum_cull::ExtractFrustumPlanesD3D(viewProj, fp);

        // Inside: unit box centered at (0,0,10)
        Aabb inside = UnitBox();
        inside.min = {-0.5f, -0.5f, 9.5f};
        inside.max = {0.5f, 0.5f, 10.5f};
        inside.valid = true;
        Expect(frustum_cull::AabbIntersectsFrustum(inside, fp), "inside frustum");

        // Outside to the right
        Aabb right = UnitBox();
        right.min = {50.0f, -0.5f, 9.5f};
        right.max = {51.0f, 0.5f, 10.5f};
        right.valid = true;
        Expect(!frustum_cull::AabbIntersectsFrustum(right, fp), "outside right");

        // Outside to the left
        Aabb left = UnitBox();
        left.min = {-51.0f, -0.5f, 9.5f};
        left.max = {-50.0f, 0.5f, 10.5f};
        left.valid = true;
        Expect(!frustum_cull::AabbIntersectsFrustum(left, fp), "outside left");

        // Intersecting: straddles the right edge (large box)
        Aabb straddling{};
        straddling.min = {-1.0f, -1.0f, 5.0f};
        straddling.max = {80.0f, 1.0f, 15.0f};
        straddling.valid = true;
        Expect(frustum_cull::AabbIntersectsFrustum(straddling, fp), "intersecting right edge");

        // Behind camera (negative Z in LH view looking +Z)
        Aabb behind{};
        behind.min = {-0.5f, -0.5f, -10.5f};
        behind.max = {0.5f, 0.5f, -9.5f};
        behind.valid = true;
        Expect(!frustum_cull::AabbIntersectsFrustum(behind, fp), "behind camera");

        // In front of near plane but clipped by near (z < zn=1)
        Aabb nearClip{};
        nearClip.min = {-0.1f, -0.1f, 0.1f};
        nearClip.max = {0.1f, 0.1f, 0.5f};
        nearClip.valid = true;
        Expect(!frustum_cull::AabbIntersectsFrustum(nearClip, fp), "in front of near plane");

        // Beyond far plane (zf=100)
        Aabb farClip{};
        farClip.min = {-0.5f, -0.5f, 150.0f};
        farClip.max = {0.5f, 0.5f, 151.0f};
        farClip.valid = true;
        Expect(!frustum_cull::AabbIntersectsFrustum(farClip, fp), "beyond far plane");

        // Invalid bounds must never cull
        Aabb invalid{};
        Expect(frustum_cull::AabbIntersectsFrustum(invalid, fp), "invalid aabb not culled");
    }

    if (g_fails != 0) {
        std::fprintf(stderr, "%d frustum cull check(s) failed\n", g_fails);
        return 1;
    }
    std::puts("PASS: frustum plane extract, AABB transform, AABB-vs-frustum");
    return 0;
}
