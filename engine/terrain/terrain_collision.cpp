// terrain_collision.cpp
// See terrain_collision.h (ADR 0011).

#include "engine/terrain/terrain_collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace terrain {

namespace {

Float3 Sub(const Float3& a, const Float3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Float3 Add(const Float3& a, const Float3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Float3 Scale(const Float3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float Dot(const Float3& a, const Float3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Float3 Cross(const Float3& a, const Float3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float Length(const Float3& a) { return std::sqrt(Dot(a, a)); }
Float3 Normalized(const Float3& a) {
    const float len = Length(a);
    return len > 0.0f ? Scale(a, 1.0f / len) : Float3{0.0f, 1.0f, 0.0f};
}
bool Finite(const Float3& a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }

// The two triangles of cell (cx, cz), same split as SampleHeight / the chunk mesh.
struct CellTriangles {
    Float3 tri[2][3];
};

CellTriangles Triangles(const Layout& layout, const Heightmap& map, int cx, int cz) {
    const Float3 a = SamplePosition(layout, map, cx, cz);
    const Float3 b = SamplePosition(layout, map, cx + 1, cz);
    const Float3 c = SamplePosition(layout, map, cx, cz + 1);
    const Float3 d = SamplePosition(layout, map, cx + 1, cz + 1);
    return CellTriangles{{{a, c, d}, {a, d, b}}};
}

// Up-facing unit normal of a surface triangle (both triangles of a cell wind so that
// cross(v1 - v0, v2 - v0) points up).
Float3 TriangleNormal(const Float3 t[3]) {
    return Normalized(Cross(Sub(t[1], t[0]), Sub(t[2], t[0])));
}

// Ericson, Real-Time Collision Detection 5.1.5.
Float3 ClosestPointOnTriangle(const Float3& p, const Float3& a, const Float3& b, const Float3& c) {
    const Float3 ab = Sub(b, a), ac = Sub(c, a), ap = Sub(p, a);
    const float d1 = Dot(ab, ap), d2 = Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    const Float3 bp = Sub(p, b);
    const float d3 = Dot(ab, bp), d4 = Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return Add(a, Scale(ab, d1 / (d1 - d3)));
    const Float3 cp = Sub(p, c);
    const float d5 = Dot(ab, cp), d6 = Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return Add(a, Scale(ac, d2 / (d2 - d6)));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return Add(b, Scale(Sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    const float denom = 1.0f / (va + vb + vc);
    return Add(a, Add(Scale(ab, vb * denom), Scale(ac, vc * denom)));
}

// Moller-Trumbore, two-sided. t >= 0 only.
bool RayTriangle(const Float3& o, const Float3& d, const Float3 t[3], float& outT) {
    const Float3 e1 = Sub(t[1], t[0]), e2 = Sub(t[2], t[0]);
    const Float3 p = Cross(d, e2);
    const float det = Dot(e1, p);
    if (std::fabs(det) < 1e-12f)
        return false;
    const float inv = 1.0f / det;
    const Float3 s = Sub(o, t[0]);
    const float u = Dot(s, p) * inv;
    if (u < -1e-6f || u > 1.0f + 1e-6f)
        return false;
    const Float3 q = Cross(s, e1);
    const float v = Dot(d, q) * inv;
    if (v < -1e-6f || u + v > 1.0f + 1e-6f)
        return false;
    const float tt = Dot(e2, q) * inv;
    if (tt < 0.0f)
        return false;
    outT = tt;
    return true;
}

// Inclusive cell range covered by [lo, hi] along one axis; false when outside the map.
bool CellRange(float lo, float hi, float cell, int cells, int& outFirst, int& outLast) {
    if (!(cell > 0.0f) || hi < 0.0f || lo > cell * static_cast<float>(cells))
        return false;
    outFirst = std::clamp(static_cast<int>(std::floor(lo / cell)), 0, cells - 1);
    outLast = std::clamp(static_cast<int>(std::floor(hi / cell)), 0, cells - 1);
    return true;
}

} // namespace

void SampleRange(const Heightmap& map, std::uint16_t& outLow, std::uint16_t& outHigh) {
    outLow = 0;
    outHigh = 0;
    if (!map.IsValid())
        return;
    const auto mm = std::minmax_element(map.samples.begin(), map.samples.end());
    outLow = *mm.first;
    outHigh = *mm.second;
}

bool Raycast(const Layout& layout, const Heightmap& map, const Float3& origin, const Float3& dir,
             float maxDistance, float& outT, Float3& outNormal) {
    std::uint16_t lo, hi;
    SampleRange(map, lo, hi);
    return Raycast(layout, map, lo, hi, origin, dir, maxDistance, outT, outNormal);
}

bool Raycast(const Layout& layout, const Heightmap& map, std::uint16_t lo, std::uint16_t hi,
             const Float3& origin, const Float3& dir, float maxDistance, float& outT, Float3& outNormal) {
    if (!map.IsValid() || !Finite(origin) || !Finite(dir) || !(maxDistance >= 0.0f))
        return false;
    if (Dot(dir, dir) < 1e-12f)
        return false;
    if (lo > hi)
        std::swap(lo, hi);

    const float boxMin[3] = {0.0f, SampleToHeight(layout, lo) - 1e-3f, 0.0f};
    const float boxMax[3] = {layout.sizeX, SampleToHeight(layout, hi) + 1e-3f, layout.sizeZ};
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};

    // Clip the ray to the terrain's box (slab test).
    float t0 = 0.0f, t1 = maxDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(d[axis]) < 1e-12f) {
            if (o[axis] < boxMin[axis] || o[axis] > boxMax[axis])
                return false;
            continue;
        }
        float ta = (boxMin[axis] - o[axis]) / d[axis];
        float tb = (boxMax[axis] - o[axis]) / d[axis];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1)
            return false;
    }

    // Walk the cells the ray's XZ projection crosses (Amanatides-Woo). Triangles of a
    // cell lie inside its XZ footprint, so the first cell with a hit holds the nearest.
    const int cells = map.resolution - 1;
    const float cellX = CellSizeX(layout, map);
    const float cellZ = CellSizeZ(layout, map);
    const Float3 start = Add(origin, Scale(dir, t0));
    int ix = std::clamp(static_cast<int>(std::floor(start.x / cellX)), 0, cells - 1);
    int iz = std::clamp(static_cast<int>(std::floor(start.z / cellZ)), 0, cells - 1);
    const int stepX = dir.x > 0.0f ? 1 : (dir.x < 0.0f ? -1 : 0);
    const int stepZ = dir.z > 0.0f ? 1 : (dir.z < 0.0f ? -1 : 0);
    const float inf = std::numeric_limits<float>::infinity();
    float tMaxX = inf, tMaxZ = inf, tDeltaX = inf, tDeltaZ = inf;
    if (stepX != 0) {
        const float boundary = static_cast<float>(ix + (stepX > 0 ? 1 : 0)) * cellX;
        tMaxX = (boundary - origin.x) / dir.x;
        tDeltaX = cellX / std::fabs(dir.x);
    }
    if (stepZ != 0) {
        const float boundary = static_cast<float>(iz + (stepZ > 0 ? 1 : 0)) * cellZ;
        tMaxZ = (boundary - origin.z) / dir.z;
        tDeltaZ = cellZ / std::fabs(dir.z);
    }

    const int maxSteps = 2 * (cells + 1) + 2;
    for (int step = 0; step < maxSteps; ++step) {
        const CellTriangles ct = Triangles(layout, map, ix, iz);
        float best = inf;
        int bestTri = -1;
        for (int k = 0; k < 2; ++k) {
            float t;
            if (RayTriangle(origin, dir, ct.tri[k], t) && t < best) {
                best = t;
                bestTri = k;
            }
        }
        if (bestTri >= 0 && best <= maxDistance) {
            outT = best;
            Float3 n = TriangleNormal(ct.tri[bestTri]);
            if (Dot(n, dir) > 0.0f)
                n = Scale(n, -1.0f);
            outNormal = n;
            return true;
        }
        const float next = std::min(tMaxX, tMaxZ);
        if (next > t1)
            break;
        if (tMaxX < tMaxZ) {
            ix += stepX;
            tMaxX += tDeltaX;
        } else {
            iz += stepZ;
            tMaxZ += tDeltaZ;
        }
        if (ix < 0 || iz < 0 || ix >= cells || iz >= cells)
            break;
    }
    return false;
}

bool PointsContact(const Layout& layout, const Heightmap& map, const Float3* points, int count,
                   SurfaceContact& out) {
    if (!map.IsValid() || !points || count <= 0)
        return false;
    bool hit = false;
    for (int i = 0; i < count; ++i) {
        const Float3& p = points[i];
        float h;
        Float3 n;
        if (!SampleHeight(layout, map, p.x, p.z, h) || !SampleSurfaceNormal(layout, map, p.x, p.z, n))
            continue;
        if (!(p.y < h))
            continue;
        const float depth = (h - p.y) * n.y;
        if (!hit || depth > out.depth) {
            out.point = p;
            out.normal = n;
            out.depth = depth;
            hit = true;
        }
    }
    return hit;
}

bool SphereContact(const Layout& layout, const Heightmap& map, const Float3& center, float radius,
                   SurfaceContact& out) {
    if (!map.IsValid() || !Finite(center) || !(radius > 0.0f))
        return false;

    // Centre below the surface: push out along the surface normal there.
    float hc;
    Float3 nc;
    if (SampleHeight(layout, map, center.x, center.z, hc) &&
        SampleSurfaceNormal(layout, map, center.x, center.z, nc) && center.y < hc) {
        out.normal = nc;
        out.depth = radius + (hc - center.y) * nc.y;
        out.point = {center.x, hc, center.z};
        return true;
    }

    const int cells = map.resolution - 1;
    int x0, x1, z0, z1;
    if (!CellRange(center.x - radius, center.x + radius, CellSizeX(layout, map), cells, x0, x1) ||
        !CellRange(center.z - radius, center.z + radius, CellSizeZ(layout, map), cells, z0, z1))
        return false;

    bool hit = false;
    for (int cz = z0; cz <= z1; ++cz) {
        for (int cx = x0; cx <= x1; ++cx) {
            const CellTriangles ct = Triangles(layout, map, cx, cz);
            for (const auto& tri : ct.tri) {
                const Float3 q = ClosestPointOnTriangle(center, tri[0], tri[1], tri[2]);
                const Float3 delta = Sub(center, q);
                const float dist = Length(delta);
                if (dist >= radius)
                    continue;
                const Float3 triN = TriangleNormal(tri);
                // Only the top side pushes; a centre under this triangle's plane but above
                // the surface at its own XZ sits next to a ridge and is caught by the
                // neighbouring triangles.
                if (Dot(delta, triN) < 0.0f)
                    continue;
                const float depth = radius - dist;
                if (!hit || depth > out.depth) {
                    out.point = q;
                    out.normal = dist > 1e-6f ? Scale(delta, 1.0f / dist) : triN;
                    out.depth = depth;
                    hit = true;
                }
            }
        }
    }
    return hit;
}

bool CapsuleContact(const Layout& layout, const Heightmap& map, const Float3& a, const Float3& b,
                    float radius, SurfaceContact& out) {
    if (!(radius > 0.0f) || !Finite(a) || !Finite(b))
        return false;
    const float len = Length(Sub(b, a));
    const int spheres = std::min(64, std::max(2, static_cast<int>(std::ceil(len / radius)) + 1));
    bool hit = false;
    for (int i = 0; i < spheres; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(spheres - 1);
        SurfaceContact c;
        if (SphereContact(layout, map, Add(a, Scale(Sub(b, a), t)), radius, c) && (!hit || c.depth > out.depth)) {
            out = c;
            hit = true;
        }
    }
    return hit;
}

} // namespace terrain
