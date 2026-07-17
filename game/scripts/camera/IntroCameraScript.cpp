#include "IntroCameraScript.h"

#include "engine/core/engine_gui.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

#include <cmath>

using namespace DirectX;

REGISTER_SCRIPT(IntroCameraScript)

namespace {
constexpr float kOrbitRadius = 30.0f * 1.41421356f;
constexpr float kOrbitPitch = 0.78539816339f;
constexpr float kOrbitYawSpeed = 0.15f;

bool s_active = false;
float s_orbitYaw = 3.14159265f;

void SetInitialPose() {
    ScriptApi_CameraSetPosition({ 0.0f, 30.0f, -30.0f });
    ScriptApi_CameraSetTarget({ 0.0f, 0.0f, 0.0f });
    ScriptApi_CameraSetUp({ 0.0f, 1.0f, 0.0f });
}

void ApplyOrbitPose() {
    const float cosPitch = std::cosf(kOrbitPitch);
    const float sinPitch = std::sinf(kOrbitPitch);
    const float sinYaw = std::sinf(s_orbitYaw);
    const float cosYaw = std::cosf(s_orbitYaw);

    ScriptApi_CameraSetPosition({
        kOrbitRadius * cosPitch * sinYaw,
        kOrbitRadius * sinPitch,
        kOrbitRadius * cosPitch * cosYaw,
    });
    ScriptApi_CameraSetTarget({ 0.0f, 0.0f, 0.0f });
    ScriptApi_CameraSetUp({ 0.0f, 1.0f, 0.0f });
}
} // namespace

void IntroCameraScript::OnCreate(engine::GameObject& /*self*/) {
}

void IntroCameraScript::OnDestroy(engine::GameObject& /*self*/) {
    s_active = false;
}

void IntroCameraScript::Begin() {
    s_active = true;
    s_orbitYaw = 3.14159265f;
    SetInitialPose();
    EngineGUI_SetCameraMode(CameraMode::Intro);
}

void IntroCameraScript::End() {
    s_active = false;
}

bool IntroCameraScript::IsActive() {
    return s_active;
}

void IntroCameraScript::OnUpdate(engine::GameObject& /*self*/, float dt) {
    if (!s_active)
        return;

    s_orbitYaw += kOrbitYawSpeed * dt;
    ApplyOrbitPose();
}
