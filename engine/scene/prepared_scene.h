#pragma once
#include "scene_loader.h"
#include "engine/core/engine_gui.h"
#include "scene_execution_policy.h"
#include "render_settings.h"
#include "engine/ui/scene_canvas.h"

// Internal load transaction. No runtime registration or gameplay OnCreate until commit.
struct PreparedScene {
    std::string path, content;
    std::vector<std::unique_ptr<GameObject>> nodes;
    std::unique_ptr<UICanvas> canvas;
    nlohmann::json canvasJson;
    SceneExecutionPolicy policy;
    RenderSettingsData render;
    CameraMode cameraMode = CameraMode::SceneCamera;
    std::string preferredCamera;
    bool debugFly = false, debugOnPlay = false, debugPosition = false;
    DirectX::XMFLOAT3 position{};
    float yaw = 0, pitch = -15;
};
