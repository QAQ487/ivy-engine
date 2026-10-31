#include "camera_gizmo.h"
#include "engine/debug_draw/debug_drawer.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/rendering/d3d11/renderer.h"
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

    // Preview the projection using the active render viewport aspect ratio.
    const float nearDist = 1.2f;
    const float viewportHeight = GetRenderViewportHeight();
    const float aspect = viewportHeight > 0.0f ? GetRenderViewportWidth() / viewportHeight : 1.0f;
    const float halfV = camera->orthographic
        ? (camera->orthographicHeight > 0.01f ? camera->orthographicHeight : 0.01f) * 0.5f
        : tanf(XMConvertToRadians(camera->fov * 0.5f)) * nearDist;
    const float halfH = halfV * aspect;

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
    if (camera->orthographic) {
        const XMFLOAT3 corners[] = { c0, c1, c2, c3 };
        XMFLOAT3 back[4];
        for (int i = 0; i < 4; ++i) {
            back[i] = AddScaled(corners[i], forward, -nearDist);
            AppendLine(lines, back[i], corners[i]);
        }
        for (int i = 0; i < 4; ++i)
            AppendLine(lines, back[i], back[(i + 1) % 4]);
    }
    else {
        AppendLine(lines, eye, c0);
        AppendLine(lines, eye, c1);
        AppendLine(lines, eye, c2);
        AppendLine(lines, eye, c3);
    }
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
