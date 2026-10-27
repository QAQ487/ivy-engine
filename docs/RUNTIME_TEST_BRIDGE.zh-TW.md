# Runtime Test Bridge（Phase 1）

只支援 `hello`／`status`／`quit`。冇 gameplay input、observation、capture、shell、任意檔案操作；唔係玩法驗收。

## 啟用

只喺 `_DEBUG && GAME_RUNTIME` 編譯。正常 Debug 預設 off；Release 完全冇 bridge API／worker。
由**同一 Windows 使用者嘅互動 session launcher**，為獨立 playable package 設定：

```powershell
$env:IVY_RUNTIME_TEST_BRIDGE='1'
$env:IVY_RUNTIME_TEST_RUN_ID=[guid]::NewGuid().ToString('N')
# Launcher 自行啟動新 build，同時記錄 exact PID／SessionId，同持有 process handle 作 watchdog。
```

run ID 必須係 1–64 個 ASCII 英數或 `-`。每輪必須用新 ID。
Endpoint：`\\.\pipe\IvyRuntimeTest-<PID>-<runId>`。
使用 `PIPE_REJECT_REMOTE_CLIENTS`、first-instance、防繼承 DACL；唯一 allow ACE 係 process token 嘅 user SID。
呢個只隔離其他使用者，唔防同一 user 嘅其他 process，亦唔保證對抗系統管理員／SYSTEM。
無效設定／建 pipe 失敗會 fail closed（冇 endpoint）；client startup deadline 會報錯，唔當成功。

```text
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 1 hello
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 2 status
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 3 quit
```

Client 只連本機，而且用 `GetNamedPipeServerProcessId` 驗 server PID。唔負責建立／殺 runtime；launcher 必須設啟動、ready 同退出總 deadline，只清理自己持有 handle 嘅 child。CLI exit 0 代表 request 成功，**唔代表 runtime 已正常退出**；launcher 仍要 wait 同檢查 exit code。

## Protocol v1

UTF-8 JSON，每個 request／response 以 LF 結尾；request 最多 4096 bytes（唔計 LF）。
只接受以下四個欄位；唔接受額外 tick budget／payload：

```json
{"protocolVersion":1,"requestId":1,"runId":"unique-run-id","verb":"hello"}
```

`requestId` 係 1–2147483647 整數，喺同一 run 內必須嚴格遞增，包括 reconnect。
有效 envelope 嘅未知 verb 亦消耗 ID；schema／version／run mismatch 唔消耗。
所有回應有 `protocolVersion`、`requestId`（無效 envelope 為 null）、`runId`、`ok`；失敗有 `error`。
Errors：`malformed_json`、`invalid_request`、`unsupported_version`、`run_mismatch`、`unknown_verb`、`duplicate_request_id`、`message_too_large`、`queue_full`。
Oversize 會斷線；該 error response 只係 best-effort，client 必須接受 disconnect 作失敗。

`hello`／`status` 回傳 exact PID、Windows session ID、capabilities，同**主線程複製嘅** ready／playing／state／scene。
初始 `loading`／ready false；完成 frame 後，scene path 非空且 preload inactive 才 ready。
`state` 由已知 ready／playing flags 推導，唔聲稱係精準 transition／simulation 狀態。
scene 只係 path（最多 1024 bytes）；冇 GameObject、simulation tick 或 render-frame 假數據。
I/O thread 唔讀 scene／GameObject／D3D；WinMain 喺 Update 同 render workers join 後 publish。

### Quit receipt

成功 `quit` 回應包含 `requiresReceipt:true`。Client **讀完及驗證該回應後**，再寫單獨一個 LF 作 transport receipt。
只喺收到 receipt 後，WinMain 才經 `EngineGUI_RequestExitGame()` 走既有 deferred WM_APP lifecycle。
唔係寫入 kernel buffer 就立即關 process，避免 reply 未讀就消失。
無 receipt／disconnect／2 秒 timeout 會放棄 pending quit，唔退出 runtime。

只有一個 connection，同一時間最多一個 pending quit；hello／status 讀 copied state，唔排 main-thread command。
Kernel pipe buffer 亦有固定上限。無 multi-client fairness 保證；同一 user 可以暫時獨佔 connection。
每個 frame read／write 同 quit receipt deadline 2 秒；idle 亦會 disconnect，可 reconnect。
Connect wait 可取消；shutdown 先 signal stop，取消並 reap overlapped I/O，再 join worker。
主線程唔等 client I/O。冇無限 request queue。

## 自動檢查（唔啟動 GUI）

喺 repo root：

```text
tools\run_runtime_bridge_checks.cmd x64\phase1-test\<new-check-directory>
```

需要 VS 18 Community 同 `py -3`。C++ protocol／DACL／off／shutdown checks、真實 pipe + Python stdlib client tests，同 Release compile-out translation-unit check。
Harness `--serve` 最多 15 秒，自行正常退出；Python subprocess tests 有獨立 timeout，只終止自己建立嘅 harness。
呢個唔代替 Session 1 真實 runtime hello／status／quit、off-mode、正常 close 回歸；亦唔係完整 Release runtime build。


## 實作進度與驗證記錄

更新：2026-10-27（JST）。以下係實際完成狀態，唔係待實作承諾。

- [x] **Phase 0 — Runtime 環境驗證**：fresh Debug x64 build、隔離 script-free 3D sample package；已登入 Windows Session 1 成功執行 update／draw、取得 GPU samples，exit 0。
- [x] **Phase 1 — 最小 Runtime Test Bridge**：opt-in、本機 named pipe、current-user DACL、hello／status／quit、有限指令與 timeout、可取消 I/O、Python stdlib client、正常 shutdown。
- [ ] **Phase 2 — Keyboard 測試輸入**：未開始。
- [ ] **Phase 3 — Object 狀態觀察與 PNG 截圖**：未開始。
- [ ] **Phase 4 — 固定移動／跳躍／落地驗收 runner**：未開始。
- [ ] **Phase 5 — Mouse／controller／遊戲事件**：未開始。
- [ ] **Phase 6 — Agent 互動探索**：未開始。

### 已執行驗證

- 隔離 Debug x64 full build 通過（372 warnings、0 errors）；review 修正後 incremental build 再通過（14 warnings、0 errors）。Warnings 包括既有 encoding／deprecated APIs，唔代表 warning-free。
- C++ protocol／snapshot／quit-slot、實際 pipe DACL、off／取消／join checks 通過。
- Python client 6 個 tests 通過，包括真正 named pipe、錯誤請求、重連、partial-frame timeout、oversize recovery、backpressure／quit receipt 等待期間取消。
- Release bridge translation-unit compile-out check 通過；**未做完整 Release Runtime build／runtime 測試**。
- 真正 Session 1 runtime 四項整合通過，全部 exit 0：hello/status/quit；預設 off 時冇 endpoint、update/draw/GPU samples 各 300；idle close；partial-frame close。
- Close tests 使用既有 Debug `IVY_SKIP_CLOSE_CONFIRM=1`，只驗證 close／shutdown 路徑，唔代表手動確認 dialog 已測。
- Review 發現 UTF-8 scene path 截斷同 timeout subtraction 邊界問題，已補回歸並修正；完整 standalone checks、重建同四項 runtime 整合重跑通過；獨立覆核無剩餘具體 blocker。
- 最終測試 executable SHA256：`72178408652553484ab31e9cf2a62f1ddf70fa02d02bbda5b56d088e2f46eb33`。此值只識別當次 binary，未來 rebuild 唔要求相同。
- 一次性測試排程已移除，測試 child processes 已退出；冇建立常駐 launcher。

### 已知環境限制與未涵蓋項目

- 當次 SSH Session 0 可建立 hardware D3D device，但 windowed swap chain 回 `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE`；同一 package 喺已登入 Session 1 可成功運行。唔宣稱已解決 Session 0，亦唔泛化到所有機器。
- Runtime 目前需由已登入使用者 session 啟動；鎖屏、登出、重開機後自動運行未驗證。建立可重用排程／launcher 需另外批准。
- 呢次係 script-free engine smoke，唔代表 content DLL build、角色操作、玩法斷言、截圖／視覺、resize／scene 切換互動或完整 Editor 回歸已測。
- 部分 transport regression tests 係實作後補測，唔聲稱全項嚴格 test-first；review 邊界修正有保留 RED／GREEN 紀錄。
- Per-run logs／package／manifest 保留於 ignored `x64/phase1-test/`，唔提交 binary、私密 log 或臨時排程腳本。
