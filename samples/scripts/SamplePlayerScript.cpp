#include "SamplePlayerScript.h"

#include "engine/ecs/transform.h"
#include "engine/input/controller.h"
#include "engine/input/keyboard.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "third_person_camera.h"

#include <cmath>

REGISTER_SCRIPT(SamplePlayerScript)

namespace {
    DirectX::XMFLOAT3 GetMoveInput() {
        DirectX::XMFLOAT3 move = { 0.0f, 0.0f, 0.0f };
        if (const Keyboard_State* state = Keyboard_GetState()) {
            if (state->W || state->Up)
                move.z += 1.0f;
            if (state->S || state->Down)
                move.z -= 1.0f;
            if (state->A || state->Left)
                move.x -= 1.0f;
            if (state->D || state->Right)
                move.x += 1.0f;
        }

        const DirectX::XMFLOAT2 stick = Controller_GetLeftStick(0);
        move.x += stick.x;
        move.z += stick.y;

        const float lenSq = move.x * move.x + move.z * move.z;
        if (lenSq > 1.0f) {
            const float inv = 1.0f / std::sqrt(lenSq);
            move.x *= inv;
            move.z *= inv;
        }
        return move;
    }
}

void SamplePlayerScript::OnCreate(engine::GameObject& self) {
    self.GetRigidbody().SetIsKinematic(false);
    self.GetRigidbody().SetUseGravity(true);
    self.GetRigidbody().SetDrag(2.0f);
    self.GetRigidbody().SetMass(1.0f);

    if (engine::Transform* t = ScriptApi_FindTransform(self))
        m_yaw = t->localRotation.y;
    else
        m_yaw = self.GetRotation().y;

    ThirdPersonCamera_Initialize();
    ThirdPersonCamera_SetTarget(&self.m_position, &m_yaw);
    ThirdPersonCamera_SetMouseSensitivity(0.15f);
    ThirdPersonCamera_SetPitchLimits(-35.0f, 60.0f);
    ThirdPersonCamera_ShowCursor(false);

    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->engineGuiSetCameraMode)
        api->engineGuiSetCameraMode(CameraMode::ThirdPerson);
}

void SamplePlayerScript::OnUpdate(engine::GameObject& self, float dt) {
    (void)dt;
    const DirectX::XMFLOAT3 input = GetMoveInput();
    DirectX::XMFLOAT3 forward = ScriptApi_GetCameraForward();
    forward.y = 0.0f;
    const float fwdLenSq = forward.x * forward.x + forward.z * forward.z;
    if (fwdLenSq > 0.0001f) {
        const float inv = 1.0f / std::sqrt(fwdLenSq);
        forward.x *= inv;
        forward.z *= inv;
    } else {
        forward = { 0.0f, 0.0f, 1.0f };
    }

    const DirectX::XMFLOAT3 right = { forward.z, 0.0f, -forward.x };
    DirectX::XMFLOAT3 wish = {
        right.x * input.x + forward.x * input.z,
        0.0f,
        right.z * input.x + forward.z * input.z
    };

    auto& rb = self.GetRigidbody();
    DirectX::XMFLOAT3 vel = rb.GetVelocity();
    vel.x = wish.x * moveSpeed;
    vel.z = wish.z * moveSpeed;
    rb.SetVelocity(vel);

    if (std::fabs(wish.x) + std::fabs(wish.z) > 0.01f)
        m_yaw = std::atan2(wish.x, wish.z) * (180.0f / 3.14159265f);

    DirectX::XMFLOAT3 rot = self.GetRotation();
    rot.y = m_yaw;
    self.SetRotation(rot);
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = self.GetPosition();
        t->localRotation = rot;
    }
}

void SamplePlayerScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    ThirdPersonCamera_SetTarget(nullptr, nullptr);
    ThirdPersonCamera_ShowCursor(true);
}
