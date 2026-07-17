#include "camera_gizmo.h"
#include "engine/debug/debug_drawer.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include <cmath>
#include <vector>

using namespace DirectX;

namespace {

void AppendLine(std::vector<XMFLOAT3>& lines, XMFLOAT3 a, XMFLOAT3 b) {
    lines.push_back(a);
    lines.push_back(b);
}

XMFLOAT3 AddScaled(const XMFLOAT3& origin, const XMFLOAT3& dir, float scale) {
    return { origin.x + dir.x * scale, origin.y + dir.y * scale, origin.z + dir.z * scale };
}

void VisitDraw(GameObject* node, DebugDrawer& drawer) {
    if (!node || !node->IsActive()) return;

    if (auto* cam = node->GetComponent<CameraComponent>()) {
        if (cam->enabled) {
            const engine::Transform* t = node->GetComponent<engine::Transform>();
            CameraGizmo::DrawCamera(drawer, t, cam);
        }
    }

    for (const auto& child : node->GetChildren())
        if (child) VisitDraw(child.get(), drawer);
}

} // namespace

namespace CameraGizmo {

void DrawCamera(DebugDrawer& drawer, const engine::Transform* transform,
    const CameraComponent* camera)
{
    if (!transform || !camera) return;

    const XMFLOAT3 eye = CameraComponent::ComputeEyeWS(transform);
    const XMFLOAT3 forward = CameraComponent::ComputeForwardWS(transform);
    const XMFLOAT3 up = CameraComponent::ComputeUpWS(transform);
    const XMFLOAT3 right = CameraComponent::ComputeRightWS(transform);

    const float axisLen = 1.5f;
    drawer.DrawLine(eye, AddScaled(eye, right, axisLen), { 1.0f, 0.2f, 0.2f, 1.0f });   // right
    drawer.DrawLine(eye, AddScaled(eye, up, axisLen), { 0.2f, 1.0f, 0.2f, 1.0f });      // up
    drawer.DrawLine(eye, AddScaled(eye, forward, axisLen), { 0.2f, 0.6f, 1.0f, 1.0f }); // forward

    // Near-plane frustum wire (FOV-based).
    const float nearDist = 1.2f;
    const float halfV = tanf(XMConvertToRadians(camera->fov * 0.5f)) * nearDist;
    const float halfH = halfV * 1.333f; // approx 4:3; aspect not critical for gizmo

    const XMFLOAT3 center = AddScaled(eye, forward, nearDist);
    const XMFLOAT3 c0 = {
        center.x - right.x * halfH - up.x * halfV,
        center.y - right.y * halfH - up.y * halfV,
        center.z - right.z * halfH - up.z * halfV
    };
    const XMFLOAT3 c1 = {
        center.x + right.x * halfH - up.x * halfV,
        center.y + right.y * halfH - up.y * halfV,
        center.z + right.z * halfH - up.z * halfV
    };
    const XMFLOAT3 c2 = {
        center.x + right.x * halfH + up.x * halfV,
        center.y + right.y * halfH + up.y * halfV,
        center.z + right.z * halfH + up.z * halfV
    };
    const XMFLOAT3 c3 = {
        center.x - right.x * halfH + up.x * halfV,
        center.y - right.y * halfH + up.y * halfV,
        center.z - right.z * halfH + up.z * halfV
    };

    std::vector<XMFLOAT3> lines;
    lines.reserve(32);
    AppendLine(lines, eye, c0);
    AppendLine(lines, eye, c1);
    AppendLine(lines, eye, c2);
    AppendLine(lines, eye, c3);
    AppendLine(lines, c0, c1);
    AppendLine(lines, c1, c2);
    AppendLine(lines, c2, c3);
    AppendLine(lines, c3, c0);
    drawer.DrawLines(lines, { 0.85f, 0.85f, 0.2f, 1.0f });
}

void DrawSceneCameras(const std::vector<std::unique_ptr<GameObject>>& roots, DebugDrawer& drawer) {
    for (const auto& root : roots)
        if (root) VisitDraw(root.get(), drawer);
}

} // namespace CameraGizmo
