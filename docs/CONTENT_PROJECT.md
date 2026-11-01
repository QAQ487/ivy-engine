# Content projects

A content project is a folder with `project.json` and `rom/`. The editor sets the process working directory to that folder, so scene and asset paths such as `rom/model/...` stay relative to the project. `external_content_template/` is the starter for a project that lives **outside** this repo. `testbed/` is the in-repo project used to exercise the engine without that external repo.

For day-to-day editing inside the engine editor, prefer **File → New Project / Open Project** (see [`ENGINE_USER_GUIDE.md`](ENGINE_USER_GUIDE.md) §20). New Project can copy pieces of this template when it finds the engine repo.

## In-repo testbed

`testbed/` is a normal content project (`project.json`, `rom/scene/`, `rom/scene/default_scene.txt`). Sample scenes that used to live in `samples/scenes/` are now `testbed/rom/scene/`. `samples/scripts/` stays where it is. The testbed has no `GameScripts` project, so **File → Build Current Project** and `deploy/deploy.bat` still apply to an external content project, not to `testbed/`.

Debug `IvyEngine` opens `testbed/` when no recent project can be loaded. A recent project that still exists wins, including an external game. Release does not take this fallback. **File → Open Project** on `testbed/` works in either configuration. The editor output directory contains `rom/` and is never treated as a project.

Opening the testbed does not copy engine assets into the git tree. It creates NTFS directory junctions from `testbed/rom/{branding,font,model,physmat,shader,skybox,texture}` to the same folders under the engine `rom/`. Junction targets are fixed names and must canonicalize to a child of the engine `rom/`; `..` and absolute paths are rejected (`engine/core/content_path.h`). `testbed/rom/scene/` is a real directory. `testbed/rom/editor/` is created locally for UI prefs and is gitignored, as are the junctions.

`scene_sample_menu.json` in the testbed is a copy of `rom/scene/scene_sample_menu.json` so Play can load `scene_sample_playground.json` from the same `rom/scene/`. The engine-repo copy remains the scene shipped beside the exe.

The three 2D scenes `scene_physics_2d.json`, `scene_topdown_2d.json`, and `scene_topdown_2d_next.json` reference `rom/texture/sprite_demo/{adventurer_walk,test_sheet,hp_fill,hp_frame}.png`. Those files were not in the repo. They are now tiny generated placeholder PNGs (256×128 or 128×32, solid color, under 1 KB each) in the engine `rom/`, not copies inside `testbed/`. They are not the original sprite sheets.

A later cylinder-pair scene can be added under `testbed/rom/scene/` when that collider work lands. This change does not add one and does not modify collider code.

Linux checks (no Windows runtime claim):

```bash
g++ -std=c++17 -I. tools/content_path_check.cpp -o /tmp/content_path_check && /tmp/content_path_check
python3 tools/check_testbed_assets.py
```

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
- shader compilation pipeline

## Setup
1. Copy this `external_content_template` folder to a new repo, **or** use **File → New Project** in the editor.
2. Edit `IvyEngine.props` and set `EngineRoot` to your engine repo path.
3. Put your script `.cpp/.h` files under `game/scripts/` (for example `sample/` or your own folders) and add them to `GameScripts.vcxproj`.
4. Put your game assets under `rom/` (include engine builtins: `rom/shader`, `rom/font`, `rom/texture` — New Project copies these automatically).
5. Open `<ProjectName>.sln` (or run `build_scripts.bat`) and build **GameScripts** | x64.
6. Run `deploy\deploy.bat Debug` (or `Release`).
7. Launch `deploy\IvyEngineRuntime.exe` (bat path). Prefer **File → Build Current Project** in the editor for a playable package named `<GameName>.exe`.

**File → New Project** also writes `<FolderName>.sln` and `build_scripts.bat` into the new content folder.

## Startup scene

| Context | Source of truth |
|---------|-----------------|
| Editor | `project.json` → `lastOpenedScene` / `defaultScene` |
| Runtime / deploy | `rom/scene/default_scene.txt` (kept in sync when you Save Project in the editor) |

The in-repo testbed uses `scene_sample_menu.json` by default. External projects still start from `scene_main.json` when created with New Project. The engine repo's own `rom/scene/scene_sample_menu.json` remains the boot scene copied next to the exe.

## Required runtime layout
After deploy, this layout is produced:

- `deploy/IvyEngineRuntime.exe` (from `deploy.bat`; GUI packaging uses `<GameName>.exe`)
- `deploy/libfbxsdk.dll`
- `deploy/rom/**`
- `deploy/game/scripts/bin/GameScripts.dll`

## Notes
- `GameScripts.vcxproj` references engine headers/libs through `$(EngineRoot)`.
- Post-build copies `GameScripts.dll` to the content `game/scripts/bin/` **and** to `$(EngineOutDir)game/scripts/bin/` so the editor/runtime next to the engine exe can load it.
- Build **IvyEngineRuntime** in the engine repo first.
- Opening the content folder with **File → Open Project** in **IvyEngine** is the supported editor workflow for external games.
