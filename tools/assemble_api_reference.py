"""Assemble docs/ENGINE_API_REFERENCE.md from intro + header extract."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTRACT = ROOT / "docs" / "_api_extract.md"
OUT = ROOT / "docs" / "ENGINE_API_REFERENCE.md"

def header_count(extract_text: str) -> int:
    return sum(1 for line in extract_text.splitlines() if line.startswith("### `"))

INTRO_TEMPLATE = r'''# Ivy Engine — Complete Ability & API Reference

Exhaustive catalog of **Ivy Engine** capabilities and public functions,
scanned from every header under `engine/` and `src/app/` ({header_count} headers).

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

'''

extract = EXTRACT.read_text(encoding="utf-8")
count = header_count(extract)
intro = INTRO_TEMPLATE.format(header_count=count)
OUT.write_text(intro + extract, encoding="utf-8")
print(f"Wrote {OUT} ({OUT.stat().st_size} bytes, {count} headers)")
