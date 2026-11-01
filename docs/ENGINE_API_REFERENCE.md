# Ivy Engine — Complete Ability & API Reference

Exhaustive catalog of **Ivy Engine** capabilities and public functions,
scanned from every header under `engine/` and `src/app/` (107 headers).

**Learning how to use the engine?** Start with [`ENGINE_USER_GUIDE.md`](ENGINE_USER_GUIDE.md).  
For concepts, JSON schemas, and agent conventions, see [`ENGINE_MANUAL.md`](ENGINE_MANUAL.md).  
For external content-project setup, see [`CONTENT_PROJECT.md`](CONTENT_PROJECT.md).

> **Regenerate:** `py -3 tools/extract_api_headers.py` (rebuilds the catalog and this document).

---

## 1. Engine overview

| Aspect | Detail |
|--------|--------|
| Name | Ivy Engine |
| Language | C++ (MSVC / Visual Studio) |
| Platform | Windows (Win32) |
| Graphics | Direct3D 11 |
| Math | DirectXMath |
| Models | Autodesk FBX SDK + OBJ (`Model3D`) |
| VFX | Effekseer (DX11) |
| Audio | XAudio2 + X3DAudio |
| Input | Win32 keyboard/mouse + XInput (up to 4 pads) |
| Editor UI | Dear ImGui + ImGuizmo |
| Data | nlohmann/json (scenes, prefabs, UI canvas) |
| Textures | WIC + DirectXTex (DDS cubemaps) |

### Architecture

- **Component–entity scene graph** on `GameObject` (not a sparse ECS).
- **Editor** (`IvyEngine`) vs **runtime** (`IvyEngineRuntime`, `GAME_RUNTIME`).
- **Game logic** in hot-reloadable `GameScripts.dll` via `Script` / `UICanvasScript`.
- Dual transform: legacy `m_position` / `m_rotation` / `m_scale` + `engine::Transform` (`"SceneTransform"`).

### Solution targets

| Project | Role |
|---------|------|
| `IvyEngine` | Editor / development executable |
| `IvyEngineRuntime` | Shipping-style runtime (`GAME_RUNTIME`) |
| `GameScripts` | User scripts → `GameScripts.dll` |

### Folder map

| Path | Purpose |
|------|---------|
| `engine/` | Reusable engine systems |
| `src/app/` | WinMain, game loop, cameras, fades, `GameScene` |
| `game/scripts/` | Game-specific scripts (not engine core) |
| `rom/` | Runtime assets (scenes, models, audio, effects, fonts, prefabs) |
| `shaders/` | HLSL sources |
| `libraries/` | imgui, imguizmo, nlohmann, fbxsdk, effekseer, directxtex |
| `docs/` | This manual set |

### Entry points

- `src/app/main.cpp` — Win32 entry / main loop
- `src/app/scene/game_scene.cpp` — load, update, draw passes
- `src/app/scene/scene_manager.h` — `SceneBase` / `SceneManager`
- `engine/core/engine_gui.cpp` — editor ImGui (toggle **F2**)
- `engine/core/engine_gui_runtime.cpp` — runtime GUI shims

---

## 2. Feature / ability matrix

| System | Status | Notes |
|--------|--------|-------|
| Windowing | Yes | Win32 + D3D11 swapchain; windowed/fullscreen |
| Rendering | Yes | Forward lit meshes, toon/outline, surface FX, unlit, 2D |
| Lighting | Yes | Directional / point / spot + ambient (`Lighting`, max 4) |
| Shadows | Yes | Cascaded shadow maps (`Shadow`), quality presets |
| Skybox | Yes | Cubemap + tint / exposure / rotation |
| Fog | Yes | Height fog in render settings |
| Models / FBX | Yes | Skinned + rigid, animation clips, material override |
| Billboards | Yes | Immediate batch + `BillboardComponent` |
| Particles | Yes | CPU emitters + billboard render (`ParticleSystem`) |
| Effekseer VFX | Yes | `EffectSystem`, `EffectObject`, `EffectComponent` |
| Sprites / textures | Yes | ID cache, WIC load, cubemaps |
| Scene graph / components | Yes | `GameObject` + registered components + JSON |
| Prefabs | Yes | Serialize / instantiate / runtime spawn root |
| Scene load/save | Yes | Versioned JSON + canvas + render settings |
| Physics | Yes | Custom rigidbody + shape colliders (not PhysX) |
| Input | Yes | KB / mouse / XInput + `KeyLogger` |
| Audio | Yes | 2D + 3D XAudio2 |
| UI | Yes | Canvas + scene JSON UI + UI scripts |
| Scripting | Yes | C++ DLL scripts, hot reload, `ScriptRuntimeApi` |
| Editor | Yes | ImGui, gizmos, prefab browser, play mode, content projects, fixed-aspect viewport |
| Runtime build | Yes | Editor stripped under `GAME_RUNTIME` |
| Easing / timer | Yes | `easing.h`, `system_timer.h` |
| Networking | **No** | |

### Registered component type names (JSON, case-sensitive)

`SceneTransform`, `visual`, `Billboard`, `Material`, `Rigidbody`, `Script`, `Collider`, `Effect`, `DirectionalLight`, `PointLight`, `SpotLight`, `Camera`, `Audio`, `AudioListener`

### `Tag` values

Built-in C++ enum (`enum class Tag`) — stable indices for scripts/collision helpers:

`TAG_NONE`, `TAG_SHADOW`, `TAG_GROUND`, `TAG_OBSTACLES`, `TAG_WALL`, `TAG_PLAYER`, `TAG_ENEMY`, `TAG_BULLET`, `TAG_SCLAP`, `TAG_MAX`

Project tag list (`TagRegistry`, `rom/tags.json`) starts with those builtin **names** (`NONE`, `SHADOW`, …) and may append **custom** tags after `TAG_MAX`. Scene JSON `"tag"` is the registry index (not limited to the enum). See `engine/ecs/tag_registry.h`.

### Content projects (`ProjectManager`)

Header: `engine/editor/project_manager.h`

- Open/create/save content roots with `project.json`
- Recent list under `%AppData%/GameEngine/recent_projects.json`
- Editor File menu: New / Open / Save Project (see user guide §20)

### Editor viewport aspect

The ImGui editor game viewport uses a **fixed aspect ratio** (fit-inside layout from the original 1920×1080 chrome). Projection and play use that viewport size; there is no separate aspect setting in render settings.

---

## 3. Shader assets (`shaders/`)

| File | Role |
|------|------|
| `VertexShader3D.hlsl` / `PixelShader3D.hlsl` | Standard lit |
| `PixelShader3D_Hemisphere.hlsl` | Hemisphere lighting |
| `PixelShader3D_Rim.hlsl` | Rim lighting |
| `PixelShader3D_Toon.hlsl` | Toon |
| `PixelShader3D_SurfaceFX.hlsl` | Surface FX |
| `PixelShader3D_DirectionalPixel.hlsl` / `PointPixel` / `SpotPixel` | Per-light-type pixel paths |
| `VertexShader3D_Outline.hlsl` / `PixelShader3D_Outline.hlsl` | Toon outline |
| `VertexShader2D.hlsl` / `PixelShader2D.hlsl` | Sprites / UI |
| `VertexShaderUnlit.hlsl` / `PixelShaderUnlit.hlsl` | Unlit |
| `SkyboxVS.hlsl` / `SkyboxPS.hlsl` | Cubemap sky |
| `ParticleVS.hlsl` / `ParticlePS.hlsl` | Particles |
| `ShaderCommon.hlsli` / `LightingCommon.hlsli` | Shared includes |

`Shader3DVariant` enum: `Standard`, `Hemisphere`, `Rim`, `SurfaceFX`, `Toon`, plus directional/point/spot pixel variants.

---

## 4. Script host API (`ScriptRuntimeApi` / `ScriptApi_*`)

Scripts in `GameScripts.dll` should prefer **`ScriptApi_*` wrappers**. Host function pointers live in `ScriptRuntimeApi` (`engine/scripting/script_runtime_api.h`).

### Access

- `const ScriptRuntimeApi* ScriptRuntimeApi_GetHost();`

### Input

| Host field / wrapper | Purpose |
|----------------------|---------|
| `keyboardGetState` | Current keyboard state |
| `keyLoggerIsTrigger` | Edge-triggered key |
| `mouseIsLeftDown` / `Middle` / `Right` | Mouse buttons |
| `mouseGetWheelDelta` | Wheel |
| `controllerGetLeftStick` / `RightStick` | Analog sticks |
| `controllerGetLeftTrigger` / `RightTrigger` | Triggers |
| `controllerIsPressed` | Button held |

### Camera

| API | Purpose |
|-----|---------|
| `thirdPersonCameraInitialize` | Init follow cam |
| `thirdPersonCameraSetTarget` | Live pos + yaw pointers |
| `thirdPersonCameraSetMouseSensitivity` / `ControllerSensitivity` | Look sens |
| `thirdPersonCameraSetPitchLimits` | Pitch clamp |
| `thirdPersonCameraShowCursor` / `IsCursorVisible` | Cursor |
| `thirdPersonCameraSetMagnetMode` / `SetMagnetTarget` | Magnet feel |
| `cameraGetForward` / `ScriptApi_GetCameraForward` | Forward |
| `cameraSetPosition` / `Target` / `Up` | Global camera |
| `getCameraViewMatrix` / `getCameraProjMatrix` | Matrices |
| `engineGuiSetActiveCameraNodeName` / `ScriptApi_ActivateSceneCamera` | Scene camera |

### Engine GUI / scenes

| API | Purpose |
|-----|---------|
| `engineGuiIsPlaying` | Play mode |
| `engineGuiStartPlay` / `StopPlay` | Play control |
| `engineGuiSetCameraMode` | ThirdPerson / SceneCamera / Intro |
| `engineGuiLoadFromFile` | Load scene JSON |

### Renderer / D3D

| API | Purpose |
|-----|---------|
| `getBackBufferWidth` / `Height` | Backbuffer size |
| `getRenderViewportWidth` / `Height` | Viewport |
| `getD3DDevice` / `getD3DDeviceContext` | D3D11 |

### Sprites / textures

| API | Purpose |
|-----|---------|
| `spriteDraw` | Draw textured quad |
| `textureLoad` / `textureGetSRV` / `textureRelease` | Texture IDs |
| `textureGetWhiteTexture` / `Width` / `Height` | Helpers |

### UI / SceneCanvas

| API | Purpose |
|-----|---------|
| `sceneCanvasGetCanvas` | Shared UI canvas |
| `sceneCanvasShowCountdown` / `ShowCountdownText` / `HideCountdown` | Countdown |
| `sceneCanvasShowRedOverlay` / `Hide` / `SetRedOverlayAlpha` | Overlay |
| `sceneCanvasSetTimerSeconds` | Timer text |
| `sceneCanvasRequestSceneFile` | Queue scene change |
| `uiCanvasGetElement` / `AddSprite` / `RemoveElement` / `GetSprite` | Elements |
| `uiTextSetText` / `uiElementSetVisible` | Text / visibility |
| `uiSpriteSetTexture` / `SetRadial360Fill` / `SetFillNone` / `SetSourceRect` | Sprite |

### Billboards / particles

| API | Purpose |
|-----|---------|
| `billboardAdd` / `billboardAddUv` | Submit billboard |
| `particleCreateEmitter` / `SetEmitterPosition` / `SetEmitterEmitting` | Emitters |
| `particleStopEmitter` / `DestroyEmitter` / `SpawnAt` | Control / burst |

### Colliders / visual / effect / prefabs / debug

| API | Purpose |
|-----|---------|
| `colliderComponentApplyToOwner` | Apply component |
| `gameObjectCreateBoxCollider` / `DestroyCollider` | Box collider |
| `colliderGetWorldCenter` / box size get/set / sphere radius / footprint | Queries |
| `visualPlayClip` | Animation clip |
| `effectPlay` | Effekseer on component |
| `prefabInstantiate` / `Destroy` / `SetBeforeDestroyCallback` | Prefabs |
| `debugDrawSphereWire` | Debug sphere |
| `pickupDebugSetMessage` | Debug HUD message |
| `getCurrentScene` | Current `SceneBase*` |

Full C++ signatures for every `ScriptApi_*` function are listed under `engine/scripting/script_runtime_api.h` in §6.

---

## 5. Macros for registration

| Macro | Header | Purpose |
|-------|--------|---------|
| `REGISTER_COMPONENT(TypeName)` | `component_factory.h` | Register JSON component type |
| `REGISTER_SCRIPT(TYPE)` | `script_dll_register.h` | Register gameplay script class |
| `REGISTER_UI_SCRIPT(TYPE)` | `ui_canvas_script_register.h` | Register UI canvas script |
| `SCRIPT_DECLARE_FIELD` / `SCRIPT_SERIALIZE_METHODS` / … | `script_fields.h` | Script editable fields + JSON |

---

## 6. Complete per-header function catalog

Declarations below were machine-extracted from public headers under `engine/` and `src/app/`.
Constructors, destructors, and some `private:` helpers declared in headers may appear.
Vendor header `WICTextureLoader11.h` is omitted.

### `engine/audio/audio_listener_sync.h`
**Types:** GameObject
- `void AudioListenerSync_Tick(const std::vector<std::unique_ptr<GameObject>>& roots);`
- `void AudioListenerSync_EnsureOnCamera(GameObject* cameraNode);`

### `engine/audio/audio_manager.h`
**Types:** Playback, GameObject, AudioManager
**Enums:** AudioType
- `static AudioManager& Instance();`
- `void Initialize();`
- `void Finalize();`
- `void Update();`
- `void SetListener(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& up);`
- `void SetListener(GameObject* gameObject);`
- `void OnGameObjectDestroyed(GameObject* gameObject);`
- `void PlayBGM(const std::string& id, const std::string& path, float volume = 1.0f, bool loop = true);`
- `void PlaySFX(const std::string& id, const std::string& path, float volume = 1.0f, bool loop = false);`
- `void PlayVoice(const std::string& id, const std::string& path, float volume = 1.0f);`
- `void LoadBGM(const std::string& id, const std::string& path);`
- `void PlayBGM(const std::string& id, float volume = 1.0f, bool loop = true);`
- `void LoadSFX(const std::string& id, const std::string& path);`
- `void PlaySFX(const std::string& id, float volume = 1.0f, bool loop = false);`
- `void LoadVoice(const std::string& id, const std::string& path);`
- `void PlayVoice(const std::string& id, float volume = 1.0f);`
- `void PlaySFX3D(const std::string& id, const std::string& path, const DirectX::XMFLOAT3& position, float volume = 1.0f, bool loop = false);`
- `void Load3DSFX(const std::string& id, const std::string& path);`
- `void PlaySFX3D(const std::string& id, float volume = 1.0f, bool loop = false);`
- `void Stop(const std::string& id);`
- `void StopAll();`
- `void StopAllByType(AudioType type);`
- `bool IsPlaying(const std::string& id);`
- `AudioPlaybackId StartInstance(const std::string& path, AudioType type, float volume, bool loop, bool is3D, const DirectX::XMFLOAT3& position, float distanceScale);`
- `AudioPlaybackId StartInstanceFromLoaded(const std::string& loadedId, float volume = 1.0f, bool persistAcrossScene = false);`
- `void StopInstance(AudioPlaybackId id);`
- `bool IsInstancePlaying(AudioPlaybackId id) const;`
- `void SetInstancePosition(AudioPlaybackId id, const DirectX::XMFLOAT3& position);`
- `void SetInstanceDistanceScale(AudioPlaybackId id, float scale);`
- `void SetInstanceVolume(AudioPlaybackId id, float volume);`
- `void StopAllInstances();`
- `void StopSceneInstances();`
- `void SetEmitterPosition(const std::string& id, const DirectX::XMFLOAT3& position);`
- `void SetEmitterDistanceScale(const std::string& id, float scale);`
- `void SetEmitter(const std::string& id, GameObject* gameObject);`
- `void SetListenerStereoBalance(bool centerBalance);`
- `void SetVolume(const std::string& id, float volume);`
- `void SetMasterVolume(float volume);`
- `void SetBGMVolume(float volume);`
- `void SetSFXVolume(float volume);`
- `void SetVoiceVolume(float volume);`
- `void SetUiClickSound(const std::string& path);`
- `void PlayUiClick(float volume = 1.0f);`
- `bool LoadBusVolumesFromProjectFile(const std::string& projectJsonPath);`
- `void TryLoadBusVolumesFromNearbyProject();`
- `void DrawDebug();`
- `bool GetDebugInfo( const std::string& id, float* outDistance, float* outMatrixVolumeL, float* outMatrixVolumeR );`
- `void Apply3DAudio(Playback* source);`
- `void UpdateVolume(Playback* source);`
- `bool LoadWav(const std::string& path, Playback* outSource);`
- `void ReapplyAllVolumes();`
- `void SweepFinishedInstances();`
- `void Setup3DEmitter(Playback* source, const DirectX::XMFLOAT3& position, float distanceScale);`
- `bool SubmitAndStart(Playback* source, bool loop);`

### `engine/core/debug_system.h`
- `void Init();`
- `void Shutdown();`
- `void ToggleCollisionViz();`
- `bool CollisionVizEnabled();`
- `float GetTimeScale();`
- `void CycleTimeScale();`
- `bool IsFrozen();`
- `void StepOneFrame();`
- `bool FrameStepRequested();`
- `float StepAdvance();`
- `bool ConsumeFrameStep();`
- `void SetOverlayText(const char* text);`
- `const char* GetOverlayText();`
- `DirectX::XMFLOAT4 GetOverlayColor();`
- `void RefreshOverlay();`
- `void ResetTimeScale();`

### `engine/core/easing.h`
- `double easeInSine(double t);`
- `double easeOutSine(double t);`
- `double easeInOutSine(double t);`
- `double easeInQuad(double t);`
- `double easeOutQuad(double t);`
- `double easeInOutQuad(double t);`
- `double easeInCubic(double t);`
- `double easeOutCubic(double t);`
- `double easeInOutCubic(double t);`
- `double easeInQuart(double t);`
- `double easeOutQuart(double t);`
- `double easeInOutQuart(double t);`
- `double easeInQuint(double t);`
- `double easeOutQuint(double t);`
- `double easeInOutQuint(double t);`
- `double easeInExpo(double t);`
- `double easeOutExpo(double t);`
- `double easeInOutExpo(double t);`
- `double easeInCirc(double t);`
- `double easeOutCirc(double t);`
- `double easeInOutCirc(double t);`
- `double easeInBack(double t);`
- `double easeOutBack(double t);`
- `double easeInOutBack(double t);`
- `double easeInElastic(double t);`
- `double easeOutElastic(double t);`
- `double easeInOutElastic(double t);`
- `double easeInBounce(double t);`
- `double easeOutBounce(double t);`
- `double easeInOutBounce(double t);`

### `engine/core/engine_gui.h`
**Types:** GameObject
**Enums:** CameraMode, EditorEditMode
- `bool EngineGUI_IsSceneGraphObject(GameObject* obj);`
- `void EngineGUI_Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);`
- `void EngineGUI_Finalize();`
- `void EngineGUI_Update(double elapsed_time);`
- `void EngineGUI_Draw();`
- `void EngineGUI_OnBackbufferResized();`
- `void EngineGUI_DrawSelectionIndicator();`
- `bool EngineGUI_IsPlaying();`
- `bool EngineGUI_IsDebugCameraPlayInputActive();`
- `bool EngineGUI_IsOnStart();`
- `bool EngineGUI_PreferPlayVisuals();`
- `void EngineGUI_StartPlay();`
- `void EngineGUI_StopPlay();`
- `void EngineGUI_SetEnabled(bool enabled);`
- `void EngineGUI_GetGameViewportRect(float& x, float& y, float& width, float& height);`
- `bool EngineGUI_MouseToViewportLocal(float& x, float& y);`
- `bool EngineGUI_SaveToFile(const char* filename);`
- `bool EngineGUI_LoadFromFile(const char* filename);`
- `const std::string& EngineGUI_GetDefaultScenePath();`
- `std::string EngineGUI_ResolveStartupScenePath();`
- `bool EngineGUI_IsSceneDirty();`
- `void EngineGUI_MarkSceneDirty();`
- `void EngineGUI_ClearSceneDirty();`
- `void EngineGUI_QueueUnsavedLoad(const char* filename);`
- `void EngineGUI_QueueUnsavedExit();`
- `void DrawEditorSceneProxies();`
- `void DrawSceneModels();`
- `CameraMode EngineGUI_GetCameraMode();`
- `void EngineGUI_SetCameraMode(CameraMode mode);`
- `const std::string& EngineGUI_GetActiveCameraNodeName();`
- `void EngineGUI_SetActiveCameraNodeName(const std::string& name);`
- `bool EngineGUI_IsUiOverlayVisible();`
- `void EngineGUI_SetUiOverlayVisible(bool visible);`
- `bool EngineGUI_IsSceneAssetPreloadActive();`
- `EditorEditMode EngineGUI_GetEditMode();`
- `void EngineGUI_SetEditMode(EditorEditMode mode);`
- `bool EngineGUI_IsUiEditMode();`

### `engine/core/engine_gui_internal.h`
**Types:** GameObject, UIElement
- `void ApplyEditorColliders();`
- `void RestoreEditorDebugColliderFlags();`
- `void TeardownPlaySession(bool resetEditorState);`
- `void ReturnToStart();`
- `void SoftStopPlayForSceneLoad();`
- `bool EngineGUI_LoadFromFileCore(const char* filename);`
- `void EngineGUI_QueueSceneLoad(const char* filename);`
- `void EngineGUI_BeginSceneAssetPreload();`
- `void EngineGUI_TickSceneAssetPreload();`
- `void EngineGUI_FlushSceneLoadPending();`

### `engine/core/frame_transform_snapshot.h`
**Types:** GameObject, FrameTransformEntry
- `void FrameTransformSnapshot_Capture(const std::vector<GameObject*>& objects);`
- `const FrameTransformEntry* FrameTransformSnapshot_Find(const GameObject* object);`
- `void FrameTransformSnapshot_Clear();`

### `engine/core/job_system.h`
**Types:** JobNode
- `void JobSystem_Init(int workerCount);`
- `void JobSystem_Shutdown();`
- `JobHandle JobSystem_Submit(std::function<void()> job);`
- `JobHandle JobSystem_SubmitDep(std::function<void()> job, const std::vector<JobHandle>& deps);`
- `JobHandle JobSystem_SubmitAfter(JobHandle after, std::function<void()> job);`
- `void JobSystem_WaitFor(JobHandle h);`
- `void JobSystem_WaitAll();`
- `bool JobSystem_UseParallel();`
- `void JobSystem_SetUseParallel(bool enabled);`
- `void JobSystem_SetDebugWindowVisible(bool visible);`
- `bool JobSystem_IsDebugWindowVisible();`
- `void JobSystem_ToggleDebugWindow();`
- `void JobSystem_DebugDraw();`

### `engine/core/system_timer.h`
- `void SystemTimer_Initialize(void);`
- `void SystemTimer_Reset(void);`
- `void SystemTimer_Start(void);`
- `void SystemTimer_Stop(void);`
- `void SystemTimer_Advance(void);`
- `double SystemTimer_GetTime(void);`
- `double SystemTimer_GetAbsoluteTime(void);`
- `float SystemTimer_GetElapsedTime(void);`
- `bool SystemTimer_IsStoped(void);`
- `void LimitThreadAffinityToCurrentProc(void);`

### `engine/debug_draw/debug_drawer.h`
**Types:** DebugDrawer, SphereDrawRequest, LineDrawRequest
- `bool Initialize();`
- `void Release();`
- `void DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });`
- `void DrawSphere(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }, bool enableDepth = true);`
- `static void QueueSphere(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color);`
- `static void FlushSphereQueue();`
- `static void QueueLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color);`
- `static void FlushLineQueue();`
- `void DrawCapsule(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, float height, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });`
- `void DrawCylinder(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, float height, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });`
- `void DrawLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });`
- `void DrawLines(const std::vector<DirectX::XMFLOAT3>& lineVerts, const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });`
- `void DrawLinesCached(const std::vector<DirectX::XMFLOAT3>& lineVerts, const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });`

### `engine/debug_draw/debug_ofstream.h`
**Types:** debugbuf, debug_ostream
- `virtual ~debugbuf() { sync();`
- `int sync() { OutputDebugStringA(str().c_str());`
- `public: debug_ostream() : std::basic_ostream<char, std::char_traits<char>>(new debugbuf()) {} ~debug_ostream() { delete rdbuf();`

### `engine/debug_draw/debug_text.h`
**Types:** DebugText, StringBlock, Line, Vertex
- `void SetText(const char* pText, DirectX::XMFLOAT4 color);`
- `void Draw();`
- `void Clear();`
- `void MeasureText(const char* text, float& outWidth, float& outHeight) const;`
- `void createBuffer(ULONG characterCount);`

### `engine/ecs/audio_component.h`
**Types:** AudioComponent
- `void OnAttach() override;`
- `void OnDetach() override;`
- `void Update(double elapsed_time) override;`
- `void OnReset() override;`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`
- `void Play();`
- `void Stop();`
- `bool IsPlaying() const;`
- `void SetClipPath(const std::string& path);`
- `void ApplyLiveSettings();`
- `void FlushCommands();`
- `DirectX::XMFLOAT3 OwnerPosition() const;`
- `void ApplyStopImmediate();`

### `engine/ecs/audio_listener_component.h`
**Types:** AudioListenerComponent
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`

### `engine/ecs/audio_system.h`
- `void AudioSystem_FlushCommands();`
- `void AudioSystem_Register(class AudioComponent* component);`
- `void AudioSystem_Unregister(class AudioComponent* component);`

### `engine/ecs/billboard_component.h`
**Types:** ID3D11ShaderResourceView, GameObject, BillboardComponent
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`
- `ID3D11ShaderResourceView* GetTextureSRV();`
- `static void SubmitSceneBillboards( const std::vector<std::unique_ptr<GameObject>>& roots);`
- `static void SubmitNodeBillboards(GameObject* node);`

### `engine/ecs/camera_component.h`
**Types:** CameraComponent
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`
- `static DirectX::XMFLOAT3 ComputeEyeWS(const engine::Transform* transform);`
- `static DirectX::XMFLOAT3 ComputeForwardWS(const engine::Transform* transform);`
- `static DirectX::XMFLOAT3 ComputeUpWS(const engine::Transform* transform);`
- `static DirectX::XMFLOAT3 ComputeRightWS(const engine::Transform* transform);`
- `static DirectX::XMFLOAT3 ComputeViewUpWS(const DirectX::XMFLOAT3& forwardWS);`
- `void ApplyToGlobalCamera(const engine::Transform* transform) const;`

### `engine/ecs/collider_component.h`
**Types:** ColliderComponent
- `void ApplyToOwner();`
- `void ClearFromOwner();`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`

### `engine/ecs/component.h`
**Types:** GameObject, Component
- `virtual const char* GetTypeName() const = 0;`

### `engine/ecs/component_factory.h`
**Types:** ComponentFactory
- `static ComponentFactory& Instance();`
- `void Register(const std::string& typeName, CreatorFunc creator);`
- `std::unique_ptr<Component> Create(const std::string& typeName) const;`
- `bool IsRegistered(const std::string& typeName) const;`

### `engine/ecs/directional_light_component.h`
**Types:** GameObject, DirectionalLightComponent
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`
- `static DirectX::XMFLOAT3 ComputeDirectionWS(const engine::Transform* transform);`

### `engine/ecs/effect_component.h`
**Types:** EffectComponent
- `void OnAttach() override;`
- `void OnDetach() override;`
- `void Update(double elapsed_time) override;`
- `void OnReset() override;`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`
- `void Play();`
- `void Stop();`
- `bool IsPlaying() const;`
- `void SetEffectPath(const std::string& path);`
- `void FlushCommands();`
- `bool LoadEffect();`
- `void SyncTransformImmediate();`
- `void ApplyStopImmediate();`

### `engine/ecs/effect_system_flush.h`
- `void EffectSystem_FlushCommands();`
- `void EffectSystem_RegisterComponent(class EffectComponent* component);`
- `void EffectSystem_UnregisterComponent(class EffectComponent* component);`

### `engine/ecs/game_object.h`
**Types:** GameObject
**Enums:** Tag
- `explicit GameObject(std::string name = "GameObject") : m_name(std::move(name)) {} virtual ~GameObject();`
- `T* AddComponent(Args&&... args) { auto comp = std::make_unique<T>(std::forward<Args>(args)...);`
- `std::type_index key(typeid(T));`
- `std::type_index key(typeid(*comp));`
- `T* GetComponent() { auto it = m_components.find(std::type_index(typeid(T)));`
- `const T* GetComponent() const { auto it = m_components.find(std::type_index(typeid(T)));`
- `void RemoveComponent() { auto it = m_components.find(std::type_index(typeid(T)));`
- `virtual void Initialize() {} virtual void Finalize() {} void Update(double elapsed_time);`
- `static bool IsInsideUpdate();`
- `bool Update_IsParallelSafe() const;`
- `void Reset() { for (auto& kv : m_components) kv.second->OnReset();`
- `virtual void LogicUpdate(double ) {} virtual void IntegratePhysics(double elapsed_time) { RigidbodyComponent* rc = GetComponent<RigidbodyComponent>();`
- `virtual void OnCollision(GameObject* ) {} virtual void OnCollisionExit(GameObject* ) {} virtual void OnTriggerEnter(GameObject* other);`
- `virtual void OnTriggerExit(GameObject* other);`
- `void SetTag(int tagId) { if (tagId >= 0) m_tag = static_cast<Tag>(tagId);`
- `int GetTagId() const { return static_cast<int>(m_tag);`
- `Rigidbody& GetRigidbody() { RigidbodyComponent* rc = GetComponent<RigidbodyComponent>();`
- `const Rigidbody& GetRigidbody() const { if (const RigidbodyComponent* rc = GetComponent<RigidbodyComponent>()) return rc->Body();`
- `engine::Transform* GetTransform();`
- `const engine::Transform* GetTransform() const;`
- `void SyncLegacyTransformFromComponents();`
- `void SyncComponentsFromLegacyTransform();`
- `void ReparentChild(GameObject* child, GameObject& newParent);`
- `void DrawVisual(bool useRealModel);`
- `void GetWorldPose(DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& rotation, DirectX::XMFLOAT3& scale) const;`
- `void AddIgnoreCollisionTag(Tag tag);`
- `void RemoveIgnoreCollisionTag(Tag tag);`
- `void AddIgnoreCollisionObject(GameObject* other);`
- `void RemoveIgnoreCollisionObject(GameObject* other);`
- `void ClearIgnoreCollisionObjects();`
- `bool IgnoresCollisionWith(const GameObject* other) const;`
- `void RefreshColliderData(Model3D& sourceModel) { if (m_collider && m_collider->GetType() == ColliderType::MESH) { auto* meshCol = static_cast<MeshCollider*>(m_collider);`

### `engine/ecs/material_component.h`
**Types:** ID3D11ShaderResourceView, MaterialComponent
- `void OnDetach() override;`
- `void OnReset() override;`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`
- `ID3D11ShaderResourceView* GetNormalMapSRV();`
- `ID3D11ShaderResourceView* GetRampTextureSRV();`
- `void ReleaseNormalMap();`
- `void ReleaseRampTexture();`

### `engine/ecs/model_asset_cache.h`
**Types:** ModelAssetCache, ObjectDef, ModelFolderNode
- `static ModelAssetCache& Instance();`
- `void Initialize();`
- `void Finalize();`
- `void RefreshAssetDirectory();`
- `const std::vector<ObjectDef>& GetObjectDefinitions() const;`
- `std::vector<const ObjectDef*> GetObjectDefinitionsInFolder( const std::string& folderRelativePath) const;`
- `Model3D* GetModel(const std::string& modelNameOrPath);`
- `void RebuildFolderTree();`
- `static std::string FolderPathForDef(const ObjectDef& def);`

### `engine/ecs/point_light_component.h`
**Types:** PointLightComponent
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`

### `engine/ecs/prefab_runtime.h`
**Types:** SceneBase
- `void PrefabRuntime_SetBeforeDestroyCallback(PrefabRuntimeBeforeDestroyFn callback);`
- `GameObject* PrefabRuntime_EnsureSpawnRoot();`
- `bool PrefabRuntime_IsSpawnRoot(const GameObject* obj);`
- `bool PrefabRuntime_IsRuntimeSpawned(const GameObject* obj);`
- `GameObject* PrefabRuntime_Instantiate(const char* prefabPath, const DirectX::XMFLOAT3& worldPos, const DirectX::XMFLOAT3& worldRot, GameObject* parent, bool startActive);`
- `void PrefabRuntime_Destroy(GameObject* obj);`
- `void PrefabRuntime_DestroyAllSpawnedChildren();`
- `void PrefabRuntime_FlushSpawned();`
- `void PrefabRuntime_FlushDestroyed();`
- `void PrefabRuntime_ResetSessionState();`

### `engine/ecs/prefab_serializer.h`
**Types:** PrefabSerializer
- `static std::string Serialize(const GameObject& root);`
- `static bool SaveToFile(const GameObject& root, const std::string& filePath);`
- `static std::unique_ptr<GameObject> Deserialize(const std::string& json);`
- `static std::unique_ptr<GameObject> LoadFromFile(const std::string& filePath);`

### `engine/ecs/rigidbody_component.h`
**Types:** RigidbodyComponent
- `void ApplyToOwner();`
- `void OnReset() override;`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`

### `engine/ecs/spot_light_component.h`
**Types:** SpotLightComponent
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`

### `engine/ecs/string_utils.h`
- `std::wstring w(static_cast<size_t>(needed), L'\0');`

### `engine/ecs/tag_registry.h`
**Types:** TagRegistry
- `static TagRegistry& Instance();`
- `void EnsureDefaults();`
- `bool LoadFromFile(const std::string& path = "rom/tags.json");`
- `bool SaveToFile(const std::string& path = "rom/tags.json") const;`
- `int GetCount() const { return static_cast<int>(m_names.size());`
- `const char* GetName(int id) const;`
- `bool IsValid(int id) const;`
- `int FindByName(const std::string& name) const;`
- `int AddTag(const std::string& name);`
- `bool IsBuiltin(int id) const;`
- `bool CanRemove(int id) const;`
- `bool RemoveTag(int id);`
- `private: TagRegistry();`

### `engine/ecs/transform.h`
**Types:** GameObject, Transform
- `void OnAttach() override;`
- `void OnReset() override;`
- `void SnapshotStartState();`
- `DirectX::XMMATRIX GetLocalMatrix() const;`
- `DirectX::XMMATRIX GetWorldMatrix() const;`
- `void SetFromLocalMatrix(DirectX::XMMATRIX m);`
- `DirectX::XMFLOAT3 GetWorldPosition() const;`
- `void GetWorldPositionRotationScale(DirectX::XMFLOAT3& outPosition, DirectX::XMFLOAT3& outRotation, DirectX::XMFLOAT3& outScale) const;`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& json) override;`

### `engine/ecs/visual_component.h`
**Types:** VisualComponent
- `ID3D11ShaderResourceView* GetSurfaceFxSRV();`
- `void OnDetach() override;`
- `void OnReset() override;`
- `void Update(double elapsed_time) override;`
- `void PlayClip(const std::string& clipName);`
- `void PlayClip(const std::string& clipName, bool loop);`
- `float GetActiveClipDuration() const;`
- `void SetAnimationSpeed(float speed);`
- `return GetFromCache(path);`
- `Model3D* GetRealModel() const;`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`
- `static Model3D* GetFromCache(const std::string& path);`
- `void EnsureAnimInstance();`
- `void EnsureAnimationsLoaded();`
- `void ReleaseSurfaceFxTexture();`

### `engine/ecs/visual_system.h`
- `void VisualSystem_UploadSkinning();`

### `engine/editor/camera_gizmo.h`
**Types:** DebugDrawer, GameObject, CameraComponent
- `void DrawCamera(DebugDrawer& drawer, const engine::Transform* transform, const CameraComponent* camera);`
- `void DrawSceneCameras(const std::vector<std::unique_ptr<GameObject>>& roots, DebugDrawer& drawer);`

### `engine/editor/editor_prefab_panel.h`
**Types:** GameObject
- `GameObject* PrefabPanel( const std::string& prefabFolder, std::vector<std::unique_ptr<GameObject>>& nodes);`

### `engine/editor/editor_project_browser.h`
**Types:** GameObject
- `GameObject* ProjectBrowserPanel(std::vector<std::unique_ptr<GameObject>>& nodes);`

### `engine/editor/editor_ui_panel.h`
**Types:** GameObject, UIElement
**Enums:** UiCreateType
- `UIElement* CreateUiElement(UiCreateType type);`
- `void DrawCanvasElementHierarchy(UIElement*& selectedUiElement, GameObject*& selectedNode);`
- `void DrawCanvasElementProperties(UIElement*& selectedUiElement);`

### `engine/editor/editor_undo.h`
**Types:** GameObject, UIElement, NodeRef, UiElementRef, EditorCommand, EditorUndoStack
- `virtual void Undo() = 0;`
- `virtual void Redo() = 0;`
- `virtual std::string Describe() const = 0;`
- `static EditorUndoStack& Instance();`
- `void Push(std::unique_ptr<EditorCommand> cmd);`
- `bool Undo();`
- `bool Redo();`
- `bool CanUndo() const;`
- `bool CanRedo() const;`
- `size_t Depth() const;`
- `const std::vector<std::string>& HistoryLabels() const;`
- `std::string TopUndoDescribe() const;`
- `std::string TopRedoDescribe() const;`
- `void Clear();`
- `void RebuildLabels();`
- `NodeRef MakeNodeRef(GameObject* node);`
- `GameObject* ResolveNode(const NodeRef& ref);`
- `UiElementRef MakeUiElementRef(UIElement* el);`
- `UIElement* ResolveUiElement(const UiElementRef& ref);`
- `void EditorUndo_Push(std::unique_ptr<EditorCommand> cmd);`
- `bool EditorUndo_Undo();`
- `bool EditorUndo_Redo();`
- `void EditorUndo_Clear();`
- `void EditorUndo_PushTransform(GameObject* node, std::string beforeJson, std::string afterJson);`
- `void EditorUndo_PushComponentEdit(GameObject* node, const std::string& compType, std::string beforeJson, std::string afterJson);`
- `void EditorUndo_PushCreate(GameObject* created, GameObject* parent, int insertIndex);`
- `void EditorUndo_PushDelete(GameObject* toDelete);`
- `void EditorUndo_PushRename(GameObject* node, std::string oldName, std::string newName);`
- `void EditorUndo_PushTag(GameObject* node, int oldTag, int newTag);`
- `void EditorUndo_PushReparent(GameObject* node, GameObject* oldParent, int oldIndex, GameObject* newParent, int newIndex);`
- `void EditorUndo_PushAddComponent(GameObject* node, const std::string& compType);`
- `void EditorUndo_PushRemoveComponent(GameObject* node, const std::string& compType);`
- `void EditorUndo_BeginComponentEdit(GameObject* node, const char* compType);`
- `void EditorUndo_EndComponentEdit(GameObject* node, const char* compType);`
- `void EditorUndo_DiscardComponentEdit();`
- `void EditorUndo_PushUiCreate(UIElement* created);`
- `void EditorUndo_PushUiDelete(UIElement* toDelete);`
- `void EditorUndo_PushUiEdit(UIElement* el, std::string beforeJson, std::string afterJson);`
- `void EditorUndo_PushUiReorder(UIElement* el, int oldIndex, int newIndex);`
- `void EditorUndo_BeginUiEdit(UIElement* el);`
- `void EditorUndo_EndUiEdit(UIElement* el);`
- `void EditorUndo_DiscardUiEdit();`
- `void EditorUndo_DeleteNodeInternal(GameObject* toDelete);`

### `engine/editor/light_gizmo.h`
**Types:** DebugDrawer, GameObject
- `DirectX::XMFLOAT3 GetWorldPosition(const engine::Transform* transform);`
- `DirectX::XMFLOAT3 GetShineDirectionWS(const engine::Transform* transform);`
- `DirectX::XMFLOAT3 GetIncomingDirectionWS(const engine::Transform* transform);`
- `void DrawDirectional(DebugDrawer& drawer, const engine::Transform* transform, const DirectX::XMFLOAT4& color);`
- `void DrawPoint(DebugDrawer& drawer, const engine::Transform* transform, float range, const DirectX::XMFLOAT4& color);`
- `void DrawSpot(DebugDrawer& drawer, const engine::Transform* transform, float range, float outerAngleDeg, float innerAngleDeg, const DirectX::XMFLOAT4& color);`
- `void DrawSceneLights(const std::vector<std::unique_ptr<GameObject>>& roots, DebugDrawer& drawer);`

### `engine/editor/playable_package_builder.h`
**Types:** Prefs, Request
**Enums:** Configuration
- `const char* ConfigurationName(Configuration configuration);`
- `bool TryParseConfiguration(const std::string& name, Configuration* out);`
- `std::string DefaultDestination(const std::string& contentRoot);`
- `void LoadPrefs(const std::string& contentRoot, Prefs* out);`
- `void SavePrefs(const std::string& contentRoot, const Prefs& prefs);`
- `std::string DestinationRefusalReason(const std::string& contentRoot, const std::string& destination);`
- `bool IsBuildRunning();`
- `bool StartBuild(const Request& request);`
- `void CopyLog(std::string* out);`
- `bool IsBuildFinished();`
- `bool LastBuildSucceeded();`
- `std::string LastBuildResultMessage();`

### `engine/editor/project_asset_index.h`
**Types:** ProjectAssetIndex, ProjectEntry, ProjectFolderNode
**Enums:** Kind
- `static ProjectAssetIndex& Instance();`
- `void Refresh();`
- `std::vector<ProjectEntry> GetFolderContents(const std::string& folderRelativePath) const;`
- `std::vector<ProjectEntry> GetFilesOfKind(Kind kind) const;`
- `void RebuildFolderTree();`
- `static Kind ClassifyFile(const std::string& relativePath);`

### `engine/editor/project_asset_ops.h`
**Types:** ProjectAssetOps
- `static bool RenameAsset(const std::string& oldRel, const std::string& newRel, std::string& error);`
- `static bool DeleteAssetToRecycleBin(const std::string& relPath, std::string& error);`
- `static bool CreateFolder(const std::string& parentRel, const std::string& name, std::string& error);`
- `static bool CreateMinimalScene(const std::string& parentRel, const std::string& name, std::string& error);`
- `static bool IsCurrentSceneOpen(const std::string& relPath);`
- `static bool FolderContainsCurrentScene(const std::string& relPath);`
- `static std::vector<std::string> FindSceneReferences(const std::string& relPath);`
- `static bool ShowInExplorer(const std::string& relPath, bool selectItem, std::string& error);`
- `static std::string AssetPathFromRelative(const std::string& relPath);`
- `static std::string AbsolutePathFromRelative(const std::string& relPath);`
- `static bool IsScriptsPath(const std::string& relPath);`
- `static bool IsValidLeafName(const std::string& name, std::string& error);`

### `engine/editor/project_manager.h`
**Types:** HWND__, ProjectManager, EditorSettings, PhysicsSettings, AudioSettings, ProjectData
- `static ProjectManager& Instance();`
- `bool BootstrapAtStartup();`
- `bool OpenProject(const std::string& rootPath, std::string* errorOut = nullptr);`
- `void CloseProject();`
- `bool CreateProject(const std::string& parentDir, const std::string& projectName, std::string* errorOut = nullptr);`
- `bool SaveProjectFile(std::string* errorOut = nullptr);`
- `void SetLastOpenedScene(const std::string& fileName);`
- `void SetDefaultScene(const std::string& fileName);`
- `void SetEditorUiZoomPercent(int percent);`
- `void SetPhysicsGravity(float gravity);`
- `void SetAudioBuses(float master, float bgm, float sfx, float voice);`
- `void SetUiClickSound(const std::string& path);`
- `std::string ResolveSceneToLoad() const;`
- `void TouchRecent(const std::string& rootPath);`
- `void UpdateWindowTitle(HWND hwnd) const;`
- `static bool PickFolder(HWND owner, const wchar_t* title, std::string* outPath, const char* startFolder = nullptr);`
- `static std::string GetExecutableDirectory();`
- `static std::string FindEngineRepoRoot();`
- `bool ActivateRoot(const std::string& rootPath, std::string* errorOut);`
- `bool LoadProjectFile(const std::string& rootPath, ProjectData* out, std::string* errorOut);`
- `bool WriteProjectFile(const std::string& rootPath, const ProjectData& data, std::string* errorOut) const;`
- `ProjectData MakeDefaultData(const std::string& rootPath, const std::string& name) const;`
- `bool ScaffoldNewProjectTree(const std::string& rootPath, const std::string& projectName, std::string* errorOut);`
- `void LoadRecentList();`
- `void SaveRecentList() const;`
- `static std::string RecentListPath();`
- `static std::string Utf8FromWide(const std::wstring& wide);`
- `static std::wstring WideFromUtf8(const std::string& utf8);`

### `engine/editor/transform_gizmo.h`
**Types:** GameObject
- `void BeginFrame();`
- `void HandleHotkeys();`
- `void SetVisible(bool visible);`
- `bool IsVisible();`
- `void Draw(GameObject* selected);`
- `bool IsInteracting();`
- `void DrawToolbarControls();`

### `engine/editor/ui_gizmo.h`
**Types:** UIElement
- `void HandleInput(UIElement* selected);`
- `void Draw(UIElement* selected);`
- `bool IsInteracting();`
- `bool WouldCaptureMouse(UIElement* selected, float viewportX, float viewportY);`

### `engine/input/controller.h`
**Enums:** ControllerButton
- `void Controller_Initialize();`
- `void Controller_Finalize();`
- `void Controller_Update();`
- `bool Controller_IsConnected(int padIndex = 0);`
- `bool Controller_IsPressed(ControllerButton button, int padIndex = 0);`
- `bool Controller_IsTrigger(ControllerButton button, int padIndex = 0);`
- `bool Controller_IsRelease(ControllerButton button, int padIndex = 0);`
- `DirectX::XMFLOAT2 Controller_GetLeftStick(int padIndex = 0);`
- `DirectX::XMFLOAT2 Controller_GetRightStick(int padIndex = 0);`
- `float Controller_GetLeftTrigger(int padIndex = 0);`
- `float Controller_GetRightTrigger(int padIndex = 0);`
- `void Controller_SetStickDeadzone(short left, short right);`
- `void Controller_SetTriggerDeadzone(BYTE trigger);`
- `void Controller_SetVibration(float leftMotor, float rightMotor, int padIndex = 0);`

### `engine/input/key_logger.h`
- `void KeyLogger_Initialize();`
- `void KeyLogger_Update(double now_time);`
- `static void Keyboard_SetKeyDown(Keyboard_State* state, Keyboard_Keys key, bool down);`
- `bool KeyLogger_IsPressed(Keyboard_Keys key);`
- `bool KeyLogger_IsTrigger(Keyboard_Keys key);`
- `bool KeyLogger_IsRelease(Keyboard_Keys key);`
- `bool KeyLogger_IsDoubleTap(Keyboard_Keys key);`
- `bool KeyLogger_IsRepeat(Keyboard_Keys key);`
- `bool KeyLogger_IsLongPress(Keyboard_Keys key);`
- `double KeyLogger_GetHoldTime(Keyboard_Keys key);`

### `engine/input/keyboard.h`
- `void Keyboard_Initialize(void);`
- `bool Keyboard_IsKeyDown(Keyboard_Keys key);`
- `bool Keyboard_IsKeyUp(Keyboard_Keys key);`
- `const Keyboard_State* Keyboard_GetState(void);`
- `const Keyboard_State* Keyboard_GetStateOld(void);`
- `bool Keyboard_IsKeyDown(Keyboard_Keys key, const Keyboard_State* pState);`
- `bool Keyboard_IsKeyUp(Keyboard_Keys key, const Keyboard_State* pState);`
- `void Keyboard_Reset(void);`
- `void Keyboard_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);`
- `bool Keyboard_IsKeyDownTrigger(Keyboard_Keys key);`
- `void keycopy();`

### `engine/input/mouse.h`
- `void Mouse_Initialize();`
- `void Mouse_ResetFrame();`
- `void Mouse_ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, HWND hWnd);`
- `void Mouse_GetDelta(int& dx, int& dy);`
- `bool Mouse_IsLeftDown();`
- `bool Mouse_IsRightDown();`
- `bool Mouse_IsMiddleDown();`
- `int Mouse_GetWheelDelta();`
- `void Mouse_ClearWheel();`
- `int Mouse_GetX();`
- `int Mouse_GetY();`
- `void Mouse_GetPosition(int& x, int& y);`
- `void Mouse_GetClientPosition(int& x, int& y);`

### `engine/physics/collider/collider.h`
**Types:** GameObject, Model3D, ContactInfo, Collider, BoxCollider, SphereCollider, CapsuleCollider, CylinderCollider, MeshCollider
**Enums:** Tag, ColliderType
- `DirectX::XMFLOAT3 GetWorldCenter() const;`
- `virtual bool RayIntersect(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, float& outT, DirectX::XMFLOAT3& outNormal) const;`
- `virtual bool CheckCollision(const Collider* other) const = 0;`
- `virtual void ResolveCollision(Collider* other) = 0;`
- `virtual void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) = 0;`
- `DirectX::XMFLOAT3 GetSize() const;`
- `bool CheckCollision(const Collider* other) const override;`
- `void ResolveCollision(Collider* other) override;`
- `bool RayIntersect(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, float& outT, DirectX::XMFLOAT3& outNormal) const override;`
- `void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;`
- `bool CheckCollision(const Collider* other) const override;`
- `void ResolveCollision(Collider* other) override;`
- `bool RayIntersect(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, float& outT, DirectX::XMFLOAT3& outNormal) const override;`
- `void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;`
- `bool CheckCollision(const Collider* other) const override;`
- `void ResolveCollision(Collider* other) override;`
- `bool RayIntersect(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, float& outT, DirectX::XMFLOAT3& outNormal) const override;`
- `void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;`
- `bool CheckCollision(const Collider* other) const override;`
- `void ResolveCollision(Collider* other) override;`
- `void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;`
- `public: MeshCollider(GameObject* owner) : Collider(ColliderType::MESH, owner) {} void SetMeshData(const std::vector<DirectX::XMFLOAT3>& vertices, const std::vector<unsigned int>& indices);`
- `void BuildFromModel(const Model3D& model);`
- `float GetWorldBoundingSphereRadius() const;`
- `void GetWorldAABB(DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize) const;`
- `bool Raycast(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, float maxDistance, DirectX::XMFLOAT3& outHitPoint, DirectX::XMFLOAT3& outHitNormal) const;`
- `bool CheckCollision(const Collider* other) const override;`
- `void ResolveCollision(Collider* other) override;`
- `bool RayIntersect(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, float& outT, DirectX::XMFLOAT3& outNormal) const override;`
- `void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;`
- `void CalculateBoundingSphere();`
- `bool RayIntersectsTriangle(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDir, const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, float& t) const;`
- `bool TriangleIntersectsSphere(const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, const DirectX::XMFLOAT3& sphereCenter, float sphereRadius, DirectX::XMFLOAT3& outNormal, float& outPenetration) const;`

### `engine/physics/core/physics_system.h`
**Types:** GameObject, Collider, PhysicsDebugStats, RayHit
- `void PhysicsSystem_Update(double elapsed_time);`
- `void PhysicsSystem_SavePositions();`
- `void PhysicsSystem_ResolveCollisions();`
- `void PhysicsSystem_ClearTriggerPairs();`
- `void PhysicsSystem_NotifyObjectDestroyed(GameObject* obj);`
- `bool PhysicsSystem_WasContactPairKnown(GameObject* a, GameObject* b);`
- `PhysicsDebugStats PhysicsSystem_GetDebugStats();`
- `void PhysicsSystem_NotifyContactResolved();`
- `void PhysicsSystem_QueueContactFlash(const DirectX::XMFLOAT3& point, const DirectX::XMFLOAT3& normalTip);`
- `void PhysicsSystem_PrepareContactFlashes();`
- `void PhysicsSystem_ClearDebugDrawQueue();`
- `void PhysicsSystem_FlushDebugDraw();`
- `bool PhysicsSystem_Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, RayHit& outHit, bool skipTriggers = false);`

### `engine/physics/core/rigidbody.h`
**Types:** Rigidbody
**Enums:** ForceMode
- `float Physics_GetGravity();`
- `void Physics_SetGravity(float gravity);`
- `void Physics_TryLoadGravityFromNearbyProject();`
- `void AddForce(const DirectX::XMFLOAT3& force, ForceMode mode = ForceMode::Force);`
- `void ClearAllForces();`
- `void ClearVelocity(DirectX::XMFLOAT3 clear_flag);`
- `void Integrate(DirectX::XMFLOAT3& position, double elapsed_time);`

### `engine/rendering/billboard/billboard.h`
**Types:** BillboardData, Billboard, VertexBillboard, CB_ViewProj, CB_Billboard
- `static bool Initialize();`
- `static void Finalize();`
- `static void Update(double elapsed_time);`
- `static void Draw();`
- `static void Add(const XMFLOAT3& position, const XMFLOAT2& size, const XMFLOAT4& color, ID3D11ShaderResourceView* ptr_texture, float rotation = 0.0f, bool groundAligned = false);`
- `static void Add(const XMFLOAT3& position, const XMFLOAT2& size, const XMFLOAT4& color, ID3D11ShaderResourceView* ptr_texture, float rotation, bool groundAligned, const XMFLOAT2& uvOffset, const XMFLOAT2& uvScale);`
- `static void Add(const BillboardData& data);`
- `static void ClearDrawList() { s_draw_list.clear();`

### `engine/rendering/d3d11/post_process.h`
- `void PostProcess_Initialize();`
- `void PostProcess_Shutdown();`
- `void PostProcess_SetEffects(float rgbShift, float mosaicBlocks);`
- `void PostProcess_GetEffects(float& outRgbShift, float& outMosaicBlocks);`
- `void PostProcess_TriggerBurst(float rgbShift, float durationSeconds);`
- `void PostProcess_Update(float dt);`
- `void PostProcess_Draw();`

### `engine/rendering/d3d11/renderer.h`
**Types:** VERTEX_3D, MATERIAL
- `HRESULT InitRenderer(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);`
- `void UninitRenderer(void);`
- `void Clear(void);`
- `void Present(void);`
- `ID3D11Device* GetDevice(void);`
- `ID3D11DeviceContext* GetDeviceContext(void);`
- `void SetDepthEnable(bool Enable);`
- `void SetCullFront(bool enable);`
- `float GetBackBufferWidth();`
- `float GetBackBufferHeight();`
- `float GetClientWidth();`
- `float GetClientHeight();`
- `void Renderer_ClientToBackbuffer(float clientX, float clientY, float& bufX, float& bufY);`
- `void Renderer_BackbufferToClient(float bufX, float bufY, float& clientX, float& clientY);`
- `HRESULT Renderer_UpdateClientSize();`
- `HRESULT Renderer_ResizeToClient();`
- `HRESULT Renderer_SetWindowMode(BOOL bWindow);`
- `BOOL Renderer_IsWindowed();`
- `void SetRenderViewport(float x, float y, float width, float height);`
- `void ResetRenderViewport();`
- `float GetRenderViewportWidth();`
- `float GetRenderViewportHeight();`
- `HRESULT PostRT_Initialize();`
- `void PostRT_Shutdown();`
- `HRESULT PostRT_Resize(UINT w, UINT h);`
- `ID3D11RenderTargetView* PostRT_GetRTV();`
- `ID3D11ShaderResourceView* PostRT_GetSRV();`
- `void Renderer_BindPostTarget();`
- `void Renderer_BindBackBuffer();`
- `HWND GetWindowHandle();`

### `engine/rendering/d3d11/shader.h`
**Types:** SURFACE_FX_PARAMS
**Enums:** Shader3DVariant
- `bool Shader_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);`
- `void Shader_Finalize();`
- `void Shader_SetProjectionMatrix(const DirectX::XMMATRIX& matrix);`
- `void Shader_SetViewMatrix(const DirectX::XMMATRIX& matrix);`
- `void Shader_SetWorldMatrix(const DirectX::XMMATRIX& matrix);`
- `void Shader_SetColor(const DirectX::XMFLOAT4& color);`
- `void Shader_SetMaterial(const MATERIAL& material);`
- `void Shader_SetSkinPalette(const DirectX::XMFLOAT4X4* bones, int boneCount, bool enabled);`
- `void Shader_SetSurfaceFXParams(const SURFACE_FX_PARAMS& params);`
- `void Shader_SetSurfaceFXMeshRadius(float radiusOS);`
- `void Shader_CommitSurfaceFX();`
- `DirectX::XMMATRIX Shader_GetProjectionMatrix();`
- `DirectX::XMMATRIX Shader_GetViewMatrix();`
- `DirectX::XMMATRIX Shader_GetWorldMatrix();`
- `void Shader_SetShadowMatrix(const DirectX::XMMATRIX& lightViewProj, float bias = 0.0015f);`
- `void Shader_SetShadowMatrices(const DirectX::XMMATRIX* lightViewProjArray, int cascadeCount, float bias = 0.0015f, float slope = 1.5f);`
- `void Shader_SetShadowCascadeParams(const DirectX::XMFLOAT4& splits, float blend);`
- `void Shader_BindShadowMap(ID3D11ShaderResourceView* shadowSRV);`
- `void Shader_BindShadowMaps(ID3D11ShaderResourceView* const* shadowSRVs, int lightCount);`
- `void Shader_SetUseExternalVP(bool enable);`
- `bool Shader_IsExternalVP();`
- `void Shader2D_Begin();`
- `void Shader3D_Begin(Shader3DVariant variant = Shader3DVariant::Standard);`
- `void Shader3D_BeginOutline();`
- `void Shader3D_UseGpuSkin();`
- `Shader3DVariant Shader3DVariantFromId(const std::string& id);`
- `const char* Shader3DVariantLabel(Shader3DVariant v);`
- `const char* Shader3DVariantId(Shader3DVariant v);`
- `int Shader3DVariantCount();`
- `Shader3DVariant Shader3DVariantByIndex(int i);`
- `bool Shader3DVariantIsUserVisible(Shader3DVariant v);`

### `engine/rendering/effect/effect_object.h`
**Types:** EffectObject
- `explicit EffectObject(const wchar_t* path, bool autoPlay = true) { LoadFromFileW(path);`
- `void SetSpeed(float speed);`
- `void SetPaused(bool paused);`
- `void SetShown(bool shown);`
- `void Play();`
- `void Stop();`
- `bool LoadFromFileW(const wchar_t* path);`
- `EffectObject& WithPosition(const DirectX::XMFLOAT3& pos) { GameObject::SetPosition(pos);`
- `EffectObject& WithRotation(const DirectX::XMFLOAT3& rotDeg) { GameObject::SetRotation(rotDeg);`
- `EffectObject& WithScale(const DirectX::XMFLOAT3& s) { GameObject::SetScale(s);`
- `EffectObject& WithScale(float uniform);`
- `EffectObject& AttachToCurrentScene();`
- `EffectObject& InitNow();`
- `EffectObject& PlayNow() { Play();`
- `EffectObject& StopNow() { Stop();`
- `EffectObject& Speed(float s) { SetSpeed(s);`
- `EffectObject& Paused(bool p) { SetPaused(p);`
- `EffectObject& Shown(bool s) { SetShown(s);`
- `static EffectObject* SpawnW(const wchar_t* path, const DirectX::XMFLOAT3& pos, bool autoDestroyOnEnd = true, const DirectX::XMFLOAT3& rotDeg = {0, 0, 0}, const DirectX::XMFLOAT3& scale = {1, 1, 1});`
- `void Initialize() override;`
- `void Finalize() override;`
- `void LogicUpdate(double elapsed_time) override;`
- `void Draw() override;`
- `void Effect_Initialize();`
- `void Effect_Finalize();`
- `void Effect_Update(double elapsed_time);`
- `void Effect_Draw();`

### `engine/rendering/effect/effect_system.h`
- `bool Initialize();`
- `void Finalize();`
- `void Update(double deltaSeconds);`
- `void Draw3D();`
- `int Load(const char16_t* path);`
- `int LoadW(const wchar_t* path);`
- `int LoadCached(const wchar_t* path);`
- `int Play(int effectId, const DirectX::XMFLOAT3& pos);`
- `void Stop(int handle);`
- `void StopAll();`
- `bool Exists(int handle);`
- `void SetLocation(int handle, const DirectX::XMFLOAT3& pos);`
- `void SetRotationEulerDeg(int handle, const DirectX::XMFLOAT3& rotDeg);`
- `void SetScale(int handle, float uniform);`
- `void SetScale(int handle, const DirectX::XMFLOAT3& scale);`
- `void SetSpeed(int handle, float speed);`
- `void SetPaused(int handle, bool paused);`
- `void SetShown(int handle, bool shown);`

### `engine/rendering/environment/skybox.h`
- `bool Skybox_Initialize();`
- `void Skybox_Finalize();`
- `void Skybox_ReloadFromSettings();`
- `void Skybox_Draw(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);`

### `engine/rendering/lighting/lighting.h`
**Types:** Light, Params
**Enums:** LightType
- `bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);`
- `void Finalize();`
- `void ClearLights();`
- `void AddDirectionalLight(const Light& l);`
- `void AddPointLight(const Light& l);`
- `void AddSpotLight(const Light& l);`
- `void SetAmbientColor(const DirectX::XMFLOAT3& col);`
- `Params GetParams();`
- `DirectX::XMFLOAT3 GetDirection();`
- `void UpdatePerObject(const DirectX::XMMATRIX& world);`
- `void Bind();`

### `engine/rendering/lighting/shadow.h`
**Types:** CameraShadowParams
- `bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, UINT size = 1024, int cascadeCount = MAX_CASCADES);`
- `bool Reconfigure(UINT size, int cascadeCount);`
- `void Finalize();`
- `void SetLightDirection(int index, const DirectX::XMFLOAT3& dir);`
- `DirectX::XMFLOAT3 GetLightDirection(int index = 0);`
- `void UpdateForCamera(const CameraShadowParams& cam, float maxShadowDistance = 60.0f);`
- `int GetCascadeCount();`
- `int GetLightCount();`
- `int GetMaxCasters();`
- `DirectX::XMFLOAT4 GetCascadeSplits();`
- `float GetCascadeBlend();`
- `DirectX::XMMATRIX GetLightView(int cascadeIndex);`
- `DirectX::XMMATRIX GetLightProj(int cascadeIndex);`
- `DirectX::XMMATRIX GetLightViewProj(int cascadeIndex);`
- `void Begin(int cascadeIndex);`
- `void End();`
- `ID3D11ShaderResourceView* GetShadowSRV(int cascadeIndex);`
- `UINT GetShadowMapSize();`
- `float GetReceiverBias();`
- `float GetTexelSize();`
- `int GetRenderingLightIndex();`
- `void SetBackfaceDepth(bool enable);`
- `bool IsBackfaceDepthEnabled();`

### `engine/rendering/model/model.h`
**Types:** FbxScene, FbxMesh, FbxNode, FbxTime, AnimationClip, Model3D, VertexPNTC, MaterialOverride, Subset, SkinMeshGeo, SkinClusterBake, SkinnedMeshBake, SkinBoneBake, RigidMeshBake, BakedAnimPose, BakedAnimClip
- `public: Model3D();`
- `bool LoadOBJ(const wchar_t* objPath, const wchar_t* fallbackTex = nullptr);`
- `bool LoadFBX(const wchar_t* fbxPath, const wchar_t* fallbackTex = nullptr);`
- `fbxsdk::FbxNode* FindBone(const char* name) const;`
- `fbxsdk::FbxNode* FindBone(const wchar_t* name) const;`
- `void ScaleBoneAndChildren(fbxsdk::FbxNode* bone, const fbxsdk::FbxDouble3& scale);`
- `bool TryGetBoneLocalMatrix(const char* boneName, DirectX::XMMATRIX& out) const;`
- `void Release();`
- `void SetPosition(const DirectX::XMFLOAT3& p);`
- `void SetRotateEulerDeg(const DirectX::XMFLOAT3& rDeg);`
- `void SetScale(const DirectX::XMFLOAT3& s);`
- `void Draw();`
- `void UpdateAnimation(double dtSec = 1.0 / 60.0);`
- `void UploadSkinnedVerticesIfDirty();`
- `void AddAnimation(const std::string& animName, const char* fileName);`
- `void EnsureAnimation(const std::string& animName, const char* fileName);`
- `bool HasAnimation(const std::string& animName) const;`
- `void MergeAnimScenesFrom(const Model3D& shared);`
- `void PlayAnimation(const std::string& animName);`
- `static std::unique_ptr<Model3D> CreateAnimInstanceFrom(const Model3D& shared);`
- `void InitAnimationInfo();`
- `void BakeSkinningData();`
- `void BakeRigidMeshData();`
- `void EnsureClipBaked(const std::string& animName);`
- `bool SampleBakedClip();`
- `void SkinFromScratch();`
- `void BakeVertexBoneWeights();`
- `void RecreateVertexBufferFromCPU();`
- `void FillBindPoseScratch();`
- `void UpdateGpuSkinPalette();`
- `void PublishSkinPalette();`
- `bool CanGpuSkin() const;`
- `void updateDynamicVBFromCP(const std::vector<DirectX::XMFLOAT3>& cpPos);`
- `void uploadVertexBufferFromCPU();`
- `void markVertexUploadDirty();`
- `const SkinMeshGeo* FindGeoForCP(int cpi) const;`
- `static DirectX::XMFLOAT3 ApplyGeoPosition(const fbxsdk::FbxAMatrix& geo, const DirectX::XMFLOAT3& p);`

### `engine/rendering/model/model_util.h`
**Types:** FbxManager, FbxIOSettings, FbxScene, FbxNode, FbxMesh, FbxTexture, FbxFileTexture, FbxSurfaceMaterial, FbxAMatrix, PNTKey, PNTKeyHasher, DiffuseTexInfo, FbxShared
- `void Trim(std::string& s);`
- `std::vector<std::string> Split(const std::string& s, char sep);`
- `std::string WideToUTF8(const std::wstring& ws);`
- `std::wstring Utf8ToWide(const char* s);`
- `std::wstring DirnameW(const std::wstring& path);`
- `std::wstring JoinPathW(const std::wstring& dir, const std::wstring& file);`
- `bool IsAbsolutePathW(const std::wstring& p);`
- `bool LoadTextureWIC_SRV(const wchar_t* texPath, ID3D11ShaderResourceView** outSRV);`
- `ID3D11ShaderResourceView* CreateSolidWhiteSRV();`
- `void ModelUtil_ReleaseTextureCache();`
- `ID3D11ShaderResourceView* CreateSolidColorSRV(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);`
- `ID3D11ShaderResourceView* CreateSolidPinkSRV();`
- `bool TryLoadMTLTexture_SRV( const std::wstring& mtlPath, const std::wstring& baseDir, ID3D11ShaderResourceView** outSRV, DirectX::XMFLOAT2* outUVScale, DirectX::XMFLOAT2* outUVOffset, bool* outClamp, bool* outHadMapKd = nullptr);`
- `bool FindFirstDiffuseFileTexture(fbxsdk::FbxNode* node, DiffuseTexInfo& out);`
- `DirectX::XMFLOAT3 TransformNormal_IT(const fbxsdk::FbxAMatrix& invTrans, const DirectX::XMFLOAT3& nIn);`
- `bool HasNegativeDeterminant(const fbxsdk::FbxAMatrix& m);`
- `FbxShared& GetFbxShared();`
- `void EnumerateSkinnedMeshes(fbxsdk::FbxNode* root, std::vector<std::pair<fbxsdk::FbxMesh*, fbxsdk::FbxNode*>>& out);`
- `void ComputeSkinnedCP_ForMesh(fbxsdk::FbxMesh* mesh, fbxsdk::FbxNode* meshNode, double timeSec, std::vector<DirectX::XMFLOAT3>& outPos, std::vector<DirectX::XMFLOAT3>& outNrm, fbxsdk::FbxScene* baseScene = nullptr, fbxsdk::FbxScene* animScene = nullptr);`

### `engine/rendering/particle/particle_system.h`
**Types:** ID3D11ShaderResourceView, ParticleEmitterConfig, ParticlePool, ParticleEmitter, ParticleSystem
**Enums:** ParticleShape, ParticleSimulationSpace, ParticleBillboardBlendMode
- `void Reset(int maxParticles);`
- `bool Spawn(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity, float lifetime, float startSize, float endSize, float rotation, const DirectX::XMFLOAT4& startColor, const DirectX::XMFLOAT4& endColor);`
- `void Tick(float dt, float gravityModifier, int textureSheetFrames, float textureSheetCycles);`
- `void Compact();`
- `void CopyParticle(int dst, int src);`
- `public: ParticleEmitter(std::uint32_t id, const ParticleEmitterConfig& config, const DirectX::XMFLOAT3& position);`
- `ID3D11ShaderResourceView* GetTextureSRV() const;`
- `void Tick(float dt);`
- `void Render() const;`
- `void Stop(bool destroyWhenEmpty);`
- `void SetConfig(const ParticleEmitterConfig& config);`
- `void Emit(int count);`
- `DirectX::XMFLOAT3 RandomShapePosition();`
- `DirectX::XMFLOAT3 RandomDirection();`
- `float RandomRange(float minValue, float maxValue);`
- `DirectX::XMFLOAT4 RandomRange(const DirectX::XMFLOAT4& minValue, const DirectX::XMFLOAT4& maxValue);`
- `static ParticleSystem& Instance();`
- `bool Initialize();`
- `void Finalize();`
- `void Reset();`
- `void Tick(float dt);`
- `void Render(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);`
- `std::uint32_t CreateEmitter(const ParticleEmitterConfig& config, const DirectX::XMFLOAT3& position);`
- `void SetEmitterPosition(std::uint32_t emitterId, const DirectX::XMFLOAT3& position);`
- `void SetEmitterEmitting(std::uint32_t emitterId, bool emitting);`
- `void StopEmitter(std::uint32_t emitterId, bool destroyWhenEmpty);`
- `void DestroyEmitter(std::uint32_t emitterId);`
- `void SetEmitterConfig(std::uint32_t emitterId, const ParticleEmitterConfig& config);`
- `void SpawnParticle(std::uint32_t emitterId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT4& color, float size, float lifetime);`
- `ParticleEmitter* FindEmitter(std::uint32_t emitterId);`
- `bool ParticleRenderer_Initialize();`
- `void ParticleRenderer_Finalize();`
- `void ParticleRenderer_RenderEmitter(const ParticleEmitter& emitter, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);`

### `engine/rendering/sprite/sprite.h`
- `void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);`
- `void Sprite_Finalize(void);`
- `void Sprite_Draw(int texid, float X, float Y, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});`
- `void Sprite_Draw(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});`
- `void Sprite_Draw(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, float angle, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});`
- `void Sprite_DrawRadialFill(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, float angle, float fillAmount, float startAngle, bool clockwise, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});`

### `engine/rendering/sprite/texture.h`
- `void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);`
- `void Texture_Finalize();`
- `int TextureLoad(const wchar_t* ptr_file_name, bool mipmap = true);`
- `int TextureCreateFromRGBA(const wchar_t* name, const unsigned char* rgba, int width, int height);`
- `int TextureGetWhiteTexture();`
- `void TextureAllRelease();`
- `void TextureSetTexture(int texture_id);`
- `DirectX::XMUINT2 TextureGetSize(int texture_id);`
- `const unsigned int& TextureGetWidth(int texture_id);`
- `const unsigned int& TextureGetHeight(int texture_id);`
- `ID3D11ShaderResourceView* TextureGetSRV(int texture_id);`
- `ID3D11ShaderResourceView* CubemapLoad(const wchar_t* fileName);`
- `void CubemapReleaseAll();`

### `engine/scene/render_settings.h`
**Types:** ShadowQualityPreset, RenderSettingsData
**Enums:** ShadowQuality
- `void RenderSettings_ResetToDefaults();`
- `RenderSettingsData& RenderSettings_Get();`
- `const RenderSettingsData& RenderSettings_GetConst();`
- `void RenderSettings_ApplyFromJson(const nlohmann::json& renderJson);`
- `nlohmann::json RenderSettings_ToJson();`
- `void RenderSettings_ReloadSkybox();`
- `Shader3DVariant RenderSettings_GetDefaultShader();`
- `ShadowQualityPreset RenderSettings_GetShadowPreset();`
- `ShadowQualityPreset RenderSettings_GetShadowPreset(ShadowQuality quality);`
- `const char* RenderSettings_ShadowQualityLabel(ShadowQuality quality);`
- `ShadowQuality RenderSettings_ShadowQualityFromId(const std::string& id);`
- `const char* RenderSettings_ShadowQualityId(ShadowQuality quality);`
- `void RenderSettings_ApplyShadowQuality();`

### `engine/scene/scene_asset_preload.h`
**Types:** GameObject, SceneAssetPreloadTask, SceneAssetPreloader, Step
**Enums:** Kind
- `std::vector<SceneAssetPreloadTask> SceneAssetPreload_BuildTaskList( const std::vector<std::unique_ptr<GameObject>>& roots);`
- `std::vector<SceneAssetPreloadTask> SceneAssetPreload_BuildTaskListFromSceneFiles( const std::string& sceneDirRelative = "rom/scene");`
- `static SceneAssetPreloader& BootPreloader();`
- `void Begin(const std::vector<SceneAssetPreloadTask>& tasks);`
- `bool Tick();`
- `float Progress01() const;`
- `void Cancel();`
- `void RebuildLabel();`
- `bool RunOneStep();`

### `engine/scene/scene_camera.h`
**Types:** GameObject
- `void SceneCameraSyncFromActiveNode(const std::vector<std::unique_ptr<GameObject>>& roots);`

### `engine/scene/scene_lighting.h`
**Types:** GameObject
- `void SceneLightingSyncFromNodes(const std::vector<std::unique_ptr<GameObject>>& roots);`

### `engine/scene/scene_loader.h`
- `std::vector<std::unique_ptr<GameObject>>& SceneLoader_GetSceneNodes();`
- `void SceneLoader_StopScriptWatcher();`
- `void SceneLoader_Initialize(bool enableScriptWatcher);`
- `void SceneLoader_Finalize();`
- `void SceneLoader_RegisterComponentTypes();`
- `void SceneLoader_LoadScriptDlls();`
- `void SceneLoader_UnloadScriptDlls();`
- `void SceneLoader_ScanScriptDllsForChanges(bool forceReload);`
- `void SceneLoader_DrainScriptReloadQueue( const std::function<void(const std::string& name)>& onReloaded);`
- `const std::string& SceneLoader_GetDefaultScenePath();`
- `const std::string& SceneLoader_GetCurrentScenePath();`
- `void SceneLoader_LoadDefaultSceneSetting();`
- `void SceneLoader_RefreshSceneAssets();`
- `void SceneLoader_SetDefaultSceneFileName(const std::string& fileName, bool persist);`
- `void SceneLoader_CopySceneFileName(const std::string& fileName);`
- `bool SceneLoader_LoadFromFile(const char* filename);`
- `bool SceneLoader_SaveToFile(const char* filename);`
- `void SceneLoader_ClearSceneNodes();`
- `void SyncSceneRuntime();`
- `void ClearSceneRuntime(bool destroyColliders = false);`
- `void ApplyEditorColliders();`
- `void RestoreEditorDebugColliderFlags();`
- `void TeardownAllScripts();`
- `void TeardownScenePlaySession(bool resetTransforms);`
- `void DrawSceneModels();`
- `bool SceneLoader_IsSceneGraphObject(GameObject* obj);`
- `std::unique_ptr<GameObject> GameObjectFromLegacyBlock(const nlohmann::json& blockJson);`
- `void SnapshotSceneStartTransforms();`
- `bool SceneLoader_TryGetSceneAsset(int index, std::string* outPath, std::string* outDisplayName, std::string* outFileName);`
- `int SceneLoader_GetSceneAssetCount();`
- `bool SceneLoader_ApplySceneAssetByIndex(int index);`
- `std::string SceneLoader_GetDefaultSceneDisplayName();`
- `const std::string& SceneLoader_GetDefaultSceneFileName();`
- `bool SceneLoader_IsSceneAssetsScanned();`
- `const char* SceneLoader_GetSaveFileName();`
- `char* SceneLoader_GetSaveFileNameBuffer();`
- `bool SceneLoader_DebugCameraEnabledOnPlay();`
- `bool SceneLoader_DebugCameraFlyOnPlay();`

### `engine/scene/scene_runtime.h`
**Types:** SceneBase, SceneRuntime
- `static SceneRuntime& Instance();`
- `void Clear(SceneBase* scene, bool destroyColliders = false);`
- `void SyncFromEditorRoots(const std::vector<std::unique_ptr<GameObject>>& roots, SceneBase* scene);`
- `void ApplyCollidersFromEditorRoots( const std::vector<std::unique_ptr<GameObject>>& roots);`
- `void RegisterSubtree(GameObject* root, SceneBase* scene);`
- `void UnregisterSubtree(GameObject* root, SceneBase* scene, bool destroyColliders = true);`
- `void RegisterTree(GameObject* node, SceneBase* scene);`
- `void UnregisterTree(GameObject* node, SceneBase* scene, bool destroyColliders);`
- `void ApplyCollidersTree(GameObject* node);`

### `engine/scripting/Script.h`
**Types:** Script

### `engine/scripting/script_component.h`
**Types:** ScriptComponent
- `void OnAttach() override;`
- `void OnDetach() override;`
- `void Update(double elapsed_time) override;`
- `void OnReset() override;`
- `void Teardown();`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string& jsonStr) override;`
- `Script* GetScript() const { return m_script.get();`
- `nlohmann::json GetEditableProperties() const;`
- `void ApplyProperties(const nlohmann::json& properties);`
- `void HotReload();`
- `void CreateScript();`

### `engine/scripting/script_dll_register.h`
**Types:** TYPE
- `void DllScript_Register(const std::string& name, std::function<Script*()> creator);`

### `engine/scripting/script_factory.h`
**Types:** ScriptFactory, DllEntry, DllApi, PendingReload
- `static ScriptFactory& Instance();`
- `void Register(const std::string& name, Creator creator);`
- `ScriptPtr Create(const std::string& name) const;`
- `bool IsRegistered(const std::string& name) const;`
- `bool IsDllScript(const std::string& name) const;`
- `nlohmann::json SerializeScriptProperties(const std::string& name, const Script* script) const;`
- `void DeserializeScriptProperties(const std::string& name, Script* script, const nlohmann::json& properties) const;`
- `bool LoadScriptDll(const std::string& scriptName, const std::string& dllPath, std::vector<std::string>* loadedScripts = nullptr);`
- `void UnloadScriptDll(const std::string& scriptName);`
- `void UnloadAllDlls();`
- `void RequestReload(const std::string& name, const std::string& dllPath);`
- `size_t PendingReloadCount() const;`
- `void DrainReloadQueue(const std::function<void(const std::string&)>& onReloaded);`
- `void ReleaseRetiredDlls();`
- `bool InvokeExportOnLoadedDlls(const char* exportName) const;`
- `void* ResolveDllExport(const char* exportName) const;`

### `engine/scripting/script_register.h`
**Types:** TYPE

### `engine/scripting/script_runtime_api.h`
**Types:** ScriptRuntimeApi
- `unsigned int (*textureGetWidth)(int textureId);`
- `unsigned int (*textureGetHeight)(int textureId);`
- `const ScriptRuntimeApi* ScriptRuntimeApi_GetHost();`
- `SceneBase* ScriptApi_GetCurrentScene();`
- `engine::Transform* ScriptApi_FindTransform(engine::GameObject& object);`
- `const engine::Transform* ScriptApi_FindTransform(const engine::GameObject& object);`
- `ColliderComponent* ScriptApi_FindColliderComponent(engine::GameObject& object);`
- `EffectComponent* ScriptApi_FindEffectComponent(engine::GameObject& object);`
- `VisualComponent* ScriptApi_FindVisualComponent(engine::GameObject& object);`
- `MaterialComponent* ScriptApi_FindMaterialComponent(engine::GameObject& object);`
- `void ScriptApi_Visual_PlayClip(VisualComponent* component, const char* clipName);`
- `bool ScriptApi_Visual_TryGetBoneWorld(VisualComponent* component, const char* boneName, DirectX::XMFLOAT3& outPos, DirectX::XMFLOAT3& outRotDeg);`
- `void ScriptApi_Visual_PlayClipLoop(VisualComponent* component, const char* clipName, bool loop);`
- `float ScriptApi_Visual_GetClipDuration(VisualComponent* component);`
- `void ScriptApi_Visual_SetAnimSpeed(VisualComponent* component, float speed);`
- `void ScriptApi_Effect_Play(EffectComponent* component);`
- `AudioComponent* ScriptApi_FindAudioComponent(engine::GameObject& object);`
- `void ScriptApi_Audio_Play(AudioComponent* component);`
- `void ScriptApi_Audio_Stop(AudioComponent* component);`
- `void ScriptApi_AudioSetMasterVolume(float volume);`
- `float ScriptApi_AudioGetMasterVolume();`
- `void ScriptApi_MouseGetPosition(int& x, int& y);`
- `void ScriptApi_PostFX_Burst(float rgbShift, float durationSeconds);`
- `bool ScriptApi_Physics_Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, DirectX::XMFLOAT3* outPoint, DirectX::XMFLOAT3* outNormal, float* outDistance);`
- `void ScriptApi_ApplyColliderComponent(ColliderComponent& component);`
- `BoxCollider* ScriptApi_CreateBoxCollider(engine::GameObject& object, DirectX::XMFLOAT3 size, DirectX::XMFLOAT3 center);`
- `void ScriptApi_DestroyCollider(engine::GameObject& object);`
- `void ScriptApi_GameObjectAddIgnoreCollisionTag(engine::GameObject& object, Tag tag);`
- `void ScriptApi_GameObjectRemoveIgnoreCollisionTag(engine::GameObject& object, Tag tag);`
- `void ScriptApi_GameObjectAddIgnoreCollisionObject(engine::GameObject& object, engine::GameObject& other);`
- `void ScriptApi_GameObjectRemoveIgnoreCollisionObject(engine::GameObject& object, engine::GameObject& other);`
- `void ScriptApi_GameObjectClearIgnoreCollisionObjects(engine::GameObject& object);`
- `DirectX::XMFLOAT3 ScriptApi_GetColliderWorldCenter(const Collider& collider);`
- `DirectX::XMFLOAT3 ScriptApi_GetBoxColliderSize(const BoxCollider& collider);`
- `void ScriptApi_SetBoxColliderSize(BoxCollider& collider, DirectX::XMFLOAT3 size);`
- `bool ScriptApi_TryGetColliderFootprint(const Collider& collider, DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize);`
- `void ScriptApi_EngineGUIStopPlay();`
- `void ScriptApi_CameraSetPosition(DirectX::XMFLOAT3 position);`
- `void ScriptApi_CameraSetTarget(DirectX::XMFLOAT3 target);`
- `void ScriptApi_CameraSetUp(DirectX::XMFLOAT3 up);`
- `void ScriptApi_SetActiveCameraNodeName(const char* name);`
- `void ScriptApi_ActivateSceneCamera(const char* name);`
- `int ScriptApi_GetBackBufferWidth();`
- `int ScriptApi_GetBackBufferHeight();`
- `void ScriptApi_SpriteDraw(int texId, float x, float y, float width, float height, int tx, int ty, int tw, int th, float angle, DirectX::XMFLOAT4 color);`
- `int ScriptApi_TextureGetWhiteTexture();`
- `unsigned int ScriptApi_TextureGetWidth(int textureId);`
- `unsigned int ScriptApi_TextureGetHeight(int textureId);`
- `void ScriptApi_TextureRelease(int textureId);`
- `bool ScriptApi_EngineGUILoadFromFile(const char* filename);`
- `void ScriptApi_EngineGUIStartPlay();`
- `float ScriptApi_GetSphereColliderRadius(const SphereCollider& collider);`
- `DirectX::XMFLOAT3 ScriptApi_GetCameraForward();`
- `UISprite* ScriptApi_UICanvasAddSprite(UICanvas& canvas, const char* name, DirectX::XMFLOAT2 position, UIAnchor anchor, float width, float height);`
- `void ScriptApi_UICanvasRemoveElement(UICanvas& canvas, const char* name);`
- `void ScriptApi_UISpriteSetTexture(UISprite& sprite, const wchar_t* texturePath);`
- `UISprite* ScriptApi_UICanvasGetSprite(UICanvas& canvas, const char* name);`
- `void ScriptApi_UISpriteSetRadial360Fill(UISprite& sprite, float fillAmount, bool clockwise = true);`
- `void ScriptApi_UISpriteSetFillNone(UISprite& sprite);`
- `void ScriptApi_UISpriteSetSourceRect(UISprite& sprite, int x, int y, int width, int height);`
- `int ScriptApi_TextureLoad(const wchar_t* fileName, bool mipmap = true);`
- `ID3D11ShaderResourceView* ScriptApi_TextureGetSRV(int textureId);`
- `void ScriptApi_BillboardAdd(DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 size, DirectX::XMFLOAT4 color, ID3D11ShaderResourceView* texture, float rotation = 0.0f, bool groundAligned = false);`
- `void ScriptApi_BillboardAddUv(DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 size, DirectX::XMFLOAT4 color, ID3D11ShaderResourceView* texture, float rotation, bool groundAligned, DirectX::XMFLOAT2 uvOffset, DirectX::XMFLOAT2 uvScale);`
- `std::uint32_t ScriptApi_ParticleCreateEmitter(const engine::ParticleEmitterConfig& config, DirectX::XMFLOAT3 position);`
- `void ScriptApi_ParticleSetEmitterPosition(std::uint32_t emitterId, DirectX::XMFLOAT3 position);`
- `void ScriptApi_ParticleSetEmitterEmitting(std::uint32_t emitterId, bool emitting);`
- `void ScriptApi_ParticleStopEmitter(std::uint32_t emitterId, bool destroyWhenEmpty = true);`
- `void ScriptApi_ParticleDestroyEmitter(std::uint32_t emitterId);`
- `void ScriptApi_ParticleSpawnAt(std::uint32_t emitterId, DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 velocity, DirectX::XMFLOAT4 color, float size, float lifetime);`
- `float ScriptApi_GetRenderViewportWidth();`
- `float ScriptApi_GetRenderViewportHeight();`
- `ID3D11Device* ScriptApi_GetD3DDevice();`
- `ID3D11DeviceContext* ScriptApi_GetD3DDeviceContext();`
- `void ScriptApi_GetCameraViewMatrix(DirectX::XMFLOAT4X4* out);`
- `void ScriptApi_GetCameraProjMatrix(float width, float height, DirectX::XMFLOAT4X4* out);`
- `void ScriptApi_DebugDrawSphereWire(DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 center, float radius, DirectX::XMFLOAT3 rotation = {0.0f, 0.0f, 0.0f}, DirectX::XMFLOAT4 color = {0.2f, 0.8f, 1.0f, 0.95f});`
- `UICanvas* ScriptApi_SceneCanvasGetCanvas();`
- `void ScriptApi_SceneCanvasSetTimerSeconds(float seconds);`
- `void ScriptApi_SceneCanvasRequestSceneFile(const char* sceneFile, bool playOnLoad);`
- `UIElement* ScriptApi_UICanvasGetElement(UICanvas& canvas, const char* name);`
- `void ScriptApi_UITextSetText(UIText& text, const char* value);`
- `void ScriptApi_UIElementSetVisible(UIElement& element, bool visible);`
- `void ScriptApi_PickupDebugSetMessage(const char* message, float displaySeconds = 4.0f);`
- `GameObject* ScriptApi_PrefabInstantiate(const char* prefabPath, DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 rotation = {0.0f, 0.0f, 0.0f}, GameObject* parent = nullptr, bool startActive = true);`
- `void ScriptApi_PrefabDestroy(GameObject* obj);`
- `void ScriptApi_PrefabSetBeforeDestroyCallback(void (*callback)(GameObject* obj));`

### `engine/scripting/script_watcher.h`
**Types:** ScriptWatcher
- `void Start(const std::string& watchDir, Callback onChange);`
- `void Stop();`

### `engine/scripting/ui_canvas_script.h`
**Types:** UIElement, UICanvasScript

### `engine/scripting/ui_canvas_script_factory.h`
**Types:** UICanvasScriptFactory
- `static UICanvasScriptFactory& Instance();`
- `void Register(const std::string& name, Creator creator);`
- `ScriptPtr Create(const std::string& name) const;`
- `bool IsRegistered(const std::string& name) const;`
- `std::vector<std::string> GetRegisteredNames() const;`
- `void LoadFromModule(HMODULE module, const std::string& fallbackName, std::shared_ptr<void> keepAlive);`

### `engine/scripting/ui_canvas_script_register.h`
**Types:** TYPE
- `void DllUICanvasScript_Register(const std::string& name, std::function<UICanvasScript*()> creator);`

### `engine/ui/scene_canvas.h`
- `void SceneCanvas_Initialize();`
- `void SceneCanvas_Finalize();`
- `void SceneCanvas_Update(double elapsed_time);`
- `void SceneCanvas_Draw();`
- `UICanvas* SceneCanvas_GetCanvas();`
- `void SceneCanvas_ClearSceneCanvas();`
- `void SceneCanvas_LoadSceneCanvas(const nlohmann::json& canvasJson);`
- `nlohmann::json SceneCanvas_SaveSceneCanvas();`
- `std::string SceneCanvas_SerializeElement(const UIElement& element);`
- `UIElement* SceneCanvas_InsertElementJson(const std::string& elementJson, int index);`
- `int SceneCanvas_GetElementIndex(const UIElement* element);`
- `void SceneCanvas_RemoveElementInternal(UIElement* element);`
- `void SceneCanvas_RegisterButtonAction(const std::string& scriptName, SceneCanvasButtonActionBinder binder);`
- `void SceneCanvas_BindButtonAction(UIButton* button);`
- `void SceneCanvas_RequestSceneFile(const std::string& sceneFile, bool playOnLoad = false);`
- `std::string SceneCanvas_ConsumeRequestedSceneFile(bool* playOnLoad = nullptr);`
- `void SceneCanvas_ShowRedOverlay();`
- `void SceneCanvas_HideRedOverlay();`
- `void SceneCanvas_SetRedOverlayAlpha(float alpha);`
- `void SceneCanvas_SetTimerSeconds(float seconds);`

### `engine/ui/ui_bar.h`
**Types:** UIBar
- `public: UIBar(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::TOP_LEFT, float maxValue = 100.0f, const std::string& label = "");`
- `void Initialize() override;`
- `void Update(double elapsed_time) override;`
- `void Draw() override;`
- `void Finalize() override;`
- `bool GetScreenRect(UIScreenRect& out) const override;`
- `void SetBackgroundTexturePath(const std::string& texturePath);`
- `void SetFillTexturePath(const std::string& texturePath);`

### `engine/ui/ui_button.h`
**Types:** UIButton
- `public: UIButton(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER, float width = 200.0f, float height = 60.0f);`
- `void Initialize() override;`
- `void Update(double elapsed_time) override;`
- `void Draw() override;`
- `void Finalize() override;`
- `void SetVisible(bool visible) override;`
- `void SetTexture(const wchar_t* texturePath);`
- `void SetTexturePath(const std::string& texturePath);`
- `void SetHoverTexture(const wchar_t* texturePath);`
- `void SetHoverTexturePath(const std::string& texturePath);`
- `void SetPressedTexture(const wchar_t* texturePath);`
- `void SetPressedTexturePath(const std::string& texturePath);`
- `void SetAction(const std::string& action, const std::string& scene = "", bool playOnLoad = false);`
- `void Activate();`
- `bool GetScreenRect(UIScreenRect& out) const override;`
- `bool ContainsPoint(float x, float y) const;`
- `static void SetFont(const wchar_t* fontTexturePath, float charWidth = 8.0f, float charHeight = 16.0f, int charsPerRow = 32);`
- `static void SetFontPath(const std::string& fontTexturePath, float charWidth = 8.0f, float charHeight = 16.0f, int charsPerRow = 32);`
- `static void ShutdownSharedFont();`

### `engine/ui/ui_button_pointer.h`
**Types:** UIButtonPointerState

### `engine/ui/ui_canvas.h`
**Types:** UICanvas
- `public: UICanvas(const std::string& name);`
- `void Initialize();`
- `void Update(double elapsed_time);`
- `void Draw();`
- `void Finalize();`
- `UIButton* AddButton(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER, float width = 200.0f, float height = 60.0f);`
- `UISprite* AddSprite(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER, float width = 64.0f, float height = 64.0f);`
- `UIBar* AddBar(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::TOP_LEFT, float maxValue = 100.0f, const std::string& label = "");`
- `UIText* AddText(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER, const std::string& text = "");`
- `void AddElement(std::unique_ptr<UIElement> element);`
- `void InsertElementAt(std::unique_ptr<UIElement> element, int index);`
- `void ClearElements();`
- `void RemoveElement(const std::string& name);`
- `void RemoveElement(UIElement* element);`
- `int GetElementIndex(const UIElement* element) const;`
- `bool MoveElementToIndex(UIElement* element, int index);`
- `bool MoveElement(UIElement* element, int offset);`
- `bool MoveElementForward(UIElement* element);`
- `bool MoveElementBackward(UIElement* element);`
- `bool MoveElementToFront(UIElement* element);`
- `bool MoveElementToBack(UIElement* element);`
- `UIElement* GetElement(const std::string& name);`
- `void EnableNavigation(bool enable);`
- `void SelectButton(int index);`
- `void SelectNextButton();`
- `void SelectPreviousButton();`
- `UIButton* GetSelectedButton();`
- `size_t GetElementCount() const { return m_elements.size();`
- `size_t GetButtonCount() const { return m_buttons.size();`
- `void SetVisible(bool visible);`
- `void RebuildButtonList(UIButton* selectedButton = nullptr);`
- `void UpdateNavigation();`

### `engine/ui/ui_element.h`
**Types:** UIScreenRect, UIElement
**Enums:** UIAnchor
- `public: UIElement(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER) : m_name(name), m_position(position), m_anchor(anchor), m_isVisible(true) {} virtual ~UIElement();`
- `virtual void Initialize() = 0;`
- `virtual void Update(double elapsed_time) = 0;`
- `virtual void Draw() = 0;`
- `virtual void Finalize() = 0;`
- `DirectX::XMFLOAT2 GetScreenPosition() const;`
- `DirectX::XMFLOAT2 GetScreenPosition(float screenWidth, float screenHeight) const;`
- `DirectX::XMFLOAT2 GetAnchorScreenPosition() const;`
- `DirectX::XMFLOAT2 GetAnchorScreenPosition(float screenWidth, float screenHeight) const;`
- `virtual bool GetScreenRect(UIScreenRect& out) const = 0;`
- `bool HitTest(float viewportX, float viewportY) const;`
- `void LoadScriptFromJson(const nlohmann::json& scriptJson);`
- `nlohmann::json SaveScriptToJson() const;`
- `void SetScriptClass(const std::string& scriptClass);`
- `const std::string& GetScriptClass() const;`
- `void UpdateAttachedScript(double elapsed_time);`
- `void InvokeAttachedScriptOnClick();`
- `void TeardownAttachedScript();`
- `void HotReloadAttachedScript();`
- `DirectX::XMFLOAT2 UIGetLayoutScale();`
- `DirectX::XMFLOAT2 UIGetDesignSize();`
- `DirectX::XMFLOAT2 ScreenDeltaToDesignDelta(float dx, float dy);`

### `engine/ui/ui_element_script_host.h`
**Types:** UIElement, UICanvasScript, UIElementScriptHost
- `void SetScriptClass(const std::string& scriptClass);`
- `void SetProperties(const nlohmann::json& properties);`
- `void LoadFromJson(const nlohmann::json& scriptJson);`
- `nlohmann::json SaveToJson() const;`
- `UICanvasScript* GetScript() const { return m_script.get();`
- `void Update(UIElement& owner, double elapsed_time);`
- `void InvokeOnClick(UIElement& owner);`
- `void Teardown(UIElement& owner);`
- `void HotReload(UIElement& owner);`
- `void CreateScript();`

### `engine/ui/ui_font.h`
**Types:** UIGlyph, UIFont
- `const UIFont* UIFont_Get(const wchar_t* ttfPath);`
- `void UIFont_ClearCache();`
- `bool UIFont_IsTrueTypePath(const wchar_t* path);`
- `bool UIFont_IsTrueTypePath(const std::string& path);`

### `engine/ui/ui_sprite.h`
**Types:** UISprite
**Enums:** UISpriteFillMethod, UISpriteFillOrigin
- `public: UISprite(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER, float width = 64.0f, float height = 64.0f);`
- `void Initialize() override;`
- `void Update(double elapsed_time) override;`
- `void Draw() override;`
- `void Finalize() override;`
- `bool GetScreenRect(UIScreenRect& out) const override;`
- `void SetTexture(const wchar_t* texturePath);`
- `void SetTexturePath(const std::string& texturePath);`
- `void SetSourceRect(int x, int y, int width, int height);`
- `void ClearSourceRect();`
- `void SetFillAmount(float amount) { m_fillAmount = (amount < 0.0f) ? 0.0f : (amount > 1.0f ? 1.0f : amount);`

### `engine/ui/ui_text.h`
**Types:** UIText
**Enums:** TextAlign
- `public: UIText(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER, const std::string& text = "");`
- `void Initialize() override;`
- `void Update(double elapsed_time) override;`
- `void Draw() override;`
- `void Finalize() override;`
- `bool GetScreenRect(UIScreenRect& out) const override;`
- `void SetFont(const wchar_t* fontTexturePath, float charWidth = 16.0f, float charHeight = 32.0f, int charsPerRow = 16);`
- `void SetFontPath(const std::string& fontTexturePath, float charWidth = 16.0f, float charHeight = 32.0f, int charsPerRow = 16);`
- `float GetTextWidth() const;`

### `engine/ui/ui_text_renderer.h`
**Types:** UITextRenderer
- `float DrawScale(float scale) const;`
- `public: UITextRenderer();`
- `void Initialize(const wchar_t* fontTexturePath, float charWidth = 16.0f, float charHeight = 32.0f, int charsPerRow = 16);`
- `void Draw(const std::string& text, float x, float y, float scale = 1.0f, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });`
- `float GetTextWidth(const std::string& text, float scale = 1.0f) const;`
- `float GetLineHeight(float scale = 1.0f) const;`
- `void Finalize();`

### `src/app/camera/camera.h`
**Types:** Camera
- `public: Camera();`
- `void SetPosition(const XMFLOAT3& pos);`
- `void SetTarget(const XMFLOAT3& at);`
- `void SetUp(const XMFLOAT3& up);`
- `void SetFov(float fov);`
- `void SetClip(float nearClip, float farClip);`
- `XMMATRIX GetViewMatrix() const;`
- `XMMATRIX GetProjMatrix(float screenWidth, float screenHeight) const;`
- `XMFLOAT3 GetForward() const;`
- `Camera& GetCamera();`
- `void SetCamera(const Camera& cam);`

### `src/app/camera/camera_director.h`
**Types:** GameObject
**Enums:** CameraDirectorSource
- `void CameraDirector_Initialize();`
- `void CameraDirector_Update(double elapsed_time, const std::vector<std::unique_ptr<GameObject>>& roots);`
- `void CameraDirector_SetSource(CameraDirectorSource source);`
- `CameraDirectorSource CameraDirector_GetSource();`
- `void CameraDirector_SetIntroPosition(const DirectX::XMFLOAT3& position);`
- `void CameraDirector_SetIntroTarget(const DirectX::XMFLOAT3& target);`
- `void CameraDirector_SetIntroUp(const DirectX::XMFLOAT3& up);`
- `const CameraPose& CameraDirector_GetIntroPose();`
- `void CameraDirector_SetBlendDuration(float seconds);`

### `src/app/camera/camera_pose.h`
**Types:** CameraPose, CameraComponent
- `CameraPose CameraPose_FromGlobal();`
- `void CameraPose_ApplyToGlobal(const CameraPose& pose);`
- `CameraPose CameraPose_Lerp(const CameraPose& a, const CameraPose& b, float t);`
- `CameraPose CameraPose_FromComponent(const CameraComponent* cam, const engine::Transform* transform);`
- `void CameraPose_WriteToNode(engine::Transform* transform, CameraComponent* cam, const CameraPose& pose);`

### `src/app/camera/debug_camera.h`
**Enums:** DebugCameraControlMode
- `void DebugCamera_Update(double dt);`
- `bool DebugCamera_IsEnabled();`
- `void DebugCamera_SetEnabled(bool enabled);`
- `void DebugCamera_SetControlMode(DebugCameraControlMode mode);`
- `DebugCameraControlMode DebugCamera_GetControlMode();`
- `void DebugCamera_SetMoveSpeed(float speed);`
- `void DebugCamera_SetBoostMultiplier(float mult);`
- `void DebugCamera_SetMouseSensitivity(float degPerPixel);`
- `void DebugCamera_SetOrbitSensitivity(float sensitivity);`
- `void DebugCamera_SetPanSensitivity(float sensitivity);`
- `void DebugCamera_SetDollySensitivity(float sensitivity);`
- `void DebugCamera_SetFocalPoint(DirectX::XMFLOAT3 point);`
- `void DebugCamera_SyncOrbitFromCurrentView();`
- `void DebugCamera_SetFlyView(DirectX::XMFLOAT3 position, float yawDeg, float pitchDeg);`

### `src/app/camera/third_person_camera.h`
**Types:** GameObject
**Enums:** ThirdPersonMagnetMode
- `void ThirdPersonCamera_Initialize();`
- `void ThirdPersonCamera_Update(double elapsed_time);`
- `void ThirdPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr);`
- `void ThirdPersonCamera_SetDistance(float distance);`
- `void ThirdPersonCamera_SetHeight(float height);`
- `void ThirdPersonCamera_SetLookAtHeight(float height);`
- `void ThirdPersonCamera_SetMouseSensitivity(float sensitivity);`
- `void ThirdPersonCamera_SetControllerSensitivity(float sensitivity);`
- `void ThirdPersonCamera_SetPitchLimits(float minPitch, float maxPitch);`
- `float ThirdPersonCamera_GetPitch();`
- `void ThirdPersonCamera_ShowCursor(bool show);`
- `bool ThirdPersonCamera_IsCursorVisible();`
- `void ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode mode);`
- `void ThirdPersonCamera_SetMagnetTarget(const DirectX::XMFLOAT3& targetPos);`
- `bool ThirdPersonCamera_TryGetFollowPose(CameraPose& outPose);`
- `GameObject* ThirdPersonCamera_GetFollowCamNode();`

### `src/app/rendering/fade.h`
**Types:** FADE
**Enums:** FADE_MODE
- `void Fade_Initialize();`
- `void Fade_Finalize();`
- `void Fade_Update();`
- `void Fade_Draw();`
- `void Fade_SetOut(float r, float g, float b, float spd, bool changeScene = true);`
- `void Fade_SetIn(float spd);`
- `void Fade_SetNextScene(SceneBase* next);`
- `bool Fade_IsFadeEnd();`
- `bool Fade_IsOutEnd();`
- `bool Fade_IsInEnd();`
- `bool Fade_IsFading();`

### `src/app/rendering/grid.h`
**Types:** Grid
- `void Initialize() override;`
- `void Finalize() override;`
- `void LogicUpdate(double elapsed_time) override;`
- `void Draw() override;`
- `void Grid_Initialize();`
- `void Grid_Finalize();`
- `void Grid_Draw();`

### `src/app/rendering/scene_transition.h`
- `void SceneTransition_Initialize();`
- `void SceneTransition_Finalize();`
- `void SceneTransition_Update(double elapsed_time);`
- `void SceneTransition_Draw();`
- `bool SceneTransition_IsActive();`
- `bool SceneTransition_BlocksSceneUpdate();`
- `bool SceneTransition_TryBegin(const std::string& targetScene, bool playOnLoad);`
- `bool SceneTransition_TryHandleSceneRequest(const std::string& currentScene, const std::string& nextScene, bool playOnLoad);`

### `src/app/scene/game_scene.h`
**Types:** GameScene
**Enums:** RENDER_OBJECT_TYPE
- `void Initialize() override;`
- `void Finalize() override;`
- `void Update(double elapsed_time) override;`
- `void Draw() override;`
- `void SetRenderObject(RENDER_OBJECT_TYPE type);`
- `void Object_Draw();`

### `src/app/scene/game_scene_script_hooks.h`
- `bool GameSceneScript_InvokeBool(const char* exportName);`
- `void GameSceneScript_Invoke(const char* exportName);`
- `void GameSceneScript_InvokeWithDouble(const char* exportName, double value);`

### `src/app/scene/scene_manager.h`
**Types:** GameObject, SceneBase, SceneManager
- `virtual void Initialize() = 0;`
- `virtual void Finalize() = 0;`
- `virtual void Update(double elapsed_time) = 0;`
- `virtual void Draw() = 0;`
- `void AddObject(GameObject* obj) { if (obj && std::find(m_objects.begin(), m_objects.end(), obj) == m_objects.end()) { m_objects.push_back(obj);`
- `void RemoveObject(GameObject* obj) { m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), obj), m_objects.end());`
- `void ClearObjects() { m_objects.clear();`
- `static SceneManager& Instance();`
- `void ChangeScene(SceneBase* pNextScene);`
- `void DestroyCurrentScene();`
- `void Update(double elapsed_time);`
- `void Draw();`
- `SceneBase* GetCurrentScene();`
- `private: SceneManager();`
