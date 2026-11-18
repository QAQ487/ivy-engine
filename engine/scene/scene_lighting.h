// scene_lighting.h
// Scene lighting setup and sync helpers.

#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>

class GameObject;

// Fills the fixed GPU light slots from enabled scene lights. With more lights than
// slots and render.lightSelectionEnabled, the most influential lights at
// cameraPosition are used (see light_select_math.h); otherwise the first ones in
// scene order.
void SceneLightingSyncFromNodes(const std::vector<std::unique_ptr<GameObject>>& roots,
                                const DirectX::XMFLOAT3& cameraPosition);

// Totals from the last sync: enabled lights found vs lights placed in GPU slots.
struct SceneLightingStats {
    int total = 0;
    int used = 0;
};
SceneLightingStats SceneLighting_GetLastStats();
