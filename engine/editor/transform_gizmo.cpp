// transform_gizmo.cpp
// ImGuizmo-based transform editing gizmo.

#include "transform_gizmo.h"

#include "engine/core/engine_gui.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/editor/editor_icon_button.h"
#include "engine/editor/editor_undo.h"
#include "engine/scene/json_float.h"
#include "camera.h"
#include "debug_camera.h"
#include "engine/input/mouse.h"
#include "imgui/imgui.h"
#include "imgui/icons_font_awesome.h"
#include "imguizmo/ImGuizmo.h"

#include <cmath>
#include <cstring>
#include <string>

using namespace DirectX;

namespace {

ImGuizmo::OPERATION g_operation = ImGuizmo::TRANSLATE;
ImGuizmo::MODE      g_mode = ImGuizmo::LOCAL;
bool                g_visible = true;

// ImGuizmo expects column-major view/proj in its own LH/RH convention (see official example).
static constexpr bool kLeftHanded = true;

static float Dot3(const float* a, const float* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void Normalize3(const float* a, float* r) {
    const float invLen = 1.0f / (sqrtf(Dot3(a, a)) + FLT_EPSILON);
    r[0] = a[0] * invLen;
    r[1] = a[1] * invLen;
    r[2] = a[2] * invLen;
}

static void Cross3(const float* a, const float* b, float* r) {
    r[0] = a[1] * b[2] - a[2] * b[1];
    r[1] = a[2] * b[0] - a[0] * b[2];
    r[2] = a[0] * b[1] - a[1] * b[0];
}

static void BuildImGuizmoLookAt(const float* eye, const float* at, const float* up, float* m16) {
    float x[3], y[3], z[3], tmp[3];

    if (kLeftHanded) {
        tmp[0] = at[0] - eye[0];
        tmp[1] = at[1] - eye[1];
        tmp[2] = at[2] - eye[2];
    } else {
        tmp[0] = eye[0] - at[0];
        tmp[1] = eye[1] - at[1];
        tmp[2] = eye[2] - at[2];
    }
    Normalize3(tmp, z);
    Normalize3(up, y);

    Cross3(y, z, tmp);
    Normalize3(tmp, x);

    Cross3(z, x, tmp);
    Normalize3(tmp, y);

    m16[0] = x[0];  m16[1] = y[0];  m16[2] = z[0];  m16[3] = 0.0f;
    m16[4] = x[1];  m16[5] = y[1];  m16[6] = z[1];  m16[7] = 0.0f;
    m16[8] = x[2];  m16[9] = y[2];  m16[10] = z[2]; m16[11] = 0.0f;
    m16[12] = -Dot3(x, eye);
    m16[13] = -Dot3(y, eye);
    m16[14] = -Dot3(z, eye);
    m16[15] = 1.0f;
}

static void BuildImGuizmoPerspective(float fovyDeg, float aspect, float znear, float zfar, float* m16) {
    // Match XMMatrixPerspectiveFovLH: ymax uses half the vertical FOV angle.
    const float ymax = znear * tanf(fovyDeg * 0.5f * 3.14159265f / 180.0f);
    const float xmax = ymax * aspect;
    const float temp = 2.0f * znear;
    const float sign = kLeftHanded ? 1.0f : -1.0f;

    std::memset(m16, 0, sizeof(float) * 16);
    m16[0] = temp / (2.0f * xmax);
    m16[5] = temp / (2.0f * ymax);
    m16[10] = sign * (zfar + znear) / (zfar - znear);
    m16[11] = sign;
    m16[14] = -(temp * zfar) / (zfar - znear);
}

static XMFLOAT3 TranslationFromXmMatrix(XMMATRIX m) {
    XMFLOAT4X4 stored;
    XMStoreFloat4x4(&stored, m);
    return { stored._41, stored._42, stored._43 };
}

static void BuildCameraMatrices(const Camera& cam, float vw, float vh,
    float view[16], float proj[16])
{
    const float eye[3] = { cam.position.x, cam.position.y, cam.position.z };
    const float at[3]  = { cam.at.x, cam.at.y, cam.at.z };
    const float up[3]  = { cam.up.x, cam.up.y, cam.up.z };
    BuildImGuizmoLookAt(eye, at, up, view);

    const float aspect = (vh > 0.0f) ? (vw / vh) : 1.0f;
    if (cam.orthographic) {
        const float height = (std::max)(0.001f, cam.orthographicHeight);
        std::memset(proj, 0, sizeof(float) * 16);
        proj[0] = 2.0f / (height * aspect);
        proj[5] = 2.0f / height;
        proj[10] = 2.0f / (cam.farClip - cam.nearClip);
        proj[14] = -(cam.farClip + cam.nearClip) / (cam.farClip - cam.nearClip);
        proj[15] = 1.0f;
    } else BuildImGuizmoPerspective(cam.fov, aspect, cam.nearClip, cam.farClip, proj);
}

static XMFLOAT3 EulerZyxDegreesFromRotationMatrix(XMMATRIX rot) {
    const float r00 = rot.r[0].m128_f32[0];
    const float r10 = rot.r[1].m128_f32[0];
    const float r20 = rot.r[2].m128_f32[0];
    const float r21 = rot.r[2].m128_f32[1];
    const float r22 = rot.r[2].m128_f32[2];
    const float r11 = rot.r[1].m128_f32[1];
    const float r12 = rot.r[1].m128_f32[2];

    const float sy = sqrtf(r00 * r00 + r10 * r10);
    float rotX, rotY, rotZ;
    if (sy > 1e-6f) {
        rotX = atan2f(r21, r22);
        rotY = atan2f(-r20, sy);
        rotZ = atan2f(r10, r00);
    } else {
        rotX = atan2f(-r12, r11);
        rotY = atan2f(-r20, sy);
        rotZ = 0.0f;
    }

    return {
        XMConvertToDegrees(rotX),
        XMConvertToDegrees(rotY),
        XMConvertToDegrees(rotZ)
    };
}

static void ExtractWorldTRS(const engine::Transform* transform,
    XMFLOAT3& position, XMFLOAT3& rotation, XMFLOAT3& scale)
{
    const XMMATRIX world = transform->GetWorldMatrix();
    XMVECTOR s, q, t;
    XMMatrixDecompose(&s, &q, &t, world);
    XMStoreFloat3(&position, t);
    XMStoreFloat3(&scale, s);
    rotation = EulerZyxDegreesFromRotationMatrix(XMMatrixRotationQuaternion(q));
}

static void PoseToMatrix(const XMFLOAT3& position, const XMFLOAT3& rotation,
    const XMFLOAT3& scale, float matrix[16])
{
    const float translation[3] = { position.x, position.y, position.z };
    const float rot[3] = { rotation.x, rotation.y, rotation.z };
    const float scl[3] = { scale.x, scale.y, scale.z };
    ImGuizmo::RecomposeMatrixFromComponents(translation, rot, scl, matrix);
}

static void MatrixToPose(const float matrix[16], XMFLOAT3& position,
    XMFLOAT3& rotation, XMFLOAT3& scale)
{
    float translation[3], rot[3], scl[3];
    ImGuizmo::DecomposeMatrixToComponents(matrix, translation, rot, scl);
    position = { translation[0], translation[1], translation[2] };
    rotation = { rot[0], rot[1], rot[2] };
    scale    = { scl[0], scl[1], scl[2] };
}

static XMMATRIX MatrixColumnMajorToXm(const float matrix[16]) {
    XMFLOAT4X4 loaded;
    std::memcpy(&loaded, matrix, sizeof(loaded));
    return XMMatrixTranspose(XMLoadFloat4x4(&loaded));
}

static void XmToImGuizmoMatrix(XMMATRIX m, float out[16]) {
    // XMMATRIX is row-major in memory; ImGuizmo expects column-major float[16].
    XMFLOAT4X4 stored;
    XMStoreFloat4x4(&stored, XMMatrixTranspose(m));
    std::memcpy(out, &stored, sizeof(stored));
}

static XMMATRIX GetParentWorldMatrix(const GameObject* obj) {
    if (!obj) return XMMatrixIdentity();
    const GameObject* parent = obj->GetParent();
    if (!parent) return XMMatrixIdentity();
    if (const engine::Transform* parentXform = parent->GetComponent<engine::Transform>())
        return parentXform->GetWorldMatrix();
    return XMMatrixIdentity();
}

static void PackTransformPose(const engine::Transform& t, float out9[9]) {
    out9[0] = t.localPosition.x; out9[1] = t.localPosition.y; out9[2] = t.localPosition.z;
    out9[3] = t.localRotation.x; out9[4] = t.localRotation.y; out9[5] = t.localRotation.z;
    out9[6] = t.localScale.x;    out9[7] = t.localScale.y;    out9[8] = t.localScale.z;
}

static void ApplyMatrixToTransform(engine::Transform* transform, GameObject* owner,
    const float matrix[16], ImGuizmo::MODE mode, ImGuizmo::OPERATION operation)
{
    if (!transform || !owner) return;

    float igT[3], igR[3], igS[3];
    ImGuizmo::DecomposeMatrixToComponents(matrix, igT, igR, igS);

    const bool translateOnly =
        (operation & ImGuizmo::TRANSLATE) != 0 &&
        (operation & (ImGuizmo::ROTATE | ImGuizmo::SCALE)) == 0;

    if (mode == ImGuizmo::WORLD && owner->GetParent()) {
        const XMMATRIX parentInv = XMMatrixInverse(nullptr, GetParentWorldMatrix(owner));
        XMVECTOR worldPos = XMVectorSet(igT[0], igT[1], igT[2], 1.0f);
        XMStoreFloat3(&transform->localPosition, XMVector3TransformCoord(worldPos, parentInv));

        if (!translateOnly) {
            float worldMat[16];
            ImGuizmo::RecomposeMatrixFromComponents(igT, igR, igS, worldMat);
            XMMATRIX world = MatrixColumnMajorToXm(worldMat);
            transform->SetFromLocalMatrix(world * parentInv);
        }
    } else {
        transform->localPosition = { igT[0], igT[1], igT[2] };
        // Translate-only must not rewrite rot/scale: ImGuizmo recompose/decompose
        // injects float noise that marked the scene dirty on incidental clicks.
        if (!translateOnly) {
            transform->localRotation = { igR[0], igR[1], igR[2] };
            transform->localScale    = { igS[0], igS[1], igS[2] };
        }
    }

    owner->SyncLegacyTransformFromComponents();
}

static bool IsMouseInGameViewport() {
    float vx, vy, vw, vh;
    EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
    if (vw <= 0.0f || vh <= 0.0f) return false;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    return mouse.x >= vx && mouse.x < (vx + vw)
        && mouse.y >= vy && mouse.y < (vy + vh);
}

} // namespace

namespace TransformGizmo {

void BeginFrame() {
    ImGuizmo::BeginFrame();
}

bool IsInteracting() {
    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

void DrawToolbarControls() {
    if (editor_icon_button::IconToggleTooltip(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT,
            g_operation == ImGuizmo::TRANSLATE, "Move (W)")) {
        g_visible = true;
        g_operation = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (editor_icon_button::IconToggleTooltip(ICON_FA_ROTATE,
            g_operation == ImGuizmo::ROTATE, "Rotate (E)")) {
        g_visible = true;
        g_operation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (editor_icon_button::IconToggleTooltip(ICON_FA_UP_DOWN_LEFT_RIGHT,
            g_operation == ImGuizmo::SCALE, "Scale (R)")) {
        g_visible = true;
        g_operation = ImGuizmo::SCALE;
    }

    if (g_operation != ImGuizmo::SCALE) {
        ImGui::SameLine();
        if (editor_icon_button::IconToggleTooltip(
                ICON_FA_LOCATION_DOT, g_mode == ImGuizmo::LOCAL, "Local space"))
            g_mode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (editor_icon_button::IconToggleTooltip(
                ICON_FA_GLOBE, g_mode == ImGuizmo::WORLD, "World space"))
            g_mode = ImGuizmo::WORLD;
    }
}

void HandleHotkeys() {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (Mouse_IsRightDown()) return;
    if (!IsMouseInGameViewport()) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Q))
        g_visible = false;
    if (ImGui::IsKeyPressed(ImGuiKey_W)) {
        g_visible = true;
        g_operation = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
        g_visible = true;
        g_operation = ImGuizmo::ROTATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
        g_visible = true;
        g_operation = ImGuizmo::SCALE;
    }
}

void SetVisible(bool visible) {
    g_visible = visible;
}

bool IsVisible() {
    return g_visible;
}

void Draw(GameObject* selected) {
    if (!selected) return;
    if (!g_visible) return;

    engine::Transform* transform = selected->GetComponent<engine::Transform>();
    if (!transform) return;

    float vx, vy, vw, vh;
    EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
    if (vw <= 0.0f || vh <= 0.0f) return;

    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(vx, vy, vw, vh);
    ImGuizmo::SetOrthographic(GetCamera().orthographic);
    ImGuizmo::Enable(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow));

    float view[16], proj[16], matrix[16];
    BuildCameraMatrices(GetCamera(), vw, vh, view, proj);

    if (g_mode == ImGuizmo::WORLD) {
        XMFLOAT3 wPos, wRot, wScale;
        ExtractWorldTRS(transform, wPos, wRot, wScale);
        wPos = TranslationFromXmMatrix(transform->GetWorldMatrix());
        PoseToMatrix(wPos, wRot, wScale, matrix);
    } else {
        PoseToMatrix(transform->localPosition, transform->localRotation,
            transform->localScale, matrix);
    }

    static bool s_wasUsing = false;
    static std::string s_beforeJson;
    static GameObject* s_dragNode = nullptr;

    // While idle, keep a fresh pre-edit snapshot so the rising edge is correct.
    if (!s_wasUsing)
        s_beforeJson = transform->Serialize();

    const auto operation=DebugCamera_Is2DView()
        ? (g_operation==ImGuizmo::TRANSLATE ? ImGuizmo::OPERATION(ImGuizmo::TRANSLATE_X|ImGuizmo::TRANSLATE_Y)
          : g_operation==ImGuizmo::ROTATE ? ImGuizmo::ROTATE_Z
          : ImGuizmo::OPERATION(ImGuizmo::SCALE_X|ImGuizmo::SCALE_Y)) : g_operation;
    const bool changed = ImGuizmo::Manipulate(
        view, proj, operation, g_mode, matrix);

    if (changed) {
        float before9[9];
        PackTransformPose(*transform, before9);
        const float savedZ=transform->localPosition.z, savedRX=transform->localRotation.x,
            savedRY=transform->localRotation.y, savedSZ=transform->localScale.z;
        ApplyMatrixToTransform(transform, selected, matrix, g_mode, operation);
        if(DebugCamera_Is2DView()) {
            transform->localPosition.z=savedZ; transform->localRotation.x=savedRX;
            transform->localRotation.y=savedRY; transform->localScale.z=savedSZ;
            selected->SyncLegacyTransformFromComponents();
        }
        float after9[9];
        PackTransformPose(*transform, after9);
        if (JsonFloat_TransformChanged(before9, after9)) {
            transform->SnapshotStartState();
            EngineGUI_MarkSceneDirty();
        } else {
            transform->localPosition = { before9[0], before9[1], before9[2] };
            transform->localRotation = { before9[3], before9[4], before9[5] };
            transform->localScale    = { before9[6], before9[7], before9[8] };
            selected->SyncLegacyTransformFromComponents();
        }
    }

    const bool usingNow = ImGuizmo::IsUsing();
    if (usingNow && !s_wasUsing)
        s_dragNode = selected;

    if (!usingNow && s_wasUsing) {
        if (s_dragNode == selected && !EngineGUI_IsPlaying()) {
            const std::string afterJson = transform->Serialize();
            EditorUndo_PushTransform(selected, s_beforeJson, afterJson);
        }
        s_beforeJson.clear();
        s_dragNode = nullptr;
    }
    s_wasUsing = usingNow;
}

} // namespace TransformGizmo
