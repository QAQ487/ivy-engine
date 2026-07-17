# External Content Project Template

This folder is a starter template for a **content-only** project.

## What this project owns
- `rom/` assets and `rom/scene/*.json`
- `game/scripts/` script sources
- `game/scripts/GameScripts/GameScripts.vcxproj`

## What stays in engine repo
- `Game_Runtime.exe`
- engine sources/libraries
- shader compilation pipeline

## Setup
1. Copy this `external_content_template` folder to a new repo.
2. Edit `MagneticField.Engine.props` and set `EngineRoot` to your engine repo path.
3. Put your script `.cpp/.h` files under `game/scripts/` (for example `sample/` or your own folders) and add them to `GameScripts.vcxproj`.
4. Put your game assets under `rom/`.
5. Build `game/scripts/GameScripts/GameScripts.vcxproj`.
6. Run `deploy\deploy.bat Debug` (or `Release`).
7. Launch `deploy\Game_Runtime.exe`.

Startup scene is controlled by `rom/scene/default_scene.txt` (engine samples use `scene_sample_menu.json`).

## Required runtime layout
After deploy, this layout is produced:

- `deploy/Game_Runtime.exe`
- `deploy/libfbxsdk.dll`
- `deploy/rom/**`
- `deploy/game/scripts/bin/GameScripts.dll`

## Notes
- `rom/scene/default_scene.txt` controls startup scene.
- `GameScripts.vcxproj` is configured to reference engine headers/libs through `$(EngineRoot)`.
- Build `Game_Runtime` in the engine repo first.
