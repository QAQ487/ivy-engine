# Editor Play-mode Bridge (Phase 7)

## Naming note

Repo **Phase 6** already means `--scene`, observe extensions, `list_objects` / `pause` / `resume` / `step` / `perf` (`tools/runtime_bridge_phase6_check.cpp`). The user-facing request called this work “Phase 6”; in-repo it is **Phase 7 / Editor Play-mode bridge** so existing Phase 6 docs and checks stay accurate.

## Goal

Opt-in Debug named-pipe bridge in the **Editor** (same protocol v1 pipe as `IvyEngineRuntime`) so one Editor process can enter/exit Play, load scenes, toggle render settings, and run existing play-world ops without a second runtime window.

## Non-goals

- Network exposure beyond the existing local named pipe
- Shipping / Release bridge (still compiled out)
- Stealing focus / forcing the Editor foreground
- Changing existing Runtime Phase 1–6 verb semantics (except additive fields)

## Design

### Compile / enable

- `IVY_RUNTIME_TEST_BRIDGE_AVAILABLE` when `_DEBUG` (Editor **and** GAME_RUNTIME Debug).
- Release (no `_DEBUG`) still compiles the bridge out.
- Opt-in unchanged: `IVY_RUNTIME_TEST_BRIDGE=1` + valid `IVY_RUNTIME_TEST_RUN_ID`.
- Pipe name / DACL / idle lifetime unchanged; connection survives Play enter/exit and scene loads.

### Host kind

- `BridgeHostKind::{Runtime,Editor}` on `Protocol` / `Bridge`.
- `hello` / `status` advertise additive `host` (`"runtime"` | `"editor"`) and, for Editor, `playMode` (`"none"` | `"viewport"` | `"fullscreen"`).
- Capabilities stay additive: Editor advertises `play_enter`, `play_exit`, `load_scene`, `set_setting` plus existing verbs.

### New verbs (additive)

| Verb | Host | Notes |
| --- | --- | --- |
| `play_enter` | Editor | Optional `mode`: `"viewport"` (default) / `"fullscreen"`. Queued; applied on WinMain via `EngineGUI_StartPlay`. Error `already_playing` if already in Play. Runtime → `unsupported_host`. |
| `play_exit` | Editor | Queued `EngineGUI_StopPlay` (restores edit-time scene snapshot). Idempotent when already stopped. Runtime → `unsupported_host`. |
| `load_scene` | Editor | Required `scene` `rom/...json` (same path rules as `--scene` / `reset_scene`). Queued scene load; pipe stays up. Runtime → `unsupported_host` (use `reset_scene`). |
| `set_setting` | Both | Whitelist keys + validated bool values (below). Queued apply on main thread. |

### Setting whitelist

Only:

- `frustumCullingEnabled`
- `shadowCascadeCullingEnabled`
- `spriteCullingEnabled`

Value must be JSON boolean. Unknown key / wrong type / extra fields → `invalid_request` with echoed `requestId`.

### Mode gating

Editor Edit mode (`host=editor`, `playing=false`):

- Play-required ops (`keyboard`, `mouse`, `controller`, `controller_probe`, `observe`, `list_objects`, `pause`, `resume`, `step`, `perf`, `capture`, world manip verbs) → `not_playing` (echo `requestId`).
- `hello` / `status` / `quit` / `play_enter` / `play_exit` / `load_scene` / `set_setting` allowed as specified.

Runtime keeps existing `not_ready` when `!ready \|\| !playing` (no semantic change for GAME_RUNTIME clients).

### Capture

Editor capture reads the **game viewport** (`EngineGUI_GetGameViewportRect`), not the full ImGui chrome. Reply still includes absolute `path` under `IVY_RUNTIME_TEST_OUTPUT_DIR`. Runtime capture remains full backbuffer.

### Focus

Bridge handlers must not call `SetForegroundWindow` / `BringWindowToTop` / force activation.

### Tests / docs

- Linux offline: `tools/runtime_bridge_editor_play_check.cpp` (parse/whitelist/gating/capabilities) + Python client helper unit tests; wire into `run_runtime_bridge_offline_linux.sh` and `run_runtime_bridge_checks.cmd`.
- Update `docs/RUNTIME_TEST_BRIDGE.md` (+ zh-TW summary). Never claim Windows Editor PASS from Linux.
