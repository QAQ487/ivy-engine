# Runtime Test Bridge（Phase 1／Phase 2 checkpoint）

支援 `hello`／`status`／`quit` 同有限度內部 `keyboard` 測試輸入。冇 object observation、capture、shell 或任意檔案操作；唔係玩法驗收。

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
`hello`／`status`／`quit` 只接受以下四個欄位；`keyboard` 只額外接受下節嘅 `keys`／`updates`，冇 simulation tick budget：

```json
{"protocolVersion":1,"requestId":1,"runId":"unique-run-id","verb":"hello"}
```

`requestId` 係 1–2147483647 整數，喺同一 run 內必須嚴格遞增，包括 reconnect。
有效 envelope 嘅未知 verb 亦消耗 ID；schema／version／run mismatch 唔消耗。
所有回應有 `protocolVersion`、`requestId`（無效 envelope 為 null）、`runId`、`ok`；失敗有 `error`。
Errors：`malformed_json`、`invalid_request`、`unsupported_version`、`run_mismatch`、`unknown_verb`、`duplicate_request_id`、`message_too_large`、`queue_full`、`not_ready`。
Oversize 會斷線；該 error response 只係 best-effort，client 必須接受 disconnect 作失敗。

`hello`／`status` 回傳 exact PID、Windows session ID、capabilities，同**主線程複製嘅** ready／playing／state／scene。
初始 `loading`／ready false；完成 frame 後，scene path 非空且 preload inactive 才 ready。
`state` 由已知 ready／playing flags 推導，唔聲稱係精準 transition／simulation 狀態。
scene 只係 path（最多 1024 bytes）；冇 GameObject、simulation tick 或 render-frame 假數據。
I/O thread 唔讀 scene／GameObject／D3D；WinMain 喺 Update 同 render workers join 後 publish。

### Phase 2 — 內部 Keyboard 操作

```json
{"protocolVersion":1,"requestId":10,"runId":"unique-run-id","verb":"keyboard","keys":["W","SPACE"],"updates":3}
```

- `keys` 必須係 1–9 個不重複名稱，白名單只有 `W A S D SPACE UP DOWN LEFT RIGHT`（大小寫敏感）。
- `updates` 係 1–120 整數；拒絕 bool、浮點、overflow、重複／未知 key、額外欄位。
- 必須 ready 同 playing，否則 `not_ready`；同一時間只容許一個 queued／active／releasing operation，否則 `queue_full`。
- 呢個係 **engine `Update()` 呼叫次數**，唔係 simulation tick、physics step 或 render frame；freeze／fade／transition 可令該次 Update 冇執行 scene simulation，仍會計數。
- 完全唔用 `SendInput`、OS keyboard message 注入、前景視窗／focus 操作。只喺 WinMain、`KeyLogger_Update` 前覆寫 engine keyboard；worker 只處理 protocol／copied state。
- 首個 Update 先做全鍵 neutral preparation（`status:queued, preparing:true`），令已有實體 pressed／hold 狀態唔會吞掉 synthetic trigger／繼承 hold time（唔會清除既有 double-tap timestamp）；之後先維持指定 keys **恰好 N 次 Update**。
- 第 N 次 Update 完成後仍係 `active, remainingUpdates:0`；下一次 Update 全鍵 neutral，狀態 `releasing`，等完整 Update 返回先報 `completed`。所以 N=1 亦真正有一次 pressed／trigger，再有一次 release。
- preparation、按住同 release Update 全程隔離實體 keyboard，包括 focus reset。實體 messages 只更新另外嘅 physical state，唔可以覆蓋測試狀態。完成 release 後，下一次正常 Update 才恢復最新 physical state；仍按住嘅實體鍵可以重新變 pressed，唔代表 synthetic key 卡住。
- `hello`／`status` 嘅 `keyboard` 有 `status`（idle／queued／active／releasing／completed／cancelled）、operation `requestId`、`remainingUpdates`、`preparing` 同 `cancelRequested`。冇任意 object／輸入觀測 API。
- disconnect／2 秒 idle timeout 取消 queued 或 active 操作；queued 未套用可以立即 `cancelled`。已套用嘅必須由 owner 喺下一次 Update 套 neutral，等 release Update 返回先 `cancelled`。停止 playing／preload 開始亦取消。Stop／destructor 喺 owner thread 清除 synthetic state、取消 I/O 同 join。
- 每個操作由接受起有 **5 秒 wall-clock cap**。worker 收 request 時檢查 deadline；冇 traffic 則原有 2 秒 read timeout 斷線取消。主線程 Begin／Finish 亦檢查 deadline。**如果主線程 stalled，worker 唔會跨線程清 live input**；會標記 `cancelRequested:true`，保留未完成狀態，實際 release 要等下一次 Update。唔會虛報 completed；launcher watchdog 仍然必要。

CLI 必須保持同一 connection 等 completion，唔可以送完 keyboard 就斷線：

```text
py -3 tools/runtime_test_client.py --pid <PID> --run-id <runId> --request-id 10 keyboard --keys W SPACE --updates 3
```

`Client.keyboard()` 會有界 poll status（預設 completion deadline 7 秒，上限 10 秒）；出錯／timeout 會 close connection 取消。成功回傳最後 status response，`keyboard.requestId` 係原操作 ID；最外層 `requestId` 係最後一次 poll ID，之後嘅 request 必須大過佢，**唔好假設下一個一定係 11**。CLI exit 0 只代表 release Update 已完成，唔代表角色移動／跳躍成功。

### Phase 3 — Object 狀態觀察與可控角色 Fixture

```json
{"protocolVersion":1,"requestId":12,"runId":"unique-run-id","verb":"observe","target":"Player"}
```

- `target` 必須係 1–64 長度之英數、`-` 或 `_` 字串，不可為空或過長。
- 必須 ready 同 playing，否則 `not_ready`。
- 狀態由 WinMain 於 Update 及 render workers join 後複製，worker 絕不直接觸碰 Scene 或 GameObject。
- 回傳目標名稱、位置（`position` {x,y,z}）及速度（`velocity` {x,y,z}）；若目標不存在則回傳 `found: false`。
- 專用 Scene `samples/scenes/scene_runtime_bridge_player.json`：包含地標（`Landmark_North`、`Landmark_South`、`Landmark_East`、`Landmark_West`）、地板及以非動畫可見模型（`Box_Green.fbx`）呈現之可控 `Player`，於 Debug|x64 靜態編譯並註冊 `SamplePlayerScript`。
- PNG 截圖功能本 checkpoint 明確列為 pending，待後續具備執行期視覺驗證環境時實作。

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

需要 VS 18 Community 同 `py -3`。C++ protocol／DACL／off／shutdown、實際 Keyboard／KeyLogger lifecycle checks、真實 pipe + Python stdlib client tests，同 Release／Editor compile-out translation-unit checks。
Harness `--serve` 最多 15 秒，自行正常退出；Python subprocess tests 有獨立 timeout，只終止自己建立嘅 harness。
呢個唔代替 Session 1 真實 runtime hello／status／quit、off-mode、正常 close 回歸；亦唔係完整 Release runtime build。


## Phase 2 checkpoint 驗證

- 完整 standalone checks 通過：C++ protocol／DACL／shutdown；真實 Keyboard／KeyLogger 嘅 N=1、N>1、trigger／hold／release、preheld 同 key、physical／focus isolation、queued／active disconnect、重連、stalled deadline、Stop／restart；Python 10 個 tests，包括 completion、CLI 真實執行、payload validation 同 timeout-close；Release／Editor bridge／injection API compile-out 同正常 physical input。
- 已保留各修正嘅 RED→GREEN logs；早前 coding-agent 產物並非完整功能，已針對真實 behavioral failures 修正，唔將編譯失敗當作 behavioral RED。
- `key_logger.h/.cpp` 只保留已批准嘅 CP932→UTF-8 BOM 無損轉碼，冇改 logger 邏輯；既有 `system_timer.h` encoding warnings 未擴大範圍處理。
- 隔離 fresh Debug x64 Runtime full build 通過（373 warnings、0 errors；唔係 warning-free）。已用獨立 OutDir／IntDir，同停用 pre／post／pre-link build events，冇執行 copy-back。其後已完成下述 Session 2 runtime 整合；角色操作、object observation、PNG 同 gameplay 未測；Phase 3 之後仍未開始。
- 本輪 logs／isolated build artifacts 保留喺 ignored `x64/phase2a-agent/`；冇 commit／push；CLI 臨時 grants 已移除並逐 byte 還原設定，一次性測試排程已移除，冇常駐 launcher。

### 2026-10-28（JST）互動 runtime 驗證

- 使用同一 fresh Debug executable，SHA256 `f46facc6489c71e188edbf1a01bb445790b1208ee0e35e9a51ed5229e8c000ff`，launcher 同四個 child 都係 Windows Session 2。
- Keyboard W 持續 1／30／120 次 engine Update，各自等到 `completed` 同 `remainingUpdates:0`；active 操作斷線後重連，確認 `cancelled` 同 remaining 0。呢個係真實 runtime bridge integration；精確 press／hold／release 同 physical isolation 由前述真實 Keyboard／KeyLogger standalone checks 驗證，唔係 gameplay 斷言。
- 四項全部正常 exit 0：hello/status/keyboard/cancel/quit；預設 off 冇 endpoint 且 update／draw／GPU samples 皆為正；idle close；partial-frame close。close 使用 Debug `IVY_SKIP_CLOSE_CONFIRM=1`，唔代表手動 dialog 互動。
- 第一輪診斷 runner 因 Python local-variable shadowing 失敗；已清理該 exact child。只修正 runner、唔改 engine，同一 executable 完整重跑通過。成功輪所有 child 已退出，一次性排程已移除。
- 成功證據：`x64/phase2a-agent/25771ef1390b438383e027f9c15bdd02/runtime-8a9acc02655f4897979b1b8d86e2888e/observations/result.json`。
- 使用 script-free `phase1_fixture.json`；未驗證角色玩法、截圖、完整 Release／Editor executable 或鎖屏／登出運行。獨立 source review 冇發現阻擋問題；neutral preparation 唔清除 double-tap 計時。

## 實作進度與驗證記錄

更新：2026-10-28（JST）。以下係實際完成狀態，唔係待實作承諾。

- [x] **Phase 0 — Runtime 環境驗證**：fresh Debug x64 build、隔離 script-free 3D sample package；已登入 Windows Session 1 成功執行 update／draw、取得 GPU samples，exit 0。
- [x] **Phase 1 — 最小 Runtime Test Bridge**：opt-in、本機 named pipe、current-user DACL、hello／status／quit、有限指令與 timeout、可取消 I/O、Python stdlib client、正常 shutdown。
- [x] **Phase 2 — Keyboard 測試輸入**：內部注入、standalone checks、full Debug build 同 Session 2 fresh runtime 整合通過；唔代表角色操作或玩法驗收。
- [ ] **Phase 3 — Object 狀態觀察與 PNG 截圖**：部分實作；固定診斷鏡頭下移動、觀察同 PNG 已實測一致通過，但退出 crash 未修、角色仍係方塊組合，詳見最新 checkpoint。
- [ ] **Phase 4 — 固定移動／跳躍／落地驗收 runner**：部分實作；移動／觀察／截圖 assertion runner 已加入並實測通過，跳躍／落地未涵蓋。
- [ ] **Phase 5 — Mouse／controller／遊戲事件**：部分實作；Slice A（mouse 內部注入＋bridge `mouse` verb＋測試模式視角觀察）已通過 runtime 驗收，controller stick／遊戲事件／mouse 點擊 UI 未開始。
- [ ] **Phase 6 — Agent 互動探索**：未開始。

### Phase 1 已執行驗證

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


### Phase 3 最新 checkpoint：PNG 已產出，視覺／移動驗收未通過

- 本節取代上方 Phase 3「未開始」嘅舊狀態；Phase 3 仍未完成。
- Antigravity 新增 capture hook 後，full Debug build 發現 pointer 宣告 scope 錯誤（C2065）；已修正至 file scope，同一隔離輸出 incremental build 通過：70 warnings、0 errors。測試 executable SHA256：`8cfe52aa4040d9cd51146b678b1e45b22828f29f463759a7c73e3580965cff77`。
- 實際 Session 2 runtime capture 兩張 1920×1080 PNG，已檢查 PNG header、尺寸及實際畫面；唔係 timing JSON／桌面截圖。capture 位於 render worker join 後、Present 前。
- **PNG 傳輸／寫檔：PASS；角色外形／鏡頭構圖：FAIL。** 畫面只見極近、裁切嘅綠／藍幾何，冇完整可辨識角色，亦冇清晰地面／距離標記。新增外形係方塊組成頭／四肢，唔係完整角色模型／動畫。
- **本輪移動重驗：FAIL。** W 120 次 engine Update 回報 completed、remainingUpdates 0，但 Player 水平位移只有 `0.09566329020874241`，未通過既定 `>0.1` 門檻；冇降低驗收標準。舊 binary 已驗證嘅約 11.61 位移唔移植至今輪 binary。
- 此輪 fail-fast 喺移動斷言停止，quit 未執行；診斷 runner 已清理 child，所有 owned child 已退出，一次性排程已移除。之前直接 quit 嘅 `0xC0000005` 仍為使用者明確延後處理嘅 KNOWN_FAILURE，冇修改既有 shutdown 次序。
- SamplePlayerScript 使用第三人稱相機及相機相對移動；本輪視角／移動失敗原因尚未確認，唔宣稱實體 mouse 係已證實根因。需先釐清診斷 fixture 鏡頭／外部輸入處理範圍，再改動，避免擴入未授權 mouse 功能。
- 證據：`x64/phase2a-agent/bd27685e261140d8be41c5dbc073b642/runtime-7d613fe7867740d5ae10bbb12f1a90b1/observations/result.json`；同目錄 before request 23、after request 167 PNG。
- Capture UTF-8 路徑、既有檔拒絕覆寫、disconnect／deadline 等 hardening 尚未完成獨立覆核；唔將成功兩張 PNG 當作完整安全性／生命週期驗收。


### 2026-10-28（JST）第二輪 checkpoint：固定診斷鏡頭、可重現移動同 PNG

- 上一輪「W 120 Updates 只行 0.096」同「鏡頭極近／構圖不穩」嘅成因已收窄：正常第三人稱鏡頭每個 update 會用 `Mouse_GetClientPosition()` 讀 OS 游標、同畫面中心比較，再 `SetCursorPos()` 強制回中。測試視窗唔係前景視窗／游標無法回中時，就會得到一個大致固定嘅大 delta，令 pitch 一路被推到上下限、yaw 亦持續轉 → 構圖不穩、移動方向彎曲。呢個係推論（有源碼同兩次 run 嘅行為差異支持），唔係有 stack trace 證明。
- 新增只有測試模式啟用嘅固定鏡頭：`IVY_RUNTIME_TEST_FIXED_CAMERA=1`（實作於 `src/app/camera/third_person_camera.cpp`，只喺 `_DEBUG && GAME_RUNTIME` 生效）。啟用時完全跳過游標讀取／回中同 right-stick look，pitch 固定 15°。正常 sample 玩法、Release／Editor 程式路徑不變（原始檔案備份 `x64/phase2a-agent/bd27685e261140d8be41c5dbc073b642/third_person_camera.cpp.orig`）。Release 未重新編譯驗證，只做過源碼檢視。
- 同一 fresh executable SHA256 `5b120b64b7a4b90ca753621f6fd7cdccba3e394452ee34710ce8ff9977e6f6eb`（Debug x64，incremental build 0 errors；warning 仍在），固定鏡頭下 W 持續 120 次 engine Update：
  - velocity 全程 `(0, 6.0)`（moveSpeed 6.0）；
  - 位移 `delta = (0, 0, 11.612895011901855)`，水平距離 **11.612895011901855**，y 不變；
  - 兩次獨立 run 得到 **byte 相同嘅 PNG**，即構圖可重現。
- PNG 驗收：1920×1080，PNG header 同尺寸已核對，並人手檢視實際畫面。畫面顯示方塊角色（綠身、藍頭／手、深色腳）由背後可見，穩定地平線同大面積棕色地板；角色佔畫面高度約 13%。呢個係**方塊組合 placeholder**，唔係人物模型、冇動畫。兩個 landmark cube 唔喺當次構圖內（角色已沿 +z 行離）。
  - `before`：168116 bytes，SHA256 `9fefaf0609128312cfc30195ba47f93fb645ce9b041bf7f85fbeec3762dfb1ee`
  - `afterW`：265983 bytes，SHA256 `0d7cfc03779c7d8364cc14dd39263270ff68e4b16b928cdfd3f35b7f03cae24e`
- Bridge 行為（已由源碼確認，唔係故障）：`engine/testing/runtime_test_bridge.cpp` 每次讀取請求有 2000 ms deadline，逾時會 `DisconnectNamedPipe()` 收線。即係**閒置超過約 2 秒就會斷線**，client 需要重連或保持輪詢。已實測：閒置 3 秒後斷線，重新連線後 `status` 正常，`reconnects=1`。
- 已知、唔會喺今輪修：`quit` 收到 receipt 後程序仍以 **3221225477（0xC0000005）** 退出，兩次重現；Terry 明確指示暫時放低。呢個唔再當作移動／觀察功能嘅 blocker，但亦唔標示 shutdown 通過。
- 已知行為：按住 `D`（strafe）會原地打圈。`SamplePlayerScript` 會將 `m_yaw = atan2(wish.x, wish.z)`，而相機反過來跟 `m_yaw`，形成正回饋；屬 sample controller 特性，唔係 bridge 問題，未修改。
- 仍未做：精確 press／hold／release 以外嘅 W 以外按鍵驗收、A/D／跳躍、截圖路徑 hardening（UTF-8、拒覆寫、斷線／deadline）獨立覆核、完整 Release／Editor 編譯、人物模型同動畫、Phase 4 assertion runner。
- 證據：`x64/phase2a-agent/bd27685e261140d8be41c5dbc073b642/runtime-7e578b5e9c9d4a4b9ec8155aa4641689/diag/result.json`（移動＋PNG hash＋quit exit code）、`.../runtime-fedc843ebed04b1d9f3fe899381acee2/diag/`（第一次固定鏡頭 run）、`.../runtime-ce8759a16a504e5f9fbb0e29507748d0/diag/`（閒置存活 17 秒證據）。
- 本輪冇 commit／push；一次性測試排程已移除並核對；測試過程冇再改 CLI 設定。


### 2026-10-28（JST）第三輪 checkpoint：Phase 4 acceptance runner 可用

- 新增 `tools/runtime_bridge_phase4_acceptance.py`（新檔，未 commit）。佢自己喺互動 session 啟動一個 Debug GAME_RUNTIME（bridge + 固定診斷鏡頭），再做可重現斷言：ready／scene 正確 → observe Player → 按住 W 120 次 engine Update（期間約每 60 ms 抽樣 position／velocity）→ 檢查位移、直線度、速度同高度 → 要求 runtime 出 PNG 並核對 header／尺寸 → quit receipt → 記錄 native exit code。
- 實測（Session 2，exe SHA256 `5b120b64b7a4b90ca753621f6fd7cdccba3e394452ee34710ce8ff9977e6f6eb`，5.23 秒）：
  - **movement PASS**：水平 `11.612895011901855`、delta `(0, 0, 11.612895011901855)`、`maxLineDeviation` 0.0、速度 5.9999995–6.0、30 個 active samples、`completed` true。
  - **capture PASS**：1920×1080，SHA256 `0d7cfc03779c7d8364cc14dd39263270ff68e4b16b928cdfd3f35b7f03cae24e`（同前兩次 run byte 相同）。
  - **shutdown**：exit code `3221225477`（0xC0000005）→ 記為 `KNOWN_FAILURE`，預設唔會令整個 run 失敗；要強制 exit 0 可以用 `--require-clean-shutdown`。
  - 整體 status `PASS_WITH_KNOWN_SHUTDOWN_FAILURE`；child 已清理（`childCleanedUp: true`）。
- 用法：`python tools/runtime_bridge_phase4_acceptance.py --exe <package>/IvyEngineRuntime.exe --out-dir <唔存在嘅新目錄>`；`--out-dir` 必須係新目錄以避免覆寫。
- 仍未涵蓋：跳躍／落地（`SamplePlayerScript` 冇跳躍輸入）、A／D strafe 嘅原地轉圈（只記錄、唔斷言）、完整 Release／Editor build、角色模型同動畫。
- 證據：`x64/phase2a-agent/bd27685e261140d8be41c5dbc073b642/runtime-0f9c11302c4944eb90649a021ac43f56/run/result.json`。

### 2026-10-28 Phase 5 Slice A checkpoint：滑鼠內部注入與視角觀察

- **內部注入**：於 `engine/input/mouse.cpp/.h` 加入 `Mouse_TestInject(dx, dy, buttonsMask, wheel)`、`Mouse_ClearInjection()` 及 `Mouse_IsInjectionActive()`（`_DEBUG && GAME_RUNTIME` 專屬）。注入期間隔離實體 Win32 訊息與焦點變更；結束後恢復實體輸入。
- **Bridge `mouse` 動詞**：
  - Payload 格式：`{dx, dy, buttons, wheel, updates}`。`dx, dy` 限制於 `[-600, 600]`；`buttons` 支援 `LEFT, RIGHT, MIDDLE` 子集（最多 3 個）；`wheel` 限制於 `[-120, 120]`（預設 0）；`updates` 介於 `1..120`。
  - 生命週期：1 次中立 preparation update、N 次 hold、1 次中立 release update。狀態暴露於 `mouse: {status, requestId, remainingUpdates, cancelRequested}`。
- **視角消費者**：於 `src/app/camera/third_person_camera.cpp` 中，僅在 `IVY_RUNTIME_TEST_FIXED_CAMERA=1` 且 `Mouse_IsInjectionActive()` 時透過 `Mouse_GetDelta` 取得視角 look 輸入，其餘時刻視角 delta 為 0，絕不調用 `SetCursorPos` 或讀取 OS 游標。
- **未驗證項目**：Phase 5 Slice B／滑鼠點擊 UI／跳躍動作、實機遊戲內旋轉角度量測斷言。已知之退出後存取違規（0xC0000005）維持原樣延後處理。


### 2026-10-28 Phase 5 Slice A 獨立 runtime 驗收（父層核實）

- CLI 自報並已核對 artifact：`CHECKS_EXIT=0`（log 內含 `All mouse bridge checks PASSED`）見 `x64/phase2a-agent/e84a59d9002e4bfea3754edcc2762538/checks.log`；`BUILD_EXIT=0` 見 `.../95f2dde53ed34c98bec396857b458e96/build.log`；fresh exe 15,236,096 bytes，SHA256 `f379413b71ba92c524f1fd8307426ed75695c89bef5f19996b740959a8610b58`。`tools/run_runtime_bridge_checks.cmd` 已為每條 link line 加入 `engine/input/mouse.cpp` 同共用 `tools/test_renderer_stubs.cpp`，並註冊同執行 `runtime_bridge_mouse_check.exe`。
- 同一 fresh exe 重跑 Phase 4 acceptance（Session 2）：`PASS_WITH_KNOWN_SHUTDOWN_FAILURE`，水平位移 `11.612895011901855`、`maxLineDeviation` 0.0、capture PNG SHA256 `0d7cfc03779c7d8364cc14dd39263270ff68e4b16b928cdfd3f35b7f03cae24e`（同 mouse 改動前里程碑圖 byte 相同 → 證明預設／固定鏡頭路徑無被 Slice A 影響）。證據：`.../runtime-15922b3f459a44b4870912e610fc180d/phase4/result.json`。
- 新增獨立 runtime 檢查（父層自己嘅 harness，唔在 repo）：`.../runtime-3ffb021dfb4f4177b31d1fd082231c0d/mouse/result.json`，全部 PASS：
  - capabilities 含 `mouse`；`status.mouse` 存在。
  - `mouse` ack `{ok:true,status:queued}`；樣本顯示 `remainingUpdates` 由 60 遞減到 0 並轉 `completed`（requestId 28／72／118）。
  - 注入 yaw delta 後畫面真係轉：`before`、`afterForward`、`afterReverse` 三張 1920×1080 PNG SHA256 全部唔同，實際檢視 `afterForward` 見到 landmark cube 入鏡，證明視角確實旋轉。
  - 期間 Player 位置全程 `(0, 0.49926239252090454, 0)`，速度 `(0,0,0)`：注入 mouse 唔會移動角色（同 keyboard 路徑完全隔離）。
  - 邊界／非法 payload（dx=601、多餘 key、重複 button、未知 button、wheel=999）全部被拒。
  - 註：被拒請求嘅回覆唔會 echo `requestId`（現有 `invalid_request` 形狀，所有 verb 一樣），所以 stdlib client 會以 `response identity mismatch` 拋錯。呢個係現行協議 wart，未改；client 要當「唔匹配嘅 id」＝拒絕。
  - 注意：固定鏡頭模式下 mouse 係轉**鏡頭**（繞住角色），唔係轉角色本人，所以畫面仍然由背後望住角色 —— 呢個係刻意設計。
  - 未解釋觀察：轉身後嘅影格，地平線望落有輕微傾斜。鏡頭路徑只有 yaw／pitch，冇 roll 項，所以未能解釋；只作記錄，未宣稱係 bug 或已驗證。
- `quit` 後 exit code 依然 `3221225477`（0xC0000005）→ 維持 `KNOWN_FAILURE`，Terry 明確延後，未改 shutdown。
- 未涵蓋：controller stick 注入、遊戲事件、mouse 點擊 UI、固定 Updates 以外嘅 mouse 組合、完整 Release／Editor 執行檔、鎖屏／登出情境。
