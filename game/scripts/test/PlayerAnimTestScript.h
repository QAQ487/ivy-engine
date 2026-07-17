#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

class UIText;

class PlayerAnimTestScript : public Script {
#define PLAYER_ANIM_TEST_SCRIPT_FIELDS(X) \
    X(std::string, idleClip,  "idle") \
    X(std::string, runClip,   "run") \
    X(std::string, danceClip, "dance") \
    X(std::string, jumpClip,  "jump") \
    X(std::string, pushClip,  "push") \
    X(std::string, pullClip,  "pull") \
    X(std::string, throwClip, "throw") \
    X(std::string, idlePath,  "rom/model/player/idle.fbx") \
    X(std::string, runPath,   "rom/model/player/run.fbx") \
    X(std::string, dancePath, "rom/model/player/dance1.fbx") \
    X(std::string, jumpPath,  "rom/model/player/jump.fbx") \
    X(std::string, pushPath,  "rom/model/player/push.fbx") \
    X(std::string, pullPath,  "rom/model/player/pull.fbx") \
    X(std::string, throwPath, "rom/model/player/throw.fbx") \
    X(float, spinSpeed, 25.0f)

public:
    PLAYER_ANIM_TEST_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;

    SCRIPT_SERIALIZE_METHODS(PLAYER_ANIM_TEST_SCRIPT_FIELDS)

private:
    void SetupClips(engine::GameObject& self);
    void PlayNamedClip(engine::GameObject& self, const std::string& clipName);
    void BindHud();
    void SyncHud(engine::GameObject& self);

    UIText* m_statusText = nullptr;
    std::string m_lastRequestedClip;
};
