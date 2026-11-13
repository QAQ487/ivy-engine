# Runtime test bridge (condensed)

Debug-only named-pipe harness for **Runtime** (`GAME_RUNTIME`) and **Editor** Play mode. Not shipping input. Full guide: [`../RUNTIME_TEST_BRIDGE.md`](../RUNTIME_TEST_BRIDGE.md).

**Linux note:** offline protocol/unit checks exist; do **not** claim Windows live named-pipe / D3D verification from this environment.

## Availability

| Build | Bridge |
|-------|--------|
| Debug Runtime (`_DEBUG` + `GAME_RUNTIME`) | Opt-in → `host: "runtime"` |
| Debug Editor (`_DEBUG`, no `GAME_RUNTIME`) | Opt-in → `host: "editor"` |
| Release (`NDEBUG`) | Compiled out (`api:IVY_RUNTIME_TEST_BRIDGE_AVAILABLE` unset) |

## Enable (env)

| Variable | Role |
|----------|------|
| `IVY_RUNTIME_TEST_BRIDGE` | Must be exactly `1` |
| `IVY_RUNTIME_TEST_RUN_ID` | Required; 1–64 ASCII alnum or `-` |
| `IVY_RUNTIME_TEST_OUTPUT_DIR` | Optional PNG capture directory |
| `IVY_RUNTIME_TEST_FIXED_CAMERA` | Optional; skip mouse/right-stick look |
| `IVY_SKIP_CLOSE_CONFIRM` | Optional; skip close dialog in Debug |

Pipe: `\\.\pipe\IvyRuntimeTest-<PID>-<runId>` (local only). Stock client: `tools/runtime_test_client.py`.

## Protocol

UTF-8 JSON, one LF-terminated frame. Envelope: `protocolVersion` (1), `requestId` (strictly increasing), `runId`, `verb`. Additive identity: `host` (`"runtime"` \| `"editor"`). Editor also reports `playMode` (`"none"` \| `"viewport"` \| `"fullscreen"`).

## Common verbs

`hello`, `status`, `quit`, `keyboard`, `mouse`, `controller`, `controller_probe`, `observe`, `list_objects`, `pause`, `resume`, `step`, `perf`, `capture`, `events`, `reset_scene`, `set_transform`, `set_velocity`, `apply_impulse`, `apply_torque_impulse`, `set_setting`

`set_setting` whitelist: `frustumCullingEnabled`, `shadowCascadeCullingEnabled`, `spriteCullingEnabled`.

## Editor Play-mode verbs (`host: "editor"`)

| Verb | Role |
|------|------|
| `play_enter` | Enter Play; optional `mode` `"viewport"` (default) or `"fullscreen"` |
| `play_exit` | Stop Play; pipe survives |
| `load_scene` | Required `scene` `rom/...json`; Runtime → `unsupported_host` (use `reset_scene`) |

Edit mode (`playing=false`) rejects play-world ops with `not_playing`. Schema for scenario files: [`../schemas/runtime_bridge_scenario.v1.schema.json`](../schemas/runtime_bridge_scenario.v1.schema.json).
