// light_gizmo.h
// Editor gizmo for light visualization.

#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>

class DebugDrawer;
class GameObject;

namespace engine { class Transform; }

namespace LightGizmo {

DirectX::XMFLOAT3 GetWorldPosition(const engine::Transform* transform);
DirectX::XMFLOAT3 GetShineDirectionWS(const engine::Transform* transform);
DirectX::XMFLOAT3 GetIncomingDirectionWS(const engine::Transform* transform);

void DrawDirectional(DebugDrawer& drawer, const engine::Transform* transform,
    const DirectX::XMFLOAT4& color);
void DrawPoint(DebugDrawer& drawer, const engine::Transform* transform, float range,
    const DirectX::XMFLOAT4& color);
void DrawSpot(DebugDrawer& drawer, const engine::Transform* transform, float range,
    float outerAngleDeg, float innerAngleDeg, const DirectX::XMFLOAT4& color);

void DrawSceneLights(const std::vector<std::unique_ptr<GameObject>>& roots, DebugDrawer& drawer);

} // namespace LightGizmo
