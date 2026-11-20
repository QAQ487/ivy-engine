// terrain_editor.cpp
// See terrain_editor.h (ADR 0011, step 4).

#include "engine/editor/terrain_editor.h"

#include "engine/core/content_path.h"
#include "engine/core/engine_gui.h"
#include "engine/debug_draw/debug_drawer.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/terrain_component.h"
#include "engine/ecs/transform.h"
#include "engine/editor/editor_layout.h"
#include "engine/editor/editor_undo.h"
#include "engine/scene/scene_loader.h"
#include "engine/terrain/terrain_brush.h"
#include "engine/terrain/terrain_collision.h"
#include "engine/terrain/terrain_splat.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string_view>
#include <vector>

namespace {

enum class Tool { None, Raise, Lower, Smooth, Flatten, Paint };

struct ToolState {
    Tool tool = Tool::None;
    terrain::Brush brush;
    int paintLayer = 0; // Paint tool: layer that gains weight
    // "Reset to flat" options.
    int flatResolution = 257;
    float flatHeight = 0.0f;
};
ToolState g_state;

struct Stroke {
    bool active = false;
    NodeRef node;
    int resolution = 0;
    std::vector<std::uint16_t> before; // whole map at stroke start (sculpt)
    std::vector<std::uint8_t> splatBefore; // whole splat map at stroke start (paint)
    bool paint = false;
    terrain::SampleRect dirty;
};
Stroke g_stroke;

// rom path (normalised) -> hash of the bytes the editor wrote there.
std::map<std::string, std::size_t> g_ownWrites;

const char* ToolLabel(Tool t) {
    switch (t) {
    case Tool::Raise: return "Raise";
    case Tool::Lower: return "Lower";
    case Tool::Smooth: return "Smooth";
    case Tool::Flatten: return "Flatten";
    case Tool::Paint: return "Paint";
    default: return "Off";
    }
}

terrain::BrushMode ToMode(Tool t) {
    switch (t) {
    case Tool::Lower: return terrain::BrushMode::Lower;
    case Tool::Smooth: return terrain::BrushMode::Smooth;
    case Tool::Flatten: return terrain::BrushMode::Flatten;
    default: return terrain::BrushMode::Raise;
    }
}

std::string NormaliseRomPath(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return p;
}

std::size_t HashBytes(const std::vector<std::uint8_t>& bytes) {
    return std::hash<std::string_view>{}(
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

// World position the renderer places the terrain at (rotation / scale ignored).
DirectX::XMFLOAT3 TerrainOrigin(GameObject* node) {
    if (const engine::Transform* t = node->GetTransform())
        return t->GetWorldPosition();
    return node->GetPosition();
}

class TerrainStrokeCommand : public EditorCommand {
public:
    TerrainStrokeCommand(NodeRef node, int resolution, terrain::SampleRect rect,
                         std::vector<std::uint16_t> before, std::vector<std::uint16_t> after,
                         std::string label)
        : m_node(std::move(node)), m_resolution(resolution), m_rect(rect),
          m_before(std::move(before)), m_after(std::move(after)), m_label(std::move(label)) {}

    void Undo() override { Apply(m_before); }
    void Redo() override { Apply(m_after); }
    std::string Describe() const override { return m_label; }

private:
    void Apply(const std::vector<std::uint16_t>& data) {
        GameObject* node = ResolveNode(m_node);
        TerrainComponent* tc = node ? node->GetComponent<TerrainComponent>() : nullptr;
        // A reload or "Reset to flat" changed the map since: nothing to restore.
        if (!tc || tc->GetHeightmap().resolution != m_resolution)
            return;
        if (terrain::PasteRect(tc->EditHeightmap(), m_rect, data))
            tc->NotifyHeightsChanged(m_rect);
    }

    NodeRef m_node;
    int m_resolution;
    terrain::SampleRect m_rect;
    std::vector<std::uint16_t> m_before;
    std::vector<std::uint16_t> m_after;
    std::string m_label;
};

class TerrainPaintCommand : public EditorCommand {
public:
    TerrainPaintCommand(NodeRef node, int resolution, terrain::SampleRect rect,
                        std::vector<std::uint8_t> before, std::vector<std::uint8_t> after)
        : m_node(std::move(node)), m_resolution(resolution), m_rect(rect),
          m_before(std::move(before)), m_after(std::move(after)) {}

    void Undo() override { Apply(m_before); }
    void Redo() override { Apply(m_after); }
    std::string Describe() const override { return "Paint Terrain"; }

private:
    void Apply(const std::vector<std::uint8_t>& data) {
        GameObject* node = ResolveNode(m_node);
        TerrainComponent* tc = node ? node->GetComponent<TerrainComponent>() : nullptr;
        if (!tc || tc->GetSplat().resolution != m_resolution)
            return;
        if (terrain::PasteSplatRect(tc->EditSplat(), m_rect, data))
            tc->NotifySplatChanged(m_rect);
    }

    NodeRef m_node;
    int m_resolution;
    terrain::SampleRect m_rect;
    std::vector<std::uint8_t> m_before;
    std::vector<std::uint8_t> m_after;
};

void EndStroke() {
    if (!g_stroke.active)
        return;
    GameObject* node = ResolveNode(g_stroke.node);
    TerrainComponent* tc = node ? node->GetComponent<TerrainComponent>() : nullptr;
    if (tc && g_stroke.paint && !g_stroke.dirty.Empty() && tc->GetSplat().resolution == g_stroke.resolution) {
        terrain::SplatMap original;
        original.resolution = g_stroke.resolution;
        original.rgba = std::move(g_stroke.splatBefore);
        std::vector<std::uint8_t> before = terrain::CopySplatRect(original, g_stroke.dirty);
        std::vector<std::uint8_t> after = terrain::CopySplatRect(tc->GetSplat(), g_stroke.dirty);
        EditorUndoStack::Instance().Push(std::make_unique<TerrainPaintCommand>(
            g_stroke.node, g_stroke.resolution, g_stroke.dirty, std::move(before), std::move(after)));
        EngineGUI_MarkSceneDirty();
    } else if (tc && !g_stroke.paint && !g_stroke.dirty.Empty() && tc->GetHeightmap().resolution == g_stroke.resolution) {
        terrain::Heightmap original;
        original.resolution = g_stroke.resolution;
        original.samples = std::move(g_stroke.before);
        std::vector<std::uint16_t> before = terrain::CopyRect(original, g_stroke.dirty);
        std::vector<std::uint16_t> after = terrain::CopyRect(tc->GetHeightmap(), g_stroke.dirty);
        const std::string label = std::string("Sculpt Terrain (") + ToolLabel(g_state.tool) + ")";
        EditorUndoStack::Instance().Push(std::make_unique<TerrainStrokeCommand>(
            g_stroke.node, g_stroke.resolution, g_stroke.dirty, std::move(before), std::move(after), label));
        EngineGUI_MarkSceneDirty();
    }
    g_stroke = Stroke{};
}

// Circle of the brush draped over the surface (queued lines, drawn in the 3D pass).
void QueueBrushCircle(const TerrainComponent& tc, const terrain::Layout& layout,
                      const DirectX::XMFLOAT3& origin, float cx, float cz, float radius,
                      const DirectX::XMFLOAT4& color) {
    constexpr int kSegments = 48;
    const terrain::Heightmap& map = tc.GetHeightmap();
    auto point = [&](int i) {
        const float a = static_cast<float>(i) * (6.2831853f / kSegments);
        const float x = cx + std::cos(a) * radius;
        const float z = cz + std::sin(a) * radius;
        float h = 0.0f;
        if (!terrain::SampleHeight(layout, map, x, z, h))
            h = terrain::SampleToHeight(layout, map.At(0, 0));
        return DirectX::XMFLOAT3{origin.x + x, origin.y + h + 0.05f, origin.z + z};
    };
    DirectX::XMFLOAT3 prev = point(0);
    for (int i = 1; i <= kSegments; ++i) {
        const DirectX::XMFLOAT3 next = point(i);
        DebugDrawer::QueueLine(prev, next, color);
        prev = next;
    }
}

std::string SanitiseFileStem(std::string s) {
    for (char& c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || c == '_' || c == '-'))
            c = '_';
    }
    if (s.empty())
        s = "terrain";
    return s;
}

std::string SceneStem() {
    std::string name = SceneLoader_GetSaveFileName() ? SceneLoader_GetSaveFileName() : "";
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const std::size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return SanitiseFileStem(name);
}

// rom/terrain/<scene>_<object><suffix>[_N]<ext> that no file and no other terrain uses yet.
std::string NewRomPath(const std::string& objectName, const std::string& suffix, const std::string& ext,
                       const std::vector<std::string>& taken) {
    const std::string base = "rom/terrain/" + SceneStem() + "_" + SanitiseFileStem(objectName) + suffix;
    for (int n = 1; n < 1000; ++n) {
        const std::string candidate = base + (n == 1 ? std::string() : "_" + std::to_string(n)) + ext;
        const bool usedByScene = std::find(taken.begin(), taken.end(), NormaliseRomPath(candidate)) != taken.end();
        std::error_code ec;
        const bool onDisk = std::filesystem::exists(content_path::WritableFileCurrent(candidate), ec);
        if (!usedByScene && !onDisk)
            return candidate;
    }
    return base + "_new" + ext;
}

void ForEachTerrain(const std::function<void(GameObject*, TerrainComponent&)>& fn) {
    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if (!node) return;
        if (TerrainComponent* tc = node->GetComponent<TerrainComponent>())
            fn(node, *tc);
        for (const auto& child : node->GetChildren())
            visit(child.get());
    };
    for (const auto& root : SceneLoader_GetSceneNodes())
        visit(root.get());
}

} // namespace

void TerrainEditor_DrawInspector(GameObject* node, TerrainComponent& tc) {
    ImGui::PushID("TerrainSculpt");
    // Wrap every hint / status line at the panel edge instead of widening the window.
    ImGui::PushTextWrapPos(0.0f);
    ImGui::SeparatorText("Sculpt");
    if (!tc.IsLoaded()) {
        ImGui::TextDisabled("Load a heightmap (or clear the path for a flat one) to sculpt.");
        ImGui::PopTextWrapPos();
        ImGui::PopID();
        return;
    }

    const Tool tools[] = { Tool::None, Tool::Raise, Tool::Lower, Tool::Smooth, Tool::Flatten, Tool::Paint };
    {
        const char* toolLabels[6];
        int toolIndex = 0;
        for (int i = 0; i < 6; ++i) {
            toolLabels[i] = ToolLabel(tools[i]);
            if (g_state.tool == tools[i]) toolIndex = i;
        }
        // 3 per row: stays inside the panel however narrow it is.
        if (InspectorToggleGrid(toolLabels, 6, &toolIndex, 3)) {
            EndStroke();
            g_state.tool = tools[toolIndex];
        }
    }
    terrain::Brush& b = g_state.brush;
    {
        InspectorRow row("Brush Radius (m)");
        ImGui::DragFloat("##v", &b.radius, 0.1f, terrain::kMinBrushRadius, terrain::kMaxBrushRadius, "%.1f");
    }
    const char* strengthLabel = (g_state.tool == Tool::Raise || g_state.tool == Tool::Lower)
        ? "Strength (m/s)" : (g_state.tool == Tool::Paint ? "Strength (full layers/s)" : "Strength (blends/s)");
    if (g_state.tool == Tool::Paint) {
        const int layerCount = static_cast<int>(terrain::EffectiveLayers(tc.settings).size());
        g_state.paintLayer = std::min(g_state.paintLayer, layerCount - 1);
        char layerLabelBuf[8][16];
        const char* layerLabels[8];
        const int shown = std::min(layerCount, 8);
        for (int i = 0; i < shown; ++i) {
            std::snprintf(layerLabelBuf[i], sizeof(layerLabelBuf[i]), "Layer %d", i + 1);
            layerLabels[i] = layerLabelBuf[i];
        }
        InspectorToggleGrid(layerLabels, shown, &g_state.paintLayer, 4);
        if (layerCount < 2)
            ImGui::TextDisabled("Add a second layer above to paint.");
        if (!tc.EnsureSplatLoaded() && !tc.GetSplatError().empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", tc.GetSplatError().c_str());
    }
    {
        InspectorRow row("Strength");
        ImGui::DragFloat("##v", &b.strength, 0.05f, 0.0f, terrain::kMaxBrushStrength, "%.2f");
        ImGui::SetItemTooltip("%s", strengthLabel);
    }
    {
        InspectorRow row("Falloff");
        ImGui::SliderFloat("##v", &b.falloff, 0.0f, 1.0f, "%.2f");
    }
    b = terrain::SanitizeBrush(b);
    if (g_state.tool == Tool::Flatten)
        ImGui::TextDisabled("Flattens to the height under the cursor when the stroke starts.");
    if (g_state.tool != Tool::None)
        ImGui::TextDisabled("Left-drag on the terrain in the viewport. Alt + drag moves the camera.");

    const terrain::Heightmap& map = tc.GetHeightmap();
    if (tc.HasUnsavedHeights()) {
        if (tc.settings.heightmap.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                "Unsaved sculpt edits: Save Scene writes rom/terrain/%s_%s.r16",
                SceneStem().c_str(), SanitiseFileStem(node ? node->GetName() : "terrain").c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                "Unsaved sculpt edits: Save Scene writes %s", tc.settings.heightmap.c_str());
        }
    }

    if (tc.HasUnsavedSplat()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Unsaved paint: Save Scene writes %s",
            tc.settings.splatmap.empty() ? "rom/terrain/<scene>_<object>_splat.splat" : tc.settings.splatmap.c_str());
    }

    if (ImGui::TreeNode("Reset to Flat")) {
        static const int kResolutions[] = { 33, 65, 129, 257, 513, 1025, 2049, 4097 };
        char preview[16];
        std::snprintf(preview, sizeof(preview), "%d", g_state.flatResolution);
        {
            InspectorRow row("Resolution");
            if (ImGui::BeginCombo("##v", preview)) {
                for (int r : kResolutions) {
                    char label[16];
                    std::snprintf(label, sizeof(label), "%d", r);
                    if (ImGui::Selectable(label, g_state.flatResolution == r))
                        g_state.flatResolution = r;
                }
                ImGui::EndCombo();
            }
        }
        {
            InspectorRow row("Height (m)");
            ImGui::DragFloat("##v", &g_state.flatHeight, 0.1f);
        }
        ImGui::TextDisabled("Current: %d x %d samples. Replaces the whole map; cannot be undone.",
                            map.resolution, map.resolution);
        if (ImGui::Button("Reset to Flat"))
            ImGui::OpenPopup("ConfirmTerrainReset");
        if (ImGui::BeginPopupModal("ConfirmTerrainReset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Replace the terrain with a flat %d x %d map? This cannot be undone.",
                        g_state.flatResolution, g_state.flatResolution);
            if (ImGui::Button("Reset")) {
                EndStroke();
                if (tc.ResetFlat(g_state.flatResolution, g_state.flatHeight))
                    EngineGUI_MarkSceneDirty();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::TreePop();
    }
    ImGui::PopTextWrapPos();
    ImGui::PopID();
}

bool TerrainEditor_HandleViewport(GameObject* selected, const TerrainViewportInput& in) {
    TerrainComponent* tc = selected ? selected->GetComponent<TerrainComponent>() : nullptr;
    const bool usable = in.enabled && g_state.tool != Tool::None && tc && tc->IsLoaded();
    if (!usable || (g_stroke.active && ResolveNode(g_stroke.node) != selected)) {
        EndStroke();
        if (!usable)
            return false;
    }

    const terrain::Layout layout = terrain::SanitizeLayout(tc->settings.layout);
    const DirectX::XMFLOAT3 origin = TerrainOrigin(selected);
    bool hit = false;
    terrain::Float3 hitLocal;
    if (in.hasRay) {
        const terrain::Float3 o{in.rayOrigin.x - origin.x, in.rayOrigin.y - origin.y, in.rayOrigin.z - origin.z};
        const terrain::Float3 d{in.rayDir.x, in.rayDir.y, in.rayDir.z};
        float t = 0.0f;
        terrain::Float3 n;
        if (terrain::Raycast(layout, tc->GetHeightmap(), tc->GetLowSample(), tc->GetHighSample(),
                             o, d, 100000.0f, t, n)) {
            hit = true;
            hitLocal = {o.x + d.x * t, o.y + d.y * t, o.z + d.z * t};
        }
    }

    const bool painting = in.mouseDown && (g_stroke.active || (!in.mouseBlocked && hit));
    if (!painting) {
        EndStroke();
    }

    if (hit) {
        const terrain::Brush& b = g_state.brush;
        QueueBrushCircle(*tc, layout, origin, hitLocal.x, hitLocal.z, b.radius,
                         painting ? DirectX::XMFLOAT4{1.0f, 0.55f, 0.15f, 1.0f}
                                  : DirectX::XMFLOAT4{0.3f, 0.85f, 1.0f, 1.0f});
        if (b.falloff > 0.0f && b.falloff < 1.0f)
            QueueBrushCircle(*tc, layout, origin, hitLocal.x, hitLocal.z, b.radius * (1.0f - b.falloff),
                             DirectX::XMFLOAT4{0.3f, 0.85f, 1.0f, 0.45f});
    }

    if (painting && hit && g_state.tool == Tool::Paint) {
        if (tc->EnsureSplatLoaded()) {
            terrain::SplatMap& splat = tc->EditSplat();
            if (!g_stroke.active) {
                g_stroke.active = true;
                g_stroke.paint = true;
                g_stroke.node = MakeNodeRef(selected);
                g_stroke.resolution = splat.resolution;
                g_stroke.splatBefore = splat.rgba;
                g_stroke.dirty = terrain::SampleRect{};
            }
            const float dt = std::min(std::max(in.dt, 0.0f), 0.1f);
            const terrain::SampleRect changed = terrain::PaintSplat(layout, splat, g_state.paintLayer,
                                                                    g_state.brush, hitLocal.x, hitLocal.z, dt);
            if (!changed.Empty()) {
                tc->NotifySplatChanged(changed);
                g_stroke.dirty = terrain::UnionRect(g_stroke.dirty, changed);
            }
        }
    } else if (painting && hit) {
        terrain::Heightmap& map = tc->EditHeightmap();
        if (!g_stroke.active) {
            g_stroke.active = true;
            g_stroke.node = MakeNodeRef(selected);
            g_stroke.resolution = map.resolution;
            g_stroke.before = map.samples;
            g_stroke.dirty = terrain::SampleRect{};
            if (g_state.tool == Tool::Flatten)
                g_state.brush.targetHeight = hitLocal.y;
        }
        terrain::Brush b = g_state.brush;
        b.mode = ToMode(g_state.tool);
        const float dt = std::min(std::max(in.dt, 0.0f), 0.1f);
        const terrain::SampleRect changed = terrain::ApplyBrush(layout, map, b, hitLocal.x, hitLocal.z, dt);
        if (!changed.Empty()) {
            tc->NotifyHeightsChanged(changed);
            g_stroke.dirty = terrain::UnionRect(g_stroke.dirty, changed);
        }
    }
    // Own the mouse while over the terrain or mid-stroke, so clicks do not change the selection.
    return g_stroke.active || (hit && !in.mouseBlocked);
}

bool TerrainEditor_IsSculpting(const GameObject* node) {
    if (g_state.tool == Tool::None || !node)
        return false;
    return node->GetComponent<TerrainComponent>() != nullptr;
}

bool TerrainEditor_SaveEditedTerrains(std::string* error) {
    EndStroke();
    std::vector<std::string> taken;
    ForEachTerrain([&](GameObject*, TerrainComponent& tc) {
        if (!tc.settings.heightmap.empty())
            taken.push_back(NormaliseRomPath(tc.settings.heightmap));
        if (!tc.settings.splatmap.empty())
            taken.push_back(NormaliseRomPath(tc.settings.splatmap));
    });

    bool ok = true;
    ForEachTerrain([&](GameObject* node, TerrainComponent& tc) {
        if (!ok || !tc.HasUnsavedHeights())
            return;
        if (tc.settings.heightmap.empty()) {
            const std::string path = NewRomPath(node->GetName(), "", ".r16", taken);
            taken.push_back(NormaliseRomPath(path));
            tc.AdoptHeightmapPath(path);
        }
        std::string message;
        std::vector<std::uint8_t> bytes;
        if (!tc.SaveHeightmap(&message, &bytes)) {
            ok = false;
            if (error) *error = "Terrain \"" + node->GetName() + "\": " + message;
            return;
        }
        g_ownWrites[NormaliseRomPath(tc.settings.heightmap)] = HashBytes(bytes);
        hal::DebugWriteA(("[Terrain] saved " + tc.settings.heightmap + "\n").c_str());
    });
    ForEachTerrain([&](GameObject* node, TerrainComponent& tc) {
        if (!ok || !tc.HasUnsavedSplat())
            return;
        if (tc.settings.splatmap.empty()) {
            const std::string path = NewRomPath(node->GetName(), "_splat", ".splat", taken);
            taken.push_back(NormaliseRomPath(path));
            tc.AdoptSplatPath(path);
        }
        std::string message;
        std::vector<std::uint8_t> bytes;
        if (!tc.SaveSplat(&message, &bytes)) {
            ok = false;
            if (error) *error = "Terrain \"" + node->GetName() + "\" paint: " + message;
            return;
        }
        g_ownWrites[NormaliseRomPath(tc.settings.splatmap)] = HashBytes(bytes);
        hal::DebugWriteA(("[Terrain] saved " + tc.settings.splatmap + "\n").c_str());
    });
    return ok;
}

bool TerrainEditor_IsOwnWrite(const std::string& romPath) {
    const auto it = g_ownWrites.find(NormaliseRomPath(romPath));
    if (it == g_ownWrites.end())
        return false;
    std::ifstream file(content_path::ReadableFileCurrent(romPath), std::ios::binary);
    if (!file)
        return false;
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return HashBytes(bytes) == it->second;
}

std::unique_ptr<GameObject> TerrainEditor_CreateTerrainObject() {
    auto node = std::make_unique<GameObject>("Terrain");
    engine::Transform* t = node->AddComponent<engine::Transform>();
    t->localPosition = DirectX::XMFLOAT3(-128.0f, 0.0f, -128.0f);
    node->SyncLegacyTransformFromComponents();
    TerrainComponent* tc = node->AddComponent<TerrainComponent>();
    tc->settings.layout.sizeX = 256.0f;
    tc->settings.layout.sizeZ = 256.0f;
    tc->settings.layout.heightScale = 100.0f;
    tc->settings.layout.heightOffset = -20.0f;
    node->SetTag(Tag::TAG_GROUND);
    return node;
}
