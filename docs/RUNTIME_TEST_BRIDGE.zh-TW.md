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

需要 VS 同 `py -3`。新 verbs：`list_objects`、`pause`／`resume`／`step`、`perf`、`reset_scene`／`set_transform`／`set_velocity`／`apply_impulse`／`apply_torque_impulse`；`observe` 加咗 rotation／quaternion／scale／angularVelocity，以及 `rigidbody.{kinematic,mass,drag,angularDrag,friction,useGravity,localInverseInertia}`。連續 `apply_force`／`apply_torque`（N frames）未實作。

## 狀態摘要

- Phase 1–2（hello／status／quit／keyboard）同 Phase 4–5（mouse、controller axes／buttons／triggers、checkpoint `events`、consumer probe）已於 Windows Debug runtime 核實。`keyboard` 鍵名表已擴至全部 `KK_*`（無 Win 鍵；拒絕 ALT+F4）；wall deadline 隨 `updates` 放大（base 5000ms + per-update）；詳見英文 doc「Keyboard injection」。`observe` pose 優先讀 Transform／`GetWorldPose`（唔再用 load-time legacy `m_position`）。Linux offline 有 pose／deadline checks；Windows Debug runtime 核實仍 pending（若只跑 Linux）。
- `mouse` 可選絕對游標 `x`/`y`（backbuffer 像素；兩者要齊；範圍係當前 backbuffer `[0,w)×[0,h)`）。有帶時，`Mouse_GetPosition` 喺成個 mouse op isolating 期間（prep／hold／release）回傳注入座標，唔會 `SetCursorPos`；op 完／取消就清。Linux offline／in-process check 已覆蓋；**Windows live runtime 核實仍 pending**。世界座標→螢幕 helper 未做（follow-up）。詳見英文 doc「Mouse injection」。
- Phase 6：`--scene`、observe 擴充、`list_objects`／pause／step／perf；mouse／controller acceptance 會 stage fixture 並傳 `--scene`（避免 testbed 預設 `scene_sample_menu.json`／全量 boot preload 導致 not-ready）。
- World manip（plan Phase 3）：`reset_scene`／`set_transform`／`set_velocity`／impulse；fixture `scene_runtime_bridge_world_manip.json`；acceptance 經 `run_runtime_bridge_checks.cmd` 第二參數。
- Angular acceptance（plan Phase 4）：fixture `scene_runtime_bridge_angular.json`；`tools/runtime_bridge_angular_acceptance.py`；Linux 只跑 validator unit tests（`run_runtime_bridge_offline_linux.sh`），唔聲稱 Windows runtime PASS。Box friction rolling 係 expected-skip。
- Scenario runner（plan Phase 5）：`tools/runtime_bridge_scenario_runner.py` + `testbed/scenarios/*.json`；JSON Schema 喺 `docs/schemas/runtime_bridge_scenario.v1.schema.json`；AI／人寫 scenario 就唔使再加 Python acceptance。Linux offline 跑 schema／assert／fake-client unit tests；唔聲稱 Windows runtime PASS。
- Fixture：`testbed/rom/scene/scene_runtime_bridge_*.json`；玩家模型用 `rom/model/player/`。
