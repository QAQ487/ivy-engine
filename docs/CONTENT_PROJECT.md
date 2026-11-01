# Content projects

A content project is a folder with `project.json` and `rom/`. The editor sets the process working directory to that folder, so scene and asset paths such as `rom/model/...` stay relative to the project. `external_content_template/` is the starter for a project that lives **outside** this repo. `testbed/` is the in-repo project used to exercise the engine without that external repo.

For day-to-day editing inside the engine editor, prefer **File → New Project / Open Project** (see [`ENGINE_USER_GUIDE.md`](ENGINE_USER_GUIDE.md) §20). New Project can copy pieces of this template when it finds the engine repo.

## In-repo testbed

`testbed/` is a normal content project (`project.json`, `rom/scene/`, `rom/scene/default_scene.txt`). Sample scenes that used to live in `samples/scenes/` are now `testbed/rom/scene/`. `samples/scripts/` stays where it is. The testbed has no `GameScripts` project, so **File → Build Current Project** and `deploy/deploy.bat` still apply to an external content project, not to `testbed/`.

Debug `IvyEngine` opens `testbed/` when no recent project can be loaded. A recent project that still exists wins, including an external game. Release does not take this fallback. **File → Open Testbed** opens it in Debug and Release whenever `testbed/` is next to the engine repo (found from the working directory or the exe). **File → Open Project** accepts any folder that contains `rom/`, including `testbed/`. The engine repo root no longer has `rom/`, so it is not a content project. Editor and runtime output directories mirror `resources/` beside the exe; they are not content projects.

Engine defaults live in repo-root `resources/` (`font`, `texture`, `shader`, `branding`, `model/primitives`, `skybox` including `ibl`). A safe `rom/<rest>` path opens the project file when it exists, otherwise `<exe-dir>/resources/<rest>`. Writes (scenes, prefs, tags, new assets) go only to the project `rom/`. The editor does not create NTFS junctions and does not copy those defaults into a new project. `resources/skybox/source/` is the cubemap authoring input and is excluded from the OutDir mirror and from packages.

`scene_sample_menu.json` lives only in `testbed/rom/scene/`. `IvyEngineRuntime` launched from the engine OutDir has no `rom/scene` beside the exe, so it changes directory to `testbed/` and loads that boot scene. A packaged game has its own `rom/scene/default_scene.txt` and stays on the package directory.

The sprite placeholders `rom/texture/sprite_demo/{adventurer_walk,test_sheet,hp_fill,hp_frame}.png` live in `testbed/rom/texture/sprite_demo/`. They are tiny generated images, not the original sprite sheets. Colored boxes and the player clips live under `testbed/rom/model/`.

Linux checks (no Windows runtime claim):

```bash
g++ -std=c++17 -I. tools/content_path_check.cpp -o /tmp/content_path_check && /tmp/content_path_check
python3 tools/check_testbed_assets.py
python3 tools/check_engine_resource_paths.py
```

The checks above are the Linux gate. Do not treat them as a Windows runtime pass.

## External content template

This folder (`external_content_template/`) is a starter for a **content-only** project that lives outside the engine repo.

## What this project owns
- `project.json` (recommended — editor session + default/last scene)
- `<ProjectName>.sln` (GameScripts only — open this to build scripts outside the engine)
- `build_scripts.bat` (optional MSBuild helper)
- `rom/` assets and `rom/scene/*.json`
- `rom/tags.json` (project tag list; created/updated by the editor)
- `game/scripts/` script sources
- `game/scripts/GameScripts/GameScripts.vcxproj`

## What stays in engine repo
- `IvyEngineRuntime.exe` / `IvyEngine.exe`
- engine sources/libraries
- shader compilation pipeline (`shaders/` → `resources/shader/*.cso`, gitignored)
- read-only engine defaults in `resources/`

## Setup
1. Copy this `external_content_template` folder to a new repo, **or** use **File → New Project** in the editor.
2. Edit `IvyEngine.props` and set `EngineRoot` to your engine repo path.
3. Put your script `.cpp/.h` files under `game/scripts/` (for example `sample/` or your own folders) and add them to `GameScripts.vcxproj`.
4. Put your game assets under `rom/`. Engine defaults (shaders, fonts, branding, primitives, IBL, the default skybox) stay in the engine `resources/` tree and are resolved when the project does not override that `rom/` path. New Project does not copy them. The template's `rom/scene/scene_main.json` is the starter scene.
5. Open `<ProjectName>.sln` (or run `build_scripts.bat`) and build **GameScripts** | x64.
6. Run `deploy\deploy.bat Debug` (or `Release`).
7. Launch `deploy\IvyEngineRuntime.exe` (bat path). Prefer **File → Build Current Project** in the editor for a playable package named `<GameName>.exe`.

**File → New Project** also writes `<FolderName>.sln` and `build_scripts.bat` into the new content folder.

## Startup scene

| Context | Source of truth |
|---------|-----------------|
| Editor | `project.json` → `lastOpenedScene` / `defaultScene` |
| Runtime / deploy | `rom/scene/default_scene.txt` (kept in sync when you Save Project in the editor) |

The in-repo testbed uses `scene_sample_menu.json` by default. External projects and this template start from `scene_main.json`. There is no second copy of the sample menu in the engine repo.

## Required runtime layout
After deploy, this layout is produced:

- `deploy/IvyEngineRuntime.exe` (from `deploy.bat`; GUI packaging uses `<GameName>.exe`)
- `deploy/libfbxsdk.dll`
- `deploy/rom/**` (project files, overlaid `.cso` and branding, plus engine defaults the content names and does not already have; `MySkybox.dds` only when named). The editor package cooks any `skyboxCubemap` PNG into `rom/skybox/cache/` before the copy. `deploy.bat` copies `rom/` as it is and does not cook; a missing cache is baked by the runtime on load.
- `deploy/game/scripts/bin/GameScripts.dll`

## Notes
- `GameScripts.vcxproj` references engine headers/libs through `$(EngineRoot)`.
- Post-build copies `GameScripts.dll` to the content `game/scripts/bin/` **and** to `$(EngineOutDir)game/scripts/bin/` so the editor/runtime next to the engine exe can load it.
- Build **IvyEngineRuntime** in the engine repo first.
- Opening the content folder with **File → Open Project** in **IvyEngine** is the supported editor workflow for external games.
