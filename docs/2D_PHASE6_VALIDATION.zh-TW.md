# 第六階段：俯視 2D 範例及驗證

日期：2026-10-20。狀態：使用者已確認第六階段完成，首版 2D 計畫結案。以下自動驗證紀錄不等同代理已逐項操作 GUI；互動與視覺驗收採使用者的完成確認。

## 遊玩

TestProject 預設及最後開啟場景已設為 `rom/scene/scene_topdown_2d.json`。在 Editor 開啟 TestProject 後 Play；或直接執行 `TestProject/deploy_phase6/Ivy2DSample.exe`。

- WASD／方向鍵：移動，播放 idle／walk 動畫；Dynamic Rigidbody2D 負責撞牆。
- 相機是 Player2D 的子節點，跟隨角色位置；相機與角色均維持零 Z 旋轉，剛體鎖定旋轉。
- 收集三個標記更新 Score，完成後按 N 前往第二關，再完成可回第一關。
- R 重開目前關卡。停止播放時收集物的腳本恢復活動狀態，角色腳本重設分數；場景中的 authored 物件不會因收集而被刪除。
- 冒險者、HP 填色圖及字型沿用 TestProject 的既有資產，沒有新增圖像依賴。

同一批範例 JSON 而家喺 `testbed/rom/scene/`。2D 場景引用嘅 `rom/texture/sprite_demo/*.png` 係細 placeholder，字型仍係引擎 `rom/font/`。呢個 testbed 唔係一份已打包、資產齊全嘅部署套件。

## 已執行的驗證

- Debug x64 Editor、Runtime 與 TestProject GameScripts 編譯及連結成功。
- Editor 自動 smoke test 發現關閉時的全域析構次序問題：堆疊為 Grid::~Grid → GameObject::~GameObject → AudioManager::OnGameObjectDestroyed，音效 singleton 此時已被析構。Grid 改為明確初始化／Finalize 的 unique_ptr，於音效系統及 D3D 關閉前析構；保留網格 visibility 設定並在 Initialize 後套用。移除所有臨時關閉日誌及例外 handler 後，連續兩次 600 多幀 Editor 測試 exit code 0。此測試驗證關閉生命週期，不代表互動驗收。
- `tools/run_2d_regression_checks.cmd`：相機透視預設／正交 aspect、Sprite ray 選取、UI pivot、動畫時鐘／狀態、scene execution policy、30／60／144 FPS 物理、Box／Circle 撞牆、Trigger 進出／刪除 Exit／Clear、Raycast 篩選，以及五種 3D Collider 的 shape-only 序列化、舊資料及 Factory。
- `python tools/check_2d_sample.py <content-or-package-root>`：相機層級、場景出口、三個目標、Score 及貼圖／字型等參考，沒有開發機絕對路徑。呢個檢查要求資產檔就喺該 root 入面。in-repo testbed 用 `python3 tools/check_testbed_assets.py`，因為模型同字型留喺引擎 `rom/`。
- `tools/package_2d_sample.ps1` 依現有 GUI builder 的套件排列複製 runtime、libfbxsdk、project.json、rom 及 GameScripts，覆蓋同次建置的 shader／branding，並附上 Box2D MIT 授權；GUI builder 亦包含此授權檔。預設場景亦寫入套件內 default_scene.txt；不改寫來源的 default_scene.txt。完整 content rom 會一併複製，未新增資產裁剪系統。
- 空白 2D、完整 2D 範例及 3D 回歸副本均從 TEMP 工作目錄啟動獨立套件，處理暖機及取樣幀後以 exit code 0 結束，shader 路徑不依賴啟動器 cwd。
- WinMain 現在先設定資源工作目錄為 exe 所在目錄，然後才初始化 shader；開啟 content project 後仍由 ProjectManager 切換專案根目錄。
- `scene_regression_3d.json` 是既有 scene_cylinder_pairs.json 的測試副本，只另加明確的透視相機並指定 3D 模式，保留燈光／陰影、舊 Collider、剛體及 UI。實際渲染／更新 600 多幀而沒有 assertion 或異常退出；此結果不代表視覺陰影或碰撞落點已驗收。

## 量測方法與結果

同一台機、Debug x64、1280×720 backbuffer、render thread 開啟、60 Hz 主迴圈。每次獨立啟動，300 幀暖機後取 300 幀。系統列出 i5-12600、RTX 3060（driver 32.0.15.9649）及多個虛擬顯示 adapter；沒有額外確認 D3D 實際選用的 adapter。

設 `IVY_FRAME_CAPTURE=relative-report.json` 啟用量測，完成後自動退出。未設定時不建立 GPU query。CPU Update／DrawWorld 是 QPC 區段牆鐘時間；frameIntervalMeanMs 是包含節流及等待的幀間隔，不能當 CPU 工作量。GPU 使用 timestamp/disjoint，非阻塞跨幀讀取；範圍為 DrawWorld，排除 Present 及 Editor ImGui。

- 空白 2D：Update 0.213 ms，DrawWorld CPU 1.380 ms，GPU 0.027 ms（300 個有效樣本）；幀間隔 16.668 ms。
- 空白 2D：Sprite 提交 0 次／幀，文字提交 1 次／幀；pipeline IAVertices 36，對應 Debug FPS 六個字元。此配置的場景沒有其他 geometry。文字及 Sprite 計數器不代表一般 3D／ImGui 的完整 draw-call 總數。
- 空白 2D：Working Set 約 92.0 MiB、Private Bytes 128.0 MiB；這是進程記憶體，並非 VRAM。
- 完整 2D 範例：Update 0.397 ms、DrawWorld CPU 2.152 ms、GPU 0.067 ms；Sprite 116 次／幀，文字 1 次／幀。TTF/UI 目前按字元提交 Sprite，未在本階段擴張 batching。
- 3D 回歸副本：Update 1.073 ms、DrawWorld CPU 4.842 ms、GPU 0.310 ms；IAVertices 4779。這是不同內容的啟動回歸，不能拿來宣稱等量內容的 2D／3D 加速比。

原始數值：`docs/validation/phase6-*-debug.json`。量測包含診斷 query 成本、Debug FPS 及同機背景負載，未重複多次作統計結論，也沒有將 GPU query 無效值當有效時間。

啟動資源範圍按現有程式路徑核對：2D 不進入 EnsureScene3D 的陰影／skybox／IBL／Billboard／粒子資源建立，也不更新 3D 物理或繪製其 passes；仍保留共用 D3D、shader、PostRT、PostProcess 與 Effekseer manager 初始化。1280×720 RGBA8 PostRT 單一 texture 的名義大小約 3.52 MiB，未計 driver overhead。未量測逐資產載入 byte 數或 VRAM，不宣稱零 3D 共用成本。

## 實機驗收清單（使用者已確認階段完成）

Computer Use 能列出視窗，但 capture／輸入分別回報 FrameArrived timeout、coordinate input geometry is unavailable、foreground window did not report a process id，未能可靠操作遊戲；沒有改用非授權的 UI 輸入後門。

- Editor 中修改、儲存、關閉再開啟範例，確認內容保留。
- Play 後以鍵盤移動並觀察動畫、相機跟隨、障礙物阻擋。
- 收集三個標記、確認分數不重複；按 N 換關及 R 重開，確認分數與物理接觸清空。
- Stop 後確認三個標記及初始角色位置回復，再次 Play。
- 獨立套件重做相同互動；確認字型、UI pivot、畫面及 Editor 一致。
- 3D 副本檢查透視相機、陰影、UI，以及已支援形狀的實際碰撞落點。

使用者已於最終交付後確認第六階段完成；本清單保留作日後手動回歸用途。


## 播放模式跨場景回歸

- 換場景前保留目前 F2／F3 模式；載入期間繼續沿用該模式的 GUI 及 viewport 範圍。無參數 StartPlay 在 deferred load 期間繼承待恢復模式，普通啟動仍預設 F2。
- 暫停模擬與播放呈現分開；載入期間 toolbar／Inspector 等維持播放樣式，不短暫恢復編輯 gizmo。播放中換場景不顯示會 dim 整個 Editor 的 loading modal；普通編輯器載入仍保留 modal。
- Debug x64 host 整合測試：`tools/run_play_scene_switch_check.ps1`。先在 Editor 開啟 TestProject 的 scene_topdown_2d.json 並關閉 Editor；測試會另開測試 process，分別以 Fullscreen／Viewport 模式經 SceneCanvas request 前往第二關，再經無參數 StartPlay API 返回第一關。
- 每個 pending-load 幀檢查 Play mode、GUI enabled、viewport rectangle，繪製後檢查 loading modal 不為 Active；兩種模式均 PASS 並 exit code 0。測試只編入 Debug Editor，不注入 OS 鍵盤事件，未設定環境變數時不執行。
- 此證據驗證實際載入及 GUI 呈現狀態；人眼觀察的閃爍結果仍待實機確認。沒有加入遊戲 viewport 的舊畫面快照或跨場景 crossfade。
