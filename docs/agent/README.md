# Ivy Engine — agent docs

Concise English pack for AI agents working in a **content project** (game repo) against this C++ / Direct3D 11 engine. Two hosts share the same scene/components: **Editor** (`IvyEngine.exe`) and **Runtime** (`IvyEngineRuntime.exe`).

## Content project layout

A content project is a folder with:

- `project.json` — editor session / default scene
- `rom/` — scenes, models, textures, tags, prefs
- `game/scripts/` — gameplay C++ scripts (DLL)

Starter: `external_content_template/`. In-repo exercise project: `testbed/`. Full detail: [`../CONTENT_PROJECT.md`](../CONTENT_PROJECT.md).

## Asset resolution (`rom/` vs `resources/`)

Safe `rom/<rest>` paths resolve **project first**, then `<exe-dir>/resources/<rest>` (`api:ResolveRomForRead` in `api:content_path`). **Writes always go to the project** (`api:ResolveRomForWrite`) — never into exe `resources/`. Engine defaults (shaders, fonts, branding, primitives, IBL/skybox) live in repo-root `resources/` and are mirrored beside the exe.

## Drift markers

C++ API identifiers in this pack use an `api:` prefix inside backticks (for example `api:Script`) so `tools/agent_docs_check.py` can verify them against `engine/**/*.h` and `src/app/**/*.h`. JSON field names and file paths stay in normal backticks (no `api:`).

## Doc index

| File | Topic |
|------|--------|
| [`scripting.md`](scripting.md) | Script lifecycle, `REGISTER_SCRIPT`, common `ScriptApi_*` |
| [`scene-prefab.md`](scene-prefab.md) | Scene/prefab JSON, registered components |
| [`build-run.md`](build-run.md) | VS2022 solution, Debug vs Release |
| [`testing-bridge.md`](testing-bridge.md) | Debug named-pipe test bridge (condensed) |
| [`api/render.md`](api/render.md) | Render / visual / sprite / billboard / particles |
| [`api/physics-3d.md`](api/physics-3d.md) | 3D physics / colliders / raycast |
| [`api/physics-2d.md`](api/physics-2d.md) | 2D physics / sprites |
| [`api/ui.md`](api/ui.md) | Canvas UI + `UICanvasScript` |
| [`api/input.md`](api/input.md) | Keyboard / mouse / controller |
| [`api/audio.md`](api/audio.md) | Audio component + master volume |
| [`api/camera.md`](api/camera.md) | Scene / third-person / first-person cameras |
| [`api/assets.md`](api/assets.md) | Textures, prefab instantiate/destroy, content paths |

Also (engine repo docs, not always exported): `docs/RUNTIME_TEST_BRIDGE.md`, `docs/CONTENT_PROJECT.md`. Bridge scenario schema: [`../schemas/runtime_bridge_scenario.v1.schema.json`](../schemas/runtime_bridge_scenario.v1.schema.json) (`export_agent_docs.py` rewrites this link to `schemas/` under `docs/ivy-engine/`).
