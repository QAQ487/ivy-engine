# Ivy Engine — Agent Guide

Single entry point for AI agents working **in this engine repository**. Humans should read [`docs/ENGINE_USER_GUIDE.md`](docs/ENGINE_USER_GUIDE.md) instead.

Ivy Engine is a from-scratch C++ / Direct3D 11 engine: an ImGui **editor** (`IvyEngine.exe`) and an editor-less **runtime host** (`IvyEngineRuntime.exe`) that load and play **content projects**. Game logic lives in a hot-reloadable `GameScripts.dll` built by each content project.

> Agents working in a separate **game (content) repo** do not read this file: they use the exported pack in [`docs/agent/`](docs/agent/README.md) (`tools/export_agent_docs.py --game-repo <path>`).

## Where to look

| Need | Go to |
|------|-------|
| Rules, vocabulary, schemas, source map | This file |
| Every public function / signature | [`docs/ENGINE_API_REFERENCE.md`](docs/ENGINE_API_REFERENCE.md) (generated §6 catalog) or the header itself |
| How a human uses the editor / ships a game | [`docs/ENGINE_USER_GUIDE.md`](docs/ENGINE_USER_GUIDE.md) |
| Debug named-pipe test bridge (protocol, verbs, checks) | [`docs/RUNTIME_TEST_BRIDGE.md`](docs/RUNTIME_TEST_BRIDGE.md) |
| Why a design is the way it is | [`docs/adr/`](docs/adr/) |
| Engine backlog | [`docs/TODO.md`](docs/TODO.md) |
| Game-repo agent pack (exported, drift-checked) | [`docs/agent/`](docs/agent/README.md) |
| Architecture diagram | [`docs/diagrams/ivy-engine-architecture.html`](docs/diagrams/ivy-engine-architecture.html) |

## Ground rules

- **Development status:** engine development is fully unfrozen (ADR 0007, 2026-10-20). Any engine system may change within the requested task scope. `engine-v1.0-lock` is a historical tag, not a restriction.
- **Compatibility still applies** unless the task explicitly changes it: existing content-project layout, scene/prefab JSON, and the `ScriptRuntimeApi` ABI.
- **`ScriptRuntimeApi` is append-only.** Current version: **48** (`kScriptRuntimeApiVersion` in `engine/scripting/script_runtime_api.cpp`). New host functions go at the end, bump the version, keep older `ScriptRuntimeApi_Get*` entry points working, and scripts must check the version before calling newer entries.
- **Engine vs game:** do not add game-specific logic to engine code. Game behavior belongs in content-project scripts/JSON. Use game files only as examples of how the engine is consumed.
- **JSON names are contracts.** Component type names and field names are case-sensitive; preserve them unless the user asks for a migration (then also keep the loader accepting the old form).
- **Never claim Windows verification from Linux.** This container cannot build MSVC / D3D11 or run named pipes. Report Linux checks as Linux checks; Windows Editor/Runtime acceptance is the owner's.
- **Keep docs in sync** (see [Doc maintenance](#doc-maintenance)).

## Vocabulary

Use these terms; avoid the listed synonyms.

| Term | Meaning | Avoid |
|------|---------|-------|
| **Content project** | A directory owning one game's `project.json`, `rom/` assets and scripts. The editor has at most one open. | project (unqualified), Game_Runtime, Game_Project |
| **Runtime host** | The editor-less exe that plays a content project (`IvyEngineRuntime.exe`; deployed as `<GameName>.exe`). | runtime project |
| **Playable package** | Folder with the runtime host, `project.json`, one project's `rom/` and its scripts, in a launchable layout. | build folder, export |
| **Play mode** | Editor playback of the loaded scene using the same components as runtime. **F2** = viewport Play (editor chrome stays), **F3** = fullscreen Play. | preview, simulate |
| **AudioManager** | Engine singleton owning the audio device and bus volumes. | audio mixer, sound system |
| **Bus** | Volume category: Master, BGM, SFX, Voice. | mixer group, channel |
| **Audio** | Scene component that plays one WAV on a GameObject, on one bus. | AudioSource, sound component |
| **AudioListener** | Scene component that is the 3D ear; lives on a camera. | listener object |
| **UI click sound** | Project-wide SFX for UIButton activation: `project.json` → `audio.uiClickSound`; survives scene loads. | per-button clip |
| **Visual** | Component binding a mesh path and clips to a GameObject (JSON `visual`). | ModelComponent, SkinnedMesh |
| **Clip** | A named animation a Visual can play. | animation (unqualified) |
| **Anim instance** | Per-Visual playback copy (own clip time, deformed vertices); the mesh asset is shared. | cloned model |
| **Skin** | A Visual's mesh bound to bones so a Clip can deform it (the bind, not playback). | skin animation, GPU/CPU skin as component |
| **Bind** | Rest pose of a Skin (bone and mesh matrices → `bindOffset`). Wrong Bind scatters posed vertices. | T-pose, deform |
| **ThirdPerson** | Play camera following a `TAG_PLAYER` target (magnet Push/Pull/Orbit); scripts call `ThirdPersonCamera_SetTarget`. | follow cam |
| **FirstPerson** | Play camera on a walkable rig (position + eye height); parallel to ThirdPerson (ADR 0006). | ThirdPerson with distance 0 |

## Build and verify

- **Toolchain:** Visual Studio 2026 (`PlatformToolset v145`), Windows SDK, `/std:c++20`, x64. Solution `IvyEngine.sln` contains only `IvyEngine` (editor) and `IvyEngineRuntime` (`GAME_RUNTIME`). `GameScripts.vcxproj` belongs to each content project.
- **Runtime library:** Debug `/MTd`, Release `/MT` (no VC++ redist). `_DEBUG` builds include the test bridge; Release compiles it out (`IVY_RUNTIME_TEST_BRIDGE_AVAILABLE` unset).
- **Shaders:** `shaders/*.hlsl` → `resources/shader/*.cso` (FxCompile; post-build copies). Content refers to them as `rom/shader/...`.

Linux-runnable checks (use what matches the change; none of these is a Windows pass):

| Area | Command |
|------|---------|
| Editor pure-logic units | `tools/run_editor_{console_log,docking_math,settings_categories,theme,ui_scale_math}_check.sh`, `tools/run_debug_ofstream_tls_check.sh` |
| Culling / LOD | `tools/run_{frustum,shadow,sprite}_cull_check.sh`, `tools/run_lod_check.sh`; `tools/run_anim_cull_check.sh`, `tools/run_light_select_check.sh`, `tools/run_fx_cull_check.sh`; `tests/{cull_settings,euler_math,third_person_move}_test.cpp` (build lines in each file header) |
| Content paths / hot reload | `tools/run_utf8_path_check.sh`, `tools/run_content_removal_policy_check.sh`, `g++ -std=c++17 -I. tools/content_path_check.cpp -o /tmp/cpc && /tmp/cpc` |
| Asset layout | `python3 tools/check_testbed_assets.py`, `python3 tools/check_engine_resource_paths.py`, `python3 tools/check_runtime_host_assets.py` |
| Test bridge (offline protocol) | `tools/run_runtime_bridge_offline_linux.sh` |
| Agent doc pack | `python3 tools/agent_docs_check.py`, `python3 -m unittest tools/agent_docs_check_test.py tools/export_agent_docs_check.py`, `tools/run_agent_docs_export_check.sh` (New Project export; diffs C++ vs Python output) |
| Other unit tests | `tests/*_test.cpp` (single-file `g++ -std=c++20 -I.` builds; see each file header) |

Windows-only harnesses: `tools/run_*.cmd`, `tools/run_*.ps1` (2D regression, collider components, content reload, scene hot-load, play-scene switch, bridge acceptance).

## Repository map

| Path | Contents |
|------|----------|
| `engine/core/` | Editor core `engine_gui.cpp` (play state, hotkeys, scene load gating), `engine_gui_runtime.cpp` (runtime shims), `content_path.h`, `utf8_path.h`, job system, timers, debug system |
| `engine/editor/` | Editor UI split by panel: `editor_menu_bar`, `editor_hierarchy_panel`, `editor_inspector_panel`, `editor_asset_panel`, `editor_console`, `editor_settings_windows`, `editor_docking`/`editor_layout`, `editor_theme`, `editor_toolbar`, `editor_start_screen`, gizmos, undo, `project_manager`, `playable_package_builder`, content monitor |
| `engine/ecs/` | `GameObject`, components, `ComponentFactory`, prefab serializer/runtime, tag registry, model cache |
| `engine/scene/` | Scene loader, runtime sync, render settings, execution policy (2D/3D), file monitor, asset preload |
| `engine/scripting/` | `Script`, `ScriptComponent`, `ScriptFactory`, DLL registration, `ScriptRuntimeApi`, `UICanvasScript` |
| `engine/rendering/` | D3D11 renderer/shaders/post, models, sprites, billboards, particles, Effekseer, lighting/shadows, sky/IBL, culling |
| `engine/physics/` | Custom 3D rigidbody + colliders; `2d/` Box2D adapter (ADR 0008) |
| `engine/ui/`, `engine/input/`, `engine/audio/` | Canvas UI, keyboard/mouse/XInput, XAudio2 |
| `engine/testing/` | Debug runtime test bridge |
| `src/app/` | `main.cpp` (WinMain, loop, debug hotkeys), `scene/game_scene.cpp`, cameras (scene/debug/third/first person, director), fades/transitions |
| `resources/` | Read-only engine defaults (shaders, fonts, branding, primitives, skybox/IBL), mirrored beside the exe |
| `testbed/` | In-repo content project (sample + bridge fixture scenes, `scenarios/`); has no `GameScripts` |
| `samples/scripts/` | Reference scripts. `SamplePlayerScript` and `SampleParticleEmitterScript` are compiled into the Debug editor and Debug runtime host (so the testbed player walks and the particle-culling scene emits without a `GameScripts` build); `SampleCheckpointScript` into the Debug runtime host only; the rest are not compiled in-repo |
| `external_content_template/` | Starter for an external content project (`IvyEngine.props`, `GameScripts.vcxproj`, `deploy/deploy.bat`) |
| `libraries/` | imgui (docking 1.92.5), imguizmo, nlohmann, fbxsdk, effekseer, directxtex, box2d |
| `tools/`, `tests/` | Checks, generators, bridge clients, Linux Win32 shim |

Main flow: `src/app/main.cpp` → initialize renderer/input/audio/effects → `SceneManager` → `GameScene` → `SceneLoader_LoadFromFile` → `GameObject` roots → components + scripts → draw passes (models, sprites, billboards, effects, UI).

Editor hotkeys (editor build only): F1 debug camera, F2 viewport Play/Stop, F3 fullscreen Play/Stop, F6 cursor toggle, F7 time-scale cycle, F8 job-system overlay, F9 RGB burst / F10 mosaic (PostFX showcase), F11 collision visualizer, F12 frame step.

## Content projects and paths

A content project is a directory with `rom/` and usually `project.json`; the editor `SetCurrentDirectory`s into it so `rom/...` paths stay relative.

**Path resolution** (`engine/core/content_path.h`): a safe `rom/<rest>` path reads the project file if present, else `<exe-dir>/resources/<rest>` (`ResolveRomForRead`). **Writes always go to the project** (`ResolveRomForWrite`), never into `resources/`. Unsafe (absolute / `..`) rom-shaped strings resolve empty. No NTFS junctions; New Project does not copy engine defaults. `resources/skybox/source/` is authoring input, excluded from the OutDir mirror and packages.

**Startup:** editor loads the first recent project that still has `rom/`; a Debug editor with none opens `testbed/` (Release does not). **File → Open Testbed** works in both. The engine repo root and exe output dirs are not content projects. `IvyEngineRuntime` launched from the engine OutDir (no `rom/scene` beside it) changes directory to `testbed/`. Editor startup scene: `project.json` `lastOpenedScene` → `defaultScene`. Runtime/package startup: `rom/scene/default_scene.txt` (synced by Save Project).

`project.json` (version 1):

| Field | Meaning |
|-------|---------|
| `name`, `version` | Display name (window title / package exe), schema `1` |
| `lastOpenedScene`, `defaultScene` | Scene file names; `defaultScene` mirrors `rom/scene/default_scene.txt` |
| `editor.uiZoomPercent` | UI Scale percent (100–200). Also written immediately to `rom/editor/ui_prefs.json` by Preferences; effective scale = UI Scale × monitor DPI, applied once |
| `editor.uiStylePreset` | `Trellis` / `Hollow Soft` (invalid → Trellis) |
| `editor.uiColorScheme` | `Ivy` / `Ivy Dark` / `Classic Dark` / `Neutral Gray` / `Light` (invalid → Ivy) |
| `physics.gravity` | Applied in editor Play **and** runtime boot (ADR 0004) |
| `audio.masterVolume` / `bgmVolume` / `sfxVolume` / `voiceVolume` | Bus levels, applied in editor and runtime (ADR 0001) |
| `audio.uiClickSound` | UIButton click WAV |
| `build.packAllAssets` | `true` = a package copies all of `rom/` (old behaviour). Default `false`: only referenced assets. Written only when set |
| `build.alwaysInclude` | `rom/` files, folders or name prefixes a package keeps even when nothing references them (assets a script loads by a name built at runtime). Written only when non-empty |

Other per-project editor state: `rom/editor/imgui_layout.ini` (dock layout), `rom/editor/ui_prefs.json`, `rom/tags.json`. Recent list: `%AppData%/GameEngine/recent_projects.json`.

**New Project agent docs:** `CreateProject` calls `agent_docs::ExportToContentProject` (`engine/editor/agent_docs_export.h`) to copy `docs/agent/` + `docs/schemas/` into `<project>/docs/ivy-engine/`, write `ENGINE_COMMIT`, and upsert the managed section (text: `tools/agent_docs_managed_section.md`, including the engine-boundary rules) into the project's `AGENTS.md` / `CLAUDE.md`. Failure only logs a warning. `tools/export_agent_docs.py` does the same for re-exports and also prunes stale files — keep the two in sync (`tools/run_agent_docs_export_check.sh` diffs them).

**Packaging** (ADR 0002): File → Build Current Project MSBuilds the runtime host + the project's GameScripts, then assembles `<GameName>.exe`, `libfbxsdk.dll`, `project.json`, the referenced part of `rom/` (below), `game/scripts/bin/GameScripts.dll`; overlays built `.cso`, adds referenced engine defaults (except the built-in textures below), cooks equirect `skyboxCubemap` PNGs into `rom/skybox/cache/`, and compresses the packaged copy of every `rom/` PNG/JPG/JPEG/BMP to a BC7 `.dds` (mips baked, UNORM, `engine/editor/texture_cook`) and removes the source image. Content keeps naming the original file: `TextureLoad` and `LoadTextureWIC_SRV` fall back from a missing image to the same-name `.dds` (`engine/core/texture_cook_policy.h`). Kept as source images: `rom/{shader,font,branding,skybox,editor}/`, a project's own `texture/consolab_ascii_512.png`, sizes that are not multiples of 4, and images with a same-name `.dds` already present. **Referenced assets only** (`engine/editor/package_assets`, pure logic, `tests/package_assets_test.cpp`): `rom/` is not copied wholesale. The scan starts at the default scene and follows `"rom/..."` strings in scene/prefab/material text and `project.json`, and in `GameScripts.dll` (ASCII, UTF-8 and ASCII-only UTF-16 strings); a string naming a folder or the start of file names keeps them; scene file names in JSON or in the DLL pull those scenes in; texture/model file names inside FBX/OBJ/MTL/Effekseer files are matched by file name. Always kept: `scene/default_scene.txt`, `tags.json`, `editor/ui_prefs.json`, `shader/`, `skybox/cache/`. Everything else unreferenced is left out and listed in the Build log; `project.json` `lastOpenedScene` does not count as a reference. Assets a script loads by a name it builds at runtime, and non-ASCII names in UTF-16 script strings, need `build.alwaysInclude` (or `build.packAllAssets`). `deploy.bat` and `package_2d_sample.ps1` still copy all of `rom/`. The content project is never modified. Engine-owned fill/text assets are **not** files: the fade fill is a generated 32×32 white texture (`src/app/rendering/fade.cpp`), and the debug text font (IBM Plex Mono, OFL) is compiled into the exe from `resources/font/IBMPlexMono-Regular.ttf` via `tools/embed_builtin_assets.py` → `engine/core/builtin_assets_data.h` (rerun the tool when the font changes; `tools/check_runtime_host_assets.py` fails when stale). The package ships its license as `licenses/IBMPlexMono-OFL.txt`. **`builtin:` namespace** (`engine/core/builtin_paths.h`): engine-owned assets compiled into the exe have `builtin:` paths so they cannot be mistaken for project files — `rom/...` always means a file in the content project (or the `resources/` fallback), `builtin:...` never touches the disk and cannot be overridden. They are `builtin:font/IBMPlexMono-Regular.ttf` (default UI font), `builtin:branding/ivy_logo.png` (boot screen) and `builtin:ibl/brdf_lut.png` (constant PBR split-sum BRDF LUT; `render.iblBrdfLut` defaults to it). Readers: `builtin_assets::FindTexture` (`TextureLoad` / `TextureValidateFile` / IBL loader / panorama IBL check) and `FindFont` (`UIFont_Get`). The older `rom/font/IBMPlexMono-Regular.ttf`, `rom/branding/ivy_logo.png` and `rom/skybox/ibl/brdf_lut.png` spellings still resolve to the same data when no file exists there, and are never copied into a package even when a scene names them; the files stay in `resources/` for the editor. Keep the `rom/` spellings in `builtin_assets.h`, `kBuiltinRomPaths` in `playable_package_builder.cpp` and `BUILTIN_ROM_PATHS` in `tools/materialize_engine_defaults.py` identical (`tools/check_runtime_host_assets.py`); `tools/check_testbed_assets.py` accepts only the `builtin:` paths listed in `builtin_paths.h`. The per-skybox `irradiance.dds` / `prefilter.dds` are content, not built in. `hal::DebugText` draws that font through the `UIFont` atlas (`UIFont_GetBuiltinMono`). The retired bitmap atlas `rom/texture/consolab_ascii_512.png` no longer exists; UI text defaults to the built-in IBM Plex Mono (`builtin:font/IBMPlexMono-Regular.ttf`; the editor's Font Path fields have a **Use engine built-in font** button because the Asset Browser cannot list it). `LemonSlice.ttf` was removed from `resources/font/`; UI text JSON that still names it or the retired atlas is read as the default UI font unless the project provides that file (`engine/core/legacy_asset_alias.h`). `deploy.bat` does the same copy without cooking and keeps the `IvyEngineRuntime.exe` name.

## Object model

`GameObject` (`engine/ecs/game_object.h`) is a component–entity scene node (not a sparse ECS): name, tag, hierarchy, owned components keyed by C++ type, legacy transform `m_position`/`m_rotation`/`m_scale`, legacy `m_rigidbody`, optional `m_collider`, ignore-collision tags, `m_prefabAssetPath`.

- **Dual transform:** legacy fields + `engine::Transform` (`SceneTransform`). Editor and serializers keep them in sync; low-level code must use `SyncLegacyTransformFromComponents()` / `SyncComponentsFromLegacyTransform()` or stay on one path.
- **`Update()` order:** return if inactive → `Component::Update` for each component → `LogicUpdate` → `IntegratePhysics` → children. `Reset()` calls `OnReset()` on components then recurses.
- **Tags** (`engine/ecs/tag_registry.h`, `rom/tags.json` = `{ "tags": [ ... ] }`): builtins `NONE, SHADOW, GROUND, OBSTACLES, WALL, PLAYER, ENEMY, BULLET, SCLAP` keep indices matching `enum class Tag` (`TAG_*`, `TAG_MAX` ends builtins); custom tags append. Names trimmed, unique case-insensitively; builtins cannot be removed; deleting a custom tag remaps open-scene objects and ignore lists (deleted → `NONE`, higher ids decrement) and saves. Scene JSON `"tag"` is the integer index.

**Component contract** (`engine/ecs/component.h`): implement `const char* GetTypeName() const`; optional `OnAttach`, `OnDetach`, `Update(double)`, `OnReset`, `SetParam`, `Serialize` (JSON string), `Deserialize`. `owner` is set by `AddComponent`/`AddComponentRaw`. Register with `REGISTER_COMPONENT(Type)` or `ComponentFactory::Instance().Register("Name", creator)`; the scene registry is `SceneLoader_RegisterComponentTypes()`.

## Scene and prefab JSON

Saved by `SceneLoader_SaveToFile`:

```json
{
  "version": 4,
  "settings": { "renderPath": "3d", "physicsMode": "3d", "render": {}, "camera": {}, "debugCamera": {} },
  "nodes": [
    { "name": "GameObject", "tag": 0, "prefabAssetPath": "rom/prefabs/example.json",
      "components": { "SceneTransform": {} }, "children": [] }
  ],
  "canvas": {}
}
```

- Node keys: `name` (default `"GameObject"`), `tag` (int), optional `prefabAssetPath`, `components` (type name → object), `children`.
- Prefab files saved by `PrefabSerializer::SaveToFile` use `{ "version": 4, "nodes": [ node ] }`; loading also accepts older bare-node files.
- `canvas` is passed to `SceneCanvas_LoadSceneCanvas`; absent → canvas cleared.

`settings` (parsed in `engine/scene/scene_loader.cpp` `PrepareSceneSettings`, policy in `scene_execution_policy.h`):

| Field | Values |
|-------|--------|
| `renderPath` | `"3d"` (default) / `"2d"` |
| `physicsMode` | 3D path: `"3d"` (default) / `"none"`. 2D path: `"2d"` / `"none"` (default `"none"`) |
| `camera.mode` | `"thirdPerson"`, `"firstPerson"`, default `"scene"` (→ `CameraMode::SceneCamera`). `Intro` exists in C++ only |
| `camera.node` | Preferred scene camera node name |
| `debugCamera` | `mode` (`"orbit"` default / `"fly"`), `enabledOnPlay`, `yawDeg`, `pitchDeg`, optional `position {x,y,z}` |
| `render.*` | `RenderSettingsData` in `engine/scene/render_settings.h`: `skyboxEnabled`, `skyboxCubemap` (DDS, or equirect PNG cooked to `rom/skybox/cache/`; width = 360°, height ≤ width/2), `skyboxVerticalFov` (0 = auto), `skyboxTint`, `skyboxExposure`, `skyboxRotationYDeg`, `clearColor`, `defaultShader`, shadow quality, fog, `frustumCullingEnabled`, `shadowCascadeCullingEnabled`, `spriteCullingEnabled` (all culling default `true`), `lodEnabled` (default `true`), `animCullingEnabled` (default `true`; animated Visuals nothing drew last frame skip pose evaluation), `billboardCullingEnabled` (default `true`; frustum-cull billboards and individual particles), `effectCullingEnabled` (default `true`) with `effectCullRadius` (default `20`, clamped 0.5–500; assumed radius of every Effekseer effect, times its scale), `lightSelectionEnabled` (default `true`; over 4 enabled lights, the most influential at the camera fill the slots instead of the first 4 in scene order), `lodBias` (default `1`, clamped 0.1–10; >1 = lower detail sooner), `shadowLodOffset` (default `0`, 0–3; shadow casters draw that many LOD levels lower than the camera pass), `cullDistanceScale` (default `1`, clamped 0.1–10; multiplies every `visual.cullDistance`, >1 = visible longer), `shadowDistance` (default `0` = Shadow Quality preset range; >0 overrides it, clamped 5–1000) |

### Registered component types

`SceneTransform`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `CylinderCollider`, `MeshCollider`, `Rigidbody2D`, `Collider2D`, `SpriteAnimator`, `SpriteRenderer`, `visual`, `Billboard`, `Material`, `Rigidbody`, `Script`, `Collider`, `Effect`, `DirectionalLight`, `PointLight`, `SpotLight`, `Camera`, `Audio`, `AudioListener`.

| Type | Source | JSON keys |
|------|--------|-----------|
| `SceneTransform` | `ecs/transform.h` | `posX/Y/Z`, `rotX/Y/Z` (Euler degrees, local), `scaleX/Y/Z` |
| `visual` | `ecs/visual_component.h` | `model`, `proxy_model` (edit/paused), `animate`, `active_clip`, `animations` (clip → FBX), `shader` (per-object override), `lods` (array of `{model, maxDistance}`; level 0 is `model`, `maxDistance` = camera distance that switches TO that level), `cullDistance` (0 = never) |
| `Billboard` | `ecs/billboard_component.h` | `texture`, `sizeX`, `sizeY`, `color {r,g,b,a}`, `rotation`, `groundAligned`, `enabled` |
| `Script` | `scripting/script_component.h` | `scriptClass`, optional `properties` (per-instance overrides) |
| `BoxCollider` … `MeshCollider` | `ecs/collider_component*.h` | Shape-locked; only that shape's fields. One 3D collider per GameObject |
| `Collider` (legacy generic) | `ecs/collider_component.h` | `colliderType` (0 none, 1 box, 2 sphere, 3 capsule, 4 mesh, 5 cylinder), `colliderCenter`, `halfExtents`, `radius`, `height`, `meshSizePadding`, `showDebugCollider`. Still loads; saving rewrites to the shape-specific type |
| `Rigidbody` | `ecs/rigidbody_component.h` | `useGravity`, `isKinematic`, `mass`, `drag`, `restitution`, `friction` (restores the legacy body used by `IntegratePhysics`) |
| `DirectionalLight` | `ecs/directional_light_component.h` | `enabled`, `color`, `intensity`, `specColor`, `specPower`, `castShadows` |
| `PointLight` | `ecs/point_light_component.h` | `enabled`, `color`, `intensity`, `specColor`, `specPower`, `range`, `attenuation` |
| `SpotLight` | `ecs/spot_light_component.h` | point-light keys + `innerAngleDeg`, `outerAngleDeg`, `castShadows` |
| `Audio` | `ecs/audio_component.h` | `clipPath`, `bus` (`BGM`/`SFX`/`VOICE`), `volume`, `loop`, `autoPlay`, `spatial`, `distanceScale` |
| `AudioListener` | `ecs/audio_listener_component.h` | none (`{}`); presence selects the listener |
| `Material`, `Effect`, `Camera`, 2D types | their headers in `engine/ecs/` | See each component's `Serialize()` |

When adding a component: register it in `SceneLoader_RegisterComponentTypes()`, add inspector + undo support, add it to this table and `docs/agent/scene-prefab.md`.

## Scripting

- Scripts inherit `Script` (`engine/scripting/Script.h`): `OnCreate(self)` once after `Deserialize`, `OnUpdate(self, dt)` per active frame, `OnDestroy(self)`, `OnTriggerEnter/Exit`, optional `GetRuntimePhysicsProxy`. `Update_IsParallelSafe()` returns `true` only after a parallel-safety audit (job system runs safe scripts in parallel; sequential fallback is behavior-identical). 2D scripts may also implement `Physics2DListener::OnPhysics2DEvent` (dispatched on the main thread after script work).
- Register once per `.cpp` with `REGISTER_SCRIPT(Class)` from `script_dll_register.h` (DLL export table under `SCRIPT_DLL_BUILD`/`_WINDLL`, else `ScriptFactory`). UI element scripts: `UICanvasScript` + `REGISTER_UI_SCRIPT` (`OnCreate/OnUpdate/OnDestroy/OnClick` on a `UIElement&`).
- Serialized fields: `X(type, name, default)` list + `SCRIPT_DECLARE_FIELD` / `SCRIPT_SERIALIZE_METHODS` (`script_fields.h`).
- Scripts reach the host only through `ScriptRuntimeApi` (`ScriptRuntimeApi_GetHost()`) and the `ScriptApi_*` wrappers in `script_runtime_api.h` — never link the editor. Adding a host function: append to the struct, fill the host table, add the wrapper, bump the version, update `docs/agent/` and regenerate the API reference.
- DLL reload: the editor watches `GameScripts.dll`, queues a reload, tears down and recreates script instances; keep `OnCreate`/`OnDestroy` balanced.

## Subsystem notes

- **Physics 3D:** custom rigidbody + Box/Sphere/Capsule/Cylinder/Mesh colliders, triggers (`IsTrigger`), raycasts, tag/object ignore lists. Kinematic bodies are not integrated.
- **2D** (ADR 0008, Box2D 3.1.1): world XY, +Y up, rotation about Z, `pixelsPerUnit`, pivot from bottom-left. `physicsMode: "2d"` creates the Box2D world (60 Hz accumulator; Box/Circle, Static/Dynamic/Kinematic, triggers, category/mask filters, raycast skips triggers by default). Dynamic bodies write back XY + Z rotation; move via velocity, kinematic via `MovePosition`; movable bodies must be roots (or under the identity spawn root) with uniform XY scale. Architecture rules: shared core must not depend on 2D physics; the 2D renderer must not depend on Visual/PBR/shadows/sky; reuse the D3D11 device (no second backend); branch 2D/3D only at a few scene init/update/draw entry points (no `is2D` scattered through objects); editor 2D view, camera projection, render path and physics mode are separate settings; no generic render graph / plugin system / second ECS.
- **Cameras:** `CameraMode` (`engine/core/engine_gui.h`): `ThirdPerson=0`, `SceneCamera=1`, `Intro=2`, `FirstPerson=3`; `CameraDirector` samples the active source. Editor game viewport is the largest centered 16:9 rect in the dock central node (`EngineGUI_GetGameViewportRect`, backbuffer pixels); projection, shadows, picking and UI hit-tests use it. Shipped runtime uses the full backbuffer.
- **Skinning** (ADR 0003): GPU vertex-shader skin must use the same formula as `SkinFromScratch` (kept as oracle); palette `meshCount * boneCount`; matrices stored as `FbxToXM`, transposed only in `Shader_SetSkinPalette`.
- **UI:** canvas design space 1920×1080; element types `sprite`, `text`, `button`, `bar`; anchors `TOP_LEFT`…`BOTTOM_RIGHT`. Optional `pivot {x,y}` (bottom-left origin like world sprites) in shared UI data; absent → legacy layout/text alignment. Pivot is sidecar data (does not change UI class layout or vtables). Buttons use builtin `LoadScene` / `ExitGame` actions or a `UICanvasScript::OnClick`.
- **Audio:** XAudio2/X3DAudio; components preferred for authored content; tracked GameObjects must outlive their emitters/listener.
- **Editor console:** `hal::DebugWriteA`/`dout` feed a thread-safe ring buffer shown in the dockable Console; engine logs route through `hal::DebugWriteA`.

### Hot reload internals

- **Scene JSON** (`engine/scene/scene_file_monitor.h`): editor polls the current scene file every 500 ms, applies after 300 ms stability; coalesces writes; ignores identical content and its own saves; paused during Play (latest version applied after Stop). Parse/validate/deserialize happen before teardown — failure keeps the old scene and turns the Scene JSON light red until content changes. Unsaved edits → keep-editor / load-disk modal. Standalone runtime does not monitor.
- **Content** (`engine/editor/content_file_monitor.h`): recursive watch of the project `rom/` (not `resources/`; `rom/editor/` editor state, `shader/` and dot-files are ignored) in Edit and both Play modes, 500 ms batch stability, periodic audit for missed notifications. Textures/fonts/cubemaps update in place (stable IDs/pointers); scene/prefab/UI/model/material/audio/effect/DLL changes go through transactional scene preparation (Play restarts the scene in the same presentation mode). Deleted assets still referenced or cached keep resources alive and set the Content light to Failed (`engine/editor/content_removal_policy.h`). Log: `content_hot_reload.log` (project root, truncated at 1 MiB); scene timing: `editor_scene_loading.log`.
- **UTF-8 paths:** carry content paths as UTF-8 via `PathFromUtf8` / `PathToUtf8` (`engine/core/utf8_path.h`). Never construct `std::filesystem::path` or call `absolute()` from a narrow UTF-8 `std::string`, and never call `path::string()` / `generic_string()` on user filenames — on Windows these decode as the ACP and mangle or throw on CJK names. Catch filesystem errors per change so one bad path cannot drop a batch.

## Doc maintenance

There are two hand-written docs: this file (agents) and `docs/ENGINE_USER_GUIDE.md` (humans). Do not add new top-level docs for a feature; extend the right section instead.

| When you change… | Update |
|------------------|--------|
| Editor UI / menus / hotkeys / workflows | `docs/ENGINE_USER_GUIDE.md` (+ hotkeys and repo map here) |
| Component types or JSON fields, scene/prefab schema | Component table here, user guide component section, `docs/agent/scene-prefab.md` |
| `ScriptRuntimeApi` / `ScriptApi_*` | `docs/agent/api/*.md` (`api:`/`mod:` markers must pass `tools/agent_docs_check.py`), version note here |
| Any public header | Regenerate `docs/ENGINE_API_REFERENCE.md` (`python3 tools/extract_api_headers.py`) |
| Game-repo agent rules (engine boundary) | `tools/agent_docs_managed_section.md` (used by both the Python export and New Project) |
| Test bridge protocol / verbs | `docs/RUNTIME_TEST_BRIDGE.md`, `docs/agent/testing-bridge.md`, schema in `docs/schemas/` |
| A significant, hard-to-reverse decision | New ADR in `docs/adr/` |
| Backlog items | `docs/TODO.md` |
| Build toolchain / repo layout / feature highlights | `README.md` + this file |

## Cloud & Git Branch Naming Rules
- In cloud environments, if the current branch was auto-created with a random suffix (e.g., `claude/task-name-a8f9b`), rename it locally to a clean, meaningful name before pushing: `git branch -m <type>/<short-kebab-case-description>` (allowed types: `feat`, `fix`, `refactor`, `chore`, `docs`).
- Never push branches containing random hashes or IDs unless explicitly restricted by environment permissions.
- Keep all branch names and Git commit messages strictly in English.

Delivery evidence (what was verified, on which platform) belongs in the PR description, not in docs.
