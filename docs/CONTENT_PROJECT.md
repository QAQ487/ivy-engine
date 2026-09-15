# External Content Project Template

This folder (`external_content_template/`) is a starter for a **content-only** project that lives outside the engine repo.

For day-to-day editing inside the engine editor, prefer **File → New Project / Open Project** (see [`ENGINE_USER_GUIDE.md`](ENGINE_USER_GUIDE.md) §20). New Project can copy pieces of this template when it finds the engine repo.

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

Engine samples use `scene_sample_menu.json` by default.

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
