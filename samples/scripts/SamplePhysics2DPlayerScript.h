// SamplePhysics2DPlayerScript.h
// Declares SamplePhysics2DPlayerScript.

#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include "engine/physics/2d/physics2d.h"
#include <string>
class SamplePhysics2DPlayerScript : public Script, public Physics2DListener {
public:
#define PHYSICS_2D_PLAYER_FIELDS(X) \
    X(int, targetCount, 3) \
    X(std::string, sceneFile, "scene_topdown_2d.json") \
    X(std::string, nextScene, "scene_topdown_2d_next.json")
    PHYSICS_2D_PLAYER_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(PHYSICS_2D_PLAYER_FIELDS)
    void OnCreate(engine::GameObject& self) override;
    void OnDestroy(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self,float dt) override;
    void OnPhysics2DEvent(GameObject& self,const Physics2DEvent& event) override;
private:
    void UpdateScore();
    int collected=0;
    bool completed=false;
};

// Authored collectibles survive Stop; only their play-session activity changes.
class SamplePhysics2DCollectibleScript : public Script {
public:
    void OnDestroy(engine::GameObject& self) override {self.SetActive(true);}
};
