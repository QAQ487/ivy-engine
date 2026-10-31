// camera_gizmo.h
// Editor gizmo for camera visualization and editing.

#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>

class DebugDrawer;
class GameObject;

namespace engine { class Transform; }
class CameraComponent;

namespace CameraGizmo {

void DrawCamera(DebugDrawer& drawer, const engine::Transform* transform,
    const CameraComponent* camera);
void DrawSceneCameras(const std::vector<std::unique_ptr<GameObject>>& roots, DebugDrawer& drawer);

} // namespace CameraGizmo
