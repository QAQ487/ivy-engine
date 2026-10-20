# 2D 支援實作進度

第 1–6 階段已由使用者確認完成（2026-10-20），首版 2D 支援計畫完成。以下各階段的待驗收描述保留為當時交付紀錄；最終狀態以本段及第六階段驗證文件為準。

已實作：
- Camera、CameraComponent、CameraPose 的正交投影與垂直可視範圍；跨投影切換直接切換。
- SpriteRenderer 的世界座標繪製、父子 Transform、來源矩形、pivot、pixelsPerUnit、tint、翻面與穩定圖層排序。
- 場景與 Prefab 使用既有 ComponentFactory 儲存/載入 Sprite。
- Inspector 的相機投影、Sprite 新增/移除、貼圖拖放、屬性編輯及 Undo/Redo 接點。
- 正交相機 gizmo 與 Transform gizmo 投影。

使用方式：
1. 在目前 content project 建立 GameObject，新增 Sprite Renderer，填入該專案內的貼圖相對路徑。
2. 建立 Camera，啟用 Orthographic，設定 View Height，例如 10；相機放在 (0,0,-10)，旋轉為零，朝 +Z 看向 XY 平面。
3. 使用既有相機選擇功能，將此相機設為 Play mode 的場景相機。
4. Sprite 的尺寸為來源像素大小除以 Pixels Per Unit；pivot 由左下角起算。排序先比較 Sorting Layer，再比較 Order In Layer。
5. `samples/scenes/scene_sprite_2d.json` 提供兩個重疊 Sprite 與正交相機；使用時須確保 content project 有 `rom/branding/ivy_logo.png`。

驗證：Debug x64 editor/runtime 編譯與連結成功；`tools/camera_2d_check.cpp` 的透視預設、正交縮放、viewport aspect 與零尺寸防護斷言通過。第一、第二階段另有使用者完成確認；此確認不代表代理已逐項執行所有實機測試，獨立部署驗收仍留待後續階段。

第二階段：
- Rendering Settings 新增「2D Scene」與 3D 場景的「3D Physics」選項，播放時禁止修改；設定儲存在場景 `settings.renderPath` 與 `settings.physicsMode`。舊場景預設為 3D。
- 2D Sprite 直接繪製至 backbuffer，略過模型、陰影、天空盒、3D 粒子、Effekseer 更新/繪製及 PostFX；保留原有 UI 合成。
- 陰影、天空盒、IBL、Billboard 與粒子資源在需要 3D 時建立，切到 2D 時釋放；主迴圈共用的 D3D 裝置、PostFX 設施與 Effekseer manager 尚保留，不代表執行檔或所有初始化成本均已移除。
- 停用 3D 全域物理與物件自身的物理積分；第二階段當時使用 `physicsMode: none`；第五階段另提供可選的 `physicsMode: 2d`。
- Sprite 貼圖接入既有主執行緒分幀預載，包含場景 JSON 與 Prefab，沿用路徑去重和快取；動態生成的 Sprite 仍可在第一次繪製時載入。
- `tools/scene_execution_policy_check.cpp` 已通過：舊場景預設、空設定、2D 排除 3D 物理、3D 無物理與設定 round-trip。

第三階段操作：
- 工具列「2D View」切換正交 XY 編輯視角；2D 場景載入後自動啟用。這是編輯器視角，不修改遊戲 CameraComponent。
- 中鍵或 Alt＋左鍵拖曳平移，滾輪縮放；滑鼠需位於 viewport。F 框選目前物件，Sprite 使用完整世界矩陣的矩形範圍估計。
- XY 網格沿用工具列 Grid 開關；方向指示器在 2D 視角隱藏。
- 2D 場景以矩形範圍選取 Sprite，考慮父物件、旋轉、縮放、pivot 與來源裁切；重疊依 sortingLayer、orderInLayer 與穩定繪製順序選最上層。不做透明像素穿透選取。
- 2D Gizmo 限制 XY 位移、Z 旋轉、XY 縮放，沿用既有 Undo/Redo；保留物件原有 Z、X/Y 旋轉與 Z 縮放。
- 從 Asset Browser 將 PNG/JPG/JPEG/BMP 拖入 viewport，在世界 Z=0 平面建立 Sprite；沿用新增物件 Undo、選取與場景儲存流程。平行於該平面的視角無法放置。
- New Project 對話框勾選「2D Project」，建立含 Camera2D 與 Sprite 的初始場景；沿用既有腳本、solution 與打包流程。
- Play 前儲存編輯器視角，Stop 後還原；新場景清除舊視角快照，避免跨場景沿用。
- `tools/sprite_pick_check.cpp` 驗證命中/未命中、父旋轉、負縮放、pivot、平行/反向射線與零縮放；範本備用白色 PNG 已檢查 CRC 與解壓像素。

第四階段操作：
- Sprite Animator 可設定命名 Clip、來源貼圖、逐格 XYWH、FPS、Loop、Default Clip 與 Auto Play；每個物件獨立計時，貼圖仍使用共享快取。
- Inspector 的「Sheet Metadata JSON」可輸入或拖入 `rom/.../*.json`，填入 Import Clip Name 後按 Import Sheet。支援本次素材的 `image`、`frames[].x/y/w/h` 格式；匯入後設定保存在場景或 Prefab，不需在 runtime 重新讀 metadata。
- 動畫在腳本更新後推進；非 Play 不自動推進。Stop 還原預設動畫與 Sprite 的播放前屬性，不將 active clip／播放時間寫入作者資料。
- 腳本介面：`ScriptApi_SpritePlay(object, "walk", restart)`、`ScriptApi_SpritePause`、`ScriptApi_SpriteStop`、`ScriptApi_SpriteSetFlip`、`ScriptApi_SpriteSetTint`。同 Clip 的 Play 預設繼續，restart=true 才重播。
- ScriptRuntimeApi v43 追加 Sprite 操作；保留 v42 的 `ScriptRuntimeApi_Get` 入口，新腳本優先使用 `ScriptRuntimeApi_GetV43`，存取新增函式前檢查版本。DLL 範本仍直接編譯引擎共用的 API 來源。
- 所有 Clip 貼圖接入場景／Prefab 預載，沿用 content project 的 rom 打包流程。
- TestProject 的 `rom/scene/scene_sprite_animation.json` 使用引用聊天的實際素材：角色走路 Sheet、彩色圓形 Sheet、HP Frame／Fill；檔案已複製至 `rom/texture/sprite_demo/`，原始素材未修改。此場景設為 lastOpenedScene。
- 範例角色 WASD／方向鍵移動，自動切換 idle／walk，向左移動翻面；P 暫停／繼續角色，O 停止，移動時重新播放。idle 暫用走路 Sheet 第一格，walk 使用全部 8 格。兩個旁觀角色分別以 5／12 FPS 播放，方便確認計時獨立。
- 已通過動畫時間與實際元件測試：循環、非循環結尾、暫停、繼續、停止、重設、無效 Clip、相同 Clip 不重播、不同物件計時、非 Play 不推進與序列化還原。Editor/runtime/TestProject 腳本 DLL Debug x64 建置通過；已確認 v42/v43 入口同時存在。

限制與後續：
- 第二階段已由使用者確認完成；未另提供逐項實機測試紀錄，CPU/GPU 成本亦尚未量測。
- 第三階段已由使用者確認完成；不另宣稱代理逐項操作過 GUI。
- 第四階段尚待實機驗證：角色 idle/walk、P/O、Play/Stop 還原、動畫 Inspector／Undo／Prefab、腳本熱重載、舊 v42 DLL 載入及獨立部署。
- 完整部署驗收留待第六階段。
- 使用者已指示推進第五階段；第四階段視覺驗收仍以實機結果為準。


第五階段（已實作，待實機驗收）：
- 採用官方 Box2D v3.1.1，MIT 授權；原始碼與授權保留於 `libraries/box2d`。只在啟用 2D Physics 時建立 world。
- Rendering Settings 的 2D Physics 開關儲存為 `settings.physicsMode: "2d"`；與 3D 物理互斥。
- 新增獨立 Rigidbody2D、Collider2D：Static／Dynamic／Kinematic、Box／Circle、Trigger、categoryBits／maskBits、質量、重力倍率、阻尼、摩擦及反彈。預設重力倍率為 0，適合俯視遊戲。
- 2D 物理採 60 Hz accumulator；播放時間使用實際幀時間，單幀時間沿用主迴圈上限 1/15 秒。暫停不步進。
- Dynamic 由物理回寫 XY 位置與 Z 旋轉；腳本透過 SetVelocity 移動。Kinematic 使用 MovePosition。可移動剛體須為根節點，或位於 identity 的內部 runtime spawn root 下，XY 均勻縮放；Circle 亦須 XY 均勻縮放。播放時不支援變更 scale 或形狀設定。
- 腳本 API v44 新增 Physics2DSetVelocity／GetVelocity／MovePosition／Raycast；維持 v42、v43 匯出相容。
- Script 可另實作 Physics2DListener::OnPhysics2DEvent；事件在腳本工作完成後於主執行緒派送，不使用 3D Collider 指標。移除對方後的 Exit 事件 `other` 為 nullptr；場景清理及停止播放清空 world 與事件。
- Raycast 預設略過 Trigger，可選包含並按 category mask 篩選；沿用 Box2D 射線起點位於形狀內時不命中的行為。
- 無效或超出支援範圍的物理數值不建立 body；位置、尺寸、質量等建立數值上限 100000，density 上限 1e8。單位建議使用米。
- TestProject 的 `rom/scene/scene_physics_2d.json`：WASD／方向鍵移動、四邊牆、中央障礙物、黃色收集物 Trigger；沿用已有冒險者及 HP 圖像。停止播放後恢復場景。
- 自動檢查：`tools/run_physics2d_check.cmd` 驗證 30／60／144 FPS、Box／Circle 撞牆、Trigger 進出及移除 Exit、碰撞／射線篩選、Clear 後重建及極端輸入拒絕。
- 尚待實機確認：Inspector 編輯／Undo、播放／停止／重開、動態 Prefab、場景切換及既有 3D 場景回歸。第六階段再完成遊戲 UI、相機跟隨、獨立部署與效能量測。


第五階段 Collider 顯示補充：
- Collider 2D 提供可儲存及 Undo 的 Show Debug Collider；播放時亦可切換。
- F11 沿用原有全域 collision visualizer，同時顯示場景樹內所有啟用的 2D Box／Circle Collider，包含子節點；實體為綠色，Trigger 為黃色。
- 輪廓以世界位置、Z 旋轉、XY 縮放及 collider offset 計算，在 viewport 內於 UI／編輯器 GUI 之前繪製；不需要啟動物理解算器亦可預覽。
- Add Component 提供 Add Box Collider 2D／Add Circle Collider 2D；既有 Shape 下拉選單仍可切換 Box／Circle。
- Debug x64 Editor／Runtime 建置成功。F11、輪廓位置與 GUI 遮擋結果仍待實機視覺驗證。


元件新增選單及 3D Collider：
- Properties 的 Add Component 改為可搜尋下拉選單，隱藏已存在的元件；提供 Camera、燈光、Visual、Sprite、2D／3D 物理及其餘既有元件。
- 3D Collider 拆成 BoxColliderComponent、SphereColliderComponent、CapsuleColliderComponent、CylinderColliderComponent、MeshColliderComponent，各自固定形狀，面板及序列化只呈現相關參數。共用底層 3D 物理解算，不新增另一套解算器。
- 每個 GameObject 仍只支援一個 3D Collider；這次未加入 Unity 的多 Collider／compound 功能。
- 舊場景／Prefab 的 Collider+colliderType 仍可讀取；使用者儲存後按形狀寫入新的元件名稱。沒有批次改寫任何既有 content 檔案；舊版引擎未註冊新名稱，回到舊版時須使用原有版本的場景檔。
- SceneRuntime、選取輪廓、F11、Undo／Redo 及 ScriptApi_FindColliderComponent 均接入新類型。直接查找時可使用 GetComponent<BoxColliderComponent>() 等具體類型；共用介面使用 FindColliderComponent／ScriptApi_FindColliderComponent。
- tools/run_collider_components_check.cmd 實際測試正式序列化模組：五種具體類型、形狀鎖定、相關欄位裁剪、round-trip、舊資料映射、Factory 及數值範圍。
- 尚待實機驗證搜尋選單操作、Inspector、Undo／Redo 及 3D 場景碰撞。


第六階段：
- 已加入兩關俯視範例、角色子節點相機跟隨、收集計分、R 重開、完成後 N 換場景及 Stop 恢復收集物。
- 已加入可重跑回歸、資產檢查、套件組裝工具及 opt-in CPU／GPU／提交數量測，並修正任意 cwd 啟動找不到 shader 的問題。
- Editor／Runtime／腳本建置、自動測試、獨立套件空白及完整 2D／3D 啟動回歸均已通過。
- 互動、視覺、編輯儲存重開及實際 3D 碰撞落點仍待實機驗收；詳見 `docs/2D_PHASE6_VALIDATION.zh-TW.md`。使用者已確認第六階段完成；首版計畫結案。代理自動驗證與使用者實機驗收的證據分開記錄。
