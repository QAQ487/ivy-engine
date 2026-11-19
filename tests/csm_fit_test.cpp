// Cascade fitting math (engine/rendering/lighting/csm_fit.h): rotation-invariant slice
// spheres and world-anchored texel snapping.
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/csm_fit_test tests/csm_fit_test.cpp

#include "engine/rendering/lighting/csm_fit.h"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace csm_fit;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static bool Near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

// The 8 corners of the slice, built the same way as shadow.cpp BuildFrustumCorners.
static void SliceCorners(Vec3 eye, Vec3 forward, Vec3 upIn, float n, float f, float tanHalf,
                         float aspect, Vec3 out[8]) {
    forward = Normalize(forward, Vec3{ 0, 0, 1 });
    Vec3 right = Normalize(Cross(upIn, forward), Vec3{ 1, 0, 0 });
    Vec3 up = Normalize(Cross(forward, right), Vec3{ 0, 1, 0 });
    right = Cross(up, forward);
    const float d[2] = { n, f };
    int k = 0;
    for (float dist : d) {
        const float h = tanHalf * dist, w = h * aspect;
        const Vec3 c = Add(eye, Scale(forward, dist));
        out[k++] = Add(c, Sub(Scale(up, h), Scale(right, w)));
        out[k++] = Add(c, Add(Scale(up, h), Scale(right, w)));
        out[k++] = Sub(c, Add(Scale(up, h), Scale(right, w)));
        out[k++] = Sub(c, Sub(Scale(up, h), Scale(right, w)));
    }
}

static float MaxCornerDistance(Vec3 center, const Vec3 c[8]) {
    float m = 0.0f;
    for (int i = 0; i < 8; ++i) {
        const float d = Length(Sub(c[i], center));
        if (d > m) m = d;
    }
    return m;
}

static bool IsWholeTexels(float v, float texel) {
    const float t = v / texel;
    return std::fabs(t - std::round(t)) < 1e-2f;
}

int main() {
    const float tanHalf = std::tan(60.0f * 3.14159265f / 360.0f);  // 60 deg vertical FOV
    const float aspect = 16.0f / 9.0f;
    const Vec3 worldUp{ 0, 1, 0 };

    // The sphere contains every corner of the slice, for narrow and wide slices.
    const float slices[][2] = { { 0.1f, 6.0f }, { 6.0f, 18.0f }, { 18.0f, 40.0f }, { 40.0f, 70.0f },
                                { 10.0f, 10.5f } };
    for (const auto& s : slices) {
        const SliceSphere sp = FitSliceSphere(s[0], s[1], tanHalf, aspect);
        Vec3 corners[8];
        SliceCorners(Vec3{ 0, 0, 0 }, Vec3{ 0, 0, 1 }, worldUp, s[0], s[1], tanHalf, aspect, corners);
        const Vec3 center{ 0, 0, sp.centerDist };
        Expect(MaxCornerDistance(center, corners) <= sp.radius + 1e-3f, "sphere contains the slice corners");
        Expect(sp.centerDist >= s[0] - 1e-4f && sp.centerDist <= s[1] + 1e-4f, "centre lies inside the slice");
        Expect(sp.radius == std::ceil(sp.radius / kRadiusStep) * kRadiusStep, "radius is quantised");
    }
    // A wide, shallow slice clamps the centre to the far plane.
    {
        const SliceSphere sp = FitSliceSphere(10.0f, 10.5f, std::tan(1.2f), aspect);
        Expect(sp.centerDist == 10.5f, "wide shallow slice: centre at the far plane");
    }
    // Bad input falls back to a small sphere instead of NaN.
    {
        const SliceSphere a = FitSliceSphere(5.0f, 5.0f, tanHalf, aspect);
        const SliceSphere b = FitSliceSphere(std::numeric_limits<float>::quiet_NaN(), 9.0f, tanHalf, aspect);
        Expect(a.radius == kMinRadius && std::isfinite(a.centerDist), "empty slice: minimum sphere");
        Expect(b.radius == kMinRadius && std::isfinite(b.centerDist), "NaN slice: minimum sphere");
    }

    // Rotation: turning the camera (any yaw / pitch) does not change the cascade radius, and
    // the sphere still covers the rotated slice.
    {
        const SliceSphere sp = FitSliceSphere(6.0f, 18.0f, tanHalf, aspect);
        const Vec3 eye{ 3.0f, 2.0f, -7.0f };
        for (int yaw = 0; yaw < 360; yaw += 17) {
            for (int pitch = -80; pitch <= 80; pitch += 20) {
                const float cy = std::cos(yaw * 0.01745329f), sy = std::sin(yaw * 0.01745329f);
                const float cp = std::cos(pitch * 0.01745329f), spi = std::sin(pitch * 0.01745329f);
                const Vec3 fwd{ sy * cp, spi, cy * cp };
                Vec3 corners[8];
                SliceCorners(eye, fwd, worldUp, 6.0f, 18.0f, tanHalf, aspect, corners);
                const Vec3 center = Add(eye, Scale(fwd, sp.centerDist));
                const SliceSphere again = FitSliceSphere(6.0f, 18.0f, tanHalf, aspect);
                Expect(again.radius == sp.radius, "radius independent of camera rotation");
                Expect(MaxCornerDistance(center, corners) <= sp.radius + 1e-3f,
                    "rotated slice still inside the sphere");
            }
        }
    }

    // Light basis is orthonormal and follows the up rule.
    {
        const LightBasis b = MakeLightBasis(Vec3{ -0.7f, -0.45f, 0.55f });
        Expect(Near(Length(b.right), 1, 1e-4f) && Near(Length(b.up), 1, 1e-4f) && Near(Length(b.forward), 1, 1e-4f),
            "basis vectors are unit length");
        Expect(Near(Dot(b.right, b.up), 0, 1e-4f) && Near(Dot(b.right, b.forward), 0, 1e-4f)
            && Near(Dot(b.up, b.forward), 0, 1e-4f), "basis is orthogonal");
        Expect(b.up.y > 0.0f, "slanted light keeps world-up as image up");
        const LightBasis v = MakeLightBasis(Vec3{ 0.0f, -1.0f, 0.0f });
        Expect(Near(Length(v.right), 1, 1e-4f) && std::isfinite(v.up.x), "straight-down light uses the +Z fallback");
        const LightBasis z = MakeLightBasis(Vec3{ 0, 0, 0 });
        Expect(Near(z.forward.y, -1, 1e-6f), "zero light direction falls back to straight down");
    }

    // Translation: as the camera slides, the snapped centre's light-space X/Y stay whole
    // texels from the world origin, so successive positions differ by whole texels.
    {
        const LightBasis b = MakeLightBasis(Vec3{ -0.7f, -0.45f, 0.55f });
        const SliceSphere sp = FitSliceSphere(6.0f, 18.0f, tanHalf, aspect);
        const float half = CascadeHalfExtent(sp.radius, 2048);
        const float texel = TexelWorldSize(half, 2048);
        Expect(Near(half - texel, sp.radius, 1e-4f), "half extent = radius + one texel");
        const Vec3 fwd = Normalize(Vec3{ 0.3f, -0.2f, 1.0f }, Vec3{ 0, 0, 1 });
        Vec3 prev{};
        bool havePrev = false;
        for (int step = 0; step < 200; ++step) {
            const Vec3 eye{ 0.0137f * step, 1.7f, 0.0291f * step - 5.0f };
            const Vec3 center = Add(eye, Scale(fwd, sp.centerDist));
            const Vec3 snapped = SnapCenterToTexels(center, b, texel);
            Expect(IsWholeTexels(Dot(snapped, b.right), texel), "snapped X is whole texels");
            Expect(IsWholeTexels(Dot(snapped, b.up), texel), "snapped Y is whole texels");
            Expect(Near(Dot(snapped, b.forward), Dot(center, b.forward), 1e-3f), "depth along the light kept");
            Expect(std::fabs(Dot(Sub(snapped, center), b.right)) <= texel + 1e-5f
                && std::fabs(Dot(Sub(snapped, center), b.up)) <= texel + 1e-5f, "snap moves at most one texel");
            if (havePrev) {
                const Vec3 d = Sub(snapped, prev);
                Expect(IsWholeTexels(Dot(d, b.right), texel) && IsWholeTexels(Dot(d, b.up), texel),
                    "frame-to-frame offset is whole texels");
            }
            prev = snapped;
            havePrev = true;
        }
        // After snapping (<= one texel per axis) the sphere still fits inside the ortho square.
        {
            Vec3 corners[8];
            const Vec3 eye{ 1.234f, 1.7f, -3.21f };
            SliceCorners(eye, fwd, worldUp, 6.0f, 18.0f, tanHalf, aspect, corners);
            const Vec3 snapped = SnapCenterToTexels(Add(eye, Scale(fwd, sp.centerDist)), b, texel);
            bool inside = true;
            for (const Vec3& c : corners) {
                const Vec3 d = Sub(c, snapped);
                if (std::fabs(Dot(d, b.right)) > half + 1e-4f || std::fabs(Dot(d, b.up)) > half + 1e-4f)
                    inside = false;
            }
            Expect(inside, "slice corners stay inside the snapped cascade square");
        }
        Expect(CascadeHalfExtent(1.0f, 2) == 2.0f, "tiny map: generous fallback");
        Expect(TexelWorldSize(sp.radius, 0) == sp.radius * 2.0f, "map size 0 treated as 1");
        const Vec3 c{ 1, 2, 3 };
        const Vec3 same = SnapCenterToTexels(c, b, 0.0f);
        Expect(same.x == c.x && same.y == c.y && same.z == c.z, "zero texel size leaves the centre alone");
    }

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("PASS: CSM cascade fit (rotation-invariant sphere, world-anchored texel snap)\n");
    return 0;
}
