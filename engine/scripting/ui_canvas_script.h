#pragma once

#include "libraries/nlohmann/json.hpp"

class UIElement;

class UICanvasScript {
public:
    virtual ~UICanvasScript() = default;

    virtual void OnCreate(UIElement& owner) {}
    virtual void OnUpdate(UIElement& owner, float dt) {}
    virtual void OnDestroy(UIElement& owner) {}

    virtual nlohmann::json Serialize() const { return {}; }
    virtual void Deserialize(const nlohmann::json& j) { (void)j; }
};
