// SamplePlayerScript.h
// Declares SamplePlayerScript.

#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

#include <string>

// WASD / stick mover with Space jump, Y Bot clips and a standard third-person rig:
// the mouse / right stick orbit the camera, WASD move relative to the camera, and
// the character turns toward its movement direction (camera yaw and character
// facing are separate, so moving never spins the camera).
class SamplePlayerScript : public Script {
public:
#define SAMPLE_PLAYER_FIELDS(X) \
    X(float, moveSpeed, 6.0f) \
    X(float, jumpForce, 20.0f) \
    X(float, jumpRiseMultiplier, 2.0f) \
    X(float, fallMultiplier, 5.5f) \
    X(float, turnSpeed, 720.0f) \
    X(std::string, idleClip, "idle") \
    X(std::string, runClip, "run") \
    X(std::string, jumpClip, "jump")

    SAMPLE_PLAYER_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_PLAYER_FIELDS)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

private:
    // Written by ThirdPersonCamera (mouse / right stick); movement is relative to it.
    float m_cameraYaw = 0.0f;
    // Character heading in degrees; turns toward the move direction at turnSpeed deg/s.
    float m_facingYaw = 0.0f;
    bool m_wasJumpHeld = false;
    bool m_jumpOneShot = false;
    float m_jumpTakeoffY = 0.0f;
    bool m_jumpLeftGround = false;
    std::string m_locomotionClip;

    void TryJump(engine::GameObject& self);
    void SyncPlayerAnimation(engine::GameObject& self, float dt);
    bool JumpHasLanded(engine::GameObject& self);
};
