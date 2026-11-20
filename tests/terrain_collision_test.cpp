// terrain_collision_test.cpp
// Linux-runnable unit tests for engine/terrain/terrain_collision.{h,cpp} (ADR 0011).
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/terrain_collision_test tests/terrain_collision_test.cpp engine/terrain/terrain_collision.cpp engine/terrain/terrain_heightmap.cpp
//   /tmp/terrain_collision_test

#include "engine/terrain/terrain_collision.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace terrain;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static bool Near(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

// 33 x 33 samples over 32 x 32 m; one sample unit = one metre (heightScale 65535).
static Layout MetreLayout() {
    Layout l;
    l.sizeX = 32.0f;
    l.sizeZ = 32.0f;
    l.heightScale = 65535.0f;
    l.heightOffset = 0.0f;
    return l;
}

static Heightmap Flat(std::uint16_t h) {
    Heightmap m;
    CreateFlat(33, h, m);
    return m;
}

// Exact plane h = x + 2 z + 4 (whole metres at every sample). Normal (-1, 1, -2) / sqrt(6).
static Heightmap Ramp() {
    Heightmap m;
    CreateFlat(33, 0, m);
    for (int z = 0; z < 33; ++z)
        for (int x = 0; x < 33; ++x)
            m.Set(x, z, static_cast<std::uint16_t>(x + 2 * z + 4));
    return m;
}
static const float kRampNormalLen = std::sqrt(6.0f);

// Pseudo-random bumps (deterministic).
static Heightmap Bumpy() {
    Heightmap m;
    CreateFlat(33, 0, m);
    unsigned s = 12345u;
    for (auto& v : m.samples) {
        s = s * 1103515245u + 12345u;
        v = static_cast<std::uint16_t>(5 + (s >> 16) % 6); // 5 .. 10 m
    }
    return m;
}

static float Rand01(unsigned& s) {
    s = s * 1664525u + 1013904223u;
    return static_cast<float>(s >> 8) / static_cast<float>(1u << 24);
}

// Reference: nearest hit over every triangle of the map.
static bool BruteRaycast(const Layout& l, const Heightmap& m, const Float3& o, const Float3& d,
                         float maxDist, float& outT) {
    bool hit = false;
    float best = std::numeric_limits<float>::infinity();
    for (int cz = 0; cz < m.resolution - 1; ++cz) {
        for (int cx = 0; cx < m.resolution - 1; ++cx) {
            const Float3 a = SamplePosition(l, m, cx, cz), b = SamplePosition(l, m, cx + 1, cz);
            const Float3 c = SamplePosition(l, m, cx, cz + 1), dd = SamplePosition(l, m, cx + 1, cz + 1);
            const Float3 tris[2][3] = {{a, c, dd}, {a, dd, b}};
            for (const auto& t : tris) {
                const Float3 e1{t[1].x - t[0].x, t[1].y - t[0].y, t[1].z - t[0].z};
                const Float3 e2{t[2].x - t[0].x, t[2].y - t[0].y, t[2].z - t[0].z};
                const Float3 p{d.y * e2.z - d.z * e2.y, d.z * e2.x - d.x * e2.z, d.x * e2.y - d.y * e2.x};
                const float det = e1.x * p.x + e1.y * p.y + e1.z * p.z;
                if (std::fabs(det) < 1e-12f) continue;
                const Float3 sv{o.x - t[0].x, o.y - t[0].y, o.z - t[0].z};
                const float u = (sv.x * p.x + sv.y * p.y + sv.z * p.z) / det;
                if (u < 0.0f || u > 1.0f) continue;
                const Float3 q{sv.y * e1.z - sv.z * e1.y, sv.z * e1.x - sv.x * e1.z, sv.x * e1.y - sv.y * e1.x};
                const float v = (d.x * q.x + d.y * q.y + d.z * q.z) / det;
                if (v < 0.0f || u + v > 1.0f) continue;
                const float tt = (e2.x * q.x + e2.y * q.y + e2.z * q.z) / det;
                if (tt >= 0.0f && tt <= maxDist && tt < best) {
                    best = tt;
                    hit = true;
                }
            }
        }
    }
    outT = best;
    return hit;
}

static void TestSampleRange() {
    std::uint16_t lo = 1, hi = 1;
    SampleRange(Bumpy(), lo, hi);
    Expect(lo >= 5 && hi <= 10 && lo < hi, "range of the bumpy map");
    SampleRange(Heightmap{}, lo, hi);
    Expect(lo == 0 && hi == 0, "invalid map -> 0, 0");
}

static void TestRaycastBasics() {
    const Layout l = MetreLayout();
    const Heightmap flat = Flat(10);
    float t = 0.0f;
    Float3 n;
    Expect(Raycast(l, flat, {5.3f, 30.0f, 7.7f}, {0, -1, 0}, 100.0f, t, n), "straight down hits");
    Expect(Near(t, 20.0f) && Near(n.y, 1.0f), "distance and up normal");
    Expect(!Raycast(l, flat, {5.3f, 30.0f, 7.7f}, {0, -1, 0}, 19.0f, t, n), "max distance respected");
    Expect(!Raycast(l, flat, {5.3f, 30.0f, 7.7f}, {0, 1, 0}, 100.0f, t, n), "pointing away misses");
    Expect(!Raycast(l, flat, {-5.0f, 30.0f, 7.7f}, {0, -1, 0}, 100.0f, t, n), "outside the XZ area misses");
    Expect(Raycast(l, flat, {5.0f, 2.0f, 5.0f}, {0, 1, 0}, 100.0f, t, n), "from below hits");
    Expect(Near(t, 8.0f) && Near(n.y, -1.0f), "normal faces the ray from below");
    Expect(!Raycast(l, flat, {5.0f, 30.0f, 5.0f}, {0, 0, 0}, 100.0f, t, n), "zero direction rejected");
    Expect(!Raycast(l, Heightmap{}, {5.0f, 30.0f, 5.0f}, {0, -1, 0}, 100.0f, t, n), "invalid map rejected");
    Expect(Raycast(l, flat, 10, 10, {5.3f, 30.0f, 7.7f}, {0, -1, 0}, 100.0f, t, n) && Near(t, 20.0f),
           "overload with a cached sample range");
    Expect(Raycast(l, flat, 10, 10, {5.0f, 2.0f, 5.0f}, {0, 1, 0}, 100.0f, t, n) && Near(t, 8.0f),
           "cached range from below");

    // Ray entering the terrain box from the side, sloping down onto the flat ground.
    const float inv = 1.0f / std::sqrt(2.0f);
    Expect(Raycast(l, flat, {-10.0f, 20.0f, 16.0f}, {inv, -inv, 0}, 100.0f, t, n), "slanted ray from outside");
    Expect(Near(-10.0f + t * inv, 0.0f, 1e-2f), "lands at x = 0 (enters through the side)");

    const Heightmap ramp = Ramp();
    float h = 0.0f;
    SampleHeight(l, ramp, 12.0f, 20.0f, h);
    Expect(Raycast(l, ramp, {12.0f, 150.0f, 20.0f}, {0, -1, 0}, 200.0f, t, n) && Near(150.0f - t, h),
           "vertical ray matches SampleHeight on a slope");
    Expect(Near(n.x, -1.0f / kRampNormalLen) && Near(n.z, -2.0f / kRampNormalLen), "slope normal");
}

static void TestRaycastMatchesBruteForce() {
    const Layout l = MetreLayout();
    const Heightmap m = Bumpy();
    unsigned s = 7u;
    int hits = 0, agree = 0, rays = 400;
    for (int i = 0; i < rays; ++i) {
        const Float3 o{-8.0f + 48.0f * Rand01(s), 2.0f + 18.0f * Rand01(s), -8.0f + 48.0f * Rand01(s)};
        Float3 d{Rand01(s) - 0.5f, -Rand01(s), Rand01(s) - 0.5f};
        if (i % 5 == 0) d.y = 0.0f; // some horizontal rays
        const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (len < 1e-3f) { ++agree; continue; }
        d = {d.x / len, d.y / len, d.z / len};
        float tFast = 0.0f, tRef = 0.0f;
        Float3 n;
        const bool fast = Raycast(l, m, o, d, 80.0f, tFast, n);
        const bool ref = BruteRaycast(l, m, o, d, 80.0f, tRef);
        if (ref) ++hits;
        if (fast == ref && (!fast || Near(tFast, tRef, 2e-3f)))
            ++agree;
        else
            std::fprintf(stderr, "  ray %d: fast=%d t=%f ref=%d t=%f\n", i, fast, tFast, ref, tRef);
    }
    Expect(hits > 100, "enough random rays hit to be meaningful");
    Expect(agree == rays, "grid walk agrees with testing every triangle");
}

static void TestPointsContact() {
    const Layout l = MetreLayout();
    const Heightmap flat = Flat(10);
    const Float3 pts[3] = {{4.0f, 10.5f, 4.0f}, {5.0f, 9.8f, 5.0f}, {6.0f, 9.5f, 6.0f}};
    SurfaceContact c;
    Expect(PointsContact(l, flat, pts, 3, c), "points below the surface contact");
    Expect(Near(c.depth, 0.5f) && Near(c.point.x, 6.0f) && Near(c.normal.y, 1.0f), "deepest point wins");
    const Float3 above[2] = {{4.0f, 10.5f, 4.0f}, {40.0f, 0.0f, 4.0f}};
    Expect(!PointsContact(l, flat, above, 2, c), "points above or outside do not contact");
    Expect(!PointsContact(l, flat, nullptr, 0, c), "no points");

    // On a slope the depth is the distance to the plane, not the vertical gap.
    const Heightmap ramp = Ramp();
    float h = 0.0f;
    SampleHeight(l, ramp, 10.0f, 10.0f, h);
    const Float3 below{10.0f, h - 1.0f, 10.0f};
    Expect(PointsContact(l, ramp, &below, 1, c), "point under the slope");
    Expect(Near(c.depth, 1.0f / kRampNormalLen), "depth along the slope normal");
}

static void TestSphereContact() {
    const Layout l = MetreLayout();
    const Heightmap flat = Flat(10);
    SurfaceContact c;
    Expect(SphereContact(l, flat, {8.3f, 10.4f, 9.1f}, 0.5f, c), "resting sphere overlaps");
    Expect(Near(c.depth, 0.1f) && Near(c.normal.y, 1.0f) && Near(c.point.y, 10.0f), "depth / normal / point");
    Expect(!SphereContact(l, flat, {8.3f, 10.6f, 9.1f}, 0.5f, c), "sphere just above: no contact");
    Expect(SphereContact(l, flat, {8.3f, 9.0f, 9.1f}, 0.5f, c), "centre below the surface");
    Expect(Near(c.depth, 1.5f) && Near(c.normal.y, 1.0f), "pushed out along the normal");
    Expect(!SphereContact(l, flat, {-3.0f, 10.0f, 9.1f}, 0.5f, c), "outside the terrain");
    Expect(SphereContact(l, flat, {-0.3f, 10.2f, 9.1f}, 0.5f, c), "overhanging the edge still touches");
    Expect(!SphereContact(l, flat, {8.0f, 10.0f, 8.0f}, 0.0f, c), "zero radius rejected");

    // Sphere on the slope: depth = radius - distance to the plane.
    const Heightmap ramp = Ramp();
    float h = 0.0f;
    SampleHeight(l, ramp, 16.0f, 16.0f, h);
    const float nlen = kRampNormalLen;
    const Float3 n{-1.0f / nlen, 1.0f / nlen, -2.0f / nlen};
    const float gap = 0.4f; // centre 0.4 m from the plane, radius 0.5
    const Float3 center{16.0f + n.x * gap, h + n.y * gap, 16.0f + n.z * gap};
    Expect(SphereContact(l, ramp, center, 0.5f, c), "sphere on the slope");
    Expect(Near(c.depth, 0.1f) && Near(c.normal.x, n.x) && Near(c.normal.y, n.y), "slope contact");

    // A sharp spike under a sphere: closest point is the spike tip.
    Heightmap spike = Flat(10);
    spike.Set(16, 16, 12);
    Expect(SphereContact(l, spike, {16.0f, 12.3f, 16.0f}, 0.5f, c), "sphere on a spike tip");
    Expect(Near(c.depth, 0.2f) && Near(c.normal.y, 1.0f), "tip contact pushes straight up");
}

static void TestCapsuleContact() {
    const Layout l = MetreLayout();
    const Heightmap flat = Flat(10);
    SurfaceContact c;
    Expect(CapsuleContact(l, flat, {8.0f, 10.4f, 8.0f}, {8.0f, 12.4f, 8.0f}, 0.5f, c), "standing capsule");
    Expect(Near(c.depth, 0.1f) && Near(c.normal.y, 1.0f), "bottom sphere decides");
    Expect(!CapsuleContact(l, flat, {8.0f, 10.6f, 8.0f}, {8.0f, 12.6f, 8.0f}, 0.5f, c), "lifted capsule");
    Expect(CapsuleContact(l, flat, {4.0f, 10.3f, 8.0f}, {12.0f, 10.3f, 8.0f}, 0.5f, c), "lying capsule");
    Expect(Near(c.depth, 0.2f), "lying capsule depth");
    Expect(!CapsuleContact(l, flat, {4.0f, 10.3f, 8.0f}, {12.0f, 10.3f, 8.0f}, 0.0f, c), "zero radius rejected");
}

int main() {
    TestSampleRange();
    TestRaycastBasics();
    TestRaycastMatchesBruteForce();
    TestPointsContact();
    TestSphereContact();
    TestCapsuleContact();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("terrain_collision_test: all checks passed\n");
    return 0;
}
