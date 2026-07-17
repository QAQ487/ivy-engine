# MagneticField Engine — User Guide

A practical guide for humans: how to open the project, build scenes, attach components, write scripts, and ship a playable build.

The repo ships a **minimal sample pack** (menu + playground) instead of a full game. Use those scenes as a starting point.

If you need every function signature, use [`ENGINE_API_REFERENCE.md`](ENGINE_API_REFERENCE.md).  
If you need JSON schemas and source maps for tools/agents, use [`ENGINE_MANUAL.md`](ENGINE_MANUAL.md).  
For a content-only external repo, see [`CONTENT_PROJECT.md`](CONTENT_PROJECT.md).

---

## Table of contents

1. [What you are working with](#1-what-you-are-working-with)
2. [Get running in 15 minutes](#2-get-running-in-15-minutes)
3. [Mental model](#3-mental-model)
4. [Editor tour](#4-editor-tour)
5. [Where content lives (`rom/`)](#5-where-content-lives-rom)
6. [Scenes and objects](#6-scenes-and-objects)
7. [Components (what to attach)](#7-components-what-to-attach)
8. [Prefabs](#8-prefabs)
9. [Writing gameplay scripts](#9-writing-gameplay-scripts)
10. [Physics and collision](#10-physics-and-collision)
11. [Cameras](#11-cameras)
12. [Look: lighting, skybox, shaders](#12-look-lighting-skybox-shaders)
13. [UI](#13-ui)
14. [Audio](#14-audio)
15. [VFX: effects, particles, billboards](#15-vfx-effects-particles-billboards)
16. [Input](#16-input)
17. [Play mode, hot reload, shipping](#17-play-mode-hot-reload-shipping)
18. [Common recipes](#18-common-recipes)
19. [Troubleshooting](#19-troubleshooting)
20. [Projects (New / Open / Save)](#20-projects-new--open--save)

---

## 1. What you are working with

**MagneticField Engine** is a Windows / Direct3D 11 game engine with:

- A **scene graph** of `GameObject` nodes
- **Components** serialized to JSON (transform, mesh, lights, colliders, scripts, …)
- An **ImGui editor** for placing content and pressing Play
- **C++ scripts** compiled into `GameScripts.dll` (hot-reloadable)
- A separate **runtime** executable for play without the editor

You do not write a new engine every time you make a game. You:

1. Put assets under `rom/`
2. Build scenes / prefabs (editor or JSON)
3. Write scripts in `game/scripts/`
4. Press Play in the editor, or run `Game_Runtime`

### The three Visual Studio projects

| Project | What it is |
|---------|------------|
| **Game_Project** | Editor + development app. This is what you run day to day. |
| **GameScripts** | Your game logic DLL. Rebuild this when you change scripts. |
| **Game_Runtime** | Player build without editor UI (`GAME_RUNTIME`). |

Open `Game_Project.sln` in Visual Studio 2022 (x64 Debug or Release).

---

## 2. Get running in 15 minutes

### Prerequisites

- Visual Studio 2022 with C++ desktop workload
- Windows 10/11 x64
- This repository checked out with `libraries/` present (FBX, Effekseer, ImGui, etc.)

### Build and run

1. Open `Game_Project.sln`.
2. Set configuration to **Debug | x64** (or Release | x64).
3. Build **Game_Project** (shaders compile as part of the project).
4. Build **GameScripts**.
5. Set **Game_Project** as the startup project and run it (F5).

You should get a window (design size **1920×1080**). The editor opens a **content project** (folder with `rom/` and `project.json`). Startup prefers:

1. Last entry in **Recent Projects** (if still valid)
2. Else the current working directory when it contains `rom/` (the sample next to the exe after build)
3. The scene to load comes from `project.json` → `lastOpenedScene` (fallback: `defaultScene` / `rom/scene/default_scene.txt`)

The in-repo sample ships [`project.json`](../project.json) pointing at `scene_sample_menu.json`. Change **File → Save Project** or **Project Settings → Default Scene** to update defaults.

See [§20 Projects](#20-projects-new--open--save) for New / Open / Save Project.

### Try the sample pack

1. Run **Game_Project**.
2. Press **F3** (or Play) on the sample menu.
3. Click **Play** to load `scene_sample_playground.json`.
4. Move with **WASD** / left stick. A blue box spins via `SampleSpinScript`.
5. Press **F3** again to stop Play mode in the editor.

### First keys to remember

| Key | Action |
|-----|--------|
| **F2** | Show / hide the ImGui editor |
| **F3** | Play / stop (also toolbar Play button) |
| **F1** | Toggle debug camera (Maya-style orbit / fly) |

When the editor is visible, the status line reminds you: *F2 toggle GUI | F3 play/stop*.

---

## 3. Mental model

Think in four layers:

```text
Scene JSON  →  GameObject tree  →  Components  →  Scripts (DLL)
```

### GameObject

Every thing in the world is a `GameObject`:

- Has a **name**, optional **tag** (from the project tag list — see [Tags](#tags))
- Has **position / rotation / scale** (legacy fields + `SceneTransform` component)
- Can have **children** (hierarchy)
- Can have a **rigidbody** and a **collider**
- Can hold any number of **components**

Inactive objects (`SetActive(false)`) skip update and draw.

### Tags

Tags are a **project-wide name list**, not only a fixed C++ enum.

- Built-in names (`NONE`, `PLAYER`, `ENEMY`, `GROUND`, …) keep stable indices that match `enum class Tag` in code.
- You can **add custom tags** in the inspector (**Add New**). They are appended after the builtins.
- The list is stored in `rom/tags.json` and reloads when you open a project or load a scene.
- **Deleting a custom tag** remaps objects in the open scene: that tag → `NONE`, and higher custom ids shift down. Built-ins cannot be deleted.
- Collision ignore lists use the same ids and are remapped the same way.

In scene JSON, `"tag"` is an integer index into this list (0 = `NONE`).

### Components

Components are small data + behavior packs attached to an object. Important ones:

| JSON name | Role |
|-----------|------|
| `SceneTransform` | Local position, rotation (degrees), scale |
| `visual` | FBX/OBJ model, animations, shader |
| `Material` | Color, roughness, metallic, toon params |
| `Collider` | Box / sphere / capsule / cylinder / mesh |
| `Script` | Your C++ class by name (`scriptClass`) |
| `Billboard` | Always-facing sprite in 3D |
| `Effect` | Effekseer effect on the node |
| `Camera` | Scene camera |
| `DirectionalLight` / `PointLight` / `SpotLight` | Lights |

Names are **case-sensitive**. Use exactly these strings in JSON.

### Scripts

Scripts are **not** engine code. They live in `game/scripts/`, compile to `GameScripts.dll`, and attach via a `Script` component:

```json
"Script": {
  "scriptClass": "SampleSpinScript"
}
```

On Play, the engine creates your class, deserializes properties, then calls:

1. `OnCreate(self)`
2. `OnUpdate(self, dt)` every frame
3. `OnDestroy(self)` when torn down

### Editor vs Play vs Runtime

| Mode | What happens |
|------|----------------|
| **Edit** | Place objects, tweak properties, save scenes. Scripts usually idle. |
| **Play (F3)** | Scene runs with physics, scripts, cameras. Stop restores edit state. |
| **Game_Runtime** | No editor panels; loads default scene and plays. |

---

## 4. Editor tour

Press **F2** to open the editor UI.

Typical panels and workflows:

1. **Scene hierarchy** — select objects, expand children.
2. **Inspector / properties** — edit transform, components, script fields, and **tags** (pick / add / delete custom).
3. **Project / prefab browser** — drag assets and prefabs into the scene (indexes the open project’s `rom/` and `game/scripts/`).
4. **Viewport** — 3D game view with a **fixed aspect ratio** (see below); transform gizmo and selection wireframe.
5. **Play toolbar** — Play / Stop (F3).
6. **UI edit mode** — switch edit mode to place canvas elements on screen.
7. **File menu** — New / Open / Save Project, Recent Projects, Save Scene, New Scene.

### Viewport aspect ratio

The editor game viewport keeps a **constant aspect** derived from the original 1920×1080 layout (fit-inside the available center area, letterboxed). Side panels fill the gutters.

Play mode, cameras, shadows, and UI hit-testing use that viewport rectangle’s width÷height — not the full window size. There is no separate “aspect” field in Render Settings; resizing the window grows or shrinks the letterboxed preview while preserving the ratio.

### Camera modes in the editor

The editor can drive different camera behaviors (third-person, scene camera node, intro). You can also use **F1** debug camera:

- **Orbit:** Alt + LMB orbit, Alt + MMB pan, Alt + RMB / wheel dolly
- **Fly:** RMB + WASD (+ Shift boost), mouse look

### Saving work

- **Save Scene** (Ctrl+S) — writes the current scene JSON under `rom/scene/`.
- **Save Project** (Ctrl+Shift+S) — saves the scene **and** writes `project.json` (last opened scene, default scene, UI zoom). Unsaved scene changes still warn before load/exit/project switch.

---

## 5. Where content lives (`rom/`)

| Path | Put here |
|------|----------|
| `project.json` | Project name, last/default scene, editor prefs (at project root) |
| `rom/scene/*.json` | Full scenes |
| `rom/scene/default_scene.txt` | Default / play startup scene file name (kept in sync with `project.json`) |
| `rom/tags.json` | Project tag name list (builtins + custom) |
| `rom/prefabs/*.json` | Reusable object templates |
| `rom/model/` | FBX / OBJ meshes and anims |
| `rom/texture/` | Albedo, UI, fonts, sky DDS, etc. |
| `rom/audio/` | WAV (and similar) for BGM / SFX |
| `rom/effect/` | Effekseer effect files |
| `rom/shader/` | Built shader binaries (from `shaders/`) |
| `rom/font/` | Font-related assets if used |
| `rom/editor/` | Editor UI prefs (e.g. zoom) for this project |

Paths in JSON are usually relative to the working directory, like:

```text
rom/model/object/Box_Green.fbx
rom/texture/UI/title/Main menu_bg.png
```

Keep assets inside `rom/` so editor, runtime, and deploy layouts stay consistent.

---

## 6. Scenes and objects

### Scene file shape (version 4)

A scene is a JSON document roughly like:

```json
{
  "version": 4,
  "settings": {
    "render": {
      "skyboxEnabled": true,
      "clearColor": { "r": 0.69, "g": 0.77, "b": 0.87, "a": 1.0 }
    }
  },
  "nodes": [
    {
      "name": "Sun",
      "tag": 0,
      "components": { },
      "children": []
    }
  ],
  "canvas": { }
}
```

- **`nodes`** — root GameObjects (and their child trees)
- **`settings.render`** — skybox, clear color, default shader, shadows, fog
- **`canvas`** — 2D UI for that scene

### Adding an object (recommended path)

1. Create or open a prefab (see [Prefabs](#8-prefabs)).
2. Place it from the prefab browser, **or** paste a node into `nodes` with a `prefabAssetPath`.
3. Adjust transform in the inspector.
4. Save the scene.
5. Press **F3** to test.

### Minimal hand-authored node

```json
{
  "name": "GreenBox",
  "tag": 0,
  "components": {
    "SceneTransform": {
      "posX": 0.0, "posY": 1.0, "posZ": 0.0,
      "rotX": 0.0, "rotY": 0.0, "rotZ": 0.0,
      "scaleX": 1.0, "scaleY": 1.0, "scaleZ": 1.0
    },
    "visual": {
      "model": "rom/model/object/Box_Green.fbx",
      "shader": "toon"
    }
  },
  "children": []
}
```

That object renders a green box with a toon shader and no gameplay until you add a `Script` or collider.

---

## 7. Components (what to attach)

### Transform — `SceneTransform`

Always attach this for scene objects you place in the editor.

- Position / rotation / scale are **local** to the parent.
- Rotation is **Euler degrees**.
- Play mode snapshots transform so Stop can restore it.

### Mesh — `visual`

| Field (JSON) | Meaning |
|--------------|---------|
| `model` | Main FBX/OBJ path |
| `proxy_model` | Lighter mesh for edit / paused view (optional) |
| `animate` | Play skeletal animation |
| `animations` | Map of clip name → animation FBX path |
| `active_clip` | Which clip is current |
| `shader` | Override: `standard`, `toon`, `rim`, … |

From scripts, play a clip with:

```cpp
auto* visual = ScriptApi_FindVisualComponent(self);
ScriptApi_Visual_PlayClip(visual, "Run");
```

### Look — `Material`

Optional PBR / toon tuning on top of the mesh: color, roughness, metallic, normal map, ramp, outline width, rim, etc. Prefer scene default shader for global look; use Material or `visual.shader` for per-object exceptions.

### Collision — `Collider`

Declarative collider for the object. Types (integer `colliderType`):

| Value | Shape |
|------:|-------|
| 0 | None |
| 1 | Box (`halfExtents`) |
| 2 | Sphere (`radius`) |
| 3 | Capsule (`radius`, `height`) |
| 4 | Mesh (from model, optional padding) |
| 5 | Cylinder (`radius`, `height`) |

Enable `showDebugCollider` while tuning.

### Gameplay — `Script`

```json
"Script": {
  "scriptClass": "MyScript",
  "properties": {
    "moveSpeed": 7.5
  }
}
```

`properties` overrides C++ defaults for that instance. Omit `properties` when defaults are fine.

### Lights

Add a light component to an empty (or dedicated) node with a transform:

- **DirectionalLight** — sun; can cast shadows
- **PointLight** — omni with range / attenuation
- **SpotLight** — cone; can cast shadows

The scene syncs these into the GPU lighting buffer each frame.

### Camera — `Camera`

Place a camera node with FOV / near / far. Activate it from scripts or editor camera mode by node name.

### Billboard / Effect

- **Billboard** — textured quad that faces the camera (or ground-aligned)
- **Effect** — Effekseer path on the node; play/stop from script

---

## 8. Prefabs

Prefabs are reusable GameObject templates under `rom/prefabs/`.

Example (`rom/prefabs/dummy_player.json`):

```json
{
  "name": "SampleProp",
  "components": {
    "SceneTransform": { "posY": 1.0, "scaleX": 1.0, "scaleY": 1.0, "scaleZ": 1.0 },
    "visual": {
      "model": "rom/model/object/Box_Green.fbx",
      "shader": "toon"
    },
    "Collider": {
      "colliderType": 2,
      "radius": 0.5,
      "showDebugCollider": true
    },
    "Script": {
      "scriptClass": "SampleSpinScript"
    }
  },
  "children": []
}
```

### Spawn at runtime

From a script:

```cpp
GameObject* enemy = ScriptApi_PrefabInstantiate(
    "rom/prefabs/enemy_ai.json",
    { 5.0f, 1.0f, 0.0f },   // position
    { 0.0f, 90.0f, 0.0f },  // rotation degrees
    nullptr,                // parent
    true);                  // start active

// Later:
ScriptApi_PrefabDestroy(enemy);
```

Runtime spawns hang under a spawn root managed by the engine. Prefer `ScriptApi_PrefabDestroy` over deleting pointers yourself.

### Workflow tip

Author once as a prefab → place many instances in scenes → override only what differs (transform, a few script properties).

---

## 9. Writing gameplay scripts

### Where files go

```text
game/scripts/
  sample/          # SamplePlayerScript, SampleSpinScript
  GameScripts/GameScripts.vcxproj
```

Add your `.h` / `.cpp` under `game/scripts/` (any folder), add them to the **GameScripts** project, then rebuild that project.

### Minimal script

**Header**

```cpp
#pragma once
#include "engine/scripting/Script.h"

class SpinScript : public Script {
public:
    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
};
```

(See also shipped samples: `SamplePlayerScript`, `SampleSpinScript` in `game/scripts/sample/`.)

**Source**

```cpp
#include "SpinScript.h"
#include "engine/scripting/script_dll_register.h"

void SpinScript::OnCreate(engine::GameObject& self) {
    (void)self;
}

void SpinScript::OnUpdate(engine::GameObject& self, float dt) {
    auto rot = self.GetRotation();
    rot.y += 90.0f * dt; // degrees per second
    self.SetRotation(rot);
}

REGISTER_SCRIPT(SpinScript)
```

Attach with `"scriptClass": "SpinScript"`. The registration macro name must match the class name string used in JSON.

### Editable fields (inspector + JSON)

```cpp
#include "engine/scripting/script_fields.h"

class SpinScript : public Script {
public:
#define SPIN_FIELDS(X) \
    X(float, degreesPerSecond, 90.0f) \
    X(bool, spinYaw, true)

    SPIN_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SPIN_FIELDS)

    void OnUpdate(engine::GameObject& self, float dt) override {
        auto rot = self.GetRotation();
        if (spinYaw)
            rot.y += degreesPerSecond * dt;
        self.SetRotation(rot);
    }
};
```

Then instances can override in JSON:

```json
"properties": { "degreesPerSecond": 180.0 }
```

### Calling the engine from scripts

Prefer **`ScriptApi_*`** helpers (`engine/scripting/script_runtime_api.h`). Examples:

```cpp
// Input
const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
if (api->keyLoggerIsTrigger(KK_SPACE)) { /* jump */ }
auto stick = api->controllerGetLeftStick(0);

// Camera
ScriptApi_CameraSetPosition({ 0, 5, -10 });
ScriptApi_CameraSetTarget({ 0, 1, 0 });

// UI
if (UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas()) {
    if (auto* text = dynamic_cast<UIText*>(
            ScriptApi_UICanvasGetElement(*canvas, "Score"))) {
        ScriptApi_UITextSetText(*text, "Score: 10");
    }
}

// Scene change
ScriptApi_SceneCanvasRequestSceneFile("rom/scene/scene_lobby.json", true);
```

Do **not** link scripts against the full editor. The host table is the supported boundary between DLL and exe.

### UI-only scripts

For canvas elements, inherit `UICanvasScript` and register with `REGISTER_UI_SCRIPT(MyUiScript)`. Attach via the UI element’s script class field in the canvas JSON / UI editor.

### Hot reload

With the editor running, rebuild **GameScripts**. The engine watches the DLL, queues a reload, and recreates script instances. Keep `OnCreate` / `OnDestroy` balanced so reloads do not leak resources.

---

## 10. Physics and collision

### Rigidbody (on every GameObject)

```cpp
auto& rb = self.GetRigidbody();
rb.SetUseGravity(true);
rb.SetMass(1.0f);
rb.SetIsKinematic(false);
rb.AddForce({ 0, 5, 0 }, ForceMode::Impulse);
```

Force modes: `Force`, `Acceleration`, `Impulse`, `VelocityChange`.

Kinematic bodies are not integrated by physics; move them yourself.

### Colliders

Prefer the **Collider** component in the editor. For runtime boxes from scripts:

```cpp
ScriptApi_CreateBoxCollider(self, { 1, 1, 1 }, { 0, 0.5f, 0 });
```

Collision callbacks live on `GameObject`:

```cpp
// In a GameObject subclass (legacy) or via your script’s physics proxy pattern
virtual void OnCollision(GameObject* other);
virtual void OnCollisionExit(GameObject* other);
```

Tags (`PLAYER`, `ENEMY`, custom names, …) and ignore lists filter who can collide. Built-in C++ constants (`TAG_PLAYER`, …) still match the first entries in `rom/tags.json`.

Triggers: set `IsTrigger` on the collider so you get overlap without resolution.

---

## 11. Cameras

### Global camera

The rendering camera is a global `Camera` (`GetCamera()` / `SetCamera()`). Scripts usually move it through ScriptApi:

```cpp
ScriptApi_CameraSetPosition(eye);
ScriptApi_CameraSetTarget(at);
ScriptApi_CameraSetUp({ 0, 1, 0 });
```

### Third-person follow

```cpp
api->thirdPersonCameraInitialize();
api->thirdPersonCameraSetTarget(&playerPos, &playerYawDegrees);
api->thirdPersonCameraSetMouseSensitivity(0.15f);
api->thirdPersonCameraSetPitchLimits(-40.0f, 70.0f);
```

Magnet modes (push / pull / orbit) are available for game-specific camera feel.

### Scene camera nodes

Place a `Camera` component on a node, then:

```cpp
ScriptApi_ActivateSceneCamera("CutsceneCam");
// or
ScriptApi_SetActiveCameraNodeName("CutsceneCam");
```

### Debug camera

**F1** — free look for level inspection. Does not replace your game camera permanently; use it while editing or debugging.

---

## 12. Look: lighting, skybox, shaders

### Scene render settings

Edit `settings.render` in the scene JSON (or via editor if exposed):

- Skybox on/off, cubemap path, tint, exposure, Y rotation
- Clear color
- Default shader for objects without override
- Shadow quality (Low / Medium / High)
- Height fog parameters

Reload skybox after changing the cubemap path (`RenderSettings_ReloadSkybox` / editor path).

### Per-object shaders

On `visual`:

```json
"shader": "toon"
```

Available variants include standard, hemisphere, rim, surface FX, toon (plus outline pass), and specialized pixel paths. See the shaders folder and `Shader3DVariant` in the API reference.

### Lights in the scene

1. Create an empty node.
2. Add `DirectionalLight` (or point/spot).
3. Aim it with `SceneTransform` rotation.
4. Enable `castShadows` on directional/spot when needed.

Keep light count within the engine limit (a handful of active lights — see `Lighting::MAX_LIGHTS`).

---

## 13. UI

UI is a **canvas** of 2D elements (design space **1920×1080**, scaled to the viewport).

### Element types

| Type | Use for |
|------|---------|
| `sprite` | Images, HUD icons, radial fills |
| `text` | Labels, scores |
| `button` | Clickable / navigable actions |
| `bar` | Health / progress |

Anchors: `TOP_LEFT`, `CENTER`, `BOTTOM_RIGHT`, and the other six compass points.

### Scene canvas

Each scene can embed a `canvas` object. See `rom/scene/scene_sample_menu.json` for a minimal menu.

From scripts:

```cpp
UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
ScriptApi_SceneCanvasShowCountdown(3);
ScriptApi_SceneCanvasSetTimerSeconds(90.0f);
ScriptApi_SceneCanvasRequestSceneFile("rom/scene/result.json", true);
```

### Buttons and scene changes

Buttons can request another scene file (and optionally auto-play). Register custom button actions in engine/UI setup when you need scripted callbacks beyond “load this scene”.

### UI edit mode

In the editor, switch to **UI** edit mode to select and drag canvas elements with the UI gizmo instead of the 3D transform gizmo.

---

## 14. Audio

Use the singleton `AudioManager`:

```cpp
auto& audio = AudioManager::Instance();

// Preload once
audio.LoadBGM("title", "rom/audio/bgm/title.wav");
audio.LoadSFX("hit", "rom/audio/sfx/hit.wav");
audio.Load3DSFX("explode", "rom/audio/sfx/explode.wav");

// Play
audio.PlayBGM("title", 0.8f, true);
audio.PlaySFX("hit", 1.0f);
audio.PlaySFX3D("explode", 1.0f); // after setting emitter

audio.SetListener(playerObject);           // 3D listener tracks object
audio.SetEmitter("explode", boomObject); // 3D emitter tracks object
audio.SetMasterVolume(1.0f);
```

Call `Update()` every frame (the game loop already does this in app code). Stop sounds before destroying tracked GameObjects.

---

## 15. VFX: effects, particles, billboards

### Effekseer (`Effect` component or `EffectObject`)

- Put `.efk` / `.efkefc` under `rom/effect/`
- Attach an **Effect** component, or spawn an `EffectObject`
- From scripts: `ScriptApi_Effect_Play(effectComponent)`

Low-level control exists in `EffectSystem::Play / Stop / SetLocation / …`.

### CPU particles

```cpp
engine::ParticleEmitterConfig cfg;
cfg.emissionRate = 40.0f;
cfg.startLifetimeMin = 0.4f;
cfg.startLifetimeMax = 0.8f;
cfg.texturePath = "rom/texture/fx/spark.png";

std::uint32_t id = ScriptApi_ParticleCreateEmitter(cfg, position);
ScriptApi_ParticleSetEmitterEmitting(id, true);
// …
ScriptApi_ParticleDestroyEmitter(id);
```

### Billboards

Immediate submit (one frame):

```cpp
ScriptApi_BillboardAdd(pos, { 1, 1 }, { 1, 1, 1, 1 }, srv);
```

Or attach a **Billboard** component for persistent setup.

---

## 16. Input

### Keyboard

Virtual-key style enum `KK_A`, `KK_SPACE`, `KK_ESCAPE`, …

```cpp
KeyLogger_IsTrigger(KK_E);   // pressed this frame
KeyLogger_IsPressed(KK_W);   // held
KeyLogger_IsRelease(KK_E);   // released this frame
KeyLogger_IsDoubleTap(KK_SPACE);
KeyLogger_IsLongPress(KK_F);
```

From DLL scripts, use the host table (`keyLoggerIsTrigger`, `keyboardGetState`) rather than assuming static linkage.

### Mouse

```cpp
Mouse_IsLeftDown();
Mouse_GetDelta(dx, dy);
Mouse_GetWheelDelta();
```

### Controller (XInput, pads 0–3)

```cpp
Controller_IsConnected(0);
Controller_IsTrigger(ControllerButton::A, 0);
Controller_GetLeftStick(0);   // [-1, 1]
Controller_GetRightTrigger(0); // [0, 1]
Controller_SetVibration(0.5f, 0.5f, 0);
```

---

## 17. Play mode, hot reload, shipping

### Day-to-day loop

1. Edit scene / prefab / script.
2. Rebuild **GameScripts** if C++ changed.
3. **F3** Play to test.
4. **F3** Stop, tweak, repeat.
5. Save scenes when happy.

### Default scene for Runtime

Runtime still boots from `rom/scene/default_scene.txt` next to the exe (file name of the scene JSON). In the editor, **Project Settings → Default Scene** and **Save Project** keep that file aligned with `project.json` → `defaultScene`.

### Shipping with Game_Runtime

1. Build **Game_Runtime** and **GameScripts** (Release | x64 recommended).
2. Ensure `rom/` and `game/scripts/bin/GameScripts.dll` sit next to the exe as your deploy layout expects.
3. For external content repos, follow [`CONTENT_PROJECT.md`](CONTENT_PROJECT.md) and `deploy\deploy.bat` — or create a project in the editor (**File → New Project**) which scaffolds a similar layout.

Runtime builds skip the ImGui editor; players never see F2 panels.

---

## 18. Common recipes

### “I want a spinning prop”

1. Place mesh with `visual` + `SceneTransform`.
2. Add `SpinScript` (section 9).
3. Play.

### “I want an enemy I can place many times”

1. Build `rom/prefabs/my_enemy.json` with visual, collider, and your AI `scriptClass`.
2. Instantiate from the editor browser or `ScriptApi_PrefabInstantiate`.
3. Tune `properties` per instance if needed.

### “I want a title menu that opens a level”

1. Author UI in the scene `canvas` (text + buttons), or start from `scene_sample_menu.json`.
2. Set button `onClick` to `LoadScene` with your playground scene and `"play": true`.
3. Set the project default scene (editor **Project Settings → Default Scene**, or `default_scene.txt` / `project.json`) to your menu scene for boot.

### “I want gravity and floor collision”

1. Floor: mesh + mesh or box collider, tag `GROUND` (`TAG_GROUND` in code).
2. Player: sphere/capsule collider + rigidbody with gravity.
3. Movement script applies horizontal force / velocity; let integration handle fall.

### “I want to change level from code”

```cpp
ScriptApi_SceneCanvasRequestSceneFile("rom/scene/scene_training.json", true);
```

### “I want a one-shot explosion VFX”

Use Effekseer via Effect component / `EffectObject::SpawnW`, or a short-lived particle emitter with `StopEmitter(..., true)`.

---

## 19. Troubleshooting

| Symptom | Things to check |
|---------|-----------------|
| Blank / wrong startup | `project.json` last/default scene, `rom/scene/default_scene.txt`, and that the JSON exists |
| Wrong project / missing assets | **File → Open Project** on the folder that contains `rom/`; cwd must be that project root |
| New project missing fonts/UI | New projects copy `rom/texture` (and shaders/fonts) from the editor; recreate if an older scaffold is empty |
| Script never runs | `REGISTER_SCRIPT`, matching `scriptClass`, GameScripts built, Play mode on |
| Model missing | Path under `rom/`, FBX SDK DLL beside exe, case of path |
| Collider wrong | Type/radius/center; enable `showDebugCollider`; mesh needs model |
| Tag missing / wrong id | `rom/tags.json`; custom tags only after builtins; remapping after delete |
| No hot reload | Rebuild GameScripts; editor must be the host that loaded the DLL |
| UI wrong scale | Design for 1920×1080 anchors; check letterboxed viewport vs full window |
| 3D audio silent / crash | Listener/emitter GameObject still alive; call Stop before destroy |
| Shadows ugly | Raise shadow quality in render settings; check light direction |

### Transform gotcha

The engine keeps **legacy** `m_position` / `m_rotation` / `m_scale` and the **`SceneTransform`** component. Editor and serializers sync them; if you write low-level code, call the sync helpers or always go through the same path you started with.

---

## 20. Projects (New / Open / Save)

A **content project** is a folder the editor opens as the working root:

```text
MyGame/
  project.json
  rom/          # scenes, assets, tags, shaders, …
  game/scripts/ # optional GameScripts sources
```

The editor calls `SetCurrentDirectory` on that folder so relative paths like `rom/model/...` keep working.

### File menu

| Command | What it does |
|---------|----------------|
| **New Project…** | Pick a parent folder, enter a name → scaffolds `rom/`, blank `scene_main.json`, copies engine shaders/fonts/textures, optional scripts template |
| **Open Project…** | Pick any folder that contains `rom/` (creates `project.json` if missing) |
| **Save Project** (Ctrl+Shift+S) | Saves the current scene + writes `project.json` (and syncs `default_scene.txt`) |
| **Recent Projects** | Reopen up to 12 recent roots (`%AppData%/GameEngine/recent_projects.json`) |

### `project.json` fields

```json
{
  "name": "MyGame",
  "version": 1,
  "lastOpenedScene": "scene_main.json",
  "defaultScene": "scene_main.json",
  "editor": {
    "uiZoomPercent": 100
  }
}
```

| Field | Meaning |
|-------|---------|
| `name` | Shown in the window title |
| `lastOpenedScene` | Scene file restored when the project opens |
| `defaultScene` | Play / runtime default (also written to `rom/scene/default_scene.txt`) |
| `editor.uiZoomPercent` | ImGui UI scale (100 / 125 / 150 / 175 / 200) |

### Sample vs your game

- The **engine repo** includes a sample project (`project.json` + `rom/` + `game/`). After build, that layout is copied next to `Game_Project.exe`.
- **File → New Project** creates a separate game folder anywhere on disk. Scripts can still target the engine via `MagneticField.Engine.props` (same idea as [`CONTENT_PROJECT.md`](CONTENT_PROJECT.md)).

---

## Where to go next

| Document | When to open it |
|----------|-----------------|
| **This guide** | Learning workflows |
| [`ENGINE_API_REFERENCE.md`](ENGINE_API_REFERENCE.md) | Looking up a specific function |
| [`ENGINE_MANUAL.md`](ENGINE_MANUAL.md) | JSON field names, registration rules, agent/source map |
| [`CONTENT_PROJECT.md`](CONTENT_PROJECT.md) | Splitting game content into another repo / deploy |
| `rom/prefabs/`, `rom/scene/` | Real examples to copy |
| `game/scripts/` | Real gameplay patterns |

Happy building.
