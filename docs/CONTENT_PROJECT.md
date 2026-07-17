# External Content Project Template

This folder (`external_content_template/`) is a starter for a **content-only** project that lives outside the engine repo.

For day-to-day editing inside the engine editor, prefer **File → New Project / Open Project** (see [`ENGINE_USER_GUIDE.md`](ENGINE_USER_GUIDE.md) §20). New Project can copy pieces of this template when it finds the engine repo.

## What this project owns
- `project.json` (recommended — editor session + default/last scene)
- `rom/` assets and `rom/scene/*.json`
- `rom/tags.json` (project tag list; created/updated by the editor)
- `game/scripts/` script sources
- `game/scripts/GameScripts/GameScripts.vcxproj`

## What stays in engine repo
- `Game_Runtime.exe` / `Game_Project.exe`
- engine sources/libraries
- shader compilation pipeline

## Setup
1. Copy this `external_content_template` folder to a new repo, **or** use **File → New Project** in the editor.
2. Edit `MagneticField.Engine.props` and set `EngineRoot` to your engine repo path.
3. Put your script `.cpp/.h` files under `game/scripts/` (for example `sample/` or your own folders) and add them to `GameScripts.vcxproj`.
4. Put your game assets under `rom/` (include engine builtins: `rom/shader`, `rom/font`, `rom/texture` — New Project copies these automatically).
5. Build `game/scripts/GameScripts/GameScripts.vcxproj`.
6. Run `deploy\deploy.bat Debug` (or `Release`).
7. Launch `deploy\Game_Runtime.exe`.

## Startup scene

| Context | Source of truth |
|---------|-----------------|
| Editor | `project.json` → `lastOpenedScene` / `defaultScene` |
| Runtime / deploy | `rom/scene/default_scene.txt` (kept in sync when you Save Project in the editor) |

Engine samples use `scene_sample_menu.json` by default.

## Required runtime layout
After deploy, this layout is produced:

- `deploy/Game_Runtime.exe`
- `deploy/libfbxsdk.dll`
- `deploy/rom/**`
- `deploy/game/scripts/bin/GameScripts.dll`

## Notes
- `GameScripts.vcxproj` is configured to reference engine headers/libs through `$(EngineRoot)`.
- Build `Game_Runtime` in the engine repo first.
- Opening the content folder with **File → Open Project** in `Game_Project` is the supported editor workflow for external games.
