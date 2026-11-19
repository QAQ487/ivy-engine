# Ivy Engine — Complete Ability & API Reference

Exhaustive catalog of **Ivy Engine** capabilities and public functions,
scanned from every header under `engine/` and `src/app/` (181 headers; regenerate to refresh).

**Learning how to use the engine?** Start with [`ENGINE_USER_GUIDE.md`](ENGINE_USER_GUIDE.md) (humans).  
For JSON schemas, source map, and change rules, see [`AGENTS.md`](../AGENTS.md) (AI agents).

> **Regenerate:** `python3 tools/extract_api_headers.py` (or `py -3 …` on Windows) rewrites the §6 catalog. Sections 1–5 are hand-maintained in this file; the generator keeps them.

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
| `IvyEngine` | Editor / development executable (in `IvyEngine.sln`) |
| `IvyEngineRuntime` | Shipping-style runtime (`GAME_RUNTIME`, in `IvyEngine.sln`) |
| `GameScripts` | User scripts → `GameScripts.dll` (lives in each content project, not in the engine `.sln`) |

### Folder map

| Path | Purpose |
|------|---------|
| `engine/` | Reusable engine systems |
| `src/app/` | WinMain, game loop, cameras, fades, `GameScene` |
| `resources/` | Read-only engine defaults (mirrored beside the exe; `rom/<rest>` falls back here) |
| `testbed/` | In-repo content project (`project.json` + `rom/`) |
| `external_content_template/` | Starter for an external content project (`game/scripts/`, `rom/`) |
| `shaders/` | HLSL sources → `resources/shader/*.cso` |
| `libraries/` | imgui, imguizmo, nlohmann, fbxsdk, effekseer, directxtex, box2d |
| `samples/scripts/` | Reference gameplay scripts (copy into a content project's `game/scripts/`) |
| `tools/`, `tests/` | Generators, Linux/Windows checks, test-bridge clients |
| `docs/` | User guide, this reference, ADRs, test-bridge guide, `docs/agent/` for game-repo agents |

### Entry points

- `src/app/main.cpp` — Win32 entry / main loop
- `src/app/scene/game_scene.cpp` — load, update, draw passes
- `src/app/scene/scene_manager.h` — `SceneBase` / `SceneManager`
- `engine/core/engine_gui.cpp` — editor core (play state, hotkeys: **F2** viewport Play, **F3** fullscreen Play)
- `engine/editor/editor_*.cpp` — editor panels (menu bar, hierarchy, inspector, asset browser, console, settings, docking)
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

`SceneTransform`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `CylinderCollider`, `MeshCollider`, `Rigidbody2D`, `Collider2D`, `SpriteAnimator`, `SpriteRenderer`, `visual`, `Billboard`, `Material`, `Rigidbody`, `Script`, `Collider`, `Effect`, `DirectionalLight`, `PointLight`, `SpotLight`, `Camera`, `Audio`, `AudioListener`

### `Tag` values

Built-in C++ enum (`enum class Tag`) — stable indices for scripts/collision helpers:

`TAG_NONE`, `TAG_SHADOW`, `TAG_GROUND`, `TAG_OBSTACLES`, `TAG_WALL`, `TAG_PLAYER`, `TAG_ENEMY`, `TAG_BULLET`, `TAG_SCLAP`, `TAG_MAX`

Project tag list (`TagRegistry`, `rom/tags.json`) starts with those builtin **names** (`NONE`, `SHADOW`, …) and may append **custom** tags after `TAG_MAX`. Scene JSON `"tag"` is the registry index (not limited to the enum). See `engine/ecs/tag_registry.h`.

### Content projects (`ProjectManager`)

Header: `engine/editor/project_manager.h`

- Open/create/save content roots with `project.json`
- Recent list under `%AppData%/GameEngine/recent_projects.json`
- Editor File menu: New / Open / Save Project (see the user guide, *Projects*)

### Editor viewport aspect

The ImGui editor game viewport is the largest centered **16:9** rect inside the docking central node (black letterbox/pillarbox gutters). `EngineGUI_GetGameViewportRect` returns that rect in backbuffer pixels (ImGui client → backbuffer via `Renderer_ClientToBackbuffer`). F2 keeps chrome; F3 letterboxes 16:9 in the full window; shipped runtime uses the full backbuffer. There is no aspect setting in render settings.

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
- `bool ReloadContent(const std::string& path);`
- `bool IsContentLoaded(const std::string& path) const;`
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

### `engine/audio/wav_parse.h`
**Types:** WavChunks, FormatCopy
- `inline std::uint32_t ReadU32LE(const std::uint8_t* p) { return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);`
- `inline bool TagEquals(const std::uint8_t* p, const char* tag) { return p[0] == static_cast<std::uint8_t>(tag[0]) && p[1] == static_cast<std::uint8_t>(tag[1]) && p[2] == static_cast<std::uint8_t>(tag[2]) && p[3] == static_cast<std::uint8_t>(tag[3]);`

### `engine/core/builtin_assets.h`
**Types:** Asset, TextureEntry
- `inline bool FindTexture(const std::basic_string<Char>& romPath, Asset* out) { return detail::Lookup(romPath, &TextureEntries, out);`
- `inline bool FindFont(const std::basic_string<Char>& romPath, Asset* out) { return detail::Lookup(romPath, &FontEntries, out);`

### `engine/core/content_path.h`
**Types:** RomRoots
- `inline std::filesystem::path ReadableFile(std::string_view path, const RomRoots& roots) { if (IsRomShapedPath(path)) return ResolveRomForRead(path, roots);`
- `return PathFromUtf8(path);`
- `inline std::filesystem::path WritableFile(std::string_view path, const RomRoots& roots) { if (IsRomShapedPath(path)) return ResolveRomForWrite(path, roots);`
- `return PathFromUtf8(path);`
- `std::filesystem::path ExecutableDirectory();`
- `RomRoots CurrentRomRoots();`
- `std::filesystem::path ReadableFileCurrent(std::string_view path);`
- `std::filesystem::path WritableFileCurrent(std::string_view path);`
- `std::wstring ReadableWideCurrent(const wchar_t* path);`
- `inline std::filesystem::path LexicalChildPath(const std::filesystem::path& path, const std::filesystem::path& root) { auto asChild = [](const std::filesystem::path& candidate, const std::filesystem::path& base) -> std::filesystem::path { const std::filesystem::path rel = candidate.lexically_normal().lexically_relative(base.lexically_normal());`
- `return asChild(absPath, absRoot);`
- `return std::filesystem::is_directory(root, ec) && std::filesystem::is_directory(root / "rom", ec);`

### `engine/core/debug_system.h`
- `void Init();`
- `void Shutdown();`
- `void ToggleCollisionViz();`
- `bool CollisionVizEnabled();`
- `float GetTimeScale();`
- `void CycleTimeScale();`
- `bool IsFrozen();`
- `void PulseTimeScale(float scale, unsigned durationMs);`
- `void UpdateTimeScalePulse(float realDeltaMs);`
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
**Enums:** PlayMode, CameraMode, EditorEditMode
- `bool EngineGUI_IsSceneGraphObject(GameObject* obj);`
- `void EngineGUI_Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);`
- `void EngineGUI_Finalize();`
- `void EngineGUI_Update(double elapsed_time);`
- `void EngineGUI_Draw();`
- `void EngineGUI_OnBackbufferResized();`
- `void EngineGUI_DrawSelectionIndicator();`
- `bool EngineGUI_IsPlaying();`
- `PlayMode EngineGUI_GetPlayMode();`
- `bool EngineGUI_IsDebugCameraPlayInputActive();`
- `bool EngineGUI_IsOnStart();`
- `bool EngineGUI_PreferPlayVisuals();`
- `void EngineGUI_StartPlay(PlayMode mode = PlayMode::None);`
- `void EngineGUI_StopPlay();`
- `void EngineGUI_RequestExitGame();`
- `void EngineGUI_RequestBridgeHostQuit();`
- `void EngineGUI_SetEnabled(bool enabled);`
- `bool EngineGUI_IsEnabled();`
- `void EngineGUI_GetGameViewportRect(float& x, float& y, float& width, float& height);`
- `bool EngineGUI_MouseToViewportLocal(float& x, float& y);`
- `bool EngineGUI_SaveToFile(const char* filename);`
- `bool EngineGUI_LoadFromFile(const char* filename);`
- `const std::string& EngineGUI_GetDefaultScenePath();`
- `std::string EngineGUI_ResolveStartupScenePath();`
- `bool EngineGUI_IsSceneDirty();`
- `void EngineGUI_MarkSceneDirty();`
- `void EngineGUI_ClearSceneDirty();`
- `bool EngineGUI_ApplySkyboxCubemapPath(const std::string& assetPath);`
- `void EngineGUI_QueueUnsavedLoad(const char* filename);`
- `void EngineGUI_QueueUnsavedExit();`
- `void DrawEditorSceneProxies();`
- `void DrawSceneModels();`
- `void EngineGUI_FitNewCollider(GameObject* node);`
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
- `void EngineGUI_NotifyFramePresented();`
- `void EngineGUI_RequestContentRefresh();`

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

### `engine/core/env_var.h`
- `std::string value(buf);`
- `return std::string(value);`

### `engine/core/frame_capture.h`
- `void FrameCapture_BeginFrame();`
- `void FrameCapture_EndFrame();`
- `void FrameCapture_BeginDraw();`
- `void FrameCapture_EndDraw();`
- `void FrameCapture_RecordSpriteDraw();`
- `void FrameCapture_RecordTextDraw();`
- `void FrameCapture_Shutdown();`

### `engine/core/frame_timer.h`
**Enums:** FrameTimerStage
- `void FrameTimer_Begin(FrameTimerStage stage);`
- `void FrameTimer_End(FrameTimerStage stage);`
- `void FrameTimer_BeginFrame();`
- `void FrameTimer_EndFrame();`
- `void FrameTimer_LastInterval(FrameTimerStage stage, long long* startQpc, long long* endQpc);`
- `double FrameTimer_LastMs(FrameTimerStage stage);`
- `void FrameTimer_FlushPrint(double fps, bool rtEnabled);`

### `engine/core/frame_transform_snapshot.h`
**Types:** GameObject, FrameTransformEntry
- `void FrameTransformSnapshot_Capture(const std::vector<GameObject*>& objects);`
- `const FrameTransformEntry* FrameTransformSnapshot_Find(const GameObject* object);`
- `void FrameTransformSnapshot_Clear();`

### `engine/core/job_system.h`
**Types:** JobNode
- `void JobSystem_Init(int workerCount);`
- `void JobSystem_Shutdown();`
- `void JobSystem_BeginFrame();`
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

### `engine/core/legacy_asset_alias.h`
- `inline bool IsLegacyFontPath(const std::string& path) { return detail::SamePath(path, kLegacyFontAtlas) || detail::SamePath(path, kLegacyLemonSlice);`

### `engine/core/play_fps_overlay.h`
- `inline bool IsEnabled() { return enabled.load(std::memory_order_relaxed);`
- `inline void SetEnabled(bool value) { enabled.store(value, std::memory_order_relaxed);`
- `inline bool ShouldDraw(bool playing) { return playing && IsEnabled();`
- `inline void LoadPrefs(const nlohmann::json& prefs) { const auto found = prefs.is_object() ? prefs.find("showFpsWhenPlaying") : prefs.end();`
- `inline void WritePrefs(nlohmann::json& prefs) { prefs["showFpsWhenPlaying"] = IsEnabled();`
- `inline void LoadFromFile(const char* path = kPrefsPath) { SetEnabled(false);`
- `std::ifstream file(content_path::ReadableFileCurrent(path));`

### `engine/core/render_thread.h`
- `void RenderThread_Start();`
- `void RenderThread_Stop();`
- `bool RenderThread_IsRunning();`
- `void RenderThread_Kick(void (*work)());`
- `void RenderThread_Join();`
- `bool RenderThread_IsBusy();`
- `HANDLE RenderThread_DoneEvent();`

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

### `engine/core/texture_cook_policy.h`
- `inline std::size_t ExtensionPos(const std::basic_string<Char>& path) { const std::size_t slash = path.find_last_of(std::basic_string<Char>{Char('/'), Char('\\')});`
- `inline bool IsCookableExtension(const std::basic_string<Char>& ext) { return detail::ExtensionEquals(ext, ".png") || detail::ExtensionEquals(ext, ".jpg") || detail::ExtensionEquals(ext, ".jpeg") || detail::ExtensionEquals(ext, ".bmp");`
- `template <class Char> inline std::basic_string<Char> DdsSiblingPath(const std::basic_string<Char>& path) { const std::size_t dot = detail::ExtensionPos(path);`
- `inline bool ShouldCookRomTexture(std::string_view romRelative) { const std::string path(romRelative);`

### `engine/core/utf8_path.h`
- `inline std::filesystem::path PathFromUtf8(std::string_view utf8) { #if defined(__cpp_lib_char8_t) return std::filesystem::path( std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));`
- `return std::filesystem::u8path(std::string(utf8));`
- `inline std::string PathToUtf8(const std::filesystem::path& path) { #if defined(__cpp_lib_char8_t) const auto bytes = path.generic_u8string();`
- `return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());`

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
**Types:** DebugTlsBuffer, debugbuf, debug_ostream
- `inline void SetDebugLogHook(DebugLogHook hook, void* user = nullptr) { std::lock_guard<std::mutex> lock(DebugLogHookMutex());`
- `inline void ClearDebugLogHook() { std::lock_guard<std::mutex> lock(DebugLogHookMutex());`
- `inline void DebugWriteA(const char* text) { if (text && *text) OutputDebugStringA(text);`
- `std::lock_guard<std::mutex> lock(DebugLogHookMutex());`
- `std::string utf8(static_cast<size_t>(bytes - 1), '\0');`
- `int_type overflow(int_type ch) override { if (!traits_type::eq_int_type(ch, traits_type::eof())) DoutTls().text.push_back(traits_type::to_char_type(ch));`
- `return traits_type::not_eof(ch);`
- `std::streamsize xsputn(const char* s, std::streamsize count) override { if (s && count > 0) DoutTls().text.append(s, static_cast<size_t>(count));`
- `public: debug_ostream() : std::basic_ostream<char, std::char_traits<char>>(new debugbuf()) {} ~debug_ostream() { delete rdbuf();`

### `engine/debug_draw/debug_text.h`
**Types:** UIFont, DebugText, StringBlock, Line, Vertex
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
**Types:** ColliderComponent, Name
- `void ApplyToOwner();`
- `void ClearFromOwner();`
- `std::string Serialize() const override;`
- `std::string SerializeShape(ColliderType shape) const;`
- `void Deserialize(const std::string& jsonStr) override;`
- `std::string Serialize() const override {return SerializeShape(ColliderType::Shape);`
- `void Deserialize(const std::string& data) override { \ ColliderComponent::Deserialize(data);`

### `engine/ecs/collider_component_access.h`
- `inline ColliderComponent* FindColliderComponent(const std::unique_ptr<GameObject>& object) { return FindColliderComponent(object.get());`

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

### `engine/ecs/euler_math.h`
**Types:** EulerDeg
- `inline EulerDeg RollPitchYawDegFromMatrix(const float m[3][3]) { const float cosPitch = std::sqrt(m[2][0] * m[2][0] + m[2][2] * m[2][2]);`

### `engine/ecs/game_object.h`
**Types:** GameObject
**Enums:** Tag
- `explicit GameObject(std::string name = "GameObject") : m_name(std::move(name)) {} virtual ~GameObject();`
- `T* AddComponent(Args&&... args) { auto comp = std::make_unique<T>(std::forward<Args>(args)...);`
- `std::type_index key(typeid(T));`
- `std::type_index key(typeid(*comp));`
- `const std::type_index key(typeid(*comp));`
- `void AttachPreparedComponents() { for (auto& entry : m_components) entry.second->OnAttach();`
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

### `engine/ecs/game_object_search.h`
**Enums:** Visit

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
- `static bool IsHiddenPrimitiveAlias(const std::string& assetPath);`
- `void Initialize();`
- `void Finalize();`
- `void RefreshAssetDirectory();`
- `bool BeginContentReload();`
- `bool PrepareContentModel(const std::string& path);`
- `void EndContentReload(bool commit);`
- `const std::vector<ObjectDef>& GetObjectDefinitions() const;`
- `std::vector<const ObjectDef*> GetObjectDefinitionsInFolder( const std::string& folderRelativePath) const;`
- `Model3D* GetModel(const std::string& modelNameOrPath);`
- `bool IsModelLoaded(const std::string& modelNameOrPath) const;`
- `void RebuildFolderTree();`
- `static std::string FolderPathForDef(const ObjectDef& def);`

### `engine/ecs/physics2d_components.h`
**Types:** Rigidbody2DComponent, Collider2DComponent
**Enums:** BodyType2D, Shape2D
- `std::string Serialize() const override;`
- `void Deserialize(const std::string&) override;`
- `void OnAttach() override {OnReset();`
- `void OnReset() override;`
- `void OnDetach() override;`
- `bool MovePosition(DirectX::XMFLOAT2 target);`
- `std::string Serialize() const override;`
- `void Deserialize(const std::string&) override;`
- `void OnDetach() override;`

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
- `bool PrefabRuntime_IsPendingDestroy(const GameObject* obj);`
- `void PrefabRuntime_DestroyAllSpawnedChildren();`
- `void PrefabRuntime_FlushSpawned();`
- `void PrefabRuntime_FlushDestroyed();`
- `void PrefabRuntime_ResetSessionState();`

### `engine/ecs/prefab_serializer.h`
**Types:** PrefabSerializer
- `static std::unique_ptr<GameObject> DeserializeSceneNode(const nlohmann::json& node);`
- `static std::string Serialize(const GameObject& root);`
- `static bool SaveToFile(const GameObject& root, const std::string& filePath);`
- `static std::unique_ptr<GameObject> DeserializeForScene(const std::string& json);`
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

### `engine/ecs/sprite_animation.h`
**Types:** SpriteAnimationFrame, SpriteAnimationClip, SpriteAnimationClock
- `return std::size_t(frame);`

### `engine/ecs/sprite_animator_component.h`
**Types:** SpriteAnimatorComponent
- `std::string Serialize() const override;`
- `void Deserialize(const std::string&) override;`
- `void OnAttach() override {OnReset();`
- `void OnReset() override;`
- `void Update(double dt) override;`
- `bool Play(const std::string& name,bool restart=false);`
- `void Stop() {m_clock.Reset();`
- `const SpriteAnimationClip* CurrentClip() const;`
- `const SpriteAnimationFrame* CurrentFrame() const;`
- `bool ImportSheet(const std::string& metadataPath,const std::string& clipName);`

### `engine/ecs/sprite_renderer_component.h`
**Types:** SpriteRendererComponent
- `std::string Serialize() const override;`
- `void Deserialize(const std::string&) override;`
- `void Draw() const;`
- `std::string EffectiveTexture() const;`
- `DirectX::XMFLOAT4 EffectiveRect() const;`
- `void SnapshotStartState() {m_startJson=Serialize();`
- `void OnReset() override {if(!m_startJson.empty()) Deserialize(m_startJson);`
- `void OnAttach() override {SnapshotStartState();`
- `DirectX::XMFLOAT2 GetWorldSize() const;`
- `bool HitTestRay(DirectX::FXMVECTOR origin, DirectX::FXMVECTOR direction) const;`
- `void DrawSceneSprites();`

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
**Types:** VisualComponent, LodLevel
- `Model3D* GetLodModel(int level) const;`
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

### `engine/editor/agent_docs_export.h`
- `std::string MakeManagedBlock(const std::string& body);`
- `bool UpsertManagedSection(const std::string& original, const std::string& block, std::string* out);`
- `std::string RewriteExportedMarkdown(const std::string& text);`
- `std::string ReadEngineCommit(const std::filesystem::path& engineRoot);`
- `bool ExportToContentProject(const std::filesystem::path& engineRoot, const std::filesystem::path& projectRoot, std::string* errorOut);`

### `engine/editor/asset_browser_text.h`
- `return measure(text, user);`
- `return std::string(text);`
- `return std::string(kEllipsis);`
- `return Ellipsize(text, maxWidth, measure, user);`

### `engine/editor/camera_gizmo.h`
**Types:** DebugDrawer, GameObject, CameraComponent
- `void DrawCamera(DebugDrawer& drawer, const engine::Transform* transform, const CameraComponent* camera);`
- `void DrawSceneCameras(const std::vector<std::unique_ptr<GameObject>>& roots, DebugDrawer& drawer);`

### `engine/editor/camera_orientation_gizmo.h`
- `void Draw();`
- `bool IsInteracting();`

### `engine/editor/content_file_monitor.h`
**Types:** ContentFileMonitor, File, Change
- `static std::string Utf8Path(const std::filesystem::path& path) { return PathToUtf8(path);`
- `std::ifstream stream(it->path(), std::ios::binary);`
- `std::vector<Change> Poll(const std::filesystem::path& root) { const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();`
- `const std::wstring name(info->FileName, info->FileNameLength / sizeof(wchar_t));`
- `void Stop() { if (m_worker.valid()) m_worker.wait();`
- `void StopWatch() { #ifdef _WIN32 if (m_directory != INVALID_HANDLE_VALUE) { CancelIoEx(m_directory, &m_overlapped);`

### `engine/editor/content_removal_policy.h`
**Enums:** RemovedContentAction
- `inline std::string NormalizeRomLookupKey(std::string_view path) { std::string key = LowerForwardSlashes(path);`
- `inline bool SceneJsonReferencesRomPath(std::string_view sceneJson, std::string_view romPath) { const std::string key = NormalizeRomLookupKey(romPath);`

### `engine/editor/editor_console.h`
- `EditorConsoleBuffer& EditorConsole_Buffer();`
- `EditorConsoleFilter& EditorConsole_Filter();`
- `bool& EditorConsole_ClearOnPlay();`
- `void EditorConsole_InstallLogHook();`
- `void EditorConsole_RemoveLogHook();`
- `void EditorConsole_Clear();`
- `void EditorConsole_OnPlayStarted();`
- `void DrawEditorConsolePanel(bool* open);`
- `void EditorConsole_WritePrefs(nlohmann::json& j);`
- `void EditorConsole_LoadPrefs(const nlohmann::json& j);`

### `engine/editor/editor_console_log.h`
**Types:** EditorConsoleEntry, EditorConsoleFilter, EditorConsoleBuffer
**Enums:** EditorConsoleLevel
- `std::string s(piece);`
- `explicit EditorConsoleBuffer(size_t capacity = 2000) : m_capacity(capacity < 1 ? 1 : capacity) { } void SetCapacity(size_t capacity) { std::lock_guard<std::mutex> lock(m_mutex);`
- `void Clear() { std::lock_guard<std::mutex> lock(m_mutex);`
- `void Push(std::string_view text, EditorConsoleLevel level, std::int64_t timestampMs) { const std::vector<std::string> lines = SplitConsoleLogLines(text);`
- `std::lock_guard<std::mutex> lock(m_mutex);`
- `void PushAutoLevel(std::string_view text, std::int64_t timestampMs) { const std::vector<std::string> lines = SplitConsoleLogLines(text);`
- `std::lock_guard<std::mutex> lock(m_mutex);`
- `std::uint64_t Generation() const { std::lock_guard<std::mutex> lock(m_mutex);`
- `size_t Size() const { std::lock_guard<std::mutex> lock(m_mutex);`
- `std::shared_ptr<const std::vector<EditorConsoleEntry>> Snapshot( const EditorConsoleFilter& filter) const { std::lock_guard<std::mutex> lock(m_mutex);`
- `std::vector<EditorConsoleEntry> SnapshotAll() const { std::lock_guard<std::mutex> lock(m_mutex);`

### `engine/editor/editor_docking.h`
**Types:** EditorPanelVisibility
- `void EditorDocking_ConfigureImGuiIO();`
- `void EditorDocking_OnProjectOpened(const std::string& projectRootUtf8);`
- `void EditorDocking_OnProjectClosed();`
- `void EditorDocking_BeginFrame();`
- `void EditorDocking_FlushIniIfNeeded();`
- `void EditorDocking_RequestResetLayout();`
- `void EditorDocking_DrawHost();`
- `void EditorDocking_FocusDefaultBottomTabIfNeeded();`
- `const EditorClientRect& EditorDocking_GetCentralNodeClientRect();`
- `bool EditorDocking_HasCentralNode();`
- `EditorClientRect EditorDocking_GetGameViewportClientRect();`
- `void EditorDocking_DrawLetterboxBars();`
- `EditorPanelVisibility& EditorDocking_PanelVisibility();`
- `bool EditorDocking_WantCaptureMouseForGame();`

### `engine/editor/editor_docking_math.h`
**Types:** EditorDefaultPanelSizes, EditorDockSplitRatios, EditorClientRect, EditorBackbufferRect, EditorCentralRectResolve
- `inline EditorClientRect FitEditor16x9Viewport(const EditorClientRect& bounds) { return FitFixedAspectRect(bounds, kEditorViewportAspectW, kEditorViewportAspectH);`
- `inline bool IsUsableViewportRect(const EditorClientRect& r) { return r.w >= kEditorDockViewportMinWidth && r.h >= kEditorDockViewportMinHeight && std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h);`
- `inline EditorDockSplitRatios ComputeDefaultDockSplitRatios( float hostW, float hostH, float uiZoom) { const EditorDefaultPanelSizes sizes = ComputeDefaultPanelSizes(hostW, hostH, uiZoom);`
- `return PathToUtf8(root / "rom" / "editor" / "imgui_layout.ini");`

### `engine/editor/editor_gui_internal.h`
**Types:** GameObject, UIElement, ColliderComponent, ReloadLight, PrimitiveCreateSpec
**Enums:** PendingFolderPickAction, PendingEditorAction, ReloadLightState
- `bool IsSceneLoadBusy();`
- `void SaveScene();`
- `void SaveSceneToFile(const std::string& sceneFile);`
- `void NewScene();`
- `void SaveProject();`
- `void ApplyProjectSessionState();`
- `bool SwitchToOpenedProject();`
- `void RequestOpenProject(const std::string& rootPath);`
- `void RequestCloseProject();`
- `void CloseOpenedProject();`
- `void BeginNewProjectFlow();`
- `void BeginOpenProjectFlow();`
- `void OpenBuildPreflight();`
- `void RequestBuildCurrentProject();`
- `void ConsumePendingFolderPick();`
- `void RequestOpenTestbed();`
- `void SetBuildDestBuffer(const std::string& path);`
- `void RemapNodeTagsAfterDelete(GameObject* node, int deletedId);`
- `void RemapSceneTagsAfterDelete(int deletedId);`
- `std::string TrimString(const std::string& value);`
- `std::string NormalizeSceneFileName(const std::string& sceneName);`
- `std::unique_ptr<GameObject> CreateEmptyGameObject();`
- `std::unique_ptr<GameObject> CreateDirectionalLightObject();`
- `std::unique_ptr<GameObject> CreatePointLightObject();`
- `std::unique_ptr<GameObject> CreateSpotLightObject();`
- `std::unique_ptr<GameObject> CreateCameraObject(bool debugFly = false);`
- `void SpawnRootGameObject(std::unique_ptr<GameObject> node);`
- `void SpawnChildGameObject(GameObject* parent);`
- `std::unique_ptr<GameObject> DuplicateGameObject(const GameObject& src);`
- `void SpawnDuplicate(GameObject* source);`
- `void DeleteGameObject(GameObject* toDelete, bool recordUndo = true);`
- `void ClearScene();`
- `void StartPlay(PlayMode mode = PlayMode::Viewport);`
- `bool IsValidUiZoomPercent(int percent);`
- `void CaptureBaseStyleFromCurrent();`
- `void ApplyEditorTheme(editor_theme::StylePreset preset, editor_theme::ColorScheme scheme);`
- `void ApplyUiZoom(int percent);`
- `void SaveUiPrefs();`
- `void LoadUiPrefs();`
- `bool AutoFitColliderToMesh(GameObject* node, ColliderComponent* cc);`
- `void ResolveSceneReloadConflict(bool loadDisk);`
- `bool MoveNodeOrder(GameObject* node, int offset);`
- `bool MoveNodeToBack(GameObject* node);`
- `bool MoveNodeToFront(GameObject* node);`
- `bool UndoableReorderNode(GameObject* node, bool (*moveFn)(GameObject*));`
- `ReloadLightState GetReloadLightState(bool active, bool paused, bool pending, const ReloadLight& light);`
- `std::string LowerAssetExtension(const char* path);`
- `bool AssetPathHasExtension(const char* path, const char* const* extensions, int count);`
- `bool AcceptAssetPathDrop(const char* const* extensions, int count, std::string& outPath);`
- `bool ScrollValue(float* value, float baseSpeed = 1.0f);`
- `std::string ShaderVariantPreviewLabel(Shader3DVariant v);`
- `void DrawEngineStartScreen();`
- `void DrawEditorMenuBar();`
- `void DrawEditorPopups();`
- `void DrawPostFxShowcaseWindow(bool* open);`
- `void DrawShortcutHintsWindow(bool* open);`
- `void DrawRenderingSettingsBody();`
- `void DrawProjectGravitySettings();`
- `void DrawProjectAudioSettings();`
- `void DrawProjectDefaultSceneSettings();`
- `bool DrawPlayablePackageConfigFields();`
- `void DrawProjectSettingsWindow();`
- `void DrawPreferencesWindow();`
- `bool& EditorSettings_ProjectWindowOpen();`
- `bool& EditorSettings_PreferencesWindowOpen();`
- `void EditorSettings_OpenProjectSettings();`
- `void EditorSettings_OpenPreferences();`
- `void DrawEditorToolbar(bool playVisuals, bool sceneLoadBusy, float screenW, float menuBarHeight);`
- `void DrawSceneNodeHierarchy(GameObject* node, GameObject* rootOwner);`
- `void DrawEditorHierarchyPanel(bool playVisuals, bool* open);`
- `void DrawEditorInspectorPanel(bool playVisuals, bool* open);`
- `void DrawEditorAssetPanel(bool playVisuals, bool sceneLoadBusy, bool* open);`
- `float QueryEditorDpiScale();`
- `void ApplyEditorUiScaleFromBase();`
- `void PushToolbarChromeStyle();`
- `void PopToolbarChromeStyle();`
- `float ToolbarIconSize();`
- `bool ToolbarIconButton(const char* icon, const char* tooltip);`
- `bool ToolbarIconToggle(const char* icon, bool active, const char* tooltip);`
- `void DrawReloadLight(const char* id, const char* name, ReloadLightState state, const ReloadLight& light, const std::string& detail);`

### `engine/editor/editor_icon_button.h`
- `bool IconButton(const char* icon_label, const ImVec2& size = ImVec2(0, 0));`
- `bool IconToggle(const char* icon_label, bool active, const ImVec2& size = ImVec2(0, 0));`
- `bool IconButtonTooltip(const char* icon_label, const char* tooltip, const ImVec2& size = ImVec2(0, 0));`
- `bool IconToggleTooltip(const char* icon_label, bool active, const char* tooltip, const ImVec2& size = ImVec2(0, 0));`

### `engine/editor/editor_layout.h`
**Types:** EditorLayoutMetrics
- `float Ui(float px);`
- `float EditorMenuBarHeight();`
- `float EditorToolbarHeight();`
- `float EditorTopChrome();`
- `EditorLayoutMetrics ComputeEditorLayout(float screenW, float screenH);`
- `ImVec2 ToolbarWindowPadding();`
- `ImVec2 ToolbarFramePadding();`

### `engine/editor/editor_prefab_panel.h`
**Types:** GameObject
- `GameObject* PrefabPanel( const std::string& prefabFolder, std::vector<std::unique_ptr<GameObject>>& nodes);`

### `engine/editor/editor_project_browser.h`
**Types:** GameObject
- `GameObject* ProjectBrowserPanel(std::vector<std::unique_ptr<GameObject>>& nodes);`

### `engine/editor/editor_settings_categories.h`
**Types:** EditorSettingsCategory
**Enums:** EditorSettingsWindow

### `engine/editor/editor_theme.h`
**Types:** SemanticColors, Fonts
**Enums:** StylePreset, ColorScheme
- `const char* StylePresetName(StylePreset preset);`
- `const char* ColorSchemeName(ColorScheme scheme);`
- `StylePreset ParseStylePreset(const char* name);`
- `ColorScheme ParseColorScheme(const char* name);`
- `StylePreset ClampStylePreset(int value);`
- `ColorScheme ClampColorScheme(int value);`
- `const SemanticColors& GetSemanticColors();`
- `const Fonts& GetFonts();`
- `StylePreset GetStylePreset();`
- `ColorScheme GetColorScheme();`
- `void Apply(ImGuiStyle& style, StylePreset preset, ColorScheme scheme);`
- `void ApplyColors(ImGuiStyle& style, StylePreset preset, ColorScheme scheme);`
- `bool LoadFonts(ImGuiIO& io, Fonts* out, float bodySize = 16.0f, float monoSize = 13.0f);`
- `bool PushLabelFont();`
- `void PopLabelFont();`
- `bool ColorsFullyAssigned(const ImGuiStyle& style, const ImVec4& sentinel);`

### `engine/editor/editor_ui_panel.h`
**Types:** GameObject, UIElement
**Enums:** UiCreateType
- `UIElement* CreateUiElement(UiCreateType type);`
- `void DrawCanvasElementHierarchy(UIElement*& selectedUiElement, GameObject*& selectedNode);`
- `void DrawCanvasElementProperties(UIElement*& selectedUiElement);`

### `engine/editor/editor_ui_scale_math.h`
- `inline float ComputeEffectiveUiScale(int userPercent, float dpiScale) { const int percent = ClampUiScalePercent(userPercent);`
- `inline bool EffectiveUiScaleIsIdempotent(int userPercent, float dpiScale) { const float a = ComputeEffectiveUiScale(userPercent, dpiScale);`

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

### `engine/editor/package_assets.h`
**Types:** Options, Result
- `Result ComputeReferencedAssets(const Options& options);`
- `std::string ExtractStrings(const std::string& bytes);`
- `std::vector<std::string> CollectRomTokens(const std::string& text, bool strictStart = true);`

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

### `engine/editor/project_file_merge.h`
- `inline nlohmann::json MergeIntoExisting(std::string_view existingText, const nlohmann::json& patch) { nlohmann::json doc = nlohmann::json::parse(existingText.begin(), existingText.end(), nullptr, false);`

### `engine/editor/project_manager.h`
**Types:** HWND__, ProjectManager, EditorSettings, PhysicsSettings, AudioSettings, BuildSettings, ProjectData
- `static ProjectManager& Instance();`
- `bool BootstrapAtStartup();`
- `bool OpenProject(const std::string& rootPath, std::string* errorOut = nullptr);`
- `std::string ResolveInRepoTestbedPath() const;`
- `void CloseProject();`
- `bool CreateProject(const std::string& parentDir, const std::string& projectName, std::string* errorOut = nullptr, bool template2D = false);`
- `bool SaveProjectFile(std::string* errorOut = nullptr);`
- `void SetLastOpenedScene(const std::string& fileName);`
- `void SetDefaultScene(const std::string& fileName);`
- `void SetEditorUiZoomPercent(int percent);`
- `void SetEditorUiStylePreset(const std::string& name);`
- `void SetEditorUiColorScheme(const std::string& name);`
- `void SetAssetBrowserListView(bool listView);`
- `void SetAssetBrowserTileSize(int tileSize);`
- `static int ClampAssetBrowserTileSize(int tileSize);`
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
- `bool ScaffoldNewProjectTree(const std::string& rootPath, const std::string& projectName, std::string* errorOut, bool template2D);`
- `void LoadRecentList();`
- `void SaveRecentList() const;`
- `static std::string RecentListPath();`
- `static std::string Utf8FromWide(const std::wstring& wide);`
- `static std::wstring WideFromUtf8(const std::string& utf8);`

### `engine/editor/texture_cook.h`
**Types:** Stats
- `bool CookPackageTextures(const std::filesystem::path& packageRom, const std::function<void(const std::string&)>& log, Stats* stats, std::string* error);`

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
- `void Controller_TestInjectLeftStick(float lx, float ly);`
- `void Controller_TestInjectRightStick(float rx, float ry);`
- `void Controller_TestInjectTriggers(float left, float right);`
- `void Controller_TestInjectButtons(const ControllerButton* buttons, int count);`
- `void Controller_TestInjectSample(float lx, float ly, float rx, float ry, float lt, float rt, unsigned short buttons);`
- `void Controller_ClearInjection();`
- `bool Controller_IsInjectionActive();`

### `engine/input/controller_test_inject.h`
**Types:** ControllerInjectSample, ControllerInjectOverlay

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
- `void Keyboard_TestInject(const Keyboard_Keys* keys, int count);`
- `void Keyboard_ClearInjection();`
- `bool Keyboard_IsInjectionActive();`
- `void Keyboard_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);`
- `bool Keyboard_IsKeyDownTrigger(Keyboard_Keys key);`
- `void keycopy();`

### `engine/input/mouse.h`
**Enums:** Mouse_ButtonMask
- `void Mouse_TestInject(int dx, int dy, unsigned int buttonsMask, int wheel = 0, bool hasAbsolute = false, int absX = 0, int absY = 0);`
- `void Mouse_ClearInjection();`
- `bool Mouse_IsInjectionActive();`
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

### `engine/physics/2d/physics2d.h`
**Types:** GameObject, Physics2DEvent, Physics2DListener, Physics2DRayHit
- `virtual void OnPhysics2DEvent(GameObject& self,const Physics2DEvent& event)=0;`
- `void Physics2D_Update(double dt);`
- `void Physics2D_DispatchEvents();`
- `void Physics2D_Remove(GameObject* object);`
- `void Physics2D_Reset();`
- `bool Physics2D_Raycast(DirectX::XMFLOAT2 origin,DirectX::XMFLOAT2 direction,float distance, Physics2DRayHit& hit,std::uint32_t mask=0xffffffff,bool includeTriggers=false);`
- `void Physics2D_DrawDebugColliders();`

### `engine/physics/2d/physics2d_world.h`
**Types:** PhysicsBody2DSpec, PhysicsBody2DState, PhysicsContact2D, PhysicsRay2D, Physics2DWorld, Body
- `public: ~Physics2DWorld() {Clear();`
- `bool Add(std::uint64_t key,const PhysicsBody2DSpec& spec);`
- `void Remove(std::uint64_t key);`
- `void Clear();`
- `void Step(float dt);`
- `void SetVelocity(std::uint64_t key,float x,float y);`
- `void SetTransform(std::uint64_t key,float x,float y,float angle);`
- `bool State(std::uint64_t key,PhysicsBody2DState& state) const;`
- `bool Raycast(float x,float y,float dx,float dy,float distance,std::uint32_t mask,bool sensors,PhysicsRay2D& hit) const;`
- `std::vector<PhysicsContact2D> TakeEvents();`
- `void Begin(b2ShapeId a,b2ShapeId b,bool trigger,const b2Manifold* manifold=nullptr);`
- `void End(b2ShapeId a,b2ShapeId b);`

### `engine/physics/collider/collider.h`
**Types:** GameObject, Model3D, ContactInfo, Collider, BoxCollider, SphereCollider, CapsuleCollider, CylinderCollider, MeshCollider
**Enums:** Tag, ColliderType
- `DirectX::XMFLOAT3 GetScaledLocalCenter() const;`
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
- `float GetRadius() const;`
- `bool CheckCollision(const Collider* other) const override;`
- `void ResolveCollision(Collider* other) override;`
- `bool RayIntersect(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, float& outT, DirectX::XMFLOAT3& outNormal) const override;`
- `void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;`
- `float GetRadius() const;`
- `float GetHeight() const;`
- `bool CheckCollision(const Collider* other) const override;`
- `void ResolveCollision(Collider* other) override;`
- `bool RayIntersect(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, float& outT, DirectX::XMFLOAT3& outNormal) const override;`
- `void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;`
- `float GetRadius() const;`
- `float GetHeight() const;`
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
- `DirectX::XMFLOAT3 Collider_ComputeLocalInverseInertia( const Collider* collider, float mass, bool kinematic);`

### `engine/physics/collider/cylinder_pair.h`
**Types:** CylinderPairVec3, CylinderPairShape
**Enums:** CylinderPairShapeKind
- `bool CylinderPair_Contact(const CylinderPairShape& a, const CylinderPairShape& b, CylinderPairVec3& outNormal, float& outPenetration, CylinderPairVec3* outPoint);`

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
- `DirectX::XMFLOAT3 Rigidbody_InvInertiaBox(float mass, const DirectX::XMFLOAT3& fullSize);`
- `DirectX::XMFLOAT3 Rigidbody_InvInertiaSphere(float mass, float radius);`
- `DirectX::XMFLOAT3 Rigidbody_InvInertiaUnitCube(float mass);`
- `DirectX::XMFLOAT3 Rigidbody_ApplyInvInertiaWorld( const DirectX::XMFLOAT3& localInvInertia, const DirectX::XMFLOAT3& rotationDeg, const DirectX::XMFLOAT3& worldVec);`
- `void Rigidbody_ApplyWorldDeltaRotation( DirectX::XMFLOAT3& rotationDeg, const DirectX::XMFLOAT3& dThetaRad);`
- `void AddForce(const DirectX::XMFLOAT3& force, ForceMode mode = ForceMode::Force);`
- `void AddTorque(const DirectX::XMFLOAT3& torque, ForceMode mode = ForceMode::Force);`
- `void AddForceAtPosition(const DirectX::XMFLOAT3& force, const DirectX::XMFLOAT3& worldPoint, const DirectX::XMFLOAT3& ownerPosition, const DirectX::XMFLOAT3& ownerRotation, ForceMode mode = ForceMode::Force);`
- `void ClearAllForces();`
- `void ClearVelocity(DirectX::XMFLOAT3 clear_flag);`
- `void Integrate(DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& rotation, double elapsed_time);`

### `engine/physics/physics_material.h`
**Types:** ContactSurface, CombinedContactSurface, ContactFrictionDecision
**Enums:** ContactFrictionMode
- `std::string PhysicsMaterial_ReadPathFromColliderJson(const std::string& jsonStr);`
- `bool PhysicsMaterial_LoadFromFile(const std::string& path, ContactSurface& out);`
- `ContactSurface PhysicsMaterial_FromRigidbody(float friction, float restitution);`
- `CombinedContactSurface PhysicsMaterial_Combine(const ContactSurface& a, const ContactSurface& b);`
- `ContactFrictionDecision PhysicsMaterial_DecideFriction( float vtLen, float invSum, float jNormalAbs, float muStatic, float muDynamic);`

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

### `engine/rendering/culling/cull_settings_math.h`
- `return std::min(std::max(scale, kMinCullDistanceScale), kMaxCullDistanceScale);`
- `return std::min(std::max(distance, kMinShadowDistance), kMaxShadowDistance);`
- `inline float ResolveShadowDistance(float presetDistance, float overrideDistance) { const float o = ClampShadowDistanceOverride(overrideDistance);`
- `return std::min(std::max(radius, kMinEffectCullRadius), kMaxEffectCullRadius);`

### `engine/rendering/culling/frustum_cull.h`
**Types:** FrustumCullStats
- `void FrustumCull_BeginMainPass(DirectX::FXMMATRIX viewProj);`
- `void FrustumCull_EndMainPass();`
- `bool FrustumCull_IsMainPassActive();`
- `const frustum_cull::FrustumPlanes& FrustumCull_GetPlanes();`
- `void FrustumCull_RecordConsidered();`
- `void FrustumCull_RecordCulled();`
- `void FrustumCull_RecordDrawn();`
- `FrustumCullStats FrustumCull_GetStats();`
- `FrustumCullStats FrustumCull_GetLastStats();`

### `engine/rendering/culling/frustum_cull_math.h`
**Types:** Aabb, FrustumPlanes
- `inline void NormalizePlane(DirectX::XMFLOAT4& p) { const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);`
- `return MakeAabbFromPoints(worldCorners, 8);`
- `return TransformAabb(local, world);`
- `return TransformAabb(local, rows);`

### `engine/rendering/culling/fx_cull.h`
**Types:** FxCullStats
- `void FxCull_BeginFrame();`
- `void FxCull_RecordBillboards(int considered, int culled);`
- `void FxCull_RecordParticles(int considered, int culled);`
- `void FxCull_RecordEffects(int considered, int culled);`
- `FxCullStats FxCull_GetLastStats();`

### `engine/rendering/culling/fx_cull_math.h`
- `return std::max(baseRadius, 0.0f) * std::max({sx, sy, sz, 1e-3f});`

### `engine/rendering/culling/light_select_math.h`
**Types:** Candidate
- `std::vector<float> score(static_cast<size_t>(n));`
- `std::vector<int> order(static_cast<size_t>(n));`

### `engine/rendering/culling/lod_select_math.h`
- `return std::min(std::max(bias, kMinBias), kMaxBias);`
- `inline int ClampForcedLevel(int level) { return std::min(std::max(level, -1), kMaxForcedLevel);`
- `return std::min(f, std::max(count, 0));`
- `inline int ClampShadowLodOffset(int offset) { return std::min(std::max(offset, 0), kMaxShadowLodOffset);`
- `return std::min(cameraLevel + ClampShadowLodOffset(offset), std::max(last, cameraLevel));`

### `engine/rendering/culling/lod_state.h`
**Types:** LodStats, LodDebugLabel, AnimCullStats
- `void Lod_BeginFrame(const DirectX::XMFLOAT3& cameraPosition, float verticalFovDeg = 0.0f);`
- `void Lod_EndFrame();`
- `bool Lod_IsFrameActive();`
- `const DirectX::XMFLOAT3& Lod_GetCameraPosition();`
- `float Lod_GetTanHalfFov();`
- `void Lod_RecordConsidered();`
- `void Lod_RecordCulledByDistance();`
- `void Lod_RecordDrawn(int level);`
- `LodStats Lod_GetLastStats();`
- `void Lod_AddDebugLabel(const LodDebugLabel& label);`
- `const LodDebugLabel* Lod_GetDebugLabels(int& count);`
- `void Lod_SetForceLevel(int level);`
- `int Lod_GetForceLevel();`
- `void Lod_SetShowRanges(bool show);`
- `bool Lod_GetShowRanges();`
- `uint64_t Lod_GetFrameSerial();`
- `void Lod_RecordAnimUpdated();`
- `void Lod_RecordAnimSkipped();`
- `AnimCullStats Lod_GetLastAnimStats();`

### `engine/rendering/culling/shadow_cascade_cull.h`
**Types:** ShadowCascadeCullStats
- `void ShadowCascadeCull_BeginFrame();`
- `void ShadowCascadeCull_BeginCascade(int cascadeIndex, DirectX::FXMMATRIX lightViewProj);`
- `void ShadowCascadeCull_EndCascade();`
- `void ShadowCascadeCull_EndFrame();`
- `bool ShadowCascadeCull_IsCascadePassActive();`
- `int ShadowCascadeCull_GetActiveCascadeIndex();`
- `const frustum_cull::FrustumPlanes& ShadowCascadeCull_GetPlanes();`
- `void ShadowCascadeCull_RecordConsidered();`
- `void ShadowCascadeCull_RecordCulled();`
- `void ShadowCascadeCull_RecordDrawn();`
- `ShadowCascadeCullStats ShadowCascadeCull_GetStats();`
- `ShadowCascadeCullStats ShadowCascadeCull_GetLastStats();`
- `ShadowCascadeCullStats ShadowCascadeCull_GetLastCascadeStats(int cascadeIndex);`
- `int ShadowCascadeCull_GetLastCascadeCount();`

### `engine/rendering/culling/sprite_cull.h`
**Types:** SpriteCullStats
- `void SpriteCull_BeginPass( float camX, float camY, float orthoHeight, float aspect, bool orthographic);`
- `void SpriteCull_EndPass();`
- `bool SpriteCull_IsPassActive();`
- `bool SpriteCull_IsCullReady();`
- `const sprite_cull::ViewRect& SpriteCull_GetViewRect();`
- `float SpriteCull_GetMargin();`
- `void SpriteCull_RecordConsidered();`
- `void SpriteCull_RecordCulled();`
- `void SpriteCull_RecordDrawn();`
- `SpriteCullStats SpriteCull_GetStats();`
- `SpriteCullStats SpriteCull_GetLastStats();`

### `engine/rendering/culling/sprite_cull_math.h`
**Types:** ViewRect
- `inline frustum_cull::Aabb MakeSpriteWorldAabb( float width, float height, float pivotX, float pivotY, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotationDeg, const DirectX::XMFLOAT3& scale) { const frustum_cull::Aabb local = MakeSpriteLocalAabb(width, height, pivotX, pivotY);`
- `return frustum_cull::TransformAabbTRS(local, position, rotationDeg, scale);`
- `inline frustum_cull::Aabb MakeSpriteWorldAabb( float width, float height, float pivotX, float pivotY, DirectX::FXMMATRIX world) { const frustum_cull::Aabb local = MakeSpriteLocalAabb(width, height, pivotX, pivotY);`
- `return frustum_cull::TransformAabb(local, world);`

### `engine/rendering/d3d11/device_lost.h`
- `inline bool IsDeviceLostResult(std::int32_t hr) { const std::uint32_t u = static_cast<std::uint32_t>(hr);`

### `engine/rendering/d3d11/post_process.h`
**Types:** PostFXParams
- `void PostProcess_Initialize();`
- `void PostProcess_Shutdown();`
- `void PostProcess_SetEffects(float rgbShift, float mosaicBlocks);`
- `void PostProcess_SetEffects(const PostFXParams& params);`
- `void PostProcess_GetEffects(float& outRgbShift, float& outMosaicBlocks);`
- `void PostProcess_GetEffects(PostFXParams& out);`
- `void PostProcess_TriggerBurst(float rgbShift, float durationSeconds);`
- `void PostProcess_Update(float dt);`
- `void PostProcess_Draw();`

### `engine/rendering/d3d11/renderer.h`
**Types:** VERTEX_3D, MATERIAL
- `HRESULT InitRenderer(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);`
- `void UninitRenderer(void);`
- `void Clear(void);`
- `void Present(void);`
- `bool Renderer_IsDeviceLost();`
- `std::string Renderer_DeviceLostMessage();`
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
- `bool ReloadContent();`
- `bool Exists(int handle);`
- `void SetLocation(int handle, const DirectX::XMFLOAT3& pos);`
- `void SetRotationEulerDeg(int handle, const DirectX::XMFLOAT3& rotDeg);`
- `void SetScale(int handle, float uniform);`
- `void SetScale(int handle, const DirectX::XMFLOAT3& scale);`
- `void SetSpeed(int handle, float speed);`
- `void SetPaused(int handle, bool paused);`
- `void SetShown(int handle, bool shown);`

### `engine/rendering/environment/equirect_cubemap.h`
**Types:** Limits, SizeCheck, Vec3, RgbaImage, CubeBgra, CubeLevel, IblConfig, FloatCube, FileStamp, CookPaths, DecodedImage, CookResult
**Enums:** SizeCode
- `SizeCheck ValidateEquirectSize(int width, int height, const Limits& limits = {});`
- `double DefaultVerticalFovDeg(int width, int height);`
- `double ResolveVerticalFovDeg(int width, int height, double overrideDeg);`
- `Vec3 CubeFaceDirection(int face, float u, float v);`
- `void DirectionToFaceUv(float x, float y, float z, int* face, float* u, float* v);`
- `void EquirectUvFromDirection(float x, float y, float z, float* u, float* v);`
- `void EquirectUvFromDirection(float x, float y, float z, float verticalFovDeg, float* u, float* v);`
- `bool ConvertEquirectToCube( const RgbaImage& image, int faceSize, double verticalFovDeg, CubeBgra* cube, std::string* error);`
- `bool WriteLegacyCubemapDds(const CubeLevel* levels, int levelCount, std::vector<std::uint8_t>* dds, std::string* error);`
- `IblConfig ProductionIblConfig();`
- `void BakeIrradiance(const FloatCube& src, int samples, int dstSize, FloatCube* dst);`
- `void BakePrefilter(const FloatCube& src, int samples, int baseSize, int mipCount, std::vector<FloatCube>* mips);`
- `bool CacheIsFresh(const FileStamp& source, const FileStamp& recorded);`
- `std::uint64_t Fnv1a64(const std::uint8_t* data, std::size_t size);`
- `std::string FormatCacheStamp(const FileStamp& stamp);`
- `bool ParseCacheStamp(const std::string& text, FileStamp* stamp);`
- `std::string PanoramaCacheRomPath(const std::string& pngRomPath, const char* kind);`
- `CookResult CookPanoramaFiles( const std::filesystem::path& pngFile, const CookPaths& outputs, DecodeImageFn decode, const Limits& limits, const IblConfig& ibl, double verticalFovOverrideDeg = 0.0);`

### `engine/rendering/environment/ibl.h`
- `bool Ibl_Initialize();`
- `void Ibl_Finalize();`
- `void Ibl_ReloadFromSettings();`
- `void Ibl_Bind(Shader3DVariant variant);`
- `bool Ibl_IsReady();`
- `float Ibl_Intensity();`

### `engine/rendering/environment/panorama_sky.h`
**Types:** Prep
- `bool IsEquirectSkyPath(const std::string& path);`
- `bool SameRomPath(const std::string& a, const std::string& b);`
- `void LogDiagnostic(const std::string& message);`
- `Prep PrepareSky(const std::string& skyboxRomPath);`
- `const Prep& CurrentPrep();`
- `bool CookProjectPanoramas(const std::filesystem::path& projectRoot, const std::filesystem::path& resourcesRoot, std::string* error);`

### `engine/rendering/environment/skybox.h`
- `bool Skybox_Initialize();`
- `void Skybox_Finalize();`
- `void Skybox_ReloadFromSettings();`
- `void Skybox_Draw(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);`

### `engine/rendering/environment/skybox_asset_path.h`
- `return IsEligibleCubemapPath(std::string_view{path});`
- `return RomPathFromAssetPath(std::string_view{path});`

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
- `bool LoadFBXQuiet(const wchar_t* fbxPath);`
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
- `void AdvanceAnimationTime(double dtSec);`
- `void UploadSkinnedVerticesIfDirty();`
- `void AddAnimation(const std::string& animName, const char* fileName);`
- `void EnsureAnimation(const std::string& animName, const char* fileName);`
- `bool HasAnimation(const std::string& animName) const;`
- `void MergeAnimScenesFrom(const Model3D& shared);`
- `void PlayAnimation(const std::string& animName);`
- `static std::unique_ptr<Model3D> CreateAnimInstanceFrom(const Model3D& shared);`
- `bool LoadFBXInternal(const wchar_t* fbxPath, bool reportErrors);`
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
- `void CacheLocalAabbFromPositions(const DirectX::XMFLOAT3* positions, size_t count, bool inflateForSkin);`

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
- `bool ModelUtil_IsTextureCached(const wchar_t* path);`
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

### `engine/rendering/sprite/line_sprite.h`
- `void LineSprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);`
- `void LineSprite_Finalize();`
- `void LineSprite_Draw( const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float thicknessWorld, const DirectX::XMFLOAT4& color);`

### `engine/rendering/sprite/sprite.h`
- `void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);`
- `void Sprite_Finalize(void);`
- `void Sprite_Draw(int texid, float X, float Y, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});`
- `void Sprite_Draw(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});`
- `void Sprite_Draw(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, float angle, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});`
- `void Sprite_DrawRadialFill(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, float angle, float fillAmount, float startAngle, bool clockwise, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});`
- `void Sprite_DrawWorld(int texture, float width, float height, DirectX::XMFLOAT2 pivot, DirectX::XMFLOAT4 uv, DirectX::XMFLOAT4 color, const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);`

### `engine/rendering/sprite/texture.h`
**Enums:** TextureReloadResult
- `void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);`
- `void Texture_Finalize();`
- `int TextureLoad(const wchar_t* ptr_file_name, bool mipmap = true);`
- `TextureReloadResult TextureReload(const wchar_t* path);`
- `bool TextureIsLoaded(const wchar_t* path);`
- `bool CubemapIsLoaded(const wchar_t* path);`
- `bool TextureValidateFile(const wchar_t* path);`
- `int TextureLoadQuiet(const wchar_t* ptr_file_name, bool mipmap = true);`
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
- `TextureReloadResult CubemapReload(const wchar_t* path);`

### `engine/save/save_store.h`
**Types:** Store, Slot
- `bool IsValidSlotName(std::string_view slot);`
- `bool IsValidKey(std::string_view key);`
- `std::string SanitizeFolderName(std::string_view name, std::string_view fallback);`
- `bool DefaultReplaceFile(const std::filesystem::path& tmp, const std::filesystem::path& target, const std::filesystem::path& backup, std::string* error);`
- `explicit Store(std::filesystem::path directory, ReplaceFileFn replace = DefaultReplaceFile, LogFn log = {});`
- `bool SetInt(std::string_view slot, std::string_view key, std::int64_t value);`
- `bool SetDouble(std::string_view slot, std::string_view key, double value);`
- `bool SetBool(std::string_view slot, std::string_view key, bool value);`
- `bool SetString(std::string_view slot, std::string_view key, std::string_view value);`
- `bool GetInt(std::string_view slot, std::string_view key, std::int64_t* out);`
- `bool GetDouble(std::string_view slot, std::string_view key, double* out);`
- `bool GetBool(std::string_view slot, std::string_view key, bool* out);`
- `bool GetString(std::string_view slot, std::string_view key, std::string* out);`
- `bool SetJsonText(std::string_view slot, std::string_view key, std::string_view jsonText);`
- `bool GetJsonText(std::string_view slot, std::string_view key, std::string* out);`
- `bool HasKey(std::string_view slot, std::string_view key);`
- `bool DeleteKey(std::string_view slot, std::string_view key);`
- `bool Commit(std::string_view slot);`
- `bool SlotExists(std::string_view slot) const;`
- `bool DeleteSlot(std::string_view slot);`
- `std::vector<std::string> ListSlots() const;`
- `void DropCache();`
- `std::filesystem::path MainPath(std::string_view slot) const;`
- `std::filesystem::path BackupPath(std::string_view slot) const;`
- `std::filesystem::path TempPath(std::string_view slot) const;`
- `Slot* Load(std::string_view slot);`
- `bool SetValue(std::string_view slot, std::string_view key, nlohmann::json value);`
- `const nlohmann::json* Find(std::string_view slot, std::string_view key);`
- `void Log(const std::string& line) const;`

### `engine/save/save_system.h`
- `std::mutex& SaveSystem_Mutex();`
- `save_store::Store& SaveSystem_Store();`
- `void SaveSystem_Reset();`

### `engine/scene/json_float.h`
- `std::string s(buf);`
- `inline bool JsonFloat_Equal(double a, double b, int decimals = kJsonFloatDecimals) { return JsonFloat_Format(a, decimals) == JsonFloat_Format(b, decimals);`
- `inline bool JsonFloat_Changed(double before, double after, int decimals = kJsonFloatDecimals) { return !JsonFloat_Equal(before, after, decimals);`
- `const std::string pad((indent > 0) ? static_cast<size_t>((depth + 1) * indent) : 0u, ' ');`
- `const std::string padClose((indent > 0) ? static_cast<size_t>(depth * indent) : 0u, ' ');`
- `const std::string pad((indent > 0) ? static_cast<size_t>((depth + 1) * indent) : 0u, ' ');`
- `const std::string padClose((indent > 0) ? static_cast<size_t>(depth * indent) : 0u, ' ');`
- `return JsonFloat_Dump(nlohmann::json::parse(a)) == JsonFloat_Dump(nlohmann::json::parse(b));`

### `engine/scene/prepared_scene.h`
**Types:** PreparedScene

### `engine/scene/render_settings.h`
**Types:** ShadowQualityPreset, RenderSettingsData
**Enums:** ShadowQuality
- `void RenderSettings_ResetToDefaults();`
- `RenderSettingsData& RenderSettings_Get();`
- `const RenderSettingsData& RenderSettings_GetConst();`
- `bool RenderSettings_SetBoolByName(const char* key, bool value);`
- `bool RenderSettings_GetBoolByName(const char* key, bool* outValue);`
- `bool RenderSettings_SetNumberByName(const char* key, double value);`
- `bool RenderSettings_GetNumberByName(const char* key, double* outValue);`
- `RenderSettingsData RenderSettings_ParseJson(const nlohmann::json& renderJson, RenderSettingsData settings = {});`
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
- `std::string SceneCameraResolveDefault(const std::vector<std::unique_ptr<GameObject>>& roots, const std::string& preferred);`
- `void SceneCameraBeginPlay();`

### `engine/scene/scene_execution_policy.h`
**Types:** SceneExecutionPolicy

### `engine/scene/scene_file_monitor.h`
**Types:** SceneFileMonitor, Version, Stamp
- `void SetBaseline(const std::string& path, const std::string& content) { Reset();`
- `static bool Read(const std::string& path, std::string& content) { const std::filesystem::path disk = content_path::ReadableFileCurrent(path);`
- `std::ifstream file(disk, std::ios::binary);`

### `engine/scene/scene_lighting.h`
**Types:** GameObject, SceneLightingStats
- `void SceneLightingSyncFromNodes(const std::vector<std::unique_ptr<GameObject>>& roots, const DirectX::XMFLOAT3& cameraPosition);`
- `SceneLightingStats SceneLighting_GetLastStats();`

### `engine/scene/scene_loader.h`
**Types:** PreparedScene, ScenePreparationJob
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
- `std::shared_ptr<PreparedScene> SceneLoader_PrepareFromContent(const std::string& path, const std::string& content, std::string& error);`
- `void SceneLoader_ApplyPrepared(std::shared_ptr<PreparedScene> prepared, bool preserveEditorView = false);`
- `const std::string& SceneLoader_GetCurrentSceneContent();`
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
- `bool SceneLoader_Is2D();`
- `bool SceneLoader_Uses3DPhysics();`
- `bool SceneLoader_Uses2DPhysics();`
- `void SceneLoader_SetExecutionMode(bool scene2D, bool physics3D, bool physics2D=false);`
- `std::shared_ptr<ScenePreparationJob> SceneLoader_BeginPreparation(const std::string& path, const std::string* content = nullptr, bool background = true);`
- `bool SceneLoader_TickPreparation(ScenePreparationJob& job, double budgetMs = 4.0);`
- `bool SceneLoader_PreparationReading(const ScenePreparationJob& job);`
- `size_t SceneLoader_PreparationCompleted(const ScenePreparationJob& job);`
- `size_t SceneLoader_PreparationTotal(const ScenePreparationJob& job);`
- `std::shared_ptr<PreparedScene> SceneLoader_PreparationResult(const ScenePreparationJob& job);`
- `std::string SceneLoader_PreparationError(const ScenePreparationJob& job);`

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

### `engine/scripting/host_text.h`
- `inline int CopyOut(const std::string& text, char* out, int capacity) { if (out && capacity > 0) { const std::size_t n = (std::min)(text.size(), static_cast<std::size_t>(capacity - 1));`
- `bool ReadText(Getter getter, std::string* out) { const int length = getter(nullptr, 0);`
- `std::string buf(static_cast<std::size_t>(length) + 1, '\0');`

### `engine/scripting/script_abi_layout.h`
**Types:** Mismatch
**Enums:** Entry

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
**Types:** ScriptFactory, ReloadFeedback, DllEntry, DllApi, PendingReload
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
- `const ReloadFeedback& GetLastReloadFeedback() const;`
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
- `bool ScriptApi_CollisionVizEnabled();`
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
- `void ScriptApi_Camera_AddTrauma(float amount01);`
- `void ScriptApi_Camera_SetSpeedFactor(float speed01);`
- `void ScriptApi_Debug_PulseTimeScale(float scale, unsigned durationMs);`
- `void ScriptApi_LineSprite_Draw(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float thicknessWorld, const DirectX::XMFLOAT4& color);`
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
- `bool ScriptApi_RequestExitGame();`
- `bool ScriptApi_RenderSettingSetBool(const char* key, bool value);`
- `bool ScriptApi_RenderSettingGetBool(const char* key, bool& outValue);`
- `bool ScriptApi_RenderSettingSetNumber(const char* key, double value);`
- `bool ScriptApi_RenderSettingGetNumber(const char* key, double& outValue);`
- `bool ScriptApi_RigidbodyGetVelocity(GameObject& object, DirectX::XMFLOAT3& outVelocity);`
- `bool ScriptApi_RigidbodySetVelocity(GameObject& object, DirectX::XMFLOAT3 velocity);`
- `bool ScriptApi_RigidbodyGetAngularVelocity(GameObject& object, DirectX::XMFLOAT3& outVelocity);`
- `bool ScriptApi_RigidbodySetAngularVelocity(GameObject& object, DirectX::XMFLOAT3 velocity);`
- `bool ScriptApi_RigidbodyAddForce(GameObject& object, DirectX::XMFLOAT3 force, ForceMode mode = ForceMode::Force);`
- `bool ScriptApi_RigidbodyAddTorque(GameObject& object, DirectX::XMFLOAT3 torque, ForceMode mode = ForceMode::Force);`
- `GameObject* ScriptApi_FindGameObjectByName(const char* name, bool includeInactive = false);`
- `int ScriptApi_FindGameObjectsByTag(Tag tag, GameObject** out, int capacity, bool includeInactive = false);`
- `float ScriptApi_GetTimeScale();`
- `double ScriptApi_GetRealTimeSeconds();`
- `bool ScriptApi_SaveSetInt(const char* slot, const char* key, long long value);`
- `bool ScriptApi_SaveSetFloat(const char* slot, const char* key, double value);`
- `double ScriptApi_SaveGetFloat(const char* slot, const char* key, double defaultValue = 0.0);`
- `bool ScriptApi_SaveSetBool(const char* slot, const char* key, bool value);`
- `bool ScriptApi_SaveGetBool(const char* slot, const char* key, bool defaultValue = false);`
- `bool ScriptApi_SaveSetString(const char* slot, const char* key, const char* utf8Value);`
- `std::string ScriptApi_SaveGetString(const char* slot, const char* key, const char* defaultValue = "");`
- `bool ScriptApi_SaveSetJson(const char* slot, const char* key, const char* jsonText);`
- `std::string ScriptApi_SaveGetJson(const char* slot, const char* key);`
- `bool ScriptApi_SaveHasKey(const char* slot, const char* key);`
- `bool ScriptApi_SaveDeleteKey(const char* slot, const char* key);`
- `bool ScriptApi_SaveCommit(const char* slot);`
- `bool ScriptApi_SaveSlotExists(const char* slot);`
- `bool ScriptApi_SaveDeleteSlot(const char* slot);`
- `std::vector<std::string> ScriptApi_SaveListSlots();`
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
- `void ScriptApi_DebugDrawLine(DirectX::XMFLOAT3 start, DirectX::XMFLOAT3 end, DirectX::XMFLOAT4 color = {0.2f, 0.8f, 1.0f, 0.95f});`
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
- `bool ScriptApi_SpritePlay(GameObject* object, const char* clipName, bool restart = false);`
- `void ScriptApi_SpritePause(GameObject* object);`
- `void ScriptApi_SpriteStop(GameObject* object);`
- `void ScriptApi_SpriteSetFlip(GameObject* object, bool flipX, bool flipY);`
- `void ScriptApi_SpriteSetTint(GameObject* object, DirectX::XMFLOAT4 tint);`
- `bool ScriptApi_Physics2DSetVelocity(GameObject* object, DirectX::XMFLOAT2 velocity);`
- `bool ScriptApi_Physics2DGetVelocity(GameObject* object, DirectX::XMFLOAT2* velocity);`
- `bool ScriptApi_Physics2DMovePosition(GameObject* object, DirectX::XMFLOAT2 position);`
- `bool ScriptApi_Physics2DRaycast(DirectX::XMFLOAT2 origin, DirectX::XMFLOAT2 direction, float distance, GameObject** outObject, DirectX::XMFLOAT2* outPoint, DirectX::XMFLOAT2* outNormal, float* outDistance, std::uint32_t mask = 0xffffffffu, bool includeTriggers = false);`
- `void ScriptApi_GetGameViewportRect(float& x, float& y, float& width, float& height);`

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

### `engine/testing/controller_inject_protocol.h`
**Types:** ControllerCommand, ControllerOperation
**Enums:** ControllerParseStatus, State
- `inline ControllerParseStatus ParseControllerCommand(const std::string& json, ControllerCommand& out) { const auto object = nlohmann::json::parse(json, nullptr, false);`
- `return ParseControllerCommand(object, out);`
- `void Begin(bool playing) { if (!playing) Cancel();`

### `engine/testing/controller_probe_trace.h`
**Types:** ControllerProbeFrame, ControllerProbeSnapshot, ControllerProbeRing, ProbeValidation
- `void Reset() { frames_.clear();`
- `inline bool ProbeAxesNeutral(const ControllerProbeFrame& frame) { return ProbeFloatEq(frame.lx, 0.0f) && ProbeFloatEq(frame.ly, 0.0f) && ProbeFloatEq(frame.rx, 0.0f) && ProbeFloatEq(frame.ry, 0.0f) && ProbeFloatEq(frame.lt, 0.0f) && ProbeFloatEq(frame.rt, 0.0f);`
- `inline bool ProbeFullyNeutral(const ControllerProbeFrame& frame) { return ProbeAxesNeutral(frame) && ProbeButtonsNeutral(frame);`

### `engine/testing/runtime_test_bridge.h`
**Types:** ControllerProbeApiTag, ObservedTarget, ObserveVec3, ObservePose, ListedObject, PerfSample, RuntimeTestEvent, RuntimeTestEventsSnapshot, WorldOpRequest, EditorOpRequest, EditorBridgeQuitPlan, Protocol, CaptureState, WorldOpState, EditorOpState, Bridge, Impl
**Enums:** WorldOpKind, BridgeHostKind, EditorPlayModeKind, EditorOpKind, InputState, Status
- `void RuntimeTestControllerProbe_SetEnabled(bool enabled);`
- `void RuntimeTestControllerProbe_Reset();`
- `bool RuntimeTestControllerProbe_IsEnabled();`
- `void RuntimeTestControllerProbe_SampleFromGetters();`
- `ControllerProbeSnapshot RuntimeTestControllerProbe_CopySnapshot();`
- `void RuntimeTestEvents_SetEnabled(bool enabled);`
- `void RuntimeTestEvents_Reset();`
- `bool RuntimeTestEvents_IsEnabled();`
- `bool RuntimeTestEvents_RecordCheckpoint(const char* checkpoint, const char* actor, int score);`
- `RuntimeTestEventsSnapshot RuntimeTestEvents_CopySnapshot();`
- `void RuntimeTestSceneOverride_Set(std::string romPath);`
- `bool RuntimeTestSceneOverride_Has();`
- `const std::string& RuntimeTestSceneOverride_Get();`
- `void RuntimeTestSceneOverride_Clear();`
- `bool IsRuntimeTestBridgeSettingKey(const std::string& key);`
- `bool ParseRuntimeTestBridgeSettingValue(const Json& value, bool& outBool, std::string& err);`
- `bool IsRuntimeTestBridgeNumericSettingKey(const std::string& key);`
- `bool ParseRuntimeTestBridgeNumericSettingValue(const Json& value, double& outNumber, std::string& err);`
- `public: Protocol(std::string runId, unsigned long pid, unsigned long session, BridgeHostKind host = BridgeHostKind::Runtime);`
- `void Publish(bool ready, bool playing, const std::string& scene, std::vector<ObservedTarget> targets = {}, std::vector<ListedObject> objects = {}, const char* readyDetail = nullptr, EditorPlayModeKind playMode = EditorPlayModeKind::None);`
- `bool TakeQuit();`
- `void ConfirmQuitReceipt();`
- `void AbandonQuit();`
- `Json Request(const std::string& frame);`
- `void BeginInput(bool playing, std::uint64_t now);`
- `void EndInput();`
- `void CancelKeyboard();`
- `void ExpireKeyboard(std::uint64_t now);`
- `bool KeyboardIsolating() const;`
- `std::vector<std::string> InputKeys() const;`
- `void CancelMouse();`
- `void ExpireMouse(std::uint64_t now);`
- `bool MouseIsolating() const;`
- `void GetMouseInput(int& dx, int& dy, unsigned int& buttonsMask, int& wheel, bool& hasAbsolute, int& absX, int& absY) const;`
- `void CancelController();`
- `void ExpireController(std::uint64_t now);`
- `bool ControllerIsolating() const;`
- `void GetControllerInput(float& lx, float& ly, float& rx, float& ry, float& lt, float& rt, std::uint16_t& buttons) const;`
- `bool HasPendingCapture(std::int64_t& outId) const;`
- `void CompleteCapture(std::int64_t id, bool ok, uint32_t w, uint32_t h, const std::string& basename, const std::string& path, const std::string& err);`
- `bool HasPendingWorldOp(WorldOpRequest& out) const;`
- `void CompleteWorldOp(std::int64_t id, bool ok, const std::string& err);`
- `void MarkWorldOpApplied(std::int64_t id, bool ok, const std::string& err);`
- `void CancelWorldOp();`
- `bool HasPendingEditorOp(EditorOpRequest& out) const;`
- `void CompleteEditorOp(std::int64_t id, bool ok, const std::string& err);`
- `void MarkEditorOpApplied(std::int64_t id, bool ok, const std::string& err);`
- `void CancelEditorOp();`
- `bool ShouldRunSimTick();`
- `void NoteSimFrame();`
- `void NotePhysicsTick();`
- `void RecordPerfSample(const PerfSample& sample);`
- `const char* InputStatus() const;`
- `const char* MouseStatus() const;`
- `const char* ControllerStatus() const;`
- `const char* VerbName() const;`
- `void SealAppliedWorldOp();`
- `const char* VerbName() const;`
- `void SealAppliedEditorOp();`
- `public: Bridge();`
- `bool StartFromEnvironment();`
- `void Stop();`
- `void SetCaptureHandler(CaptureHandler handler);`
- `void SetWorldOpHandler(WorldOpHandler handler);`
- `void SetEditorOpHandler(EditorOpHandler handler);`
- `bool PublishAndTakeQuit(bool ready, bool playing, const std::string& scene, std::vector<ObservedTarget> targets = {}, std::vector<ListedObject> objects = {}, const char* readyDetail = nullptr, EditorPlayModeKind playMode = EditorPlayModeKind::None);`
- `void PollAndApplyInput(bool inGamePlaying);`
- `void FinishInputUpdate();`
- `void ExpireInputDeadlines();`
- `void MaybeExecuteCapture();`
- `void MaybeApplyWorldOps();`
- `void MaybeApplyEditorOps();`
- `bool IsPaused();`
- `bool ShouldRunSimTick();`
- `void NoteSimFrame();`
- `void NotePhysicsTick();`
- `void RecordPerfSample(const PerfSample& sample);`
- `std::string PipeName() const;`

### `engine/testing/runtime_test_scene_arg.h`
- `return fail("empty scene path");`
- `return fail("scene path too long");`
- `return fail("scene path contains control characters");`
- `return fail("scene path must be rom/-relative (not absolute)");`
- `return fail("scene path must be rom/-relative (not absolute)");`
- `return fail("scene path must start with rom/");`
- `return fail("scene path must not contain '.' or '..' segments");`
- `return fail("scene path segment invalid");`
- `return fail("scene path contains illegal characters");`
- `return fail("scene path must not end with '/'");`
- `return fail("scene path too short");`
- `return fail("scene path must end with .json");`

### `engine/ui/scene_canvas.h`
- `void SceneCanvas_Initialize();`
- `void SceneCanvas_Finalize();`
- `void SceneCanvas_Update(double elapsed_time);`
- `void SceneCanvas_Draw();`
- `UICanvas* SceneCanvas_GetCanvas();`
- `void SceneCanvas_ClearSceneCanvas();`
- `std::unique_ptr<UICanvas> SceneCanvas_Prepare(const nlohmann::json& canvasJson);`
- `void SceneCanvas_ApplyPrepared(std::unique_ptr<UICanvas> canvas, const nlohmann::json& canvasJson);`
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
- `void SetPivot(DirectX::XMFLOAT2 pivot);`
- `DirectX::XMFLOAT2 GetPivot() const;`
- `bool HasExplicitPivot() const;`
- `DirectX::XMFLOAT2 GetPivotScreenPosition() const;`
- `DirectX::XMFLOAT2 GetPivotDesignSize() const;`
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
- `const UIFont* UIFont_GetBuiltinMono();`
- `void UIFont_ClearCache();`
- `bool UIFont_IsTrueTypePath(const wchar_t* path);`
- `bool UIFont_IsTrueTypePath(const std::string& path);`
- `bool UIFont_Reload(const wchar_t* path);`

### `engine/ui/ui_sprite.h`
**Types:** UISprite
**Enums:** UISpriteFillMethod, UISpriteFillOrigin
- `public: UISprite(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER, float width = 64.0f, float height = 64.0f);`
- `void Initialize() override;`
- `void Update(double elapsed_time) override;`
- `void Draw() override;`
- `void Finalize() override;`
- `bool GetScreenRect(UIScreenRect& out) const override;`
- `DirectX::XMFLOAT2 GetDrawPosition() const;`
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
- `DirectX::XMFLOAT2 GetPivotDesignSize() const;`
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
- `void CameraDirector_BeginPlay(const std::vector<std::unique_ptr<GameObject>>& roots);`
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
**Enums:** DebugCameraControlMode, DebugCameraViewAxis
- `void DebugCamera_Update(double dt);`
- `void DebugCamera_Set2DView(bool enabled);`
- `bool DebugCamera_Is2DView();`
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
- `void DebugCamera_SnapToAxis(DebugCameraViewAxis axis, float durationSec = 0.25f);`
- `bool DebugCamera_IsTransitioning();`
- `void DebugCamera_CancelTransition();`
- `void DebugCamera_SaveEditorView();`
- `void DebugCamera_RestoreEditorView();`
- `void DebugCamera_ResetEditorView(bool view2D);`

### `src/app/camera/first_person_camera.h`
- `void FirstPersonCamera_Initialize();`
- `void FirstPersonCamera_Update(double elapsed_time);`
- `void FirstPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr);`
- `void FirstPersonCamera_SetEyeHeight(float eyeHeight);`
- `void FirstPersonCamera_SetMouseSensitivity(float sensitivity);`
- `void FirstPersonCamera_SetControllerSensitivity(float sensitivity);`
- `void FirstPersonCamera_SetPitchLimits(float minPitch, float maxPitch);`
- `void FirstPersonCamera_SetFov(float fovDegrees);`
- `void FirstPersonCamera_ShowCursor(bool show);`
- `bool FirstPersonCamera_IsCursorVisible();`
- `bool FirstPersonCamera_TryGetPose(CameraPose& outPose);`

### `src/app/camera/third_person_camera.h`
**Types:** GameObject
**Enums:** ThirdPersonMagnetMode
- `void ThirdPersonCamera_Initialize();`
- `void ThirdPersonCamera_Shutdown();`
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
- `void ThirdPersonCamera_AddTrauma(float amount01);`
- `float ThirdPersonCamera_GetTrauma();`
- `void ThirdPersonCamera_SetSpeedFactor(float speed01);`

### `src/app/camera/third_person_move_math.h`
**Types:** Vec2

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
- `bool Grid_IsVisible();`
- `void Grid_SetVisible(bool visible);`

### `src/app/rendering/scene_transition.h`
- `void SceneTransition_Initialize();`
- `void SceneTransition_Finalize();`
- `void SceneTransition_Update(double elapsed_time);`
- `void SceneTransition_Draw();`
- `bool SceneTransition_IsActive();`
- `bool SceneTransition_BlocksSceneUpdate();`
- `bool SceneTransition_TryBegin(const std::string& targetScene, bool playOnLoad);`
- `bool SceneTransition_TryHandleSceneRequest(const std::string& currentScene, const std::string& nextScene, bool playOnLoad);`

### `src/app/runtime_test_editor_apply.h`
- `bool RuntimeTestEditor_Apply(const ivy::testing::EditorOpRequest& op, std::string& errorOut);`

### `src/app/runtime_test_world_apply.h`
- `bool RuntimeTestWorld_Apply(const ivy::testing::WorldOpRequest& op, std::string& errorOut);`

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
