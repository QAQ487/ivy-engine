# Ivy Engine — To-do

> 引擎待辦集中入口。來源：[[Engine Changelog]]「留咗做」及 ivy-engine skill 歷史 backlog。
> 整理基準：source audit 對 `3f5afe9`（PR #7 合併）嘅引擎源碼。實作前仍要核實 hooks，避免重做已有系統。
> 已完成遊戲（MagneticField／FrisbeeArena／GravitySwing）嘅舊遊戲任務唔列入引擎待辦。建議次序見文末；其餘清單順序唔等於排期。

## 已列入目前 backlog（尚未驗收完成）

- [ ] **重做 Properties Panel／Inspector**
  - 先盤點 component/property 編輯、序列化、Undo/Redo、選取流程，提出 UI 草案同明確 scope，再實作。
  - 目標：分區／欄位清晰、編輯穩定，保留現有功能。
  - 現況：`engine/core/engine_gui.cpp` 的 `"Properties"` 已有 Transform、Tag、Visual、Light、Camera、Collider、Rigidbody、Material、Effect、Audio 同 script properties；Undo 已接 `editor_undo.cpp`。呢項係 UI 重做，唔係由零寫 inspector。
  - 驗收：常用欄位編輯／保存／重載正常；Undo/Redo 有效；新增／移除 component、資產選擇無回歸。
- [ ] **World-space UI 編輯器支援**
  - 可以建立、擺位同編輯 world-space UI；與 screen-space canvas 分開。
  - 現況：`UIElement` 係 screen overlay（anchor + `XMFLOAT2`）；場景只有一個 `UICanvas`。Billboard／sprite 唔算 UI canvas。
  - 交互同具體驗收範圍待設計。
- [x] **Cylinder pair collision**（Terry 喺 PR #8 接受 Windows runtime）
  - `Box`／`Mesh`／`Cylinder` 對 `Cylinder` 已有接觸。`engine/physics/collider/cylinder_pair.cpp` 用 GJK/EPA：Minkowski 差係 A−B，法線由 B 指向 A（`ResolveContact(A, B, normal)` 推 A 離開 B），穿透係最小平移深度，接觸點係兩個 witness 嘅中點。`collider.cpp` 唔再對呢三對靜默當穿過；`LogUnsupportedCylinderPair` 已移除。`915dce7` 停咗互相呼叫導致嘅 stack overflow；sphere–cylinder（closest point）同 capsule–cylinder（三點取樣）冇改。
  - Mesh 唔再用 box 嘅外接球，亦唔用 capsule 嘅三粒取樣球去近似圓柱。每隻三角形係零厚度凸形，對實心有限圓柱（平頂蓋）做 GJK/EPA，所以頂蓋唔會鼓出。非均勻 mesh scale 用現有 world matrix 把三角形變去世界空間，圓柱維持圓形。Broadphase 仍係圓柱外接球。Resolve 沿用 mesh 最多 4 個法線簇、`StabilizeFloorContactNormal`，slope `0.001`，而且會把接觸點交俾 `ResolveContact`。動態 mesh 對 kinematic 圓柱都會解（其他 mesh 配對仍然跳過 kinematic）。
  - 限制：只保證凸形。凹 mesh 係逐三角形，唔係實心體積。零或負 radius／half-height 當無碰撞。接觸斑係線或圓盤時，witness 可以係斑入面任意一點。GJK/EPA 有數值誤差。
  - 驗收場景：`testbed/rom/scene/scene_regression_3d.json`（紅色落體要停低）同 `testbed/rom/scene/scene_cylinder_pairs.json`（盒／mesh／圓柱、X／Z 90°、地面盒、疊柱）。Editor：File → Open Testbed，再打開場景。Linux `tests/cylinder_pair_test.cpp`（`g++ -std=c++20`）已覆蓋分離、軸對齊重疊、旋轉、頂蓋、側面、非法尺寸同法線對稱。Terry 喺 PR #8 接受咗 Windows runtime，所以呢項剔走。
- [ ] **Content reload 紅燈：刪除未被引用嘅資產**
  - `engine/core/engine_gui.cpp` 約 3241 行：`rom/` 底下任何 `change.removed` 都會 `g_contentReloadLight.Failure()`（紅燈），文案係 `Content removed; existing resources retained`。新增而且目前 scene JSON 搵唔到嘅檔會跳過；刪除唔會先查場景有冇引用。
  - 驗收：刪除未被目前場景（同已載入資源）引用嘅檔，Content 燈維持 Watching，唔好轉成 Failed。刪除確實被引用嘅檔仍要報失敗並保留現有資源。改完要分開 source review 同 editor 實測。
- [ ] **Box friction torque（可選，只有想箱子傾側先做）**
  - `collider.cpp` 的 `ResolveContact` 對 box 把摩擦衝量嘅力臂設成 0，箱子只留線性摩擦，避免喺邊緣滾動；球體仍有角向摩擦。呢個係現有設計，唔係空 stub。
  - 驗收（若開做）：box 可以因摩擦傾側，同時唔好無故原地打轉；球體滾動無回歸。
- [x] **重做 Skybox：直接接受 panorama（equirectangular）PNG，自動轉換成 DDS cubemap**
  - 路徑（PR #10 之後）：六面來源 PNG 喺 `resources/skybox/source/`（`px nx py ny pz nz`）。`resources/skybox/source/run.bat` 仍然要 PATH 上的 `texassemble`，寫 `resources/skybox/MySkybox.dds`。`rom/<rest>` 先讀專案 `rom/<rest>`，再讀 `<exe-dir>/resources/<rest>`；寫入只去專案 `rom/`。`CubemapLoad` 仍然只收 legacy DDS cubemap。
  - 實作：`settings.render.skyboxCubemap` 可以係 equirectangular PNG（寬=360°）。`engine/rendering/environment/equirect_cubemap.cpp` 喺載入／打包時烤成專案 `rom/skybox/cache/<slug>.dds`（slug 係 `rom/` 之後的路徑，`/` 換成 `__`）。Stamp 記 mtime、size、FNV-1a64、vertical FOV key；任一變就重烤，editor 再走 `CubemapReload`。PNG 由已經連結的 DirectXTex WIC 解，唔使 `texassemble` 或 Python。六面 PNG 同指向 `rom/skybox/MySkybox.dds` 的場景唔經呢條路。Asset Browser 可拖 `.png`／`.dds` 到 Rendering Settings 的路徑欄，或右鍵 **Set as > Skybox**（共用 `skybox_asset_path.h` 的 rom 路徑校驗）。
  - 覆蓋：剛好 2:1 行為同舊版（face=`height/2` 必須係 2 的冪）。`height ≤ width/2` 用垂直覆蓋 `360*height/width`（或可選 `skyboxVerticalFov`，(0,180]，0=auto）置中地平線，帶外 clamp 到頂／底行。`height > width/2`、解唔開、路徑唔安全會寫 `content_hot_reload.log` 同顯示 **Panorama failed**，唔會淨係 clear color。非 2:1 的 face 係 ≤`width/4` 的最大 2 的冪。最大 8192×4096（face 2048）。經度係 Debevec lat-long：圖像中心係 +Z，頂向 +Y，u 向右去 +X。面序同 `tools/bake_ibl.py`／texassemble：+X −X +Y −Y +Z −Z。
  - IBL：panorama 會重烤 irradiance（32）同 prefilter（128、5 mip，每面自己的 mip chain，BGRA8，同 `bake_ibl.py`）。`brdf_lut.png` 唔重做。烤唔到或 LUT 缺失就 log，editor Rendering Settings 顯示「IBL not matching sky」，唔會靜默沿用舊 `rom/skybox/ibl/`。DDS 天空仍然用場景自己的 IBL 路徑。
  - 部署：editor 打包前先烤場景 JSON 入面的 panorama，包內帶 cache DDS。Runtime 只喺 stamp 唔夾（缺檔、mtime、內容或 FOV 變）先至自己烤，寫入包裝的 `rom/`。`deploy.bat` 唔會主動烤，只係抄已有的 `rom/`；缺 cache 就等 runtime。
  - 驗收場景：`testbed/rom/scene/scene_panorama_sky.json` 用 `testbed/rom/skybox/panorama_test.png`（64×32，約 157 bytes）。Linux `tests/equirect_cubemap_test.cpp`（`g++ -std=c++20`）PASS。Terry 喺 2026-11-01 接受咗 PR #11（合併 `5965b8a`）嘅 Windows runtime：editor 畫面、PBR 反射、打包 runtime、非 2:1 panorama、拖放同右鍵 Set as > Skybox。
- [x] **Scene／prefab JSON float 三位同選取唔 dirty**（Terry 喺 2026-11-02 接受 PR #12 Windows runtime；合併 `429d57c`）
  - Scene／prefab JSON 浮點經 `engine/scene/json_float.h` 最多存三位小數：去尾零、`-0` → `0`、NaN／Inf → `null`。
  - 選物件唔再標 dirty：Inspect 唔再 `EnsureVisualComponent` 自動加 Visual；gizmo 淨係 translate 唔再改寫 rotation／scale；dirty／undo 以三位小數比較。
  - 留意：fog density UI 用 `%.4f`、UV scroll `< 0.001` 可能失精。
- [ ] **Asset Browser 長檔名／Grid·List**（source 已做；等 Windows editor 驗收）
  - Grid tile 名最多兩行，超出用 UTF-8 安全 `...`；hover tooltip 同底部 status 顯示全名 + `rom/` 路徑。
  - Toolbar 有 Grid/List 同 tile size；view mode／tile size 寫入 `project.json` → `editor` 同 `rom/editor/ui_prefs.json`。
  - Linux：`tests/asset_browser_text_test.cpp`（`g++ -std=c++17 -I.`）。Windows 要目視長檔名、list 拖放／右鍵 Skybox、設定重開後保留。


## AI 系統套件（source audit：引擎內全部 missing）

`engine/`、`src/`、`game/` 冇 Blackboard、BehaviorTree、Perception、EQS、Waypoint、NavMesh、pathfinding。Village & Goblin 嘅源碼唔喺呢個 repo，遊戲自己有冇 A* 無法喺度核實。

可重用 hooks：`PhysicsSystem_Raycast`／script `physicsRaycast`、2D `physics2DRaycast`；`DebugSystem`（collision viz、time scale、overlay）；`DebugDrawer`（box／sphere／capsule／cylinder／line）；script `debugDrawLine`、`debugDrawSphereWire`、`lineSpriteDraw`；collider ignore 同 `OnTriggerEnter`／`Exit`；`ComponentFactory`。

建議實作次序：Waypoint → Perception → BT／Blackboard → EQS。Debug visualization 跟住對應系統一齊做，唔好先做一個空視圖。

- [ ] **Waypoint component＋PatrolRoute**：editor 邏輯 marker（位置＋可選 tag）、路線陣列／current index、到達後 next。引擎內 missing。
- [ ] **Perception**：sight cone＋LOS raycast（可接現有 raycast）；LoseSightRadius > SightRadius 防 flicker，結果寫入 Blackboard。引擎內 missing。
- [ ] **Blackboard**：key-value 短期記憶。引擎內 missing；同 BT 一齊定最小 scope。
- [ ] **BehaviorTree runner**：Selector／Sequence／Task／Decorator＋Blackboard condition；abort 機制後補。引擎內 missing。
- [ ] **EQS 式 utility scoring**：候選位置按距離／方向／路徑成本加權。引擎內 missing；排喺有路徑或 steering 之後。
- [ ] **AI debug visualization**：BT current node、perception cone、path，接現有 `DebugSystem`／`DebugDrawer`。引擎內 missing；跟對應系統。

### 暫緩

- [ ] **NavMesh／AI pathfinding**：暫緩，之後按遊戲需要排期。audit 確認引擎內冇 grid pathfinding、path smoothing、steering 或 NavMesh。
  - 既有規劃包含 grid-based pathfinding＋path smoothing；與本項合併管理，唔當兩份獨立必做任務。
  - 無障礙 arena 可先用 steering，唔預設一定要 NavMesh。

## 渲染效能：Culling

現況：3D main camera pass 已有 per-object frustum culling；CSM depth passes 已有 per-cascade light-ortho caster culling（open near）；2D ortho sprite 已有 view-rect culling。仍然冇 distance／occlusion culling；billboard／particle／screen-space UI／tilemap 未做。Physics AABB 仍然唔共用。`D3D11_QUERY` 仍然只見 timestamp／pipeline stats。

參考：Unity 模型——自動 frustum culling、手動 baked occlusion、Unity 6 GPU occlusion、`CullingGroup` API。

- [x] **Frustum culling（3D）**（source 已做；等 Windows Release runtime 驗收）
  - 每個 mesh 喺 load 時 cache local AABB（FBX／OBJ／primitives；skinned 喺 bind-pose 上再 inflate 半個最大半軸，保守包動畫）。`GameObject::DrawVisual` 轉 world AABB（scale／rotation 用 8 corners），對 camera view×proj 抽出嘅 6 planes（D3D z∈[0,1]）測試；完全在外就唔 `Model3D::Draw`。無有效 bounds 永不 cull。
  - **Shadow**：main pass 先 cull；camera frustum **唔**閘 shadow depth。Per-cascade light-ortho cull 見下項。
  - Toggle：`settings.render.frustumCullingEnabled`（預設 true；Rendering Settings checkbox）。Counters：considered／culled／drawn —— Rendering Settings 即時顯示、collision-viz overlay、play FPS overlay、runtime bridge `perf.frustumCull`。
  - Linux：`g++ -std=c++17 -I. -Itools/linux_win32_shim tools/frustum_cull_check.cpp -o /tmp/frustum_cull_check && /tmp/frustum_cull_check`。
  - 剩餘：Windows Release 大場景目視（culled 隨視角升、toggle off 畫面一致、畫面外 caster 陰影仍在）；更緊嘅 per-clip skinned bounds（而家係 inflate）。
- [x] **Shadow cascade caster culling**（source 已做；等 Windows Release runtime 驗收）
  - 每個 cascade 用 `lightView*lightProj` 抽出 6 planes；AABB 測試 **跳過 near**（open toward light／pancaking），只 cull 側向同 far。畫面外但仍投影入 cascade 嘅 caster 會畫 shadow。
  - Toggle：`settings.render.shadowCascadeCullingEnabled`（預設 true）。Counters：frame totals + per-cascade —— Rendering Settings；frame totals —— FPS／collision-viz／`perf.shadowCull`。
  - Linux：`./tools/run_shadow_cull_check.sh`。
  - 剩餘：Windows Release A/B（ON vs OFF capture 像素一致於畫面內陰影；遠方 caster `shadowCull.culled` 上升；畫面外 caster 陰影仍在）。
- [x] **2D camera rect culling**（source 已做；等 Windows Release runtime 驗收）
  - `SpriteRenderer` 喺 `Draw` 前用 size／pivot／world matrix（含 rotation／scale／renderOffset）計保守 world AABB，對 ortho camera 嘅 world-XY view rect（含細 margin）測試；完全在外就唔 `Sprite_DrawWorld`。Animator／physics／scripts 唔受影響。Screen-space UI 唔閘。Perspective camera 下 sprite 保持 unculled。
  - Toggle：`settings.render.spriteCullingEnabled`（預設 true）。Counters：considered／culled／drawn —— Rendering Settings、collision-viz、FPS `2CULL`、`perf.spriteCull`。
  - Linux：`./tools/run_sprite_cull_check.sh`。
  - 剩餘：Windows Release A/B（ON vs OFF capture 像素一致；離屏 sprite `spriteCull.culled` 上升）；tilemap／billboard／particle 之後另做。
- [ ] **Distance／layer cull distance 同 LOD**
  - 仿 Unity `Camera.layerCullDistances` 同 LOD Group，按 layer 設剔除距離，近遠換 mesh 精度。排喺 frustum culling 之後。
- [ ] **Occlusion culling（暫緩）**
  - 等 Release profiling 顯示 draw call／overdraw 係瓶頸、場景有大量遮擋先做。起點：D3D11 occlusion predicate（`D3D11_QUERY_OCCLUSION_PREDICATE`）或 Hi-Z（上一 frame depth pyramid）；baked PVS（似 Unity Umbra）需要 editor bake 流程，放最後。

## 歷史 backlog（source audit 結果）

以下已對 `3f5afe9` 源碼同 git 核實。勾選代表 source review 完成，唔等於另一次 runtime acceptance。

- [x] **Trigger 系統**：Enter／Exit 已有。`1499a44`；`Collider::m_isTrigger`、`physics_system.cpp` 的 `DiffAndDispatchTriggerEvents`、script `OnTriggerEnter`／`OnTriggerExit`、Properties checkbox。`OnTriggerStay` 未實作；只有需要 Stay 先另開一項，唔好再掛「還債」。
- [x] **Prefab drag-drop Undo**：hierarchy 同 root drop zone 接受 `ASSET_PREFAB` 後呼叫 `EditorUndo_PushCreate`（`engine/core/engine_gui.cpp`）；`CreateObjectCommand::Undo` 會刪走個節點（`engine/editor/editor_undo.cpp`）。
- [x] **deploy.bat executable 命名**：bat 仍複製 `IvyEngineRuntime.exe`（`external_content_template/deploy/deploy.bat`）。Editor package 用 `SanitizeGameExeStem` 寫成 `<GameName>.exe`（`engine/editor/playable_package_builder.cpp`）。兩邊嘅分別已寫喺 `docs/CONTENT_PROJECT.md`。若要 bat 都輸出遊戲名，先另開窄項。
- [x] **Asset cache LRU／refcount**：`ModelAssetCache` 係 process-lifetime `std::map`（`engine/ecs/model_asset_cache.h`），同 README 寫法一致。冇 LRU、冇 refcount，亦冇「曾經做完再拆走」嘅 commit。有記憶體數字先重開。
- [x] **Mesh collider recursion crash**：`915dce7` 停止 Box／mesh／cylinder 互相呼叫直到 stack overflow。當時改為無碰撞並 log。接觸已喺 source 實作，Windows runtime 未驗收，見上面「Cylinder pair collision」。唔好再當未還原嘅舊故障。

PBR 球場已由引擎待辦移除：倉庫只有 PBR shader 同 `testbed/rom/scene/scene_pbr_showcase.json` 材質展示，冇球場內容；`FrisbeeArena` 屬已完成遊戲。

## 最近同步留下嘅驗證事項

- [x] **ImGuizmo／ImGui AddPolyline 參數順序**（source 已做；等 Windows editor 目視驗收）
  - `libraries/imguizmo/ImGuizmo.cpp` 三個 `AddPolyline` 改返 ImGui 1.92.5 簽名 `(flags, thickness)`。Linux：`python3 tools/check_imguizmo_addpolyline_order.py`。MSVC 仍保留 PR #7 對 `ImGuizmo.cpp` 嘅 C4244 suppression。
  - 剩餘：Windows editor 目視——旋轉環厚度 = `RotationLineThickness`（預設 2）；拖旋轉扇形邊閉合；平移平面外觀不變。
- [x] **PR #5 資產清理**：被刪嘅 `.ua/`、`rom/effects/`、`rom/skybox/001/`、`rom/model/character/YBot_white.fbx`、`rom/model/object/{tree,Wall,Cone_Yellow}.fbx`、`external_template/`、已刪 sample script，scene／prefab JSON 冇殘留引用。玩家模型其後喺 `testbed/rom/model/player/Y Bot_white.fbx`。`rom/effects/` 只剩 hot-reload 路徑前綴。當時 `samples/scenes` 的 2D 場景引用 `rom/texture/sprite_demo/`，呢批 PNG 唔喺 repo、亦唔係 PR #5 刪走。之後 testbed 用細 placeholder PNG 補咗四張圖（`adventurer_walk`、`test_sheet`、`hp_fill`、`hp_frame`），場景已移去 `testbed/rom/scene/`。
- [x] **In-repo testbed**：`testbed/` 係 Debug editor 喺冇最近專案可開時會打開嘅 content project。`samples/scenes` 已移入 `testbed/rom/scene/`。引擎預設其後搬去 repo 根 `resources/`，用 `rom/<rest>` → `resources/<rest>` fallback，唔再做 NTFS junction。Testbed 專用模型同 sprite placeholder 喺 `testbed/rom/`。Linux：`tools/content_path_check.cpp`、`python3 tools/check_testbed_assets.py`、`python3 tools/check_engine_resource_paths.py`。Windows editor 實測未在呢次做。
- [ ] **PR #7 視覺 regression**：Terry 確認合併前 Windows build 已成功。仍然開住嘅只係畫面：lighting、CSM、post-process、gizmo。Linux 不能代替呢次目視；ImGuizmo 線條見上一項。

> 勾選係 source review 或 Terry 對 build 嘅確認。畫面同玩法驗收仍然分開。

## 已完成項目

已完成嘅 Physics Material、primitive meshes、render thread、Inspector Undo 修補、IBL／資產補齊、angular dynamics v1、Cylinder recursion fix、Collider Auto-Fit/scaling，保留喺 [[Engine Changelog]] 嘅完成紀錄，唔再混入未完成清單。歷史 backlog 嘅勾選同證據留喺上面一節。

## 建議優先次序

1. Content reload 紅燈（刪未引用資產）。
2. ImGuizmo／PR #7 視覺檢查（lighting、CSM、post-process、gizmo）喺 Windows 做（source 已修 AddPolyline）。
3. 重做 Properties Panel／Inspector（先有 UI 草案；現有編輯同 Undo 要保住）。
4. AI 套件：Waypoint → Perception → BT／Blackboard → EQS。NavMesh 繼續暫緩。

Box friction torque 同 World-space UI 未排入上面次序：前者只有想箱子傾側先做；後者驗收仍待設計。渲染 Culling 一節亦未排入；開做時先做 frustum culling。

## 維護規則

- 之後新增／完成／暫緩 engine 待辦，集中更新本頁；Changelog 記做過嘅事同證據。
- 完成要附 commit／實測或 Terry 確認；source review 同 runtime acceptance 分開。
- 歷史項目核實前唔直接轉成 implementation prompt。

[[Engine Changelog]]
