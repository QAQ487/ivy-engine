#include "light_gizmo.h"
#include "engine/debug_draw/debug_drawer.h"
#include "engine/ecs/directional_light_component.h"
#include "engine/ecs/point_light_component.h"
#include "engine/ecs/spot_light_component.h"
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

void BuildPerpBasis(XMVECTOR dir, XMVECTOR& outA, XMVECTOR& outB) {
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    if (fabsf(XMVectorGetX(XMVector3Dot(dir, up))) > 0.95f)
        up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    outA = XMVector3Normalize(XMVector3Cross(up, dir));
    outB = XMVector3Normalize(XMVector3Cross(dir, outA));
}

void VisitDraw(GameObject* node, DebugDrawer& drawer) {
    if (!node || !node->IsActive()) return;

    const engine::Transform* t = node->GetComponent<engine::Transform>();
    if (auto* dl = node->GetComponent<DirectionalLightComponent>()) {
        if (dl->enabled) {
            const XMFLOAT4 c = { dl->color.x, dl->color.y, dl->color.z, 1.0f };
            LightGizmo::DrawDirectional(drawer, t, c);
        }
    }
    if (auto* pl = node->GetComponent<PointLightComponent>()) {
        if (pl->enabled) {
            const XMFLOAT4 c = { pl->color.x, pl->color.y, pl->color.z, 1.0f };
            LightGizmo::DrawPoint(drawer, t, pl->range, c);
        }
    }
    if (auto* sl = node->GetComponent<SpotLightComponent>()) {
        if (sl->enabled) {
            const XMFLOAT4 c = { sl->color.x, sl->color.y, sl->color.z, 1.0f };
            LightGizmo::DrawSpot(drawer, t, sl->range, sl->outerAngleDeg, sl->innerAngleDeg, c);
        }
    }

    for (const auto& child : node->GetChildren())
        if (child) VisitDraw(child.get(), drawer);
}

} // namespace

namespace LightGizmo {

XMFLOAT3 GetWorldPosition(const engine::Transform* transform) {
    if (!transform) return { 0.0f, 0.0f, 0.0f };
    const XMMATRIX world = transform->GetWorldMatrix();
    XMFLOAT3 pos {};
    XMStoreFloat3(&pos, world.r[3]);
    return pos;
}

XMFLOAT3 GetShineDirectionWS(const engine::Transform* transform) {
    if (!transform) return { 0.0f, -1.0f, 0.0f };
    const XMMATRIX world = transform->GetWorldMatrix();
    XMVECTOR localShine = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR worldShine = XMVector3TransformNormal(localShine, world);
    if (XMVector3Equal(worldShine, XMVectorZero()))
        worldShine = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
    else
        worldShine = XMVector3Normalize(worldShine);
    XMFLOAT3 dir {};
    XMStoreFloat3(&dir, worldShine);
    return dir;
}

XMFLOAT3 GetIncomingDirectionWS(const engine::Transform* transform) {
    const XMFLOAT3 shine = GetShineDirectionWS(transform);
    return { -shine.x, -shine.y, -shine.z };
}

void DrawDirectional(DebugDrawer& drawer, const engine::Transform* transform,
    const XMFLOAT4& color)
{
    const XMFLOAT3 pos = GetWorldPosition(transform);
    const XMFLOAT3 shine = GetShineDirectionWS(transform);
    const XMVECTOR dir = XMLoadFloat3(&shine);
    const XMVECTOR origin = XMLoadFloat3(&pos);

    XMVECTOR axisA {}, axisB {};
    BuildPerpBasis(dir, axisA, axisB);

    const float rayLen = 3.0f;
    const float diskR = 0.6f;
    const float spread = 0.35f;

    std::vector<XMFLOAT3> lines;
    lines.reserve(64);

    const int diskSeg = 12;
    for (int i = 0; i < diskSeg; ++i) {
        const float a0 = (float)i / diskSeg * XM_2PI;
        const float a1 = (float)(i + 1) / diskSeg * XM_2PI;
        XMVECTOR p0 = origin + (axisA * cosf(a0) + axisB * sinf(a0)) * diskR;
        XMVECTOR p1 = origin + (axisA * cosf(a1) + axisB * sinf(a1)) * diskR;
        XMFLOAT3 f0 {}, f1 {};
        XMStoreFloat3(&f0, p0);
        XMStoreFloat3(&f1, p1);
        AppendLine(lines, f0, f1);
    }

    const XMFLOAT3 offsets[] = {
        { 0.0f, 0.0f, 0.0f },
        { spread, 0.0f, 0.0f },
        { -spread, 0.0f, 0.0f },
        { 0.0f, spread, 0.0f },
        { 0.0f, -spread, 0.0f },
    };

    for (const XMFLOAT3& off : offsets) {
        XMVECTOR offV = axisA * off.x + axisB * off.y;
        XMVECTOR start = origin + offV;
        XMVECTOR end = start + dir * rayLen;
        XMFLOAT3 s {}, e {};
        XMStoreFloat3(&s, start);
        XMStoreFloat3(&e, end);
        AppendLine(lines, s, e);
    }

    drawer.DrawLines(lines, color);
}

void DrawPoint(DebugDrawer& drawer, const engine::Transform* transform, float range,
    const XMFLOAT4& color)
{
    const XMFLOAT3 pos = GetWorldPosition(transform);
    const float iconR = 0.25f;
    const float rayLen = (range > 0.0f) ? fminf(range * 0.2f, 2.0f) : 1.0f;

    drawer.DrawSphere(pos, { 0.0f, 0.0f, 0.0f }, iconR, { 0.0f, 0.0f, 0.0f }, color, false);

    std::vector<XMFLOAT3> lines;
    const XMFLOAT3 dirs[] = {
        { 1, 0, 0 }, { -1, 0, 0 },
        { 0, 1, 0 }, { 0, -1, 0 },
        { 0, 0, 1 }, { 0, 0, -1 },
    };
    for (const XMFLOAT3& d : dirs) {
        XMFLOAT3 end = {
            pos.x + d.x * rayLen,
            pos.y + d.y * rayLen,
            pos.z + d.z * rayLen
        };
        AppendLine(lines, pos, end);
    }
    drawer.DrawLines(lines, color);

    if (range > 0.0f)
        drawer.DrawSphere(pos, { 0.0f, 0.0f, 0.0f }, range, { 0.0f, 0.0f, 0.0f },
            { color.x, color.y, color.z, 0.35f }, false);
}

void DrawSpot(DebugDrawer& drawer, const engine::Transform* transform, float range,
    float outerAngleDeg, float innerAngleDeg, const XMFLOAT4& color)
{
    const XMFLOAT3 pos = GetWorldPosition(transform);
    const XMFLOAT3 shine = GetShineDirectionWS(transform);
    const XMVECTOR apex = XMLoadFloat3(&pos);
    const XMVECTOR dir = XMLoadFloat3(&shine);

    XMVECTOR axisA {}, axisB {};
    BuildPerpBasis(dir, axisA, axisB);

    const float gizmoRange = (range > 0.0f) ? fminf(range, 5.0f) : 3.0f;
    const float outerRad = XMConvertToRadians(outerAngleDeg);
    const float innerRad = XMConvertToRadians(innerAngleDeg);
    const float outerR = tanf(outerRad) * gizmoRange;
    const float innerR = tanf(innerRad) * gizmoRange;
    const XMVECTOR baseCenter = apex + dir * gizmoRange;

    std::vector<XMFLOAT3> lines;
    const int seg = 16;

    auto addRing = [&](float radius, float alpha) {
        for (int i = 0; i < seg; ++i) {
            const float a0 = (float)i / seg * XM_2PI;
            const float a1 = (float)(i + 1) / seg * XM_2PI;
            XMVECTOR p0 = baseCenter + (axisA * cosf(a0) + axisB * sinf(a0)) * radius;
            XMVECTOR p1 = baseCenter + (axisA * cosf(a1) + axisB * sinf(a1)) * radius;
            XMFLOAT3 f0 {}, f1 {};
            XMStoreFloat3(&f0, p0);
            XMStoreFloat3(&f1, p1);
            AppendLine(lines, f0, f1);
        }
        XMFLOAT4 ringColor = { color.x, color.y, color.z, alpha };
        drawer.DrawLines(lines, ringColor);
        lines.clear();
    };

    for (int i = 0; i < seg; ++i) {
        const float a = (float)i / seg * XM_2PI;
        XMVECTOR rim = baseCenter + (axisA * cosf(a) + axisB * sinf(a)) * outerR;
        XMFLOAT3 f {};
        XMStoreFloat3(&f, rim);
        AppendLine(lines, pos, f);
    }
    drawer.DrawLines(lines, color);
    lines.clear();

    addRing(outerR, 1.0f);
    if (innerR > 0.01f && innerAngleDeg < outerAngleDeg)
        addRing(innerR, 0.55f);

    drawer.DrawSphere(pos, { 0.0f, 0.0f, 0.0f }, 0.2f, { 0.0f, 0.0f, 0.0f }, color, false);
}

void DrawSceneLights(const std::vector<std::unique_ptr<GameObject>>& roots, DebugDrawer& drawer) {
    for (const auto& root : roots)
        if (root) VisitDraw(root.get(), drawer);
}

} // namespace LightGizmo
