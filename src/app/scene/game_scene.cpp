#include "engine/physics/2d/physics2d.h"
#include "engine/ecs/sprite_renderer_component.h"
#include "engine/scene/scene_camera.h"
/**===========================================================================

 File   :  [game_scene.cpp]

 Author : Terry Yeung
 Date   : 2025/1/12
 Description: GameScene ? manages the map, lighting, shadows, billboard, and
              the global enemy / player pools.  Enemies and player are spawned
              by prefab components (EnemySpawnComponent, PlayerSpawnComponent)
              added via the engine_gui Prefab tab.

 ===========================================================================**/
#include "game_scene.h"

#include "engine/rendering/d3d11/shader.h"
#include "engine/scene/render_settings.h"
#include "engine/core/builtin_paths.h"
#include "engine/rendering/culling/frustum_cull.h"
#include "engine/rendering/culling/lod_state.h"
#include "engine/rendering/culling/shadow_cascade_cull.h"
#include "engine/rendering/lighting/shadow.h"
#include "engine/physics/core/physics_system.h"
#include "engine/physics/core/rigidbody.h"
#include "engine/rendering/lighting/lighting.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/core/content_path.h"
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
#include "first_person_camera.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/d3d11/post_process.h"
#include "engine/rendering/billboard/billboard.h"
#include "engine/rendering/particle/particle_system.h"
#include "engine/rendering/environment/skybox.h"
#include "engine/rendering/environment/ibl.h"
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

#ifdef GAME_RUNTIME
#include "engine/scene/scene_asset_preload.h"
#include "engine/debug_draw/debug_text.h"
#include "engine/core/debug_system.h"
#include "engine/rendering/sprite/sprite.h"
#include "engine/testing/runtime_test_bridge.h"
#include "libraries/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void DrawBootLoadingScreen(float progress01, const std::string& label) {
	ID3D11Device* device = GetDevice();
	ID3D11DeviceContext* ctx = GetDeviceContext();
	if (!device || !ctx)
		return;

	Renderer_BindBackBuffer();
	ResetRenderViewport();

	ID3D11RenderTargetView* rtv = nullptr;
	ID3D11DepthStencilView* dsv = nullptr;
	ctx->OMGetRenderTargets(1, &rtv, &dsv);
	const float cream[4] = { 0.961f, 0.945f, 0.910f, 1.0f };
	if (rtv)
		ctx->ClearRenderTargetView(rtv, cream);
	if (dsv)
		ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	if (rtv)
		rtv->Release();
	if (dsv)
		dsv->Release();

	const float screenW = GetBackBufferWidth();
	const float screenH = GetBackBufferHeight();
	// Intended on-screen green (#2b5843). PixelShader2D does vertex*tex*g_Color and
	// Sprite_Draw sets both factors to the same value ? use sqrt so solids land on target.
	const float ivyR = 0x2b / 255.0f;
	const float ivyG = 0x58 / 255.0f;
	const float ivyB = 0x43 / 255.0f;
	const DirectX::XMFLOAT4 ivyGreenText(ivyR, ivyG, ivyB, 1.0f);
	const DirectX::XMFLOAT4 ivyGreenFill(
		sqrtf(ivyR), sqrtf(ivyG), sqrtf(ivyB), 1.0f);
	const DirectX::XMFLOAT4 ivyGreenTrack(
		sqrtf(ivyR), sqrtf(ivyG), sqrtf(ivyB), sqrtf(0.2f));

	static int s_logoTex = -1;
	static bool s_logoLoadAttempted = false;
	if (!s_logoLoadAttempted) {
		s_logoLoadAttempted = true;
		// The logo is compiled into the exe ("builtin:" path, builtin_assets.h) and never
		// touches the disk. Quiet load: a failure must never pop a dialog.
		s_logoTex = TextureLoadQuiet(builtin_paths::kBrandLogoW, false);
	}

	float contentBottom = screenH * 0.42f;
	if (s_logoTex >= 0) {
		const float side = 0.33f * (std::min)(screenW, screenH);
		const float logoX = (screenW - side) * 0.5f;
		const float logoY = screenH * 0.42f - side * 0.5f;
		const int texW = static_cast<int>(TextureGetWidth(s_logoTex));
		const int texH = static_cast<int>(TextureGetHeight(s_logoTex));
		// Logo PNG pixels are already #2b5843; white tint preserves them (1*tex*1).
		Sprite_Draw(s_logoTex, logoX, logoY, side, side, 0, 0, texW, texH,
			DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		contentBottom = logoY + side;
	}

	const float barW = 0.40f * screenW;
	const float barH = 10.0f;
	const float barX = (screenW - barW) * 0.5f;
	const float barY = contentBottom + 24.0f;
	const int white = TextureGetWhiteTexture();
	if (white >= 0) {
		const int whiteW = static_cast<int>(TextureGetWidth(white));
		const int whiteH = static_cast<int>(TextureGetHeight(white));
		Sprite_Draw(white, barX, barY, barW, barH, 0, 0, whiteW, whiteH, ivyGreenTrack);

		float progress = progress01;
		if (progress < 0.0f) progress = 0.0f;
		if (progress > 1.0f) progress = 1.0f;
		const float fillW = barW * progress;
		if (fillW > 0.0f) {
			Sprite_Draw(white, barX, barY, fillW, barH,
				0, 0, static_cast<int>(progress * whiteW), whiteH, ivyGreenFill);
		}
	}

	std::string displayLabel = label.empty() ? "Loading..." : label;
	// ASCII font atlas has no Unicode ellipsis ? CurrentLabel uses U+2026.
	const std::string ellipsisUtf8 = "\xe2\x80\xa6";
	for (size_t pos = 0; (pos = displayLabel.find(ellipsisUtf8, pos)) != std::string::npos; ) {
		displayLabel.replace(pos, ellipsisUtf8.size(), "...");
		pos += 3;
	}
	hal::DebugText dbg(device, ctx,
		static_cast<UINT>(screenW), static_cast<UINT>(screenH),
		0.0f, 0.0f,
		0, 0,
		24.0f, 12.0f);
	float textW = 0.0f;
	float textH = 0.0f;
	dbg.MeasureText(displayLabel.c_str(), textW, textH);
	const float textY = (white >= 0) ? (barY + barH + 16.0f) : (contentBottom + 24.0f);
	dbg.SetOffset((screenW - textW) * 0.5f, textY);
	dbg.SetText(displayLabel.c_str(), ivyGreenText);
	dbg.Draw();
	dbg.Clear();

	SetDepthEnable(true);
}

bool PumpBootMessages() {
	MSG msg{};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT)
			return false;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

void RunBootAssetPreload() {
	if (!content_path::ReadableFileCurrent("rom/skip_boot_preload").empty())
		return;

	const auto tasks = SceneAssetPreload_BuildTaskListFromSceneFiles("rom/scene");
	if (tasks.empty())
		return;

	DebugSystem::ResetTimeScale();
	if (DebugSystem::CollisionVizEnabled())
		DebugSystem::ToggleCollisionViz();

	SceneAssetPreloader& pre = SceneAssetPreloader::BootPreloader();
	pre.Begin(tasks);
	if (!pre.IsActive())
		return;

	HWND hwnd = GetWindowHandle();
	if (hwnd) {
		ShowWindow(hwnd, SW_SHOWDEFAULT);
		UpdateWindow(hwnd);
	}

	while (pre.IsActive()) {
		if (!PumpBootMessages()) {
			pre.Cancel();
			break;
		}
		pre.Tick();
		DrawBootLoadingScreen(pre.Progress01(), pre.CurrentLabel());
		Present();
	}
}

std::wstring WideFromUtf8Local(const std::string& utf8) {
	if (utf8.empty())
		return {};
	const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
	if (size <= 0)
		return {};
	std::wstring out(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), out.data(), size);
	return out;
}

bool TryReadProjectName(const std::string& path, std::string* outName) {
	if (!outName)
		return false;
	try {
		std::ifstream file(path);
		if (!file.is_open())
			return false;
		nlohmann::json j;
		file >> j;
		if (!j.contains("name") || !j["name"].is_string())
			return false;
		std::string name = j["name"].get<std::string>();
		while (!name.empty() && (unsigned char)name.front() <= ' ')
			name.erase(name.begin());
		while (!name.empty() && (unsigned char)name.back() <= ' ')
			name.pop_back();
		if (name.empty())
			return false;
		*outName = std::move(name);
		return true;
	}
	catch (...) {
		return false;
	}
}

void ApplyRuntimeWindowTitleFromNearbyProject() {
	namespace fs = std::filesystem;
	std::string name;
	if (!TryReadProjectName("project.json", &name)) {
		wchar_t exePath[MAX_PATH] = {};
		if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) != 0) {
			const fs::path nearby = fs::path(exePath).parent_path() / "project.json";
			TryReadProjectName(nearby.string(), &name);
		}
	}
	if (name.empty())
		name = "Ivy Engine";

	HWND hwnd = GetWindowHandle();
	if (hwnd)
		SetWindowTextW(hwnd, WideFromUtf8Local(name).c_str());
}

} // namespace
#endif // GAME_RUNTIME

namespace {
bool s_scene3DReady=false;
void ReleaseScene3D() {
    if(!s_scene3DReady) return;
    engine::ParticleSystem::Instance().Finalize(); Billboard::Finalize();
    Ibl_Finalize(); Skybox_Finalize(); Shadow::Finalize();
    s_scene3DReady=false;
}
void EnsureScene3D() {
    if(s_scene3DReady) return;
	// Cascaded shadow maps (quality from Render Settings; default = Medium)
	{
		const ShadowQualityPreset shadowPreset = RenderSettings_GetShadowPreset();
		if (!Shadow::Initialize(GetDevice(), GetDeviceContext(), shadowPreset.mapSize, shadowPreset.cascadeCount)) {
			MessageBoxA(nullptr, "Shadow::Initialize failed", "Error", MB_OK | MB_ICONERROR);
			exit(1);
		}
		Shadow::SetBackfaceDepth(false);
	}

	// Billboard system
	if (!Skybox_Initialize()) {
		MessageBoxA(nullptr, "Skybox_Initialize failed", "Error", MB_OK | MB_ICONERROR);
	}
	if (!Ibl_Initialize()) {
		MessageBoxA(nullptr, "Ibl_Initialize failed", "Error", MB_OK | MB_ICONERROR);
	}
	if (!Billboard::Initialize()) {
		MessageBoxA(nullptr, "Billboard::Initialize failed", "Error", MB_OK | MB_ICONERROR);
	}
	if (!engine::ParticleSystem::Instance().Initialize()) {
		MessageBoxA(nullptr, "ParticleSystem::Initialize failed", "Error", MB_OK | MB_ICONERROR);
	}
    s_scene3DReady=true;
    RenderSettings_ReloadSkybox();
}
}
void GameScene::Initialize() {
	srand((unsigned int)time(nullptr));

	Lighting::SetAmbientColor({ 0.6f, 0.7f, 0.8f });

#ifdef GAME_RUNTIME
	// A packaged game has rom/ beside the exe. The engine OutDir does not:
	// it only mirrors resources/. Open the in-repo testbed so the dev host
	// still has a boot scene. Asset reads then use testbed rom/ first and
	// <exe-dir>/resources after that.
	{
		std::error_code ec;
		if (!std::filesystem::is_regular_file("rom/scene/default_scene.txt", ec)) {
			ec.clear();
			const auto cwd = std::filesystem::current_path(ec);
			const auto testbed = content_path::ResolveInRepoTestbedPath(
				ec ? std::filesystem::path{} : cwd,
				content_path::ExecutableDirectory());
			if (!testbed.empty())
				SetCurrentDirectoryW(testbed.wstring().c_str());
		}
	}
#endif

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
		if (ProjectManager::Instance().IsOpen()) {
			std::string scenePath = EngineGUI_ResolveStartupScenePath();
			if (scenePath.empty())
				scenePath = EngineGUI_GetDefaultScenePath();
			if (!scenePath.empty())
				EngineGUI_LoadFromFile(scenePath.c_str());
		}
#else
		AudioManager::Instance().TryLoadBusVolumesFromNearbyProject();
		ApplyRuntimeWindowTitleFromNearbyProject();
		Physics_TryLoadGravityFromNearbyProject();
		RunBootAssetPreload();
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
		if (ivy::testing::RuntimeTestSceneOverride_Has()) {
			const std::string& romPath = ivy::testing::RuntimeTestSceneOverride_Get();
			const auto resolved = content_path::ReadableFileCurrent(romPath);
			if (resolved.empty()) {
				std::fprintf(stderr,
					"[runtime_test_bridge] --scene could not be resolved via rom/ -> resources/: %s\n",
					romPath.c_str());
				std::fflush(stderr);
				ExitProcess(2);
			}
			if (!EngineGUI_LoadFromFile(romPath.c_str())) {
				std::fprintf(stderr,
					"[runtime_test_bridge] --scene failed to load: %s\n", romPath.c_str());
				std::fflush(stderr);
				ExitProcess(2);
			}
		} else
#endif
		EngineGUI_LoadFromFile(EngineGUI_GetDefaultScenePath().c_str());
#endif
	}

    if(!SceneLoader_Is2D() && !SceneLoader_GetCurrentScenePath().empty()) EnsureScene3D();
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

	ReleaseScene3D();
	GameSceneScript_Invoke("GameScene_Finalize");
}

void GameScene::Update(double elapsed_time) {
	// Edit mode must not simulate: gravity, velocity, and collision resolution
	// would move authored poses (Unity keeps objects parked until Play).
	if (EngineGUI_IsPlaying() && SceneLoader_Uses3DPhysics())
		PhysicsSystem_Update(elapsed_time);

	// Snapshot dynamic object positions before running scripts.
	// This captures the post-physics (pre-orbit) position so the swept collision
	// pass below can detect tunneling caused by orbit and other script-driven moves.
	// Also run in edit mode so the first Play frame sweeps from the authored pose.
	if(SceneLoader_Uses3DPhysics()) PhysicsSystem_SavePositions();

	if(!SceneLoader_Is2D()) Billboard::ClearDrawList();

	// Non-player scene roots. TAG_PLAYER roots are updated in the player batch
	// below (EngineGUI skips them so snapshot  ->  jobs  ->  flushes can own them).
	EngineGUI_Update(elapsed_time);
    if(SceneLoader_Is2D()) ReleaseScene3D();
    else if(!SceneLoader_GetCurrentScenePath().empty()) EnsureScene3D();

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
		if(!SceneLoader_Is2D()) VisualSystem_UploadSkinning();
		AudioSystem_FlushCommands();
		EffectSystem_FlushCommands();
		PrefabRuntime_FlushSpawned();
		PrefabRuntime_FlushDestroyed();

		if(SceneLoader_Uses2DPhysics()) {Physics2D_Update(elapsed_time); Physics2D_DispatchEvents();}

		// Post-script swept collision: after all scripts (EngineGUI + players).
		if(SceneLoader_Uses3DPhysics()) PhysicsSystem_ResolveCollisions();

		if(!SceneLoader_Is2D()) engine::ParticleSystem::Instance().Tick(static_cast<float>(elapsed_time));

		// Always update follow producer while playing so blends back from
		// DeathCam/Intro have a live pose. Director picks active source.
		if (!DebugCamera_IsEnabled()) {
			ThirdPersonCamera_Update(elapsed_time);
			FirstPersonCamera_Update(elapsed_time);
		}
	}
	else {
		PrefabRuntime_FlushDestroyed();
		if(s_scene3DReady) engine::ParticleSystem::Instance().Reset();
	}

	if (!DebugCamera_IsEnabled())
		CameraDirector_Update(elapsed_time, SceneLoader_GetSceneNodes());

	DebugCamera_Update(elapsed_time);
	AudioListenerSync_Tick(SceneLoader_GetSceneNodes());

	if(!SceneLoader_Is2D()) Billboard::Update(elapsed_time);
	if (EngineGUI_IsPlaying())
		GameSceneScript_InvokeWithDouble("GameScene_Update", elapsed_time);
	else
		GameSceneScript_Invoke("GameScene_OnStopPlay");

	SceneCanvas_Update(elapsed_time);
	bool playOnLoad = false;
	if (const std::string nextScene = SceneCanvas_ConsumeRequestedSceneFile(&playOnLoad); !nextScene.empty()) {
        const PlayMode resumeMode=EngineGUI_GetPlayMode()==PlayMode::Fullscreen?PlayMode::Fullscreen:PlayMode::Viewport;
		if (!SceneTransition_TryHandleSceneRequest(
				SceneLoader_GetCurrentScenePath(), nextScene, playOnLoad)) {
            if (EngineGUI_LoadFromFile(nextScene.c_str()) && playOnLoad)
                EngineGUI_StartPlay(resumeMode);
		}
	}
}

void GameScene::Draw() {
	float vx, vy, vw, vh;
	EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
#ifndef GAME_RUNTIME
	// F3 fullscreen play has no ImGui chrome to paint letterbox bars — clear the
	// full backbuffer to black so 16:9 gutters stay black (not clearColor).
	if (EngineGUI_GetPlayMode() == PlayMode::Fullscreen) {
		Renderer_BindBackBuffer();
		ID3D11RenderTargetView* target = nullptr;
		GetDeviceContext()->OMGetRenderTargets(1, &target, nullptr);
		if (target) {
			const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			GetDeviceContext()->ClearRenderTargetView(target, black);
			target->Release();
		}
	}
#endif
	SetRenderViewport(vx, vy, vw, vh);

#ifndef GAME_RUNTIME
	const bool preloadActive = EngineGUI_IsSceneAssetPreloadActive();
#else
	const bool preloadActive = false;
#endif
    const bool missingGameCamera = !DebugCamera_IsEnabled()
        && EngineGUI_GetCameraMode()==CameraMode::SceneCamera
        && SceneCameraResolveDefault(SceneLoader_GetSceneNodes(),EngineGUI_GetActiveCameraNodeName()).empty();
    if(missingGameCamera) {
        Renderer_BindBackBuffer();
        ID3D11RenderTargetView* target=nullptr;
        GetDeviceContext()->OMGetRenderTargets(1,&target,nullptr);
        if(target) {
            const float emptyColor[4]={0.08f,0.1f,0.14f,1.0f};
            GetDeviceContext()->ClearRenderTargetView(target,emptyColor);
            target->Release();
        }
        SceneCanvas_Draw();
        ResetRenderViewport();
        return;
    }
    if(!preloadActive && SceneLoader_Is2D()) {
        ReleaseScene3D(); Renderer_BindBackBuffer();
        ID3D11RenderTargetView* target=nullptr;
        GetDeviceContext()->OMGetRenderTargets(1,&target,nullptr);
        if(target) {
            const auto c=RenderSettings_GetConst().clearColor;
            const float color[]={c.x,c.y,c.z,c.w};
            GetDeviceContext()->ClearRenderTargetView(target,color); target->Release();
        }
        SetRenderViewport(vx,vy,vw,vh);
    } else if(!preloadActive) { EnsureScene3D(); Object_Draw(); }


	if (!preloadActive) DrawSceneSprites();
	// 2D scenes skip Object_Draw(), where the deferred debug queues are flushed.
	// Flush queued script debug lines after sprites so they remain visible above the scene.
	if (!preloadActive && SceneLoader_Is2D())
		DebugDrawer::FlushLineQueue();

	// Effekseer into PostRT (same game viewport as the 3D scene).
	SetRenderViewport(vx, vy, vw, vh);
	if (!preloadActive && !SceneLoader_Is2D())
		EffectSystem::Draw3D();

	if (!preloadActive && !SceneLoader_Is2D())
		PostProcess_Draw();

	// Post resolve leaves the game viewport bound; re-assert for UI overlays.
	SetRenderViewport(vx, vy, vw, vh);
    if (!preloadActive) Physics2D_DrawDebugColliders();

#ifndef GAME_RUNTIME
	if (!preloadActive)
		EngineGUI_DrawSelectionIndicator();
#endif
	if (!preloadActive && (EngineGUI_PreferPlayVisuals() || EngineGUI_IsUiOverlayVisible()))
		SceneCanvas_Draw();
	if (!preloadActive && EngineGUI_IsPlaying())
		GameSceneScript_Invoke("GameScene_PostUIDraw");
	// EngineGUI_Draw (ImGui / F8) runs on WinMain after the render-thread join.

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
	SceneLightingSyncFromNodes(SceneLoader_GetSceneNodes(), GetCamera().position);

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
		// LOD distances use the main camera for both shadow and camera passes.
		Lod_BeginFrame(cam.position);
	}

	// Cascade depth passes (optional per-cascade light-ortho caster cull).
	const int cascadeCount = Shadow::GetCascadeCount();
	ShadowCascadeCull_BeginFrame();
	for (int i = 0; i < cascadeCount; ++i) {
		Shadow::Begin(i);
		Shader3D_Begin(RenderSettings_GetDefaultShader());
		SetDepthEnable(true);
		Shader_SetProjectionMatrix(Shadow::GetLightProj(i));
		Shader_SetViewMatrix(Shadow::GetLightView(i));
		Shader_SetUseExternalVP(true);
		ShadowCascadeCull_BeginCascade(i, Shadow::GetLightViewProj(i));
		SetRenderObject(RENDER_SHADOW);
		ShadowCascadeCull_EndCascade();
		Shader_SetUseExternalVP(false);
		Shadow::End();
	}
	ShadowCascadeCull_EndFrame();

	// Camera pass into offscreen PostRT (shadows use their own DSVs).
	Renderer_BindPostTarget();

	// Camera pass
	{
		SetRenderViewport(vx, vy, vw, vh);

		Shader3D_Begin(RenderSettings_GetDefaultShader());
		SetDepthEnable(true);

		const auto& cam = GetCamera();
		const DirectX::XMMATRIX view = cam.GetViewMatrix();
		const DirectX::XMMATRIX proj = cam.GetProjMatrix(vw, vh);
		Shader_SetProjectionMatrix(proj);
		Shader_SetViewMatrix(view);

		std::vector<DirectX::XMMATRIX> vps;
		vps.reserve(cascadeCount);
		for (int i = 0; i < cascadeCount; ++i) vps.push_back(Shadow::GetLightViewProj(i));
		Shader_SetShadowMatrices(vps.data(), cascadeCount, Shadow::GetReceiverBias());
		Shader_SetShadowCascadeParams(Shadow::GetCascadeSplits(), Shadow::GetCascadeBlend());

		ID3D11ShaderResourceView* srvs[Shadow::MAX_CASCADES] = {};
		for (int i = 0; i < cascadeCount; ++i) srvs[i] = Shadow::GetShadowSRV(i);
		Shader_BindShadowMaps(srvs, cascadeCount);

		Skybox_Draw(view, proj);

		// Cull against this viewport's camera (editor debug cam or play cam).
		// Shadow passes above use per-cascade light-ortho cull (open near).
		FrustumCull_BeginMainPass(view * proj);
		SetRenderObject(RENDER_MAIN);
		FrustumCull_EndMainPass();
		Lod_EndFrame();
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
	PhysicsSystem_PrepareContactFlashes();
	DebugDrawer::FlushSphereQueue();
	DebugDrawer::FlushLineQueue();
	PhysicsSystem_FlushDebugDraw();
}
