// shadow_volume_asset_check.cpp
// Linux check: every Visual with "shadowVolume": true in a testbed scene must point at
// a closed mesh, otherwise the engine skips it with a warning and it casts nothing.
//
// Reads ASCII FBX 7.x (Vertices + PolygonVertexIndex), de-indexes one vertex per
// polygon corner like Model3D::LoadFBX, fan-triangulates each polygon and runs the
// engine's builder (engine/rendering/lighting/shadow_volume_mesh.h). Binary FBX files
// are reported as not checkable (failure: the demo assets are ASCII on purpose).
//
// Build + run: tools/run_shadow_volume_asset_check.sh

#include "engine/rendering/lighting/shadow_volume_mesh.h"
#include "libraries/nlohmann/json.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using shadow_volume::Vec3;

namespace {

bool ReadArray(const std::string& text, const std::string& key, std::vector<double>& out) {
    const size_t k = text.find(key + ": *");
    if (k == std::string::npos) return false;
    const size_t a = text.find("a:", k);
    const size_t end = text.find('}', a);
    if (a == std::string::npos || end == std::string::npos) return false;
    std::string body = text.substr(a + 2, end - a - 2);
    for (char& c : body)
        if (c == ',' || c == '\n' || c == '\r' || c == '\t') c = ' ';
    std::istringstream in(body);
    double v;
    while (in >> v) out.push_back(v);
    return !out.empty();
}

// Returns an error string, empty on success.
std::string CheckFbx(const fs::path& path, shadow_volume::BuildStats& stats) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "cannot open";
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    if (text.rfind("Kaydara FBX Binary", 0) == 0)
        return "binary FBX (this check reads ASCII FBX only)";

    std::vector<double> v, pvi;
    if (!ReadArray(text, "Vertices", v) || v.size() % 3 != 0)
        return "no Vertices array";
    if (!ReadArray(text, "PolygonVertexIndex", pvi))
        return "no PolygonVertexIndex array";

    std::vector<Vec3> corners;  // one vertex per polygon corner (like the engine loader)
    std::vector<uint32_t> indices;
    std::vector<int> poly;
    for (double d : pvi) {
        int i = static_cast<int>(d);
        const bool last = i < 0;
        if (last) i = ~i;
        if (i < 0 || static_cast<size_t>(i) * 3 + 2 >= v.size()) return "index out of range";
        poly.push_back(i);
        if (!last) continue;
        const uint32_t base = static_cast<uint32_t>(corners.size());
        for (int p : poly)
            corners.push_back({ static_cast<float>(v[p * 3]), static_cast<float>(v[p * 3 + 1]),
                                static_cast<float>(v[p * 3 + 2]) });
        for (size_t k = 1; k + 1 < poly.size(); ++k) {
            indices.push_back(base);
            indices.push_back(base + static_cast<uint32_t>(k));
            indices.push_back(base + static_cast<uint32_t>(k + 1));
        }
        poly.clear();
    }
    const auto topo = shadow_volume::Build(corners.size(),
        [&](size_t i) { return corners[i]; }, indices.data(), indices.size());
    stats = topo.stats;
    if (!topo.Closed()) {
        char msg[160];
        std::snprintf(msg, sizeof(msg), "not closed (open edges %zu, non-manifold edges %zu)",
            stats.openEdges, stats.nonManifoldEdges);
        return msg;
    }
    return {};
}

void CollectCasters(const nlohmann::json& nodes, std::set<std::string>& out) {
    if (!nodes.is_array()) return;
    for (const auto& n : nodes) {
        const auto comps = n.value("components", nlohmann::json::object());
        if (comps.contains("visual")) {
            const auto& vis = comps["visual"];
            if (vis.value("shadowVolume", false) && vis.contains("model") && vis["model"].is_string())
                out.insert(vis["model"].get<std::string>());
        }
        if (n.contains("children")) CollectCasters(n["children"], out);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const fs::path root = argc > 1 ? fs::path(argv[1]) : fs::path(".");
    const fs::path project = root / "testbed";
    std::set<std::string> models;
    int scenes = 0;
    for (const auto& e : fs::directory_iterator(project / "rom/scene")) {
        if (e.path().extension() != ".json") continue;
        std::ifstream f(e.path());
        nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
        if (j.is_discarded()) continue;
        const size_t before = models.size();
        CollectCasters(j.value("nodes", nlohmann::json::array()), models);
        if (models.size() != before) ++scenes;
    }
    if (models.empty()) {
        std::fprintf(stderr, "FAIL: no Visual with shadowVolume: true in testbed scenes\n");
        return 1;
    }
    int failed = 0;
    for (const std::string& rom : models) {
        if (rom.rfind("rom/", 0) != 0) {
            std::fprintf(stderr, "FAIL: %s: not a rom/ path\n", rom.c_str());
            ++failed;
            continue;
        }
        shadow_volume::BuildStats st;
        const std::string err = CheckFbx(project / rom, st);
        if (!err.empty()) {
            std::fprintf(stderr, "FAIL: %s: %s\n", rom.c_str(), err.c_str());
            ++failed;
        } else {
            std::printf("ok   %-45s %6zu tris %6zu edges\n", rom.c_str(), st.triangles, st.edges);
        }
    }
    if (failed) return 1;
    std::printf("PASS: %zu shadow-volume caster mesh(es) in %d scene(s) are closed\n", models.size(), scenes);
    return 0;
}
