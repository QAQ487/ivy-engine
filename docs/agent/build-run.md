# Build & run

Windows-only Visual Studio toolchain. **This Linux agent environment cannot verify an MSVC / D3D11 build.**

## Solution

Open `IvyEngine.sln`. Project files set `<PlatformToolset>v145</PlatformToolset>` (Visual Studio **18 / 2026** toolset — not VS 2022’s `v143`).

| Target | Output | Role |
|--------|--------|------|
| `IvyEngine` | `IvyEngine.exe` | ImGui Editor |
| `IvyEngineRuntime` | `IvyEngineRuntime.exe` | Editor-less Runtime (`GAME_RUNTIME` define on x64 configs) |

Prefer **x64** Debug/Release. Content projects build `GameScripts` separately (engine `docs/CONTENT_PROJECT.md`).

## Debug vs Release (from `.vcxproj`)

Verified fields in `IvyEngine.vcxproj` / `IvyEngineRuntime.vcxproj` (x64):

| | Debug (x64) | Release (x64) |
|---|--------|---------|
| `UseDebugLibraries` | `true` | `false` |
| Preprocessor | `_DEBUG` (+ `GAME_RUNTIME` on Runtime) | `NDEBUG` (+ `GAME_RUNTIME` on Runtime) |
| `RuntimeLibrary` | `MultiThreadedDebug` (`/MTd`) | `MultiThreaded` (`/MT`) |
| `WholeProgramOptimization` | unset | `true` |
| `<Optimization>` | **not set** in either project | **not set** — do not claim a specific `/O*` flag from the vcxproj |
| Test bridge | Available when opted in (Runtime **and** Editor) | Compiled out (`api:IVY_RUNTIME_TEST_BRIDGE_AVAILABLE` unset) |

## Shaders

HLSL under `shaders/` is FxCompiled by the Runtime project; post-build copies `*.cso` into `resources/shader/`. Packaged/deployed games resolve those via the `rom/` → `resources/` fallback.

## Content workflow (reminder)

1. Build Editor and/or Runtime in the engine repo.
2. Open or create a content project (`project.json` + `rom/` + `game/scripts/`).
3. Build `GameScripts` DLL; deploy/package per content-project docs.
4. Runtime playable package: host exe + `rom/` + `game/scripts/bin/GameScripts.dll`.
