#pragma once

#include "engine/scripting/Script.h"

class IntroCameraScript : public Script {
public:
    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    static void Begin();
    static void End();
    static bool IsActive();
};
