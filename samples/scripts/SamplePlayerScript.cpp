// SamplePlayerScript.cpp
// Sample script: SamplePlayerScript implementation.

#include "SamplePlayerScript.h"

#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/input/controller.h"
#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/physics/core/rigidbody.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "third_person_camera.h"
#include "third_person_move_math.h"

#include <cmath>
#include <string>

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

    VisualComponent* FindPlayerVisual(engine::GameObject& self) {
        if (VisualComponent* vc = ScriptApi_FindVisualComponent(self))
            return vc;
        for (const auto& child : self.GetChildren()) {
            if (!child)
                continue;
            if (child->GetName() != "Player_Model")
                continue;
            return ScriptApi_FindVisualComponent(*child);
        }
        for (const auto& child : self.GetChildren()) {
            if (!child)
                continue;
            if (VisualComponent* vc = ScriptApi_FindVisualComponent(*child))
                return vc;
        }
        return nullptr;
    }

    bool HasGroundSupport(engine::GameObject& self) {
        Collider* collider = self.GetCollider();
        if (!collider || collider->GetType() != ColliderType::SPHERE)
            return false;

        const DirectX::XMFLOAT3 center = ScriptApi_GetColliderWorldCenter(*collider);
        const float radius = ScriptApi_GetSphereColliderRadius(
            *static_cast<const SphereCollider*>(collider));

        DirectX::XMFLOAT3 origin = center;
        origin.y -= (radius - 0.03f);
        const DirectX::XMFLOAT3 down = { 0.0f, -1.0f, 0.0f };

        DirectX::XMFLOAT3 hitPoint{};
        DirectX::XMFLOAT3 hitNormal{};
        float hitDistance = 0.0f;
        if (!ScriptApi_Physics_Raycast(origin, down, 0.12f, &hitPoint, &hitNormal, &hitDistance))
            return false;

        // Own sphere exit normal points down; floor normal points up.
        return hitNormal.y > 0.5f
            && std::fabs(self.GetRigidbody().GetVelocity().y) < 0.35f;
    }
}

void SamplePlayerScript::OnCreate(engine::GameObject& self) {
    self.GetRigidbody().SetIsKinematic(false);
    self.GetRigidbody().SetUseGravity(true);
    self.GetRigidbody().SetDrag(2.0f);
    self.GetRigidbody().SetMass(1.0f);

    if (engine::Transform* t = ScriptApi_FindTransform(self))
        m_facingYaw = t->localRotation.y;
    else
        m_facingYaw = self.GetRotation().y;
    m_cameraYaw = m_facingYaw;

    ThirdPersonCamera_Initialize();
    // The camera orbit owns m_cameraYaw only; the character heading is m_facingYaw.
    ThirdPersonCamera_SetTarget(&self.m_position, &m_cameraYaw);
    ThirdPersonCamera_SetMouseSensitivity(0.15f);
    // Keep the camera above the ground plane (head height 3, distance 8).
    ThirdPersonCamera_SetPitchLimits(-20.0f, 60.0f);
    ThirdPersonCamera_ShowCursor(false);

    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->engineGuiSetCameraMode)
        api->engineGuiSetCameraMode(CameraMode::ThirdPerson);
}

void SamplePlayerScript::TryJump(engine::GameObject& self) {
    const bool jumpHeld = KeyLogger_IsTrigger(KK_SPACE)
        || (!m_wasJumpHeld && Controller_IsPressed(ControllerButton::A, 0));
    m_wasJumpHeld = Controller_IsPressed(ControllerButton::A, 0);

    if (!jumpHeld)
        return;

    auto& rb = self.GetRigidbody();
    const DirectX::XMFLOAT3 vel = rb.GetVelocity();
    const bool grounded = HasGroundSupport(self);
    if (!grounded)
        return;

    rb.AddForce({ 0.0f, jumpForce, 0.0f }, ForceMode::Impulse);

    VisualComponent* vc = FindPlayerVisual(self);
    if (!vc || !vc->playAnimation || jumpClip.empty())
        return;

    ScriptApi_Visual_PlayClipLoop(vc, jumpClip.c_str(), false);
    float duration = ScriptApi_Visual_GetClipDuration(vc);
    if (duration <= 0.01f)
        duration = 0.45f;

    const float g = Physics_GetGravity();
    const float gBase = g > 0.1f ? g : 9.81f;
    const float rise = jumpRiseMultiplier > 1.0f ? jumpRiseMultiplier : 1.0f;
    const float fall = fallMultiplier > 1.0f ? fallMultiplier : 1.0f;
    float mass = rb.GetMass();
    if (mass < 0.001f)
        mass = 1.0f;
    const float v0 = jumpForce / mass;
    const float airtime = (v0 / (gBase * rise)) + (v0 / std::sqrt(gBase * rise * gBase * fall));
    const float playAir = airtime > 0.08f ? airtime : 0.08f;
    ScriptApi_Visual_SetAnimSpeed(vc, duration / playAir);

    m_jumpOneShot = true;
    m_jumpLeftGround = false;
    m_jumpTakeoffY = self.GetPosition().y;
    m_locomotionClip.clear();
}

bool SamplePlayerScript::JumpHasLanded(engine::GameObject& self) {
    const DirectX::XMFLOAT3 pos = self.GetPosition();
    const DirectX::XMFLOAT3 vel = self.GetRigidbody().GetVelocity();
    if (!m_jumpLeftGround) {
        if (vel.y > 1.0f || pos.y > m_jumpTakeoffY + 0.08f)
            m_jumpLeftGround = true;
        return false;
    }
    return pos.y <= m_jumpTakeoffY + 0.12f && vel.y <= 0.25f;
}

void SamplePlayerScript::SyncPlayerAnimation(engine::GameObject& self, float dt) {
    (void)dt;
    VisualComponent* vc = FindPlayerVisual(self);
    if (!vc || !vc->playAnimation)
        return;

    if (m_jumpOneShot) {
        if (!JumpHasLanded(self))
            return;
        m_jumpOneShot = false;
        m_locomotionClip.clear();
    }

    const DirectX::XMFLOAT3 input = GetMoveInput();
    const bool moving = std::fabs(input.x) + std::fabs(input.z) > 0.01f;
    const std::string& targetClip = (moving && !runClip.empty()) ? runClip : idleClip;
    if (targetClip.empty() || targetClip == m_locomotionClip)
        return;

    m_locomotionClip = targetClip;
    ScriptApi_Visual_PlayClipLoop(vc, targetClip.c_str(), true);
    ScriptApi_Visual_SetAnimSpeed(vc, 1.0f);
}

void SamplePlayerScript::OnUpdate(engine::GameObject& self, float dt) {
    const DirectX::XMFLOAT3 input = GetMoveInput();
    // WASD relative to the camera orbit yaw. ThirdPersonCamera looks along
    // +(sin yaw, cos yaw); using the yaw directly (not the smoothed camera pose) keeps
    // movement exact and lag-free. See third_person_move_math.h (unit tested).
    const tp_move::Vec2 wishDir = tp_move::CameraRelativeWish(m_cameraYaw, input.x, input.z);
    const DirectX::XMFLOAT3 wish = { wishDir.x, 0.0f, wishDir.z };

    auto& rb = self.GetRigidbody();
    DirectX::XMFLOAT3 vel = rb.GetVelocity();
    vel.x = wish.x * moveSpeed;
    vel.z = wish.z * moveSpeed;
    rb.SetVelocity(vel);

    if (std::fabs(wish.x) + std::fabs(wish.z) > 0.01f) {
        const float targetYaw = tp_move::HeadingDeg(wishDir);
        m_facingYaw = turnSpeed > 0.0f
            ? tp_move::TurnToward(m_facingYaw, targetYaw, turnSpeed * dt)
            : targetYaw;
    }

    DirectX::XMFLOAT3 rot = self.GetRotation();
    rot.y = m_facingYaw;
    self.SetRotation(rot);
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = self.GetPosition();
        t->localRotation = rot;
    }

    // Cartoon jump: extra gravity while rising / falling (Integrate already applies -G).
    vel = rb.GetVelocity();
    float jumpMul = 1.0f;
    if (vel.y > 0.0f)
        jumpMul = jumpRiseMultiplier;
    else if (vel.y < 0.0f)
        jumpMul = fallMultiplier;
    if (jumpMul > 1.0f) {
        const float extraG = Physics_GetGravity() * (jumpMul - 1.0f);
        rb.AddForce({ 0.0f, -extraG, 0.0f }, ForceMode::Acceleration);
    }

    TryJump(self);
    SyncPlayerAnimation(self, dt);
}

void SamplePlayerScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    ThirdPersonCamera_SetTarget(nullptr, nullptr);
    ThirdPersonCamera_ShowCursor(true);
}
