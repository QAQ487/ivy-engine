// Shadow volume topology builder tests (engine/rendering/lighting/shadow_volume_mesh.h).
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/shadow_volume_mesh_test tests/shadow_volume_mesh_test.cpp
//
// Besides the topology counts, these tests run the same extrusion rule as
// shaders/VertexShaderShadowVolume.hlsl on the CPU (corners whose face normal does not
// face the light move along the light direction) and check that the stretched volume
// is closed and outward-oriented, which is what the stencil z-fail count relies on.

#include "engine/rendering/lighting/shadow_volume_mesh.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <tuple>
#include <vector>

using shadow_volume::Vec3;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static bool Near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

struct Mesh {
    std::vector<Vec3> pos;
    std::vector<uint32_t> idx;
};

// Unit-ish cube [-1,1]^3 with 4 split vertices per face (like an FBX with hard normals).
// Faces wound so cross(b-a, c-a) points outward (positive signed volume).
static Mesh MakeSplitCube() {
    const Vec3 c[8] = {
        {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
    };
    // Quads listed counter-clockwise when seen from outside in a right-handed sense,
    // i.e. cross(b-a, c-a) is outward.
    const int faces[6][4] = {
        {0, 3, 2, 1},  // -z
        {4, 5, 6, 7},  // +z
        {0, 4, 7, 3},  // -x
        {1, 2, 6, 5},  // +x
        {0, 1, 5, 4},  // -y
        {3, 7, 6, 2},  // +y
    };
    Mesh m;
    for (const auto& f : faces) {
        const uint32_t base = static_cast<uint32_t>(m.pos.size());
        for (int k = 0; k < 4; ++k)
            m.pos.push_back(c[f[k]]);
        const uint32_t q[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
        m.idx.insert(m.idx.end(), q, q + 6);
    }
    return m;
}

static Mesh MakeTetra() {
    Mesh m;
    m.pos = { {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
    // Outward winding (cross(b-a,c-a) away from the opposite vertex).
    m.idx = { 0, 2, 1,  0, 1, 3,  0, 3, 2,  1, 2, 3 };
    return m;
}

static shadow_volume::Topology BuildFrom(const Mesh& m) {
    return shadow_volume::Build(m.pos.size(),
        [&](size_t i) { return m.pos[i]; }, m.idx.data(), m.idx.size());
}

static double SignedVolume(const std::vector<Vec3>& p, const std::vector<uint32_t>& idx) {
    double v6 = 0.0;
    for (size_t t = 0; t + 2 < idx.size(); t += 3)
        v6 += shadow_volume::Dot(p[idx[t]], shadow_volume::Cross(p[idx[t + 1]], p[idx[t + 2]]));
    return v6 / 6.0;
}

// CPU copy of the vertex-shader rule. shine = direction the light travels.
static std::vector<Vec3> Extrude(const Mesh& m, const shadow_volume::Topology& topo,
                                 const Vec3& shine, float distance) {
    std::vector<Vec3> normals;
    shadow_volume::FillCornerNormals(topo, [&](size_t i) { return m.pos[i]; }, normals);
    std::vector<Vec3> out(topo.CornerCount());
    for (size_t i = 0; i < out.size(); ++i) {
        const Vec3 p = m.pos[topo.cornerSource[i]];
        const float facing = -shadow_volume::Dot(normals[i], shine);
        if (facing <= 0.0f)
            out[i] = { p.x + shine.x * distance, p.y + shine.y * distance, p.z + shine.z * distance };
        else
            out[i] = p;
    }
    return out;
}

// Closed + consistently oriented check on a triangle soup, ignoring zero-area triangles.
static bool SoupClosed(const std::vector<Vec3>& p, const std::vector<uint32_t>& idx) {
    using Key = std::tuple<long long, long long, long long>;
    std::map<Key, int> ids;
    auto id = [&](const Vec3& v) {
        const Key k { std::llround(v.x * 1e4), std::llround(v.y * 1e4), std::llround(v.z * 1e4) };
        auto it = ids.find(k);
        if (it == ids.end())
            it = ids.emplace(k, static_cast<int>(ids.size())).first;
        return it->second;
    };
    std::map<std::pair<int, int>, int> directed;
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        const Vec3 n = shadow_volume::Cross(shadow_volume::Sub(p[idx[t + 1]], p[idx[t]]),
                                            shadow_volume::Sub(p[idx[t + 2]], p[idx[t]]));
        if (shadow_volume::Dot(n, n) < 1e-10f)
            continue;
        const int a = id(p[idx[t]]), b = id(p[idx[t + 1]]), c = id(p[idx[t + 2]]);
        ++directed[{ a, b }];
        ++directed[{ b, c }];
        ++directed[{ c, a }];
    }
    for (const auto& kv : directed) {
        auto rev = directed.find({ kv.first.second, kv.first.first });
        if (rev == directed.end() || rev->second != kv.second)
            return false;
    }
    return !directed.empty();
}

static void TestSplitCubeTopology() {
    const Mesh cube = MakeSplitCube();
    const auto topo = BuildFrom(cube);
    Expect(topo.Closed(), "split cube welds into a closed mesh");
    Expect(topo.stats.triangles == 12, "cube keeps 12 triangles");
    Expect(topo.stats.edges == 18, "cube has 18 welded edges");
    Expect(topo.CornerCount() == 36, "3 corners per triangle");
    Expect(topo.indices.size() == 36 + 18 * 6, "caps + one quad per edge");
    Expect(!topo.stats.flippedWinding, "outward cube keeps its winding");
    std::vector<Vec3> rest(topo.CornerCount());
    for (size_t i = 0; i < rest.size(); ++i)
        rest[i] = cube.pos[topo.cornerSource[i]];
    Expect(Near(SignedVolume(rest, topo.indices), 8.0, 1e-4), "rest volume equals the cube (quads are zero-area)");
}

static void TestFlippedWinding() {
    Mesh cube = MakeSplitCube();
    for (size_t t = 0; t + 2 < cube.idx.size(); t += 3)
        std::swap(cube.idx[t + 1], cube.idx[t + 2]);
    const auto topo = BuildFrom(cube);
    Expect(topo.Closed(), "inward cube is still closed");
    Expect(topo.stats.flippedWinding, "inward winding is detected");
    std::vector<Vec3> rest(topo.CornerCount());
    for (size_t i = 0; i < rest.size(); ++i)
        rest[i] = cube.pos[topo.cornerSource[i]];
    Expect(SignedVolume(rest, topo.indices) > 0.0, "output winding is outward after the flip");
}

static void TestOpenAndNonManifold() {
    Mesh open = MakeSplitCube();
    open.idx.resize(open.idx.size() - 6);  // drop the +y face
    const auto topoOpen = BuildFrom(open);
    Expect(!topoOpen.Closed(), "cube without a face is not closed");
    Expect(topoOpen.stats.openEdges == 4, "missing face leaves 4 open edges");

    Mesh fin = MakeTetra();
    fin.pos.push_back({ 0.5f, 0.5f, -1.0f });
    fin.idx.insert(fin.idx.end(), { 0, 1, 4 });  // third triangle on edge 0-1
    const auto topoFin = BuildFrom(fin);
    Expect(!topoFin.Closed(), "fin on an edge is not closed");
    Expect(topoFin.stats.nonManifoldEdges >= 1, "fin edge is reported non-manifold");

    Mesh degenerate = MakeTetra();
    degenerate.pos.push_back({ 0, 0, 0 });  // duplicate of vertex 0
    degenerate.idx.insert(degenerate.idx.end(), { 0, 4, 1 });
    const auto topoDeg = BuildFrom(degenerate);
    Expect(topoDeg.Closed(), "zero-area sliver does not open the mesh");
    Expect(topoDeg.stats.degenerateTriangles == 1, "welded sliver is dropped");

    Mesh bad = MakeTetra();
    bad.idx.insert(bad.idx.end(), { 0, 1, 99 });
    Expect(!BuildFrom(bad).Closed(), "out-of-range index fails the closed check");

    Expect(!shadow_volume::Build(0, [](size_t) { return Vec3{}; }, nullptr, 0).Closed(),
        "empty mesh is not closed");
}

static void TestCubeExtrusionStraightDown() {
    const Mesh cube = MakeSplitCube();
    const auto topo = BuildFrom(cube);
    const float D = 10.0f;
    const auto p = Extrude(cube, topo, { 0, -1, 0 }, D);
    Expect(SoupClosed(p, topo.indices), "down-light volume is closed");
    // Top face stays at y=1; everything else moves down by D: a 2x2 column from y=1 to -1-D.
    Expect(Near(SignedVolume(p, topo.indices), 4.0 * (2.0 + D), 1e-3), "down-light volume is the swept column");
}

static void TestObliqueLights() {
    const Vec3 lights[] = {
        { 0.3f, -1.0f, 0.2f }, { -0.5f, -1.0f, 0.5f }, { 1.0f, -0.2f, 0.0f }, { 0.0f, 1.0f, 0.0f },
    };
    const Mesh meshes[] = { MakeSplitCube(), MakeTetra() };
    for (const Mesh& m : meshes) {
        const auto topo = BuildFrom(m);
        Expect(topo.Closed(), "test mesh is closed");
        std::vector<Vec3> rest(topo.CornerCount());
        for (size_t i = 0; i < rest.size(); ++i)
            rest[i] = m.pos[topo.cornerSource[i]];
        const double restVolume = SignedVolume(rest, topo.indices);
        for (Vec3 L : lights) {
            const float len = std::sqrt(shadow_volume::Dot(L, L));
            L = { L.x / len, L.y / len, L.z / len };
            const auto p = Extrude(m, topo, L, 25.0f);
            Expect(SoupClosed(p, topo.indices), "oblique volume is closed");
            Expect(SignedVolume(p, topo.indices) > restVolume, "oblique volume is outward and larger than the caster");
        }
    }
}

int main() {
    TestSplitCubeTopology();
    TestFlippedWinding();
    TestOpenAndNonManifold();
    TestCubeExtrusionStraightDown();
    TestObliqueLights();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("PASS: shadow volume mesh (weld, closed check, winding, extrusion)\n");
    return 0;
}
