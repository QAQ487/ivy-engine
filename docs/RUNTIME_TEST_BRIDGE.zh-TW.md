# Runtime Test Bridge（摘要）

Debug-only named-pipe harness，只喺 `_DEBUG && GAME_RUNTIME` 編譯；Release／Editor 會 compile out。唔係玩法輸入。

**完整操作說明（啟用、protocol、controller／probe、`--scene`、pause／step／perf、檢查指令）以英文為準：**  
[`RUNTIME_TEST_BRIDGE.md`](RUNTIME_TEST_BRIDGE.md)

## 快速啟用

```powershell
$env:IVY_RUNTIME_TEST_BRIDGE='1'
$env:IVY_RUNTIME_TEST_RUN_ID=[guid]::NewGuid().ToString('N')
```

Endpoint：`\\.\pipe\IvyRuntimeTest-<PID>-<runId>`（本機、current-user DACL）。

可選：`IvyEngineRuntime.exe --scene rom/scene/scene_runtime_bridge_player.json`（只喺 bridge 啟用時生效；路徑要 `rom/`-relative、`.json`，經 `rom/` → `resources/` resolve；失敗會非零退出）。

## 常用檢查

```text
tools\run_runtime_bridge_checks.cmd x64\phase6-bridge-check
tools\run_runtime_bridge_checks.cmd x64\phase6-bridge-check x64\Debug\IvyEngineRuntime.exe
```

需要 VS 同 `py -3`。新 verbs：`list_objects`、`pause`／`resume`／`step`、`perf`；`observe` 加咗 rotation／quaternion／scale／angularVelocity／rigidbody.kinematic。

## 狀態摘要

- Phase 1–2（hello／status／quit／keyboard）同 Phase 4–5（mouse、controller axes／buttons／triggers、checkpoint `events`、consumer probe）已於 Windows Debug runtime 核實。
- Phase 6：`--scene`、observe 擴充、`list_objects`／pause／step／perf；mouse／controller acceptance 會 stage fixture 並傳 `--scene`（避免 testbed 預設 `scene_sample_menu.json`／全量 boot preload 導致 not-ready）。
- Fixture：`testbed/rom/scene/scene_runtime_bridge_*.json`；玩家模型用 `rom/model/player/`。
