# Ivy Engine — User Guide

A practical guide for humans: how to open the project, build scenes, attach components, write scripts, and ship a playable build. This is the one hand-written doc for people; AI agents read [`AGENTS.md`](../AGENTS.md) instead.

The repo ships a **minimal sample pack** (the in-repo `testbed/` project) instead of a full game. Use those scenes as a starting point.

If you need every function signature, use [`ENGINE_API_REFERENCE.md`](ENGINE_API_REFERENCE.md).  
AI agents in a separate game repo: give them [`docs/agent/`](agent/README.md) (export with `tools/export_agent_docs.py`).

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
21. [External content projects](#21-external-content-projects)

---

## 1. What you are working with

**Ivy Engine** is a Windows / Direct3D 11 game engine with:

- A **scene graph** of `GameObject` nodes
- **Components** serialized to JSON (transform, mesh, lights, colliders, scripts, …)
- An **ImGui editor** for placing content and pressing Play
- **C++ scripts** compiled into `GameScripts.dll` (hot-reloadable)
- A separate **runtime** executable for play without the editor

You do not write a new engine every time you make a game. You:

1. Put assets under `rom/`
2. Build scenes / prefabs (editor or JSON)
3. Write scripts in `game/scripts/`
4. Press Play in the editor, or run `IvyEngineRuntime`

### The three Visual Studio projects

| Project | Where | What it is |
|---------|-------|------------|
| **IvyEngine** | `IvyEngine.sln` | Editor + development app. This is what you run day to day. |
| **IvyEngineRuntime** | `IvyEngine.sln` | Player build without editor UI (`GAME_RUNTIME`). |
| **GameScripts** | Your content project's `<Name>.sln` | Your game logic DLL. Rebuild this when you change scripts. |

Open `IvyEngine.sln` in Visual Studio 2026 (toolset `v145`, x64 Debug or Release).

---

## 2. Get running in 15 minutes

### Prerequisites

- Visual Studio 2026 with the C++ desktop workload (the projects use toolset `v145` and C++20)
- Windows 10/11 x64
- This repository checked out with `libraries/` present (FBX, Effekseer, ImGui, Box2D, etc.)

### Build and run

1. Open `IvyEngine.sln`.
2. Set configuration to **Debug | x64** (or Release | x64).
3. Build **IvyEngine** (shaders compile as part of the build).
4. Set **IvyEngine** as the startup project and run it (F5).
5. When you have a content project with scripts, build its **GameScripts** from that project's own `.sln` (see [§21](#21-external-content-projects)).

You should get a window (design size **1920×1080**). The editor opens a **content project** (folder with `rom/` and `project.json`). Startup prefers:

1. The first **Recent Projects** entry that still has a `rom/` folder
2. Otherwise, a **Debug** build opens the in-repo [`testbed/`](../testbed) project. Release does not. The build output folder also contains `rom/` and is not auto-opened.
3. The scene comes from that project's `project.json` → `lastOpenedScene` (fallback: `defaultScene` / `rom/scene/default_scene.txt`)

`testbed/project.json` points at `scene_sample_menu.json`. Use **File → Save Project** or **Edit → Project Settings → Scenes** to change defaults. See [§20 Projects](#20-projects-new--open--save).

### Try the sample pack

1. Run **IvyEngine**. If a recent external project opens instead, use **File → Open Testbed**. With no recent project, a Debug build opens `testbed/` on its own.
2. The startup scene is the sample menu inside `testbed/rom/scene/`. Other scenes there cover PBR, panorama sky, mesh LOD (`scene_lod.json`, walkable; `scene_lod_fly.json` for a script-free fly camera), 2D sprites, 2D physics and a top-down 2D sample.
3. Press **F2** (or the toolbar Play button) and click **Play** in the menu to load `scene_sample_playground.json`.
4. Move with **WASD** / left stick (the character always faces where the camera looks: **W** forward, **D** right, **S** back, **A** left), turn the camera and character with the **mouse** / right stick, **Space** to jump; the cursor is hidden while playing (**F6** toggles it). The testbed has no `GameScripts` of its own: script-driven behavior (for example the spinning box from `SampleSpinScript`) only runs when a script DLL that contains those classes is loaded. The Debug editor and Debug runtime host additionally compile in `SamplePlayerScript` (WASD / jump / third-person camera), so scenes with that player walk without any script build. Reference scripts live in `samples/scripts/`.
5. Press **F2** again to stop Play mode.

### Keys to remember

| Key | Action |
|-----|--------|
| **F2** | Play / stop inside the editor viewport (editor panels stay visible) |
| **F3** | Play / stop fullscreen (16:9 letterboxed in the whole window) |
| **F1** | Toggle debug camera (Maya-style orbit / fly) |
| **F6** | Show / hide the mouse cursor while playing |
| **F7** | Cycle time scale |
| **F8** | Job-system debug overlay |
| **F9** / **F10** | PostFX showcase: RGB burst / mosaic toggle |
| **F11** | Collision visualizer (3D and 2D colliders) |
| **F12** | Step one frame |
| **Ctrl+S** / **Ctrl+Shift+S** | Save Scene / Save Project |
| **Ctrl+Z** / **Ctrl+Y** | Undo / Redo |

F1 and F6–F12 are editor-build debug keys; a shipped game never sees them.

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
| `BoxCollider` / `SphereCollider` / `CapsuleCollider` / `CylinderCollider` / `MeshCollider` | 3D collision shape (one per object) |
| `Rigidbody` | Gravity, mass, drag, kinematic flag (JSON-authored physics body) |
| `Script` | Your C++ class by name (`scriptClass`) |
| `Billboard` | Always-facing sprite in 3D |
| `Effect` | Effekseer effect on the node |
| `Camera` | Scene camera |
| `Audio` | Play one WAV clip on this node (bus, loop, spatial 3D) |
| `AudioListener` | Mark this node as the 3D ear (usually on the active camera) |
| `DirectionalLight` / `PointLight` / `SpotLight` | Lights |
| `SpriteRenderer` / `SpriteAnimator` / `Rigidbody2D` / `Collider2D` | 2D sprites, sprite animation, 2D physics |

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
| **Play (F2 / F3)** | Scene runs with physics, scripts, cameras. Stop restores edit state. |
| **IvyEngineRuntime** | No editor panels; loads default scene and plays. |

---

## 4. Editor tour

The editor UI is always visible in Edit mode. Panels dock around a central game viewport.

Typical panels and workflows:

1. **Hierarchy** — select objects, expand children, drag to reparent, drop prefabs.
2. **Properties** — edit transform, components, script fields, and **tags** (pick / add / delete custom). **Add Component** is a searchable list that hides components the object already has.
3. **Asset Browser (project / prefab browser)** — indexes the open project’s `rom/` and `game/scripts/`. Folder tree + breadcrumb navigate; **Refresh** and **Search** filter the current folder. **Grid / List** toggles the right-hand view; in Grid, a **Tile** size slider (64–160) scales icons. Grid names wrap up to two lines and ellipsize when still too long; hover shows the full file name and `rom/`-relative path; the status line at the bottom shows the selected asset’s name and path. List view is one row per asset (icon + full name, single-line ellipsis only if the panel is narrower than the name). Drag-drop (`ASSET_PATH` / `ASSET_PREFAB`), double-click, and right-click menus (including **Set as > Skybox** for eligible cubemaps) work in both views. View mode and tile size are stored in `project.json` → `editor` and `rom/editor/ui_prefs.json`.
4. **Console** — engine log output (`dout` / debug writes) in a dockable panel, with selection and copy.
5. **Viewport** — game view in the docking central node (fixed **16:9** letterbox/pillarbox; see below); transform gizmo and selection wireframe.
6. **Toolbar** — Play / Stop (F2 viewport / F3 fullscreen), edit mode, and three reload lights on the right: **Scene JSON**, **Content**, **Script DLL** (see [Hot reload](#hot-reload)).
7. **UI edit mode** — switch edit mode to place canvas elements on screen.

### Menus

| Menu | Contents |
|------|----------|
| **File** | New Scene, Save Scene, Clear Scene · New / Open Project, Open Testbed, Recent Projects, Close Project, Save Project · Build Current Project · Exit |
| **Edit** | Undo / Redo · **Project Settings…** · **Preferences…** |
| **GameObject** | Create Empty, 3D Object (primitives), Camera / Debug Camera, Light (Directional / Point / Spot), UI (Button / Text / Sprite / Bar) |
| **View** | Show Grid, Show Gizmos, Show UI Overlay, 2D View, Show FPS When Playing, Collision Visualizer (F11) |
| **Window** | Hierarchy, Properties, Asset Browser, Console, Project Settings, Preferences, PostFX Showcase, Reset Layout |

**Project Settings** (saved with the project) has pages **Rendering** (skybox, fog, shadows, culling, 2D scene / physics toggles), **Physics** (gravity), **Audio** (bus volumes, UI click sound), **Scenes** (default scene) and **Build** (playable package output folder and configuration).

**Preferences** (per project, saved immediately to `rom/editor/ui_prefs.json`) has **Appearance**: UI Scale (100–200 %, multiplied once by the monitor DPI), UI Style (`Trellis` / `Hollow Soft`) and Color Scheme (`Ivy`, `Ivy Dark`, `Classic Dark`, `Neutral Gray`, `Light`).

### Viewport aspect ratio

The editor game viewport is the largest centered **16:9** rectangle inside the ImGui docking central node (passthrough: the D3D scene shows through the game rect; panels dock around the node). Unused gutters are solid black letterbox/pillarbox bars — the image is never stretched. There is no aspect menu or render-setting toggle.

Play mode uses the same 16:9 contract (F2 inside the central node; F3 letterboxed in the full window). Cameras, shadows, picking, gizmos, and UI hit-testing use that rectangle’s width÷height. The shipped runtime (non-editor) still uses the full backbuffer.

The dock layout persists per content project at `rom/editor/imgui_layout.ini`. **Window → Reset Layout** restores the default: Hierarchy left, Properties right, Asset Browser and Console tabbed at the bottom (Asset Browser in front).

### Camera modes in the editor

The editor can drive different camera behaviors (third-person, scene camera node, intro). You can also use **F1** debug camera:

- **Orbit:** Alt + LMB orbit, Alt + MMB pan, Alt + RMB / wheel dolly
- **Fly:** RMB + WASD (+ Shift boost), mouse look

### Saving work

- **Save Scene** (Ctrl+S) — writes the current scene JSON under `rom/scene/`.
- **Save Project** (Ctrl+Shift+S) — saves the scene **and** writes `project.json` (last opened scene, default scene, UI scale / style preset / color scheme, Asset Browser view mode / tile size). Unsaved scene changes still warn before load/exit/project switch. Asset Browser and appearance prefs also update `rom/editor/ui_prefs.json` immediately when changed.

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
| `rom/font/` | Font-related assets if used |
| `rom/editor/` | Editor dock layout and UI prefs for this project (generated) |
| `rom/skybox/cache/` | Cooked panorama skyboxes / IBL (generated; do not edit) |

Paths in JSON are relative to the project root, like:

```text
rom/model/object/Box_Green.fbx
rom/texture/UI/title/Main menu_bg.png
```

**Engine defaults** — shaders, editor and UI fonts, branding, primitive models, the default skybox and IBL — live in the engine repo's `resources/` folder (copied beside the exe). When a `rom/<path>` file does not exist in your project, the engine reads `resources/<path>` instead. So `rom/shader/...` or `rom/font/...` work without copying anything; put a file at the same `rom/` path in your project to override a default. The engine **never writes** into `resources/`: scenes, prefs, tags and new assets always go to your project's `rom/`.

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
5. Press **F2** (viewport) or **F3** (fullscreen) to test.

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
| `lods` | Optional list of `{model, maxDistance}`. Level 0 is `model`; each row switches to that mesh at its camera distance (Play / runtime only) |
| `cullDistance` | Do not draw beyond this camera distance (`0` = never) |

**LOD:** in the Inspector's Visual section use **Add LOD** and drag an FBX/OBJ onto each row; rows stay sorted by distance. Shadows use the same level as the camera pass, levels are ignored while **Animate** is on, and a missing LOD mesh falls back to the base mesh. Keep LOD meshes at the same pivot and scale as the base mesh. Turn **Mesh LOD (3D)** off in Rendering for A/B, or lower **LOD Bias** to keep detail further out. Pick switch distances from the object size (distance = size / (0.83 x screen coverage) at a 45 degree field of view), not one fixed number for every object. Open `scene_lod.json` in the testbed to try it: a walkable pine and boulder field (meshes from `tools/gen_lod_test_assets.py`) with a side-by-side gallery of the three levels in front of the spawn. Walking uses `SamplePlayerScript`, compiled into the Debug editor and Debug runtime host; `scene_lod_fly.json` is the same field with a free-fly camera and needs no script.

From scripts, play a clip with:

```cpp
auto* visual = ScriptApi_FindVisualComponent(self);
ScriptApi_Visual_PlayClip(visual, "Run");
```

### Look — `Material`

Optional PBR / toon tuning on top of the mesh: color, roughness, metallic, normal map, ramp, outline width, rim, etc. Prefer scene default shader for global look; use Material or `visual.shader` for per-object exceptions.

### Collision — `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `CylinderCollider`, `MeshCollider`

Each shape is its own component and only shows that shape's fields (box half extents, radius, height, mesh padding, center offset, trigger). An object can have **one** 3D collider. Enable **Show Debug Collider** (or press **F11** for all colliders) while tuning.

Older scenes may still contain a generic `Collider` with an integer `colliderType` (0 none, 1 box, 2 sphere, 3 capsule, 4 mesh, 5 cylinder). It still loads; saving the scene rewrites it as the matching shape component. Engine builds older than this change cannot read the new names.

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

### 2D — `SpriteRenderer`, `SpriteAnimator`, `Rigidbody2D`, `Collider2D`

2D games use the same content project, scenes, scripts, UI, audio and packaging as 3D.

1. Turn on **2D Scene** in Project Settings → Rendering (`settings.renderPath: "2d"`). The 2D path skips models, shadows, sky, 3D particles, Effekseer and PostFX. Optionally turn on **2D Physics** (`physicsMode: "2d"`, Box2D).
2. Add a **Camera** with **Orthographic** on and a View Height (e.g. 10), placed at Z = -10 looking down +Z with zero rotation. Select it as the Play scene camera.
3. Add **Sprite Renderer** to objects: texture path, source rect, pivot (from bottom-left), Pixels Per Unit (world size = source pixels ÷ PPU), tint, flip, Sorting Layer then Order In Layer.
4. **Sprite Animator**: named clips with per-frame XYWH, FPS, loop, default clip, auto play. **Sheet Metadata JSON** imports `image` + `frames[].x/y/w/h`. Animation only advances in Play; Stop restores the authored state.
5. **Rigidbody2D** (Static / Dynamic / Kinematic, gravity scale defaults to 0 for top-down) + **Collider2D** (Box / Circle, trigger, category / mask bits). Move dynamic bodies by velocity, kinematic bodies with `MovePosition`. Movable bodies must be root objects with uniform XY scale; scale and shape cannot change during Play.

Editor helpers: **View → 2D View** (orthographic XY editing; middle-drag or Alt+LMB pan, wheel zoom, **F** frames the selection), XY-locked 2D gizmo, rectangle picking that respects sorting, drag a PNG/JPG/BMP from the Asset Browser into the viewport to create a sprite at Z = 0, and **New Project → 2D Project** for a starter scene. Sample scenes: `testbed/rom/scene/scene_sprite_2d.json`, `scene_physics_2d.json`, `scene_topdown_2d.json`.

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
  GameScripts/GameScripts.vcxproj
  <your folders>/  # e.g. player/, enemies/ — copy from samples/scripts/ to start
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

(See also the reference scripts in the engine's `samples/scripts/`: `SamplePlayerScript`, `SampleSpinScript`, `SampleTriggerLogScript`, `SampleCheckpointScript`, `SamplePhysics2DPlayerScript`. Copy them into your project's `game/scripts/` to use them.)

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

### Script reload

With the editor running, rebuild **GameScripts**. The engine watches the DLL, queues a reload, and recreates script instances. Keep `OnCreate` / `OnDestroy` balanced so reloads do not leak resources. See [§17 Hot reload](#hot-reload) for scenes and assets.

---

## 10. Physics and collision

### Rigidbody

Every `GameObject` still owns a legacy `GetRigidbody()` body used at runtime. Prefer a **Rigidbody** component in JSON for authored settings:

```json
"Rigidbody": {
  "useGravity": true,
  "isKinematic": false,
  "mass": 1.0,
  "drag": 0.05,
  "restitution": 0.0,
  "friction": 0.0
}
```

From scripts you can also tune the live body:

```cpp
auto& rb = self.GetRigidbody();
rb.SetUseGravity(true);
rb.AddForce({ 0, 5, 0 }, ForceMode::Impulse);
```

Force modes: `Force`, `Acceleration`, `Impulse`, `VelocityChange`. Kinematic bodies are not integrated; move them yourself.

Project gravity defaults come from `project.json` → `physics.gravity` (Project Settings → Gravity).

### Colliders

Prefer a shape collider component (`BoxCollider`, …) in the editor. For runtime boxes from scripts:

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

Edit these in **Edit → Project Settings → Rendering** (saved into the current scene's `settings.render`), or directly in the scene JSON:

- Skybox on/off, cubemap path, tint, exposure, Y rotation
- `skyboxCubemap` may be a DDS cubemap or one equirectangular PNG. Width is full 360°. Exactly 2:1 (180° vertical) keeps the previous face rule (`height/2` power of two). Shorter images (height ≤ width/2) are placed by vertical coverage `360*height/width` centered on the horizon with edge-row clamp; optional `skyboxVerticalFov` (degrees, clamped to (0,180], `0` = auto) overrides that. Height > width/2, unloadable, or unsafe paths fail with a clear log line in `content_hot_reload.log` and a **Panorama failed** notice in the Rendering page (not a silent clear-color sky). Face size for non-2:1 is the largest power of two ≤ `width/4` (max 8192×4096 / face 2048). Drag a `.png` / `.dds` from the Asset Browser onto the path field, or right-click the asset → **Set as > Skybox**. A PNG is cooked once into `rom/skybox/cache/` (FOV is part of the stamp). The image center is +Z, the top is toward +Y, and u increases toward +X.
- IBL for a PNG sky is rebaked (irradiance + prefilter). `brdf_lut.png` stays the shared LUT. If the bake cannot run, the Rendering page shows **IBL not matching sky** instead of keeping the previous environment quietly.
- Clear color
- Default shader for objects without override
- Shadow quality (Low / Medium / High)
- Height fog parameters
- `frustumCullingEnabled` (default `true`) — skip 3D mesh draws fully outside the **main** camera frustum. Does not gate shadow depth passes. Toggle off in the Rendering page for A/B. Counters (considered / culled / drawn) show in the Rendering page, the play FPS overlay, and collision-viz debug text.
- `shadowCascadeCullingEnabled` (default `true`) — per cascade, skip shadow casters whose world AABB is outside that cascade's light-space ortho volume. Near plane is open toward the light (casters between light and cascade still cast). Off-screen casters that still project into a cascade keep their shadows. Counters: frame totals + per-cascade in the Rendering page; frame totals on FPS / collision-viz overlays and bridge `perf.shadowCull`.
- `spriteCullingEnabled` (default `true`) — skip 2D sprite draws whose world AABB (size, pivot, rotation, scale) is fully outside the orthographic camera view rect, with a small margin against edge popping. Does not affect animation, physics, or scripts. Screen-space UI is untouched. Perspective cameras leave sprites unculled. Counters (considered / culled / drawn) show in the Rendering page, FPS overlay `2CULL`, collision-viz, and bridge `perf.spriteCull`.
- `lodEnabled` (default `true`) and `lodBias` (default `1`, 0.1–10) — distance-based mesh LOD from `visual.lods` plus `visual.cullDistance`, Play / runtime only. Bias above 1 switches to lower detail sooner. Counters (considered / culled / drawn per level) show in the Rendering page, FPS overlay `LOD`, collision-viz, and bridge `perf.lod`.
- `shadowLodOffset` (default `0`, 0–3) — **Shadow LOD Offset** in Rendering: shadow casters draw that many LOD levels lower than the camera pass, so distant or heavy meshes cost less in the shadow maps. Shadow outlines get slightly coarser; start with 1. Try it in `scene_lod_fly.json`: the forced LOD0 gallery objects cast a LOD3-shaped shadow at offset 3.
- `animCullingEnabled` (default `true`) — **Off-screen Animation Culling (3D)** in Rendering. An animated Visual that neither the main camera nor any shadow cascade drew last frame skips bone sampling, skinning and palette upload; its clip time still advances so the pose is in phase when it returns (the first frame back can show the old pose for one frame). Scripts that read a bone of an off-screen character (`TryGetBoneLocalMatrix`) see the last evaluated pose; turn the option off if gameplay depends on it. Counters: Rendering page, FPS overlay `ANIM u/s`, collision-viz, bridge `perf.anim`. Try it with `scene_anim_cull_fly.json` in the testbed.
- `lightSelectionEnabled` (default `true`) — **Light Selection (3D)** in Rendering. Only 4 lights reach the GPU. When a scene has more enabled lights, directional lights keep their slot and point / spot lights are ranked by how much they matter at the camera (brightness × range² / (range² + distance²)); a light that was in last frame gets a small bonus so borderline lights do not flicker. Scenes with 4 lights or fewer behave exactly as before. The Rendering page shows `lights enabled / used`. Turn it off to get the old first-4-in-scene-order behaviour. Try it with `scene_light_select_fly.json`: 12 coloured point lights in a row — fly along it and the 3 nearest light up.
- `billboardCullingEnabled` (default `true`) — **Billboard / Particle Culling (3D)** in Rendering. Billboards and each individual particle whose quad is fully outside the camera frustum are not submitted (simulation, sorting inputs and scripts are unaffected; picture identical). Counters in the Rendering page. Try it with `scene_billboard_cull_fly.json` (160 billboards around the camera; turn the option off and the picture must not change, only the culled count) and, for particles, `scene_particle_cull_fly.json` (40 emitters on three rings, driven by `SampleParticleEmitterScript`, which is compiled into the Debug editor and Debug runtime host only — a Release build shows no particles).
- `effectCullingEnabled` (default `true`) and `effectCullRadius` (default `20` m, 0.5–500) — **Effect Culling (3D)**. Effekseer effects have no authored bounds, so each effect counts as a sphere of `effectCullRadius` × its scale around its position; outside the frustum it is not drawn but keeps simulating. Increase the radius for effects that reach far from their origin (long beams, big explosions), otherwise they can vanish when their origin leaves the screen.
- `cullDistanceScale` (default `1`, 0.1–10) — multiplies every object's `cullDistance`, so one slider raises or lowers how far everything stays visible (bigger = visible longer). Objects with no `cullDistance` are never culled whatever the scale. Like `cullDistance`, it only applies while **Mesh LOD (3D)** is on.
- `shadowDistance` (default `0`, 5–1000) — how far from the camera shadows are drawn. `0` keeps the range of the **Shadow Quality** preset (Low 40 / Medium 70 / High 90); a positive value replaces it. This is the main shadow cost knob: the cascades are fitted over this range, so a shorter range means sharper shadows near the camera and fewer casters. Shadow Distance is shown as "Preset" while it is 0.

Reload skybox after changing the cubemap path (`RenderSettings_ReloadSkybox` / editor path). Manual check scene: `testbed/rom/scene/scene_panorama_sky.json`.

Existing `rom/skybox/MySkybox.dds` scenes stay on that DDS. Six-face PNGs under `resources/skybox/source/` and `run.bat` remain the offline `texassemble` fallback.

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

### Anchor, Position and Pivot

UI Text and Button **Font Path** accepts a `.ttf` / `.otf` (drag one from the Asset Browser) or a PNG atlas. The engine's default UI font is built into the exe, so it is not in your project's `rom/` and the Asset Browser does not list it: click **Use engine built-in font** under the Font Path field (it fills in `builtin:font/IBMPlexMono-Regular.ttf`, a path that never refers to a file in your project). New Text and Buttons already start with it.

UI Sprite, Button, Bar, Text and world Sprites share a **Pivot** dropdown: Center, Top Left, Top Center, Top Right, Middle Left, Middle Right, Bottom Left, Bottom Center, Bottom Right, Custom.

- **Anchor** — the reference point on the canvas.
- **Position** — offset of the pivot from the anchor (UI coordinates: X right, Y down).
- **Pivot** — the element's own origin, using the same convention as world sprites: bottom-left `(0,0)`, top-left `(0,1)`, center `(0.5,0.5)`.

Example: Anchor = Center, Position = (0,0), Pivot = Center centers the element. Switching Pivot to Top Left puts the element's top-left corner at the canvas center; Position is kept, so the element moves relative to its origin.

The UI gizmo's move arrows sit on the pivot; scale handles sit on the bounding-box corners and compensate Position while dragging. Rotated UI Sprites rotate around the pivot; resize them with Width / Height in Properties. For Text, the pivot applies to the whole text block (including multi-line height) and replaces the old Alignment option. A Bar's pivot uses the bar body; its label can still show above it.

Compatibility: scenes saved without a `pivot` field keep their original layout and text alignment. Only an explicitly set pivot (`pivot: {x, y}` in the element data) uses the new layout. Undo / Redo and scene saving work as usual.

---

## 14. Audio

### Scene components (preferred for authored content)

Attach an **Audio** component for a clip on a node:

```json
"Audio": {
  "clipPath": "rom/audio/sfx/hit.wav",
  "bus": "SFX",
  "volume": 1.0,
  "loop": false,
  "autoPlay": true,
  "spatial": false,
  "distanceScale": 20.0
}
```

- `bus`: `BGM`, `SFX`, or `VOICE`
- `spatial`: when true, position follows the node's transform for 3D attenuation
- Put **AudioListener** on the active camera (or player) so spatial clips pan correctly

Project-wide bus volumes and the UI click sound live in `project.json` → `audio` (see [§20](#20-projects-new--open--save)). Runtime and editor both apply those values at boot.

### AudioManager (scripts and one-offs)

For imperative playback from C++ scripts, use the singleton `AudioManager`:

```cpp
auto& audio = AudioManager::Instance();
audio.LoadSFX("hit", "rom/audio/sfx/hit.wav");
audio.PlaySFX("hit", 1.0f);
audio.SetListener(playerObject);
audio.SetMasterVolume(1.0f);
```

Call `Update()` every frame (the game loop already does this). Stop sounds before destroying tracked GameObjects.

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
3. **F2** / **F3** Play to test.
4. Press the same key to Stop, tweak, repeat.
5. Save scenes when happy.

### Hot reload

While the editor is open, most content changes apply without restarting. The three lights on the right of the toolbar show the state of each watcher — hover a light for details:

| Light | Watches |
|-------|---------|
| **Scene JSON** | The currently loaded scene file |
| **Content** | Everything under the project's `rom/` |
| **Script DLL** | `GameScripts.dll` |

Colors: **green** watching (briefly brighter after a successful reload), **amber** waiting / processing, **red** a reload failed, **grey** paused or inactive.

- **Scripts** — rebuild **GameScripts**; the editor notices the new DLL, queues a reload and recreates script instances. Keep `OnCreate` / `OnDestroy` balanced so reloads do not leak. The editor never compiles source files itself.
- **Scene JSON edited outside the editor** — checked about every 0.5 s and applied once the file has been stable for 0.3 s. Your own saves and identical content are ignored. Changes wait while Play is running and apply after Stop. If you have unsaved edits, a dialog asks: **Load disk version (discard Editor changes)** or **Keep Editor changes**. The editor camera and edit mode are kept; selection and Undo history are cleared. Invalid JSON keeps the current scene and turns the light red until the file changes again.
- **Textures, fonts, cubemaps** — replaced in place, even during Play. A panorama PNG used as `skyboxCubemap` is re-cooked into `rom/skybox/cache/`.
- **Models, prefabs, UI data, materials, audio, effects** — the scene is rebuilt from disk. In Play, the current scene restarts in the same viewport / fullscreen mode, so gameplay state resets. Broken files keep the previous scene.
- **Deleting a file** that the scene still uses (or that is still loaded) keeps the loaded copy alive and turns **Content** red (`Content removed; existing resources retained`). References are not rewritten for you. Deleting an unused file is just logged.
- **Not hot-reloaded:** files under the engine's `resources/` (restart the editor), shaders, engine code, and the standalone runtime (it never watches files).

Logs: `content_hot_reload.log` (project root, trimmed at 1 MiB) and `editor_scene_loading.log`. File names with non-ASCII characters (e.g. `hp_fill - 複製.png`) are supported.

### Default scene for Runtime

The runtime boots from `rom/scene/default_scene.txt` (the scene JSON file name). In the editor, **Edit → Project Settings → Scenes** and **Save Project** keep that file aligned with `project.json` → `defaultScene`.

### Shipping a playable package

**File → Build Current Project** (output folder and configuration in Project Settings → Build) is the recommended path. It:

1. Builds **IvyEngineRuntime** and your project's **GameScripts**.
2. Copies the runtime host renamed to `<GameName>.exe`, plus `libfbxsdk.dll`, `project.json`, the part of your `rom/` the game actually uses, and `game/scripts/bin/GameScripts.dll` into the chosen folder. "Used" means reachable from the default scene: files named in scenes, prefabs, materials and `project.json`; files named inside `GameScripts.dll` strings; scenes named by buttons or code; textures named inside your FBX / OBJ / Effekseer files. Unreferenced files (old test scenes, unused models and sounds, editor layout files) stay out, and the Build log lists them. `project.json` `lastOpenedScene` does not count.
3. Adds the built shaders and any engine defaults from `resources/` that your content names but does not contain (`rom/skybox/MySkybox.dds` only when used).
4. Cooks panorama PNG skyboxes into `rom/skybox/cache/` so the package contains the DDS.
5. Compresses the packaged textures: every `.png` / `.jpg` / `.jpeg` / `.bmp` under `rom/` becomes a BC7 `.dds` (mipmaps included) and the source image is left out of the package. Your project folder is not touched, and scene / prefab / UI JSON keep naming the original `.png`; the game finds the `.dds` itself. Not compressed (the image stays in the package): `rom/shader`, `rom/font`, `rom/branding`, `rom/skybox`, `rom/editor`, images whose width or height is not a multiple of 4, and images that already have a same-name `.dds`. The Build log lists each skipped file. BC7 is lossy, and a `.dds` can be larger on disk than a flat-colour PNG; the saving is GPU memory and load time. `deploy.bat` does not compress and still copies all of `rom/`.
6. Ships no loose engine files for the fade fill, debug text, boot logo or PBR BRDF lookup table: the fade fill is generated at startup, and the debug text font (IBM Plex Mono, license in `licenses/IBMPlexMono-OFL.txt`), the boot logo and `rom/skybox/ibl/brdf_lut.png` are compiled into the exe. They use `builtin:` paths (`builtin:branding/ivy_logo.png`, `builtin:ibl/brdf_lut.png`) so they are never confused with files in your `rom/`: a `rom/...` path is always a file in your project, a `builtin:...` path never reads the disk. Older scenes that name `rom/branding/ivy_logo.png` or `rom/skybox/ibl/brdf_lut.png` keep working and still let a file at that path override. UI text uses TrueType fonts (`rom/font/LemonSlice.ttf` by default); UI text defaults to the built-in IBM Plex Mono (`builtin:font/IBMPlexMono-Regular.ttf`, no file needed). `LemonSlice.ttf` is no longer shipped: old scenes that still name it, or the retired `rom/texture/consolab_ascii_512.png` bitmap atlas, are read as the default font unless your project has its own copy.

**If something is missing from a package.** The scan cannot see a file name your script builds at runtime (for example `"rom/audio/sfx_" + number + ".wav"` is handled, but a name read from a table or a save file is not), nor a non-ASCII name inside a UTF-16 script string. Add such files to `project.json`:

```json
"build": {
  "alwaysInclude": ["rom/audio/", "rom/texture/icons/"],
  "packAllAssets": false
}
```

`alwaysInclude` entries are files, folders or name prefixes under `rom/`. `"packAllAssets": true` switches back to copying everything. The Build log prints how many files were packed and left out, so check it after adding a new kind of asset.

The package reads gravity and audio bus volumes from its `project.json`. Players never see the editor or the debug hotkeys. Release | x64 is recommended; the runtime is statically linked, so no VC++ redistributable is needed.

Without the editor, an external content project can run `deploy\deploy.bat` instead (see [§21](#21-external-content-projects)).

---

## 18. Common recipes

### “I want a spinning prop”

1. Place mesh with `visual` + `SceneTransform`.
2. Add `SpinScript` ([§9](#9-writing-gameplay-scripts)).
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
| Missing shaders / fonts / branding | These come from the engine `resources/` beside the exe (not copied into projects). Check the exe folder has `resources/`, and that your project does not shadow the path with a broken file |
| Script never runs | `REGISTER_SCRIPT`, matching `scriptClass`, GameScripts built, Play mode on |
| Model missing | Path under `rom/`, FBX SDK DLL beside exe, case of path |
| Collider wrong | Shape fields / center; enable Show Debug Collider or F11; mesh collider needs a model |
| Tag missing / wrong id | `rom/tags.json`; custom tags only after builtins; remapping after delete |
| No hot reload | Check the toolbar lights and `content_hot_reload.log`; rebuild GameScripts; the editor must be the host that loaded the DLL; `resources/` changes need a restart |
| UI wrong scale | Design for 1920×1080 anchors; check letterboxed viewport vs full window |
| 3D audio silent / crash | Listener/emitter GameObject still alive; call Stop before destroy |
| Shadows ugly | Raise shadow quality in Project Settings → Rendering; check light direction |
| Panels lost / layout broken | **Window → Reset Layout** |
| Editor text too small / large | **Edit → Preferences → Appearance → UI Scale** |

### Transform gotcha

The engine keeps **legacy** `m_position` / `m_rotation` / `m_scale` and the **`SceneTransform`** component. Editor and serializers sync them; if you write low-level code, call the sync helpers or always go through the same path you started with.

---

## 20. Projects (New / Open / Save)

A **content project** is a folder the editor opens as the working root:

```text
MyGame/
  project.json
  rom/          # scenes, assets, tags, …
  game/scripts/ # optional GameScripts sources
```

The editor calls `SetCurrentDirectory` on that folder so relative paths like `rom/model/...` keep working.

### File menu

| Command | What it does |
|---------|----------------|
| **New Project…** | Pick a parent folder, enter a name → scaffolds `rom/`, blank `scene_main.json`, optional scripts template, `<Name>.sln` + `build_scripts.bat`, and AI-agent docs (`docs/ivy-engine/`, `AGENTS.md`, `CLAUDE.md`; see [§21](#ai-agents-in-the-game-repo)). Engine shaders, fonts, branding, primitives, and IBL stay in `resources/` and are used when the project does not override that `rom/` path |
| **Open Project…** | Pick any folder that contains `rom/` (creates `project.json` if missing), including the in-repo `testbed/` |
| **Open Testbed** | Open `<engine repo>/testbed` when that folder exists. Shown in Debug and Release. Use this when a recent external project is already open |
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
    "uiZoomPercent": 100,
    "uiStylePreset": "Trellis",
    "uiColorScheme": "Ivy"
  },
  "physics": {
    "gravity": 9.81
  },
  "audio": {
    "masterVolume": 1.0,
    "bgmVolume": 1.0,
    "sfxVolume": 1.0,
    "voiceVolume": 1.0,
    "uiClickSound": "rom/audio/ui_click.wav"
  }
}
```

| Field | Meaning |
|-------|---------|
| `name` | Shown in the window title |
| `lastOpenedScene` | Scene file restored when the project opens |
| `defaultScene` | Play / runtime default (also written to `rom/scene/default_scene.txt`) |
| `editor.uiZoomPercent` | UI Scale percent (100 / 125 / 150 / 175 / 200; Preferences → Appearance), multiplied by monitor DPI |
| `editor.uiStylePreset` | ImGui style preset: `Trellis` (default) or `Hollow Soft` (Preferences → Appearance) |
| `editor.uiColorScheme` | ImGui color scheme: `Ivy` (default), `Ivy Dark`, `Classic Dark`, `Neutral Gray`, or `Light` (Preferences → Appearance) |
| `physics.gravity` | World gravity applied in play and runtime builds |
| `audio.masterVolume` / `bgmVolume` / `sfxVolume` / `voiceVolume` | Bus levels (editor + runtime) |
| `audio.uiClickSound` | WAV played when a UIButton activates (persists across scene loads) |

### Sample vs your game

- **Debug IvyEngine** opens [`testbed/`](../testbed) when no recent project loads. Sample scenes are in `testbed/rom/scene/`; engine defaults come from `resources/` and are not copied in.
- The engine repo root is not a content project. It has `resources/` (read-only defaults) and `testbed/` (the sample content project). `IvyEngineRuntime` started from the engine output folder opens `testbed/` when there is no `rom/scene` beside the exe.
- `testbed/` has no `GameScripts` project, so **Build Current Project** and `deploy.bat` are for your own content project, not for the testbed.
- **File → New Project** creates a separate game folder anywhere on disk (see [§21](#21-external-content-projects)). An existing external project in Recent Projects opens instead of the testbed.

---

## 21. External content projects

Keep each game in its own folder (usually its own git repo). `external_content_template/` in the engine repo is the starter; **File → New Project** creates the same layout for you.

### What the content project owns

- `project.json` — editor session, default scene, gravity, audio
- `<ProjectName>.sln` — **GameScripts only**; open this to build scripts
- `build_scripts.bat` — optional MSBuild helper
- `IvyEngine.props` — `EngineRoot` path to your engine checkout
- `rom/` — assets, `rom/scene/*.json`, `rom/tags.json`
- `game/scripts/` — your script sources and `GameScripts/GameScripts.vcxproj`
- `deploy/deploy.bat` — editor-free packaging
- `docs/ivy-engine/`, `AGENTS.md`, `CLAUDE.md` — docs for AI coding agents (see below)

The engine repo keeps the editor and runtime exes, engine sources and libraries, the shader pipeline (`shaders/` → `resources/shader/*.cso`), and the read-only defaults in `resources/`.

### Setup

1. Use **File → New Project** (tick **2D Project** for a 2D starter), **or** copy `external_content_template/` to a new folder.
2. If you copied the template, set `EngineRoot` in `IvyEngine.props` to your engine repo path.
3. Build **IvyEngineRuntime** in the engine repo first.
4. Put scripts under `game/scripts/` (any sub-folder) and add them to `GameScripts.vcxproj`.
5. Put assets under `rom/`. The starter scene is `rom/scene/scene_main.json`.
6. Open `<ProjectName>.sln` (or run `build_scripts.bat`) and build **GameScripts | x64**. Post-build copies `GameScripts.dll` to the project's `game/scripts/bin/` **and** to the engine output's `game/scripts/bin/`, so the editor and runtime beside the engine exe load it.
7. Open the folder with **File → Open Project** in IvyEngine.
8. Package with **File → Build Current Project**, or run `deploy\deploy.bat Debug` / `Release` and launch `deploy\IvyEngineRuntime.exe`.

### AI agents in the game repo

New Project prepares the game repo so an AI coding agent can build the game **without reading or changing the engine**:

- `docs/ivy-engine/` — the engine's game-facing docs (scripting, scene/prefab JSON, per-system APIs, build, test bridge, bridge scenario schema) and `ENGINE_COMMIT`, the engine version they describe.
- `AGENTS.md` and `CLAUDE.md` — a managed section between `<!-- ivy-engine-docs:begin -->` and `<!-- ivy-engine-docs:end -->` that points the agent at those docs and sets the **engine boundary**: treat the engine as frozen and read-only, never read or edit the `EngineRoot` source, use only documented APIs, and log anything missing in `docs/engine-requests.md` instead of working around the engine.

Write your own game rules **outside** the markers; they are never touched. After the engine changes, refresh the docs (this also removes stale files) from the engine repo:

```bash
python3 tools/export_agent_docs.py --game-repo /path/to/MyGame
```

For a project created another way (copied template, older project), run the same command once. The script still needs the engine checkout on disk to build `GameScripts`; the agent just does not need to open it.

### Startup scene

| Context | Source of truth |
|---------|-----------------|
| Editor | `project.json` → `lastOpenedScene`, then `defaultScene` |
| Runtime / package | `rom/scene/default_scene.txt` (kept in sync by Save Project) |

### `deploy.bat` layout

- `deploy/IvyEngineRuntime.exe` (the editor build names it `<GameName>.exe` instead) and `deploy/libfbxsdk.dll`
- `deploy/rom/**` — your files, plus built `.cso` shaders, branding, and engine defaults your content names but does not already have. `deploy.bat` copies `rom/` as-is and does not cook panorama skies; a missing cache is baked by the runtime on first load.
- `deploy/game/scripts/bin/GameScripts.dll`

---

## Where to go next

| Document | When to open it |
|----------|-----------------|
| **This guide** | Learning workflows |
| [`ENGINE_API_REFERENCE.md`](ENGINE_API_REFERENCE.md) | Looking up a specific function |
| [`RUNTIME_TEST_BRIDGE.md`](RUNTIME_TEST_BRIDGE.md) | Driving a Debug build from automated tests |
| [`adr/`](adr/) | Why a design decision was made |
| [`TODO.md`](TODO.md) | Engine backlog |
| [`AGENTS.md`](../AGENTS.md) | Rules and schemas for AI agents working on the engine |
| `testbed/rom/scene/`, `samples/scripts/` | Real examples to copy |

Happy building.
