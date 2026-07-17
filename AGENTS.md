# AGENTS.md

## Cursor Cloud specific instructions

### What this repository is
`MagneticField` is a **native Windows C++ / DirectX 11 game engine + game** built with
**Visual Studio 2022 / MSBuild**. Entry point is a Win32 `WinMain` (`src/app/main.cpp`)
that links `d3d11.lib`, XAudio2/X3DAudio, XInput/DirectInput, WIC, and the vendored FBX SDK.
See `docs/ENGINE_MANUAL.md` and `docs/CONTENT_PROJECT.md` for architecture and APIs.

Build targets (all Windows-only, defined in `Game_Project.sln`):
- `Game_Project.vcxproj` → `Game_Project.exe` — editor/app (ImGui editor + dev runtime). Primary dev target.
- `Game_Runtime.vcxproj` → `Game_Runtime.exe` — standalone player (`GAME_RUNTIME` defined).
- `game/scripts/GameScripts/GameScripts.vcxproj` → `GameScripts.dll` — hot-reloadable game logic.

Layout: engine in `engine/`, app/host glue in `src/app/`, game logic in `game/scripts/`,
content (scenes/prefabs/models/textures/audio/effects) in `rom/`, HLSL in `shaders/`,
vendored third-party libs in `libraries/` (imgui, imguizmo, nlohmann, directxtex, fbxsdk, effekseer).

### Critical caveat: this project cannot be built or run on the Linux Cloud VM
The Cloud Agent VM is **Ubuntu Linux**. This project requires the **MSVC toolchain, the
Windows SDK, and Direct3D 11**, none of which exist on Linux (there is no `msbuild`/`dotnet`,
no DirectX, and no Wine/GPU path set up). There is **no CMake / cross-platform / headless
build** and **no CI build script** in the repo. Do **not** attempt to build `.sln`/`.vcxproj`
or run the executables here — it will not work.

To actually build/run (on a Windows machine with Visual Studio 2022):
- `msbuild Game_Project.sln /p:Configuration=Debug /p:Platform=x64` then run `x64\Debug\Game_Project.exe`.
- The app loads `rom/scene/scene_title.json` at startup (see `rom/scene/default_scene.txt`).
- `libfbxsdk.dll` must sit next to the built executable at runtime (only `libraries/fbxsdk/include/` is vendored; the DLL/lib is supplied by the Windows FBX SDK install).

### Dependencies
- **No package manager.** All C++ dependencies are vendored in `libraries/` and are already in the repo — nothing to install.
- The only tooling that runs on this Linux VM is the **Python asset generators** under `tools/` and `rom/`, which use the **standard library only** (no `pip install` needed). Example: `python3 tools/generate_default_sky_cubemap.py` writes `rom/texture/sky/default_sky.dds` (a valid 6-face cubemap DDS). This is the one part of the dev toolchain that is verifiable in the Cloud VM.

### Lint / test / build / run
- There are **no Linux-runnable lint or automated test suites**; tests under `game/scripts/test/` are C++ game scripts compiled into `GameScripts.dll` and require the Windows build.
- Build/lint/run all require Windows + Visual Studio 2022 (see above).
- Do not change game content or game-specific scripts (`rom/`, `game/scripts/`) unless the task is explicitly about game behavior (per `docs/ENGINE_MANUAL.md`).
