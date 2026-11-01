# Runtime Test Bridge（摘要）

Debug-only named-pipe harness，只喺 `_DEBUG && GAME_RUNTIME` 編譯；Release／Editor 會 compile out。唔係玩法輸入。

**完整操作說明（啟用、protocol、controller／probe、檢查指令）以英文為準：**  
[`RUNTIME_TEST_BRIDGE.md`](RUNTIME_TEST_BRIDGE.md)

## 快速啟用

```powershell
$env:IVY_RUNTIME_TEST_BRIDGE='1'
$env:IVY_RUNTIME_TEST_RUN_ID=[guid]::NewGuid().ToString('N')
```

Endpoint：`\\.\pipe\IvyRuntimeTest-<PID>-<runId>`（本機、current-user DACL）。

## 常用檢查

```text
tools\run_runtime_bridge_checks.cmd x64\phase1-test\<new-check-directory>
```

需要 VS 同 `py -3`。Controller inject／`controller_probe` 詳情見英文頁。

## 狀態摘要（2026-10）

- Phase 1–2（hello／status／quit／keyboard）同 Phase 4–5（mouse、controller axes／buttons／triggers、checkpoint `events`、consumer probe）已於 Windows Debug runtime 核實。
- Fixture：`testbed/rom/scene/scene_runtime_bridge_*.json`；玩家模型用 `rom/model/player/`。
- 詳細逐日 checkpoint 日誌已精簡；需要歷史細節時查 git history 中本檔舊版。
