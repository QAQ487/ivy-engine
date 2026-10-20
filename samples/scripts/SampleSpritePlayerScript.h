#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

class SampleSpritePlayerScript : public Script {
public:
#define SPRITE_PLAYER_FIELDS(X) X(float, moveSpeed, 3.0f)
    SPRITE_PLAYER_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SPRITE_PLAYER_FIELDS)
    void OnCreate(engine::GameObject&) override {paused=false;stopped=false;}
    bool paused=false,stopped=false;
    void OnUpdate(engine::GameObject& self,float dt) override;
};
