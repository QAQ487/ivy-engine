// scene_lighting.cpp
// Implements SceneLightEntry.

#include "scene_lighting.h"
#include "engine/ecs/directional_light_component.h"
#include "engine/ecs/point_light_component.h"
#include "engine/ecs/spot_light_component.h"
#include "engine/ecs/game_object.h"
#include "engine/editor/light_gizmo.h"
#include "engine/ecs/transform.h"
#include "engine/rendering/lighting/lighting.h"
#include "engine/rendering/lighting/shadow.h"
#include "engine/rendering/culling/light_select_math.h"
#include "engine/scene/render_settings.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

using namespace DirectX;

namespace {

struct SceneLightEntry {
    enum class Kind { Directional, Point, Spot };
    Kind kind;
    DirectionalLightComponent* directional = nullptr;
    PointLightComponent* point = nullptr;
    SpotLightComponent* spot = nullptr;
};

void VisitLights(GameObject* node, std::vector<SceneLightEntry>& out) {
    if (!node || !node->IsActive()) return;

    if (auto* dl = node->GetComponent<DirectionalLightComponent>()) {
        if (dl->enabled) out.push_back({ SceneLightEntry::Kind::Directional, dl, nullptr, nullptr });
    }
    if (auto* pl = node->GetComponent<PointLightComponent>()) {
        if (pl->enabled) out.push_back({ SceneLightEntry::Kind::Point, nullptr, pl, nullptr });
    }
    if (auto* sl = node->GetComponent<SpotLightComponent>()) {
        if (sl->enabled) out.push_back({ SceneLightEntry::Kind::Spot, nullptr, nullptr, sl });
    }

    for (const auto& child : node->GetChildren())
        if (child) VisitLights(child.get(), out);
}

void ApplyFallbackLighting() {
    Lighting::ClearLights();
    Lighting::Light L0 {};
    L0.type = Lighting::DIRECTIONAL;
    L0.directionWS = { -0.5f, -1.0f, 0.5f };
    L0.color = { 1.0f, 1.0f, 1.0f };
    L0.specColor = { 1.0f, 1.0f, 1.0f };
    L0.specPower = 10.0f;
    Lighting::AddDirectionalLight(L0);
    Shadow::SetLightDirection(0, L0.directionWS);
    Shadow::SetCasterLightIndex(0);  // the fallback light is Lights[0]
    Lighting::SetAmbientColor({ 0.6f, 0.7f, 0.8f });
}

static XMFLOAT3 ScaledColor(const XMFLOAT3& color, float intensity) {
    return { color.x * intensity, color.y * intensity, color.z * intensity };
}

static float DegToInnerCos(float deg) {
    return cosf(XMConvertToRadians(deg * 0.5f));
}

SceneLightingStats g_lastStats{};
// Components selected last sync (hysteresis memory for light_select).
std::unordered_set<const void*> g_prevSelected;

const void* EntryId(const SceneLightEntry& e) {
    if (e.directional) return e.directional;
    return e.point ? static_cast<const void*>(e.point) : static_cast<const void*>(e.spot);
}

// Keeps only the lights light_select picks; scene order is preserved.
void SelectLightEntries(std::vector<SceneLightEntry>& entries, const XMFLOAT3& cam) {
    std::vector<light_select::Candidate> cand;
    cand.reserve(entries.size());
    for (const SceneLightEntry& e : entries) {
        light_select::Candidate c;
        c.wasSelected = g_prevSelected.count(EntryId(e)) != 0;
        if (e.kind == SceneLightEntry::Kind::Directional) {
            c.directional = true;
        } else {
            const Component* comp = e.point ? static_cast<const Component*>(e.point)
                                            : static_cast<const Component*>(e.spot);
            const engine::Transform* t = comp && comp->owner
                ? comp->owner->GetComponent<engine::Transform>() : nullptr;
            const XMFLOAT3 pos = LightGizmo::GetWorldPosition(t);
            c.x = pos.x; c.y = pos.y; c.z = pos.z;
            const XMFLOAT3& col = e.point ? e.point->color : e.spot->color;
            const float inten = e.point ? e.point->intensity : e.spot->intensity;
            c.luma = std::max({col.x, col.y, col.z}) * inten;
            c.range = e.point ? e.point->range : e.spot->range;
        }
        cand.push_back(c);
    }
    std::vector<int> idx(static_cast<size_t>(Lighting::MAX_LIGHTS));
    const int n = light_select::Select(cand.data(), static_cast<int>(cand.size()),
        cam.x, cam.y, cam.z, Lighting::MAX_LIGHTS, idx.data());
    std::vector<SceneLightEntry> kept;
    kept.reserve(static_cast<size_t>(n));
    g_prevSelected.clear();
    for (int i = 0; i < n; ++i) {
        kept.push_back(entries[static_cast<size_t>(idx[static_cast<size_t>(i)])]);
        g_prevSelected.insert(EntryId(kept.back()));
    }
    entries = std::move(kept);
}

} // namespace

SceneLightingStats SceneLighting_GetLastStats() { return g_lastStats; }

void SceneLightingSyncFromNodes(const std::vector<std::unique_ptr<GameObject>>& roots,
                                const XMFLOAT3& cameraPosition) {
    std::vector<SceneLightEntry> entries;
    entries.reserve(Lighting::MAX_LIGHTS);
    for (const auto& root : roots)
        if (root) VisitLights(root.get(), entries);

    g_lastStats.total = static_cast<int>(entries.size());
    if (entries.empty()) {
        g_lastStats.used = 0;
        g_prevSelected.clear();
        ApplyFallbackLighting();
        return;
    }

    if (RenderSettings_GetConst().lightSelectionEnabled
        && entries.size() > static_cast<size_t>(Lighting::MAX_LIGHTS)) {
        SelectLightEntries(entries, cameraPosition);
    } else {
        g_prevSelected.clear();
    }
    g_lastStats.used = std::min(static_cast<int>(entries.size()), Lighting::MAX_LIGHTS);

    Lighting::ClearLights();
    int shadowIndex = 0;
    int casterLightIndex = -1;
    const int shadowCapacity = Shadow::GetMaxCasters();

    for (const SceneLightEntry& e : entries) {
        if (Lighting::GetParams().lightCount >= Lighting::MAX_LIGHTS)
            break;

        Component* comp = e.directional
            ? static_cast<Component*>(e.directional)
            : (e.point ? static_cast<Component*>(e.point) : static_cast<Component*>(e.spot));
        GameObject* owner = comp ? comp->owner : nullptr;
        const engine::Transform* transform = owner
            ? owner->GetComponent<engine::Transform>()
            : nullptr;

        if (e.kind == SceneLightEntry::Kind::Directional && e.directional) {
            Lighting::Light L {};
            L.type = Lighting::DIRECTIONAL;
            L.directionWS = LightGizmo::GetShineDirectionWS(transform);
            L.color = ScaledColor(e.directional->color, e.directional->intensity);
            L.specColor = e.directional->specColor;
            L.specPower = e.directional->specPower;
            const int lightIndex = Lighting::GetParams().lightCount;  // slot AddDirectionalLight fills
            Lighting::AddDirectionalLight(L);

            if (e.directional->castShadows && shadowIndex < shadowCapacity) {
                Shadow::SetLightDirection(shadowIndex, L.directionWS);
                if (shadowIndex == 0)
                    casterLightIndex = lightIndex;
                ++shadowIndex;
            }
        }
        else if (e.kind == SceneLightEntry::Kind::Point && e.point) {
            Lighting::Light L {};
            L.type = Lighting::POINT;
            L.positionWS = LightGizmo::GetWorldPosition(transform);
            L.directionWS = { 0.0f, -1.0f, 0.0f };
            L.color = ScaledColor(e.point->color, e.point->intensity);
            L.specColor = e.point->specColor;
            L.specPower = e.point->specPower;
            L.range = e.point->range;
            L.attenuation = e.point->attenuation;
            Lighting::AddPointLight(L);
        }
        else if (e.kind == SceneLightEntry::Kind::Spot && e.spot) {
            Lighting::Light L {};
            L.type = Lighting::SPOT;
            L.positionWS = LightGizmo::GetWorldPosition(transform);
            L.directionWS = LightGizmo::GetIncomingDirectionWS(transform);
            L.color = ScaledColor(e.spot->color, e.spot->intensity);
            L.specColor = e.spot->specColor;
            L.specPower = e.spot->specPower;
            L.range = e.spot->range;
            L.attenuation = e.spot->attenuation;
            L.innerCos = DegToInnerCos(e.spot->innerAngleDeg);
            L.outerCos = DegToInnerCos(e.spot->outerAngleDeg);
            Lighting::AddSpotLight(L);
            // Spot shadow maps are out of scope for the CSM directional path.
        }
    }

    Shadow::SetCasterLightIndex(casterLightIndex);
    Lighting::SetAmbientColor({ 0.6f, 0.7f, 0.8f });
}
