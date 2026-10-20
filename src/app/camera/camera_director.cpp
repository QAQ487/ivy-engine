#include "engine/scene/scene_camera.h"
#include "camera_director.h"
#include "camera.h"
#include "debug_camera.h"
#include "third_person_camera.h"
#include "first_person_camera.h"
#include "engine/core/engine_gui.h"
#include "engine/core/easing.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"

using namespace DirectX;

namespace {

constexpr float kDefaultBlendDuration = 0.45f;

CameraDirectorSource s_source = CameraDirectorSource::Follow;
CameraDirectorSource s_appliedSource = CameraDirectorSource::Follow;
bool s_initialized = false;
bool s_blending = false;
float s_blendT = 0.0f;
float s_blendDuration = kDefaultBlendDuration;
CameraPose s_blendFrom;
CameraPose s_introPose;

GameObject* FindNodeByName(GameObject* node, const std::string& name) {
	if (!node) return nullptr;
	if (node->GetName() == name) return node;
	for (const auto& child : node->GetChildren()) {
		if (GameObject* found = FindNodeByName(child.get(), name))
			return found;
	}
	return nullptr;
}

GameObject* FindNodeByNameInRoots(const std::vector<std::unique_ptr<GameObject>>& roots,
                                  const std::string& name) {
	if (name.empty()) return nullptr;
	for (const auto& root : roots) {
		if (GameObject* found = FindNodeByName(root.get(), name))
			return found;
	}
	return nullptr;
}

CameraDirectorSource SourceFromCameraMode(CameraMode mode) {
	switch (mode) {
	case CameraMode::SceneCamera: return CameraDirectorSource::SceneNode;
	case CameraMode::Intro:       return CameraDirectorSource::Intro;
	case CameraMode::FirstPerson: return CameraDirectorSource::FirstPerson;
	case CameraMode::ThirdPerson:
	default:                      return CameraDirectorSource::Follow;
	}
}

bool TrySampleSceneNode(const std::vector<std::unique_ptr<GameObject>>& roots, CameraPose& outPose) {
	GameObject* node = FindNodeByNameInRoots(roots, EngineGUI_GetActiveCameraNodeName());
	if (!node || !node->IsActive())
		return false;

	auto* cam = node->GetComponent<CameraComponent>();
	if (!cam || !cam->enabled)
		return false;

	auto* transform = node->GetComponent<engine::Transform>();
	if (!transform)
		return false;

	outPose = CameraPose_FromComponent(cam, transform);
	return true;
}

bool TrySampleActiveSource(const std::vector<std::unique_ptr<GameObject>>& roots,
                           CameraDirectorSource source,
                           CameraPose& outPose) {
	switch (source) {
	case CameraDirectorSource::Follow:
		if (!ThirdPersonCamera_TryGetFollowPose(outPose))
			return false;
		return true;
	case CameraDirectorSource::FirstPerson:
		return FirstPersonCamera_TryGetPose(outPose);
	case CameraDirectorSource::SceneNode:
		return TrySampleSceneNode(roots, outPose);
	case CameraDirectorSource::Intro:
		outPose = s_introPose;
		return true;
	}
	return false;
}

void BeginBlendFromCurrent() {
	s_blendFrom = CameraPose_FromGlobal();
	s_blendT = 0.0f;
	s_blending = true;
}

} // namespace

void CameraDirector_Initialize() {
	s_source = CameraDirectorSource::Follow;
	s_appliedSource = CameraDirectorSource::Follow;
	s_blending = false;
	s_blendT = 0.0f;
	s_blendDuration = kDefaultBlendDuration;
	s_introPose = {};
	s_introPose.fov = 45.0f;
	s_introPose.nearClip = 0.5f;
	s_introPose.farClip = 1000.0f;
	s_introPose.up = { 0.0f, 1.0f, 0.0f };
	s_initialized = true;
}

void CameraDirector_BeginPlay(const std::vector<std::unique_ptr<GameObject>>& roots) {
    if(!s_initialized) CameraDirector_Initialize();
    s_source=SourceFromCameraMode(EngineGUI_GetCameraMode());
    s_appliedSource=s_source;
    s_blending=false;
    s_blendT=0;
    CameraDirector_Update(0,roots);
}

void CameraDirector_SetSource(CameraDirectorSource source) {
	if (!s_initialized)
		CameraDirector_Initialize();
	if (source == s_source)
		return;
	s_source = source;
	BeginBlendFromCurrent();
	s_appliedSource = source;
}

CameraDirectorSource CameraDirector_GetSource() {
	return s_source;
}

void CameraDirector_SetIntroPosition(const XMFLOAT3& position) {
	s_introPose.eye = position;
}

void CameraDirector_SetIntroTarget(const XMFLOAT3& target) {
	s_introPose.lookAt = target;
}

void CameraDirector_SetIntroUp(const XMFLOAT3& up) {
	s_introPose.up = up;
}

const CameraPose& CameraDirector_GetIntroPose() {
	return s_introPose;
}

void CameraDirector_SetBlendDuration(float seconds) {
	s_blendDuration = (seconds > 0.01f) ? seconds : 0.01f;
}

void CameraDirector_Update(double elapsed_time, const std::vector<std::unique_ptr<GameObject>>& roots) {
	if (!s_initialized)
		CameraDirector_Initialize();

	if (DebugCamera_IsEnabled())
		return;

	const CameraDirectorSource desired = SourceFromCameraMode(EngineGUI_GetCameraMode());
	if (desired != s_source) {
		s_source = desired;
		BeginBlendFromCurrent();
		s_appliedSource = desired;
	}

	if (s_source == CameraDirectorSource::SceneNode) {
        const auto resolved=SceneCameraResolveDefault(roots,EngineGUI_GetActiveCameraNodeName());
        if(resolved.empty()) { SetCamera(Camera{}); s_blending=false; return; }
        EngineGUI_SetActiveCameraNodeName(resolved);
    }
    CameraPose targetPose = CameraPose_FromGlobal();
	if (!TrySampleActiveSource(roots, s_source, targetPose)) {
		// Keep last global pose if source unavailable (e.g. empty DeathCam name).
		if (!s_blending)
			return;
	}

	CameraPose finalPose = targetPose;
	if (s_blending) {
		const float dt = static_cast<float>(elapsed_time);
		s_blendT += (s_blendDuration > 0.0f) ? (dt / s_blendDuration) : 1.0f;
		float t = (s_blendT < 0.0f) ? 0.0f : ((s_blendT > 1.0f) ? 1.0f : s_blendT);
		t = static_cast<float>(easeInOutCubic(t));
		finalPose = CameraPose_Lerp(s_blendFrom, targetPose, t);
		if (s_blendT >= 1.0f)
			s_blending = false;
	}

	CameraPose_ApplyToGlobal(finalPose);
}
