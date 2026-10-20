# Ivy Engine 2D 支援計畫

日期：2026-10-20。狀態：第 1–6 階段已由使用者確認完成，首版計畫結案。完整狀態、驗證與限制見 `2D_IMPLEMENTATION_STATUS.zh-TW.md` 及 `2D_PHASE6_VALIDATION.zh-TW.md`。後續項目依實際遊戲需求另行啟動。

## 目標與範圍

在同一套 Ivy Engine 中製作、編輯並打包純 2D 遊戲，共用現有 content project、GameObject、Transform、場景、腳本、輸入、音效、UI 與部署流程。2D 使用獨立的世界 Sprite 繪製模組與可選物理模組，不複製引擎，也不重寫現有 3D 渲染器。

第一個交付目標是一個可打包的俯視 2D 小場景：角色移動、相機跟隨、Sprite 動畫、障礙物碰撞、Trigger 與 UI。平台跳躍、Tilemap、混合 2D/3D 場景不列為首版驗收要求。

## 已確認的現況

- `engine/rendering/sprite/` 與 `shaders/VertexShader2D.hlsl` 已有 Sprite 基礎，UI 與啟動畫面正在使用；不能直接把 UI 座標規則當作世界座標。
- `engine/ecs/camera_component.*`、`src/app/camera/camera.*` 與 `camera_pose.*` 目前的投影資料以 FOV 為主；正交投影必須沿整條相機路徑傳遞。
- `src/app/scene/game_scene.cpp` 目前直接處理陰影、天空盒、3D 物理與 3D render passes；僅隱藏編輯器選項不會消除這些成本。
- `engine/ecs/transform.*` 已支援父子世界矩陣，可供 2D 共用。
- 場景載入、Prefab、Undo/Redo、資源預載與打包各有整合點；新增 Sprite 必須接入完整生命週期。
- 引擎已依 ADR 0007 於 2026-10-20 全面解凍；2D 支援與其他引擎開發不再需要另行解凍。`engine-v1.0-lock` 保留為歷史基準。

## 架構規則：控制臃腫

1. 共用核心不依賴 2D 物理；2D renderer 不依賴 Visual、PBR、陰影或天空盒。
2. 沿用 D3D11 裝置、貼圖資源與視窗，不建立第二套 graphics backend。
3. 在場景初始化、更新、繪製的少數入口選擇路徑，不把 `is2D` 判斷散落到每個物件。
4. 2D 編輯視角、相機投影、場景渲染路徑與物理選擇是不同設定；正交相機也能用於 3D 場景。
5. 第一版排除不需要的初始化與每幀工作；執行檔移除未用 3D 程式碼是後續建置工作，不能宣稱不執行就等於套件變小。
6. 先採少量明確函式與資料結構，不建立泛用 render graph、插件框架或第二套 ECS。

## 第 0 階段：定義契約與變更範圍

工作：
- 依全面解凍政策直接規劃 2D 實作，保留既有 3D 預設與腳本相容性；本計畫仍依 2D 需求控制變更範圍。
- 世界座標採 XY 平面、+Y 向上、繞 Z 軸旋轉；Transform 維持三維格式。2D 遊戲邏輯不以 Z 表示碰撞深度。
- 定義 `pixelsPerUnit` 為每單位世界長度對應的貼圖像素數；Sprite 初始尺寸由裁切區域除以此值產生，再套用 Transform。
- 提案設定：content project 的 `template: 2d` 只影響新場景預設；場景明確儲存 `renderPath: 2d|3d` 與 `physicsMode: none|2d|3d`。欄位名稱在實作 ADR 中定案。
- 舊資料缺欄位時維持既有 3D 行為；首版不提供混合渲染或同場景雙物理世界。

驗收：契約與代表性 JSON 範例定案，現有 content project 不需轉檔。

## 第 1 階段：相機與最小世界 Sprite

工作：
- CameraComponent、Camera 與 CameraPose 加入 Perspective/Orthographic、正交垂直可視範圍；水平範圍由實際 viewport aspect 推導。
- 涵蓋 CameraDirector、場景相機同步、Play mode、runtime 與相機 gizmo。跨投影切換先採直接切換，不插值混合兩種投影矩陣。
- 新增 `SpriteRenderer`：貼圖路徑、來源像素矩形、pivot、pixelsPerUnit、tint、flipX/Y、sortingLayer、orderInLayer。
- 新增獨立世界 Sprite renderer，接收 world matrix 與 view/projection；保留現有 UI Sprite 介面與座標規則。
- 接入場景、Prefab 的序列化與建立流程，貼圖採既有 content project 路徑規則。
- 初版使用簡單 quad，明確設定 sampler、blend、depth 與 rasterizer state；驗證透明貼圖邊緣與 alpha 契約。

主要接點：`engine/ecs/camera_component.*`、`src/app/camera/`、`engine/scene/scene_camera.*`、`engine/scene/scene_loader.*`、`engine/ecs/prefab_serializer.*`；新增 `engine/ecs/sprite_renderer_component.*` 與 `engine/rendering/2d/`。

驗收：世界 Sprite 可隨父物件移動、旋轉與縮放；正交相機移動與縮放正確；調整視窗後不拉伸；場景儲存重開後一致；UI 不受相機移動影響。

## 第 2 階段：獨立 2D 執行路徑

工作：
- 在 GameScene 初始化、更新、繪製入口選擇場景路徑；共用腳本、場景切換、UI、載入畫面與呈現流程。
- 純 2D 路徑不初始化或執行陰影、天空盒、3D lighting、3D particles、模型繪製與 3D 物理；逐項檢查其初始化及釋放是否成對。
- 明確指定 2D color target、clear、viewport 與 UI 合成順序。PostFX 預設關閉；若保留共享 target，確認略過 3D passes 後仍完整清除並呈現。
- 首版採 sortingLayer → orderInLayer → 穩定提交順序的 painter 排序；純 2D Sprite 不寫入深度。Z 不取代排序欄位。
- 依現有 render-thread join 規則管理繪製資料與貼圖生命週期，避免渲染執行緒讀取正在變更或已釋放的物件。
- 擴充資源預載，辨識 Sprite 與之後的動畫貼圖；測試 2D/3D 場景切換時的資源清理。

驗收：純 2D 場景沒有上述 3D passes 或物理更新；透明疊圖順序固定；場景切換與 resize 無殘影；既有 3D 場景畫面維持一致。

## 第 3 階段：可用的 2D 編輯流程

工作：
- Scene viewport 加入 2D 切換、平移、縮放、XY 網格、Frame Selection 與相機範圍顯示；不改變遊戲相機的已儲存投影。
- 以 viewport-local 座標轉換建立世界 XY 平面的 picking，重疊 Sprite 依可見繪製順序選取。
- Gizmo 限制 XY 位移與 Z 旋轉；保留共用 Transform。首版以矩形範圍選取即可。
- Inspector 支援 Sprite 欄位；Asset Browser 可將圖片拖入場景建立 Sprite。
- 將新增/移除 Sprite、欄位修改、複製、刪除與 Prefab 操作接入現有 Undo/Redo。
- 新建 2D content project 範本，預設正交相機與簡單 Sprite 場景，沿用既有打包入口。

主要接點：`engine/core/engine_gui*`、`engine/editor/transform_gizmo.*`、`engine/editor/editor_undo.*`、`engine/editor/editor_project_browser.*`、`engine/editor/project_manager.*`。

驗收：不用手改 JSON，即可建立、編輯、儲存、重開並播放 2D 場景；Undo/Redo 可還原 Sprite 與 Transform 變更。

第 1–3 階段完成即為「可編輯的 2D 渲染 MVP」，可先用腳本製作不需物理的遊戲。

## 第 4 階段：最小 Sprite 動畫與腳本

工作：
- 新增 `SpriteAnimator`：命名 clip、來源矩形序列、FPS、loop、播放/暫停/停止；每個物件持有自己的播放狀態，共用貼圖。
- 以既有腳本更新與 Play mode reset 規則推進動畫；不引入 Animator graph 或狀態機編輯器。
- 讓腳本控制 clip、翻面與必要的 Sprite 屬性；ScriptRuntimeApi 若需擴充，採既有相容性慣例追加並同步 DLL 範本。
- 動畫資料接入儲存、預載、Prefab 與 playable package。

驗收：角色可在 idle/run 間切換；不同物件動畫時間互不影響；離開再進入 Play mode 可重設；既有腳本載入流程正常。

## 第 5 階段：可選 2D 物理

工作：
- 不把現有 3D Collider 強制壓平。為 2D 定義獨立的 Rigidbody2D 與 Collider2D 模組。
- 實作前評估 Box2D 與有限的自製解法，依授權、依賴、C++ 建置相容性及維護成本定案；若採外部函式庫，屆時確認官方文件與版本。
- 首版只做 box/circle、static/dynamic/kinematic、Trigger、碰撞過濾與 raycast；不做 joints 或任意 polygon。
- 固定時間步進、碰撞事件與 Play mode reset 必須明確；觸發事件在安全的主執行緒時點派送。
- 定義 Transform 與物理的所有權：dynamic 由物理回寫位置；kinematic 由明確移動操作驅動；首版動態剛體限制於根節點且禁止執行中改變非均勻縮放，編輯器提供說明。
- 以獨立 2D 事件介面傳遞碰撞資料，禁止把 2D 接觸硬塞進既有 3D Collider 指標。

驗收：不同渲染 FPS 下移動結果穩定；障礙物阻擋角色；Trigger 進出、物件刪除與場景切換不遺留事件；純 2D 不更新 3D 物理。

## 第 6 階段：首版遊戲與回歸驗證

- 新增俯視 2D sample：角色移動、idle/run、相機跟隨、障礙物、收集物 Trigger、計分 UI、場景切換。
- 驗證編輯器與 runtime host 表現一致，playable package 在獨立目錄可啟動，貼圖、動畫與腳本無開發機絕對路徑依賴。
- 同時建置 editor/runtime；回歸代表性的既有 3D 場景，包含 3D 相機、陰影、物理與 UI。
- 以小型確定性檢查驗證投影/座標轉換、排序、序列化預設及動畫時間；物理加入固定步進與事件生命週期測試。視覺、編輯器和打包採實際操作驗證。
- 記錄相同視窗尺寸與建置設定下的空白 2D 場景 CPU/GPU 時間、draw calls、記憶體及啟動資源；不預先承諾沒有量測依據的效能數字。

完成條件：上述 sample 可編輯、重開、播放與獨立部署；純 2D 路徑不負擔不需要的 3D 系統；代表性 3D 回歸通過。達成後停止首版擴充。

## 後續項目：依遊戲需求啟動

- Sprite batching：先量測，再對排序後相鄰且狀態相容的 Sprite 合批；不能為減少 draw calls 而改變透明繪製順序。
- Atlas、Tilemap、像素對齊相機、Pixel Perfect、9-slice、2D lights、進階動畫工具。
- 混合 2D/3D：另訂深度、透明、燈光與物理互動契約。
- 若套件大小確實成為問題，再做建置時排除 3D 模組與依賴；驗證關閉 3D 的 runtime target，而非複製原始碼。

## 建議執行順序

0 → 1 → 2 → 3 → 4 → 5 → 6。先在第 3 階段驗證編輯體驗，再決定是否推進動畫與物理；需要碰撞的遊戲可優先做第 5 階段。各階段以能獨立驗收的小變更交付，不累積成一次大型重寫。
