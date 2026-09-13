/**===========================================================================

 File   :  [game_scene.cpp]

 Author : Terry
 Date   : 2025/1/12
 Description: GameScene – manages the map, lighting, shadows, billboard, and
              the global enemy / player pools.  Enemies and player are spawned
              by prefab components (EnemySpawnComponent, PlayerSpawnComponent)
              added via the engine_gui Prefab tab.

 ===========================================================================**/
#include "game_scene.h"

#include "engine/rendering/d3d11/shader.h"
#include "engine/scene/render_settings.h"
#include "engine/rendering/lighting/shadow.h"
#include "engine/physics/core/physics_system.h"
#include "engine/physics/core/rigidbody.h"
#include "engine/rendering/lighting/lighting.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/input/key_logger.h"
#include "engine/rendering/model/model.h"
#include "camera.h"
#include "debug_camera.h"
#include "engine/core/engine_gui.h"
#include <cstdio>
#include <Windows.h>
#ifndef GAME_RUNTIME
#include "grid.h"
#endif
#include "engine/scene/scene_loader.h"
#include "camera_director.h"
#include "scene_transition.h"
#include "engine/ecs/model_asset_cache.h"
#ifndef GAME_RUNTIME
#include "engine/editor/project_manager.h"
#endif
#include "engine/scene/scene_runtime.h"
#include "engine/ecs/prefab_runtime.h"
#include "engine/ecs/visual_system.h"
#include "engine/ecs/audio_system.h"
#include "engine/ecs/effect_system_flush.h"
#include "engine/core/job_system.h"
#include "engine/core/frame_transform_snapshot.h"
#include "third_person_camera.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/d3d11/post_process.h"
#include "engine/rendering/billboard/billboard.h"
#include "engine/rendering/particle/particle_system.h"
#include "engine/rendering/environment/skybox.h"
#include "engine/rendering/effect/effect_system.h"
#include "engine/ecs/billboard_component.h"
#include "engine/ecs/directional_light_component.h"
#include "engine/scene/scene_lighting.h"
#include "engine/scene/scene_loader.h"
#include "game_scene_script_hooks.h"
#include "engine/debug_draw/debug_drawer.h"


#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

#include "engine/ui/scene_canvas.h"
#include "engine/audio/audio_manager.h"
#include "engine/audio/audio_listener_sync.h"
void GameScene::Initialize() {
	srand((unsigned int)time(nullptr));

	// Cascaded shadow maps (quality from Render Settings; default = Medium)
	{
		const ShadowQualityPreset shadowPreset = RenderSettings_GetShadowPreset();
		if (!Shadow::Initialize(GetDevice(), GetDeviceContext(), shadowPreset.mapSize, shadowPreset.cascadeCount)) {
			MessageBoxA(nullptr, "Shadow::Initialize failed", "Error", MB_OK | MB_ICONERROR);
			exit(1);
		}
		Shadow::SetBackfaceDepth(false);
	}

	Lighting::SetAmbientColor({ 0.6f, 0.7f, 0.8f });

	// Scene + default level
	{
#ifndef GAME_RUNTIME
		Grid_Initialize();
		ProjectManager::Instance().BootstrapAtStartup();
#endif
		ModelAssetCache::Instance().Initialize();
		EngineGUI_Initialize(GetWindowHandle(), GetDevice(), GetDeviceContext());
		SceneCanvas_Initialize();
#ifndef GAME_RUNTIME
		{
			std::string scenePath = EngineGUI_ResolveStartupScenePath();
			if (scenePath.empty())
				scenePath = EngineGUI_GetDefaultScenePath();
			EngineGUI_LoadFromFile(scenePath.c_str());
		}
#else
		AudioManager::Instance().TryLoadBusVolumesFromNearbyProject();
		Physics_TryLoadGravityFromNearbyProject();
		EngineGUI_LoadFromFile(EngineGUI_GetDefaultScenePath().c_str());
#endif
	}

	// Billboard system
	if (!Skybox_Initialize()) {
		MessageBoxA(nullptr, "Skybox_Initialize failed", "Error", MB_OK | MB_ICONERROR);
	}
	if (!Billboard::Initialize()) {
		MessageBoxA(nullptr, "Billboard::Initialize failed", "Error", MB_OK | MB_ICONERROR);
	}
	if (!engine::ParticleSystem::Instance().Initialize()) {
		MessageBoxA(nullptr, "ParticleSystem::Initialize failed", "Error", MB_OK | MB_ICONERROR);
	}
	if (!GameSceneScript_InvokeBool("GameScene_Initialize")) {
		MessageBoxA(nullptr, "GameScene_Initialize failed", "Error", MB_OK | MB_ICONERROR);
	}

#ifndef GAME_RUNTIME
	// Start with the debug camera so the scene is immediately navigable.
	DebugCamera_SetEnabled(true);
#endif
}

void GameScene::Finalize() {
	const std::vector<GameObject*> activeObjects = m_objects;
	for (auto* obj : activeObjects) {
		if (!obj) continue;
		if (obj->GetTag() == Tag::TAG_PLAYER)
			obj->Finalize();
	}

	SceneCanvas_Finalize();
	EngineGUI_Finalize();
	m_objects.clear();
	ModelAssetCache::Instance().Finalize();
#ifndef GAME_RUNTIME
	Grid_Finalize();
#endif

	Shadow::Finalize();
	GameSceneScript_Invoke("GameScene_Finalize");
	engine::ParticleSystem::Instance().Finalize();
	Billboard::Finalize();
	Skybox_Finalize();
}

void GameScene::Update(double elapsed_time) {
	// Edit mode must not simulate: gravity, velocity, and collision resolution
	// would move authored poses (Unity keeps objects parked until Play).
	if (EngineGUI_IsPlaying())
		PhysicsSystem_Update(elapsed_time);

	// Snapshot dynamic object positions before running scripts.
	// This captures the post-physics (pre-orbit) position so the swept collision
	// pass below can detect tunneling caused by orbit and other script-driven moves.
	// Also run in edit mode so the first Play frame sweeps from the authored pose.
	PhysicsSystem_SavePositions();

	Billboard::ClearDrawList();

	// Non-player scene roots. TAG_PLAYER roots are updated in the player batch
	// below (EngineGUI skips them so snapshot → jobs → flushes can own them).
	EngineGUI_Update(elapsed_time);

	if (EngineGUI_IsPlaying()) {
		// Copy m_objects to guard against iterator invalidation from any
		// object removals during the update loop.
		const std::vector<GameObject*> activeObjects = m_objects;

		// Read-only pose snapshot for neighbor reads (players + disc/TAG_BULLET).
		FrameTransformSnapshot_Capture(activeObjects);

		// Collect TAG_PLAYER from m_objects and scene roots (roots without
		// colliders are not in m_objects but still need Update).
		std::vector<GameObject*> players;
		players.reserve(activeObjects.size());
		auto pushPlayer = [&players](GameObject* obj) {
			if (!obj || !obj->IsActive()) return;
			if (obj->GetTag() != Tag::TAG_PLAYER) return;
			for (GameObject* existing : players) {
				if (existing == obj) return;
			}
			players.push_back(obj);
		};
		for (auto* obj : activeObjects)
			pushPlayer(obj);
		for (auto& node : SceneLoader_GetSceneNodes())
			pushPlayer(node.get());

		// Parallel-safe players on workers; WaitAll before sequential players
		// so main-thread scripts never read mid-update neighbor state.
		if (JobSystem_UseParallel()) {
			for (GameObject* obj : players) {
				if (obj->Update_IsParallelSafe()) {
					JobSystem_Submit([obj, elapsed_time]() {
						obj->Update(elapsed_time);
					});
				}
			}
			JobSystem_WaitAll();
			for (GameObject* obj : players) {
				if (!obj->Update_IsParallelSafe())
					obj->Update(elapsed_time);
			}
		}
		else {
			for (GameObject* obj : players)
				obj->Update(elapsed_time);
		}

		// Main-thread flushes after the player batch (run every frame).
		VisualSystem_UploadSkinning();
		AudioSystem_FlushCommands();
		EffectSystem_FlushCommands();
		PrefabRuntime_FlushSpawned();
		PrefabRuntime_FlushDestroyed();

		// Post-script swept collision: after all scripts (EngineGUI + players).
		PhysicsSystem_ResolveCollisions();

		engine::ParticleSystem::Instance().Tick(static_cast<float>(elapsed_time));

		// Always update follow producer while playing so blends back from
		// DeathCam/Intro have a live pose. Director picks active source.
		if (!DebugCamera_IsEnabled())
			ThirdPersonCamera_Update(elapsed_time);
	}
	else {
		PrefabRuntime_FlushDestroyed();
		engine::ParticleSystem::Instance().Reset();
	}

	if (!DebugCamera_IsEnabled())
		CameraDirector_Update(elapsed_time, SceneLoader_GetSceneNodes());

	DebugCamera_Update(elapsed_time);
	AudioListenerSync_Tick(SceneLoader_GetSceneNodes());

	Billboard::Update(elapsed_time);
	if (EngineGUI_IsPlaying())
		GameSceneScript_InvokeWithDouble("GameScene_Update", elapsed_time);
	else
		GameSceneScript_Invoke("GameScene_OnStopPlay");

	SceneCanvas_Update(elapsed_time);
	bool playOnLoad = false;
	if (const std::string nextScene = SceneCanvas_ConsumeRequestedSceneFile(&playOnLoad); !nextScene.empty()) {
		if (!SceneTransition_TryHandleSceneRequest(
				SceneLoader_GetCurrentScenePath(), nextScene, playOnLoad)) {
			EngineGUI_LoadFromFile(nextScene.c_str());
			if (playOnLoad)
				EngineGUI_StartPlay();
		}
	}
}

void GameScene::Draw() {
	float vx, vy, vw, vh;
	EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
	SetRenderViewport(vx, vy, vw, vh);

#ifndef GAME_RUNTIME
	const bool preloadActive = EngineGUI_IsSceneAssetPreloadActive();
#else
	const bool preloadActive = false;
#endif
	if (!preloadActive)
		Object_Draw();

	// Effekseer into PostRT (same game viewport as the 3D scene).
	SetRenderViewport(vx, vy, vw, vh);
	if (!preloadActive)
		EffectSystem::Draw3D();

	if (!preloadActive)
		PostProcess_Draw();

	// Post resolve leaves the game viewport bound; re-assert for UI overlays.
	SetRenderViewport(vx, vy, vw, vh);

#ifndef GAME_RUNTIME
	if (!preloadActive)
		EngineGUI_DrawSelectionIndicator();
#endif
	if (!preloadActive && (EngineGUI_PreferPlayVisuals() || EngineGUI_IsUiOverlayVisible()))
		SceneCanvas_Draw();
	if (!preloadActive && EngineGUI_IsPlaying())
		GameSceneScript_Invoke("GameScene_PostUIDraw");
#ifndef GAME_RUNTIME
	EngineGUI_Draw();
#endif

	ResetRenderViewport();
}

void GameScene::SetRenderObject(RENDER_OBJECT_TYPE type) {
	switch (type) {
	case RENDER_SHADOW:
		for (auto* obj : m_objects) {
			if (obj && obj->IsActive() && obj->GetTag() == Tag::TAG_PLAYER)
				obj->Draw();
		}
		DrawSceneModels();
		break;
	case RENDER_MAIN:
#ifndef GAME_RUNTIME
		if (!EngineGUI_PreferPlayVisuals())
			Grid_Draw();
#endif
		for (auto* obj : m_objects) {
			if (obj && obj->IsActive() && obj->GetTag() == Tag::TAG_PLAYER)
				obj->Draw();
		}
#ifdef GAME_RUNTIME
		DrawSceneModels();
#else
		if (!EngineGUI_PreferPlayVisuals()) DrawEditorSceneProxies();
		else DrawSceneModels();
#endif
		break;
	}
}

void GameScene::Object_Draw() {
	SceneLightingSyncFromNodes(SceneLoader_GetSceneNodes());

	float vx, vy, vw, vh;
	EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
	const float aspect = (vh > 1e-3f) ? (vw / vh) : (16.0f / 9.0f);

	{
		const auto& cam = GetCamera();
		Shadow::CameraShadowParams cp {};
		cp.eye = cam.position;
		cp.at = cam.at;
		cp.up = cam.up;
		cp.fovYDegrees = cam.fov;
		cp.nearClip = cam.nearClip;
		cp.farClip = cam.farClip;
		cp.aspect = aspect;
		Shadow::UpdateForCamera(cp, RenderSettings_GetShadowPreset().maxDistance);
	}

	// Cascade depth passes
	const int cascadeCount = Shadow::GetCascadeCount();
	for (int i = 0; i < cascadeCount; ++i) {
		Shadow::Begin(i);
		Shader3D_Begin(RenderSettings_GetDefaultShader());
		SetDepthEnable(true);
		Shader_SetProjectionMatrix(Shadow::GetLightProj(i));
		Shader_SetViewMatrix(Shadow::GetLightView(i));
		Shader_SetUseExternalVP(true);
		SetRenderObject(RENDER_SHADOW);
		Shader_SetUseExternalVP(false);
		Shadow::End();
	}

	// Camera pass into offscreen PostRT (shadows use their own DSVs).
	Renderer_BindPostTarget();

	// Camera pass
	{
		SetRenderViewport(vx, vy, vw, vh);

		Shader3D_Begin(RenderSettings_GetDefaultShader());
		SetDepthEnable(true);

		const auto& cam = GetCamera();
		Shader_SetProjectionMatrix(cam.GetProjMatrix(vw, vh));
		Shader_SetViewMatrix(cam.GetViewMatrix());

		std::vector<DirectX::XMMATRIX> vps;
		vps.reserve(cascadeCount);
		for (int i = 0; i < cascadeCount; ++i) vps.push_back(Shadow::GetLightViewProj(i));
		Shader_SetShadowMatrices(vps.data(), cascadeCount, Shadow::GetReceiverBias());
		Shader_SetShadowCascadeParams(Shadow::GetCascadeSplits(), Shadow::GetCascadeBlend());

		ID3D11ShaderResourceView* srvs[Shadow::MAX_CASCADES] = {};
		for (int i = 0; i < cascadeCount; ++i) srvs[i] = Shadow::GetShadowSRV(i);
		Shader_BindShadowMaps(srvs, cascadeCount);

		Skybox_Draw(cam.GetViewMatrix(), cam.GetProjMatrix(vw, vh));

		SetRenderObject(RENDER_MAIN);
	}

	// Billboard pass
	{
		const auto& cam = GetCamera();
		Billboard::SetViewMatrix(cam.GetViewMatrix());
		Billboard::SetProjectionMatrix(cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
		SetRenderObject(RENDER_BILLBOARD);
		BillboardComponent::SubmitSceneBillboards(SceneLoader_GetSceneNodes());
		Billboard::Draw();
		engine::ParticleSystem::Instance().Render(
			cam.GetViewMatrix(),
			cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
		if (EngineGUI_IsPlaying())
			GameSceneScript_Invoke("GameScene_PostBillboardDraw");
	}

	// Debug sphere / line / collider-wire pass (deferred from Update).
	DebugDrawer::FlushSphereQueue();
	DebugDrawer::FlushLineQueue();
	PhysicsSystem_FlushDebugDraw();
}
