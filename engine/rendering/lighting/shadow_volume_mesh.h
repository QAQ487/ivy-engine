// shadow_volume_mesh.h
// Pure topology builder for stencil shadow volumes (no D3D; Linux-tested by
// tests/shadow_volume_mesh_test.cpp).
//
// The volume mesh is extruded in the vertex shader (VertexShaderShadowVolume.hlsl):
//   - every source triangle becomes 3 unshared corners that carry the triangle's face
//     normal (the "caps": lit triangles stay as the near cap, triangles facing away from
//     the light are pushed to infinity and become the far cap);
//   - every edge shared by two triangles gets a quad made of the two triangles' corners
//     for that edge. The quad has zero area at rest and stretches into a side wall when
//     exactly one of its two triangles is extruded (a silhouette edge).
//
// Source vertices are welded by quantised position first, so FBX/OBJ vertices split
// for UV or normal seams still share their edges. A mesh is usable only when it is
// closed: every welded edge is used by exactly two triangles in opposite directions.
// The output winding is made consistent with an outward-facing (positive signed volume)
// orientation, so the volume's outward faces are D3D front faces (clockwise) and the
// face normals point outward.
#ifndef SHADOW_VOLUME_MESH_H
#define SHADOW_VOLUME_MESH_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace shadow_volume {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline Vec3 Sub(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Normalised cross(b - a, c - a); zero vector for a degenerate triangle.
inline Vec3 FaceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 n = Cross(Sub(b, a), Sub(c, a));
    const float len = std::sqrt(Dot(n, n));
    if (!(len > 0.0f) || !std::isfinite(len))
        return {};
    return { n.x / len, n.y / len, n.z / len };
}

struct BuildStats {
    size_t sourceTriangles = 0;
    size_t triangles = 0;            // kept (non-degenerate after welding)
    size_t degenerateTriangles = 0;  // dropped: two corners weld to one point
    size_t invalidIndexTriangles = 0;
    size_t edges = 0;                // unique welded edges
    size_t openEdges = 0;            // used by one triangle
    size_t nonManifoldEdges = 0;     // used by >2 triangles or twice in the same direction
    bool flippedWinding = false;     // source winding was inward; output was reversed
};

struct Topology {
    // Volume vertex (corner) -> source vertex index. 3 per kept triangle, in output winding.
    std::vector<uint32_t> cornerSource;
    // Index buffer into the corners: 3 per triangle (caps) followed by 6 per shared edge.
    std::vector<uint32_t> indices;
    BuildStats stats;

    bool Closed() const {
        return stats.triangles > 0 && stats.openEdges == 0 && stats.nonManifoldEdges == 0
            && stats.invalidIndexTriangles == 0;
    }
    size_t CornerCount() const { return cornerSource.size(); }
    size_t TriangleCount() const { return cornerSource.size() / 3; }
};

// Builds the volume topology. pos(i) returns source vertex i as a Vec3.
template <class PositionFn>
Topology Build(size_t vertexCount, PositionFn pos, const uint32_t* indices, size_t indexCount) {
    Topology out;
    const size_t triCount = indices ? indexCount / 3 : 0;
    out.stats.sourceTriangles = triCount;
    if (vertexCount == 0 || triCount == 0)
        return out;

    // 1. Weld by quantised position (cell relative to the mesh extent).
    Vec3 mn = pos(0), mx = pos(0);
    for (size_t i = 1; i < vertexCount; ++i) {
        const Vec3 p = pos(i);
        mn = { std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z) };
        mx = { std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z) };
    }
    const float extent = std::max({ mx.x - mn.x, mx.y - mn.y, mx.z - mn.z, 0.0f });
    const double cell = std::max(static_cast<double>(extent) * 1e-6, 1e-7);

    using Key = std::tuple<long long, long long, long long>;
    std::map<Key, uint32_t> weldMap;
    std::vector<uint32_t> weld(vertexCount, 0);
    for (size_t i = 0; i < vertexCount; ++i) {
        const Vec3 p = pos(i);
        const Key k { std::llround(p.x / cell), std::llround(p.y / cell), std::llround(p.z / cell) };
        auto it = weldMap.find(k);
        if (it == weldMap.end())
            it = weldMap.emplace(k, static_cast<uint32_t>(weldMap.size())).first;
        weld[i] = it->second;
    }

    // 2. Keep non-degenerate triangles; measure orientation (signed volume).
    struct Tri { uint32_t v[3]; };
    std::vector<Tri> tris;
    tris.reserve(triCount);
    double signedVolume6 = 0.0;
    for (size_t t = 0; t < triCount; ++t) {
        const uint32_t a = indices[t * 3 + 0], b = indices[t * 3 + 1], c = indices[t * 3 + 2];
        if (a >= vertexCount || b >= vertexCount || c >= vertexCount) {
            ++out.stats.invalidIndexTriangles;
            continue;
        }
        if (weld[a] == weld[b] || weld[b] == weld[c] || weld[c] == weld[a]) {
            ++out.stats.degenerateTriangles;
            continue;
        }
        const Vec3 pa = pos(a), pb = pos(b), pc = pos(c);
        signedVolume6 += static_cast<double>(Dot(pa, Cross(pb, pc)));
        tris.push_back({ { a, b, c } });
    }
    out.stats.triangles = tris.size();
    if (tris.empty())
        return out;

    if (signedVolume6 < 0.0) {
        out.stats.flippedWinding = true;
        for (Tri& t : tris)
            std::swap(t.v[1], t.v[2]);
    }

    out.cornerSource.reserve(tris.size() * 3);
    out.indices.reserve(tris.size() * 3 + tris.size() * 9);
    for (size_t t = 0; t < tris.size(); ++t) {
        for (int k = 0; k < 3; ++k) {
            out.cornerSource.push_back(tris[t].v[k]);
            out.indices.push_back(static_cast<uint32_t>(t * 3 + k));
        }
    }

    // 3. Edge adjacency on welded ids.
    struct EdgeUse {
        uint32_t fromCorner;
        uint32_t toCorner;
        bool forward;  // welded from-id < to-id
    };
    std::map<std::pair<uint32_t, uint32_t>, std::vector<EdgeUse>> edges;
    for (size_t t = 0; t < tris.size(); ++t) {
        for (int k = 0; k < 3; ++k) {
            const uint32_t c0 = static_cast<uint32_t>(t * 3 + k);
            const uint32_t c1 = static_cast<uint32_t>(t * 3 + (k + 1) % 3);
            const uint32_t w0 = weld[tris[t].v[k]];
            const uint32_t w1 = weld[tris[t].v[(k + 1) % 3]];
            edges[{ std::min(w0, w1), std::max(w0, w1) }].push_back({ c0, c1, w0 < w1 });
        }
    }
    out.stats.edges = edges.size();

    // 4. One degenerate quad per correctly shared edge.
    for (const auto& kv : edges) {
        const std::vector<EdgeUse>& uses = kv.second;
        if (uses.size() == 1) {
            ++out.stats.openEdges;
            continue;
        }
        if (uses.size() != 2 || uses[0].forward == uses[1].forward) {
            ++out.stats.nonManifoldEdges;
            continue;
        }
        // Triangle 1 has directed edge a->b (A1 -> B1); triangle 2 has b->a (B2 -> A2).
        // Loop B1 -> A1 -> A2 -> B2 holds both reversed edges, so the walls keep the
        // caps' orientation once the quad stretches.
        const uint32_t A1 = uses[0].fromCorner, B1 = uses[0].toCorner;
        const uint32_t B2 = uses[1].fromCorner, A2 = uses[1].toCorner;
        const uint32_t quad[6] = { B1, A1, A2, B1, A2, B2 };
        out.indices.insert(out.indices.end(), quad, quad + 6);
    }
    return out;
}

// Per-corner face normals (outward, same for the 3 corners of a triangle) for the
// current source positions. Used for the initial build and for CPU-animated meshes.
template <class PositionFn>
void FillCornerNormals(const Topology& topo, PositionFn pos, std::vector<Vec3>& outNormals) {
    outNormals.resize(topo.cornerSource.size());
    for (size_t t = 0; t + 2 < topo.cornerSource.size(); t += 3) {
        const Vec3 n = FaceNormal(pos(topo.cornerSource[t]),
                                  pos(topo.cornerSource[t + 1]),
                                  pos(topo.cornerSource[t + 2]));
        outNormals[t] = outNormals[t + 1] = outNormals[t + 2] = n;
    }
}

}  // namespace shadow_volume

#endif  // SHADOW_VOLUME_MESH_H
