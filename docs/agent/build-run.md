# Build & run

Windows-only Visual Studio toolchain. **This Linux agent environment cannot verify an MSVC / D3D11 build.**

## Solution

Open `IvyEngine.sln` (VS2022, toolset `v145` in project files).

| Target | Output | Role |
|--------|--------|------|
| `IvyEngine` | `IvyEngine.exe` | ImGui Editor |
| `IvyEngineRuntime` | `IvyEngineRuntime.exe` | Editor-less Runtime (`GAME_RUNTIME` define on x64) |

Prefer **x64** Debug/Release. Content projects build `GameScripts` separately (see [`../CONTENT_PROJECT.md`](../CONTENT_PROJECT.md)).

## Debug vs Release

| | Debug | Release |
|---|--------|---------|
| Preprocessor | `_DEBUG` | `NDEBUG` |
| Runtime library (x64) | `/MTd` (`MultiThreadedDebug`) | `/MT` (`MultiThreaded`) |
| Test bridge | Available when opted in (Runtime **and** Editor) | Compiled out (`IVY_RUNTIME_TEST_BRIDGE_AVAILABLE` unset) |
| Optimization | Debug defaults | No explicit `<Optimization>` in `.vcxproj`; MSVC defaults to **MaxSpeed (`/O2`)** when unset + `UseDebugLibraries=false`. Both projects set `WholeProgramOptimization` on Release |

## Shaders

HLSL under `shaders/` is FxCompiled by the Runtime project; post-build copies `*.cso` into `resources/shader/`. Packaged/deployed games resolve those via the `rom/` → `resources/` fallback.

## Content workflow (reminder)

1. Build Editor and/or Runtime in the engine repo.
2. Open or create a content project (`project.json` + `rom/` + `game/scripts/`).
3. Build `GameScripts` DLL; deploy/package per content-project docs.
4. Runtime playable package: host exe + `rom/` + `game/scripts/bin/GameScripts.dll`.
