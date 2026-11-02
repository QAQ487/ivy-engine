# Runtime Test Bridge

Debug-only named-pipe harness for Ivy Engine’s playable **GAME_RUNTIME** build. It lets a local client drive keyboard, mouse, and controller test input and read bounded observations (including a controller consumer probe). It is **not** shipping gameplay input and is compiled out of Release and Editor builds.

Cantonese summary: [`RUNTIME_TEST_BRIDGE.zh-TW.md`](RUNTIME_TEST_BRIDGE.zh-TW.md). This page is the canonical operations guide (including controller injection and `controller_probe` after PR #3).

## Build gate

| Configuration | Bridge / inject / probe |
| --- | --- |
| `_DEBUG` **and** `GAME_RUNTIME` (Debug `IvyEngineRuntime`) | Available when opted in via environment |
| Release (`NDEBUG` / no `_DEBUG`) | Compiled out (`IVY_RUNTIME_TEST_BRIDGE_AVAILABLE` unset) |
| Editor (no `GAME_RUNTIME`) | Compiled out |

Shipping controller code paths are unchanged when the bridge is off. Release/Editor compile-out is checked by `tools/runtime_bridge_release_check.cpp`.

## Enable (Windows interactive session)

From a launcher that starts Debug `IvyEngineRuntime.exe` and records the child PID:

```powershell
$env:IVY_RUNTIME_TEST_BRIDGE = '1'
$env:IVY_RUNTIME_TEST_RUN_ID = [guid]::NewGuid().ToString('N')   # 1–64 ASCII alnum or '-'
$env:IVY_RUNTIME_TEST_OUTPUT_DIR = '<capture-dir>'             # optional; PNG capture output
$env:IVY_RUNTIME_TEST_FIXED_CAMERA = '1'                       # optional; skips mouse/right-stick look
$env:IVY_SKIP_CLOSE_CONFIRM = '1'                              # optional; skip close dialog in Debug
```

- Bridge starts only if `IVY_RUNTIME_TEST_BRIDGE` is exactly `1` and `IVY_RUNTIME_TEST_RUN_ID` is present (1–64 chars).
- Pipe name: `\\.\pipe\IvyRuntimeTest-<PID>-<runId>` (local, `PIPE_REJECT_REMOTE_CLIENTS`, current-user DACL).
- Stock client: `tools/runtime_test_client.py` (stdlib + `ctypes`; verifies server PID).

## Protocol (v1)

UTF-8 JSON, one request/response per LF-terminated frame. Request body max **4096** bytes (excluding LF). Required envelope fields: `protocolVersion` (1), `requestId` (1…2147483647, **strictly increasing for the whole run**, including reconnects), `runId`, `verb`.

Capabilities advertised by `hello` / `status` (as implemented):

`hello`, `status`, `quit`, `keyboard`, `observe`, `capture`, `mouse`, `controller`, `events`, `controller_probe`, `list_objects`, `pause`, `resume`, `step`, `perf`, `reset_scene`, `set_transform`, `set_velocity`, `apply_impulse`, `apply_torque_impulse`

### Common verbs

| Verb | Role |
| --- | --- |
| `hello` / `status` | Identity, ready/playing/paused/scene snapshot, frame counters, input op status, capabilities. When not ready, `readyDetail` explains `scene_path_empty`, `preload_active`, or `not_playing`. |
| `quit` | Requests exit; response has `requiresReceipt: true` — client must send a lone LF receipt |
| `keyboard` | Internal key inject (`keys`, `updates`) |
| `mouse` | Internal mouse inject (`dx`, `dy`, `buttons`, `updates`, optional `wheel`) |
| `observe` | Named GameObject snapshot: legacy `position`/`velocity`, plus `rotation` (Euler degrees), `rotationQuaternion`, `scale`, `angularVelocity`, and `rigidbody.kinematic` when present. Sleeping/contact counts are omitted (not cheaply available). |
| `list_objects` | Bounded GameObject name + component type list (`offset`, `limit` 1…64, default 32). Response includes `total` and `truncated`. |
| `pause` / `resume` | Freeze or resume the sim loop. Status reports `paused`, `pendingSteps`, `frame`, `physicsTick`. |
| `step` | While paused only: queue `frames` 1…600 fixed-timestep update/physics ticks. Input inject and `controller_probe` advance on stepped ticks. Error `not_paused` if running. |
| `perf` | Stats over the last N frames (ring capacity 600): avg/min/max/p95 for `frameMs`/`updateMs`/`drawMs`/`presentMs` (same stages as the FPS log) plus average FPS. |
| `reset_scene` | Reload the current scene (or optional validated `scene` `rom/...json`) to its initial state. Queued like `capture`; `status.worldOp` reports completion. |
| `set_transform` | Teleport a named object (`object` matches `list_objects`/`observe` names). Optional `position`/`rotation` (euler deg length 3 or quaternion length 4)/`scale` as finite arrays; optional `zeroVelocity` (default true) clears rigidbody velocities/forces after syncing `m_previousPosition`. |
| `set_velocity` | Set dynamic rigidbody `linear` and/or `angular` velocity arrays. Error `not_dynamic` for kinematic/static/missing body. |
| `apply_impulse` | Apply a linear `impulse` array to a dynamic body; optional world-space `point` uses force-at-position. Error `not_dynamic` / `unknown_object`. |
| `apply_torque_impulse` | Apply `torqueImpulse` array to a dynamic body. |
| `capture` | Queued back-buffer PNG under `IVY_RUNTIME_TEST_OUTPUT_DIR` |
| `events` | Bounded checkpoint event ring (see existing event docs in zh-TW notes) |
| `controller` | One-at-a-time controller inject (below) |
| `controller_probe` | Read-only consumer-trace snapshot (below) |

### `--scene` (Debug GAME_RUNTIME + bridge enabled only)

```text
IvyEngineRuntime.exe --scene rom/scene/scene_runtime_bridge_player.json
```

Honored only when `IVY_RUNTIME_TEST_BRIDGE=1` (and a valid run id) starts the bridge; otherwise logged and ignored. Path rules: `rom/`-relative, no `..`, no absolute path, `.json` extension, must resolve via the existing `rom/` → `resources/` fallback. On validation/resolve/load failure the runtime logs a clear error and exits non-zero (no silent title-scene fallback). Loaded scene is reported in `hello`/`status` `scene`.

CLI examples:

```text
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 1 hello
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 2 status
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 3 controller --lx 0 --ly 0 --lt 1 --updates 2
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 4 list_objects --offset 0 --limit 32
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 5 pause
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 6 step --frames 3
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 7 observe --target Player
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 8 perf --frames 60
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 9 resume
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 10 controller_probe
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 11 pause
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 12 set_transform --object DynamicBox --position 3 2 0
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 13 apply_impulse --object DynamicBox --impulse 8 0 0
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 14 step --frames 10
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 15 reset_scene
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 16 quit
```

`Client.controller(...)` polls `status` until the controller op completes or is cancelled; the outer `requestId` on the final poll is **not** necessarily `startId + 1`. Always continue from the last returned id. World-manipulation helpers (`reset_scene`, `set_transform`, `set_velocity`, `apply_impulse`, `apply_torque_impulse`) likewise poll `status.worldOp` until `completed` / `failed`.

### World manipulation notes

- Object addressing uses the same GameObject **name** strings as `list_objects` / `observe` (field name: `object`). Unknown names fail the queued op with `unknown_object`.
- Impulses / velocity writes require a **dynamic** rigidbody (`Rigidbody` component, non-kinematic, mass > 0). Kinematic/static bodies fail with `not_dynamic`.
- Works with `pause` / `step` for deterministic tests: `pause` → `set_transform` → `apply_impulse` → `step N` → `observe`.
- Continuous `apply_force` / `apply_torque` held for N frames are **not** implemented (would need a multi-frame queue like keyboard inject); use impulses or `set_velocity` instead.
- Fixture: `testbed/rom/scene/scene_runtime_bridge_world_manip.json` (`DynamicBox` + `Ground`).

## Controller injection (`controller`)

One queued operation at a time. Requires ready + playing.

| Field | Required | Range / notes |
| --- | --- | --- |
| `lx`, `ly` | yes | Finite stick in [-1, 1] |
| `updates` | yes | Integer 1…120 (engine `Update` hold count, not physics ticks) |
| `rx`, `ry` | no | Finite stick in [-1, 1]; default 0 |
| `lt`, `rt` | no | Finite trigger in [0, 1]; default 0 |
| `buttons` | no | 0…14 unique names from the list below |

Button names: `A`, `B`, `X`, `Y`, `LB`, `RB`, `START`, `BACK`, `LEFT_THUMB`, `RIGHT_THUMB`, `DPAD_UP`, `DPAD_DOWN`, `DPAD_LEFT`, `DPAD_RIGHT`.

Lifecycle (owner thread: `PollAndApplyInput` → `Update`/`Controller_Update` → `FinishInputUpdate`):

1. **Preparation** Update — injects a neutral sample.
2. **Hold** — exactly `updates` Updates with the payload. Button **press edges** only on the first hold Update (`Controller_IsTrigger`).
3. **Release** Update — neutral sample; button **release edges** (`Controller_IsRelease`).
4. Injection cleared on the next poll after release completes. Disconnect / not-playing / deadline cancel eventually returns to neutral (no stuck inject).

Left-stick-only payloads (`lx`/`ly`/`updates` only) remain compatible with the older left-stick acceptance runner. A right-stick / button / trigger-only command with `lx:0, ly:0` holds the left stick at neutral; it does not copy the right stick onto the left.

## Consumer probe (`controller_probe`)

Samples **public** getters once per engine `Update` **after** `Controller_Update` (see `src/app/main.cpp`). Path: bridge request → inject → `Controller_Update` → getters → ring. It does **not** echo the request payload or read inject internals as a substitute for observations.

| Property | Value |
| --- | --- |
| Capacity | 256 frames (`kControllerProbeCapacity`) |
| Overflow | Oldest frame dropped; `dropped` increments; `overflow: true` |
| Verb | Read-only `{frames, latestSequence, dropped, overflow}` when ready+playing |

Each frame includes `sequence`, sticks (`lx`/`ly`/`rx`/`ry`), triggers (`lt`/`rt`), `pressed` / `pressedEdge` / `releasedEdge` (button name arrays), and `connected`.

Enabled with the bridge (reset on start/stop). In-process harnesses that call `PollAndApplyInput` must also call `Controller_Update` then `RuntimeTestControllerProbe_SampleFromGetters()` before `FinishInputUpdate` (the protocol `--serve` harness and `runtime_bridge_controller_check.cpp` do this).

### Client response size

A full 256-frame probe reply exceeds the old 8 KiB client cap. The stock client uses a derived bound from `tools/runtime_bridge_controller_probe.py`:

- Worst-case compact JSON ≈ **152278** bytes
- `RESPONSE_BYTE_LIMIT` = worst-case + 16 KiB headroom (**168662**)
- Truly larger replies still raise `ValueError: oversize response`

Assert ops against frames with `sequence > latestSequence` recorded **before** the operation (helpers: `frames_after_sequence`, `find_*_window`). Do **not** rely on `frames[-4:]` after an arbitrary sleep — idle samples can pad the suffix.

## Isolated fixture

`testbed/rom/scene/scene_runtime_bridge_controller_probe.json` — directional light + ground only (no player scripts). Player/mouse/controller acceptance uses `testbed/rom/scene/scene_runtime_bridge_player.json`. World-manip acceptance uses `testbed/rom/scene/scene_runtime_bridge_world_manip.json` (`DynamicBox`). Do **not** change repo `project.json` / default scene; stage a throwaway package (acceptance runners copy the Debug OutDir, write `rom/scene/default_scene.txt`, copy any scene-referenced `rom/` assets still missing from the package — including `testbed/rom/model/player/*` — fail fast if any remain unresolved, and pass `--scene rom/scene/...`).

Without staging/`--scene`, Debug runtime launched from OutDir falls back to in-repo `testbed/` and boots `scene_sample_menu.json` (title UI). Boot also preloads every scene under `rom/scene`, which can keep `ready=false` past a 25s acceptance deadline while the pipe is already up and FPS later appears in logs.

## How to run checks

### Linux offline protocol checks (no named pipes)

```bash
tools/run_runtime_bridge_offline_linux.sh x64/linux-bridge-offline
```

Compiles and runs `runtime_bridge_protocol_check`, `runtime_bridge_phase6_check`, and `runtime_bridge_world_manip_check` with Win32/input stubs (`tools/linux_win32_shim/`). Pipe DACL / `--serve` transport checks remain Windows-only.

### Batch harness (MSVC x64)

```bat
msbuild IvyEngine.sln /t:IvyEngineRuntime /p:Configuration=Debug /p:Platform=x64
tools\run_runtime_bridge_checks.cmd x64\phase6-bridge-check
```

Runs protocol/axes/probe/phase6 validators, in-process controller checks (including consumer assertions), Python unit tests, and Release/Editor compile-out. Optional second argument runs live acceptance runners:

```bat
tools\run_runtime_bridge_checks.cmd x64\phase6-bridge-check x64\Debug\IvyEngineRuntime.exe
```

Expected highlights:

- `PASS Phase 6 observe extensions + list_objects/pause/resume/step/perf protocol`
- `PASS world manip reset_scene/set_transform/set_velocity/impulse protocol`
- `PASS controller probe validators`
- `consumer assertions executed: 63`
- `All controller bridge checks PASSED`
- `PASS Release compile-out ...` / `PASS Editor compile-out ...`
- With an exe: mouse/controller/probe/world-manip acceptance `"status": "PASS"`

### Standalone consumer-trace runtime acceptance

Interactive Windows session:

```bat
py -3 tools\runtime_bridge_controller_probe_acceptance.py --exe x64\Debug\IvyEngineRuntime.exe --out-dir x64\phase5-probe-runtime
```

Stages the isolated fixture under the out-dir, sets bridge env vars, uses stock `runtime_test_client.py`, asserts LT/RT, all 14 buttons, combined input, right-stick left-stick neutrality, and disconnect cancel via `controller_probe` sequence windows, then `quit` with exit 0. Writes `result.json`.

## Known limitations

- **Injected triggers/sticks are exact floats** returned by `Controller_GetLeftTrigger` / `Controller_GetRightTrigger` (and stick getters) on the Debug inject path. They do **not** go through XInput BYTE quantization or deadzone remapping. Physical-pad quantization is out of scope for these acceptance checks.
- **Unplug physical controllers** (or ignore non-isolated frames) when asserting cancel/idle suffixes: after injection clears, getters reflect hardware; a connected pad can keep `connected: true` and non-zero axes.
- Linux can run pure protocol/validator tests; it cannot claim Windows named-pipe or D3D11 runtime PASS.
- Single-Update press/release edges must be taken from the per-Update probe (or in-process harness), not from client completion polling alone.
