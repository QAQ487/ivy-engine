# Ivy Engine — To-do

> 引擎待辦集中入口。來源：[[Engine Changelog]]「留咗做」及 ivy-engine skill 歷史 backlog。
> 整理基準：source audit 對 `3f5afe9`（PR #7 合併）嘅引擎源碼。實作前仍要核實 hooks，避免重做已有系統。
> 已完成遊戲（MagneticField／FrisbeeArena／GravitySwing）嘅舊遊戲任務唔列入引擎待辦。建議次序見文末；其餘清單順序唔等於排期。

## 已列入目前 backlog（尚未驗收完成）

- [ ] **重做 Properties Panel／Inspector**
  - 先盤點 component/property 編輯、序列化、Undo/Redo、選取流程，提出 UI 草案同明確 scope，再實作。
  - 目標：分區／欄位清晰、編輯穩定，保留現有功能。
  - 現況：`engine/editor/editor_inspector_panel.cpp` 的 `"Properties"` 已有 Transform、Tag、Visual、Light、Camera、Collider、Rigidbody、Material、Effect、Audio 同 script properties；Undo 已接 `editor_undo.cpp`。呢項係 UI 重做，唔係由零寫 inspector。
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
- [x] **Content reload 紅燈：刪除未被引用嘅資產**（Terry 喺 2026-11-13 喺 Windows editor 驗收三個刪檔 case：未引用、已引用、已載入）
  - `content_reload::DecideRemovedContentAction`（`engine/editor/content_removal_policy.h`）：`rom/` 刪檔若唔喺目前 scene JSON，亦唔喺 texture／cubemap／`ModelUtil`／`ModelAssetCache`／audio cache，Content 燈維持 Watching，並寫 `content_hot_reload.log`。被引用或已載入仍然 `Failure` 同保留資源。新增未引用檔繼續 skip（共用 `SceneJsonReferencesRomPath`）。
  - Linux：`tests/content_removal_policy_test.cpp`（`g++ -std=c++17 -I.`）。見 `docs/ENGINE_USER_GUIDE.md` §17「Hot reload」同 `AGENTS.md`「Hot reload internals」。
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
- [ ] **Editor ImGui theme（style preset × color scheme）**（source 已做；等 Windows editor 目視驗收）
  - `engine/editor/editor_theme.h/.cpp`：獨立兩軸——style preset（Trellis 預設／Hollow Soft）同 color scheme（Ivy 預設／Ivy Dark／Classic Dark／Neutral Gray／Light）。Play／Stop／start screen 等改用 semantic colors。
  - Fonts：IBM Plex Sans JP（body＋CJK）同 IBM Plex Mono（Trellis labels），Font Awesome merge；`resources/font/`＋OFL；經 `content_path`（project `rom/font/` → `resources/font/`）。ImGui 1.92 dynamic atlas；UI zoom 用 `FontScaleMain` 重 rasterize。
  - Engine Settings 選 preset／scheme；寫入 `rom/editor/ui_prefs.json` 同 `project.json` → `editor`；非法值回退 Trellis＋Ivy。
  - Linux：`./tools/run_editor_theme_check.sh`。Windows 要目視每組 preset×scheme、zoom 清晰度、Asset Browser CJK 檔名、Play／Stop 色。


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

現況：3D main camera frustum culling（PR #25 `ba52894`）同 CSM per-cascade caster culling（PR #27 合併 `6db6040`）已入 master；後者 Windows Release A/B 已驗收。2D ortho sprite view-rect culling（PR #28 合併 `6adb343`）Windows Release runtime 已驗收。distance culling（`visual.cullDistance`）同 mesh LOD v1 已有 source（等 Windows 驗收）；仍然冇 occlusion culling；billboard／particle／effect 未做（見下面「Culling／LOD 缺口盤點」）。Physics AABB 仍然唔共用。`D3D11_QUERY` 仍然只見 timestamp／pipeline stats。

參考：Unity 模型——自動 frustum culling、手動 baked occlusion、Unity 6 GPU occlusion、`CullingGroup` API。

- [x] **Frustum culling（3D）**（PR #25 `ba52894`；Release counters 喺 PR #27 跑時見過；單獨像素 A/B 未另記）
  - 每個 mesh 喺 load 時 cache local AABB（FBX／OBJ／primitives；skinned 喺 bind-pose 上再 inflate 半個最大半軸，保守包動畫）。`GameObject::DrawVisual` 轉 world AABB（scale／rotation 用 8 corners），對 camera view×proj 抽出嘅 6 planes（D3D z∈[0,1]）測試；完全在外就唔 `Model3D::Draw`。無有效 bounds 永不 cull。
  - **Shadow**：main pass 先 cull；camera frustum **唔**閘 shadow depth。Per-cascade light-ortho cull 見下項。
  - Toggle：`settings.render.frustumCullingEnabled`（預設 true；Rendering Settings checkbox）。Counters：considered／culled／drawn —— Rendering Settings 即時顯示、collision-viz overlay、play FPS overlay、runtime bridge `perf.frustumCull`。
  - Linux：`g++ -std=c++17 -I. -Itools/linux_win32_shim tools/frustum_cull_check.cpp -o /tmp/frustum_cull_check && /tmp/frustum_cull_check`。
  - 證據（PR #27 跑時順帶）：Windows Release 望開時 main camera culled 21／23 objects。
  - 剩餘：PR #25 單獨嘅 ON／OFF 像素 A/B 未另記；更緊嘅 per-clip skinned bounds（而家係 inflate）。
- [x] **Shadow cascade caster culling**（PR #27 合併 `6db6040`；Terry 接受 Windows Release runtime）
  - 每個 cascade 用 `lightView*lightProj` 抽出 6 planes；AABB 測試 **跳過 near**（open toward light／pancaking），只 cull 側向同 far。畫面外但仍投影入 cascade 嘅 caster 會畫 shadow。
  - Toggle：`settings.render.shadowCascadeCullingEnabled`（預設 true）。Counters：frame totals + per-cascade —— Rendering Settings；frame totals —— FPS／collision-viz／`perf.shadowCull`。
  - Linux：`./tools/run_shadow_cull_check.sh`。
  - 驗收（Windows Release A/B）：望住場景／望開：vertices −60%／−74%，GPU −0.7／−1.04 ms，像素 diff 0；畫面外 caster 陰影仍在；`shadowCull` culled 67／92（望住）同 75／92（望開）。
- [x] **2D camera rect culling**（PR #28 合併 `6adb343`；Terry 接受 Windows Release runtime）
  - `SpriteRenderer` 喺 `Draw` 前用 size／pivot／world matrix（含 rotation／scale／renderOffset）計保守 world AABB，對 ortho camera 嘅 world-XY view rect（含細 margin）測試；完全在外就唔 `Sprite_DrawWorld`。Animator／physics／scripts 唔受影響。Screen-space UI 唔閘。Perspective camera 下 sprite 保持 unculled。
  - Toggle：`settings.render.spriteCullingEnabled`（預設 true）。Counters：considered／culled／drawn —— Rendering Settings、collision-viz、FPS `2CULL`、`perf.spriteCull`。
  - Linux：`./tools/run_sprite_cull_check.sh`。
  - 驗收：Windows Debug＋Release 已 build；Release ON vs OFF capture 像素 diff 0；115-sprite 場景 overlay `2CULL c:115 x:84 d:31`，sprite draws 115→31，GPU 0.97→0.17 ms。Terry 批准合併。
  - 剩餘：tilemap／billboard／particle 之後另做。
- [ ] **Distance cull 同 mesh LOD**（source 已做 v1；等 Windows 驗收）
  - `visual.lods`（`{model, maxDistance}` 陣列，level 0 係 `model`，`maxDistance` 係切換去該級嘅 camera 距離）同 `visual.cullDistance`。距離用 camera → 基礎 mesh world AABB 中心，5% hysteresis；`render.lodEnabled`（預設 true）、`render.lodBias`（預設 1，0.1–10）。選擇數學喺 `engine/rendering/culling/lod_select_math.h`，狀態同計數器喺 `lod_state.{h,cpp}`，接入點係 `GameObject::DrawVisual`。
  - Shadow pass 跟主 camera 揀嘅 level（`Lod_BeginFrame` 喺 shadow cascade 之前）。`animate: true` 嘅 Visual 只用 level 0（`cullDistance` 仍生效）；`proxy_model` edit view 唔行 LOD；LOD mesh 缺失退回 level 0。Preload 會載入 LOD mesh。
  - Counters：Rendering Settings、collision-viz、FPS `LOD`、bridge `perf.lod`；bridge `set_setting` 加 `lodEnabled`。Inspector Visual 區有 LOD 清單同 Cull Distance。
  - Linux：`tools/run_lod_check.sh`。驗收場景：`testbed/rom/scene/scene_lod.json`——可行走嘅場地（200×200 地面、`SamplePlayerScript` TPS 玩家、70 棵松樹、28 粒岩石）。程序生成嘅原創 mesh（`testbed/rom/model/lod/`，每個物件 4 級、同形狀同底部原點）：`Boulder_LOD0..3`（2562／642／162／42 頂點，即 5120／1280／320／80 三角面；LOD0 平滑，其餘稜角；大塊起伏每級一致，細緻脊線同碎片只喺高級別出現）同 `PineTree_LOD0..3`（390／102／54／31 頂點，6／5／4／3 層，LOD0 係波浪邊針葉）。有程序貼圖（`rock_albedo.png` 512²、`bark.png`、`needles.png`，可無縫重複，固定 seed），FBX 用 Blender 同款 `Texture`／`Video`／`DiffuseColor` 連接結構，材質顏色設白色避免同貼圖相乘變暗；岩石用 box 投影 UV，樹用圓柱 UV（逐 polygon 解包，冇接縫拉伸）。**LOD 距離按物件大細計**：用螢幕覆蓋比例反推（FOV 45°，d = 大小 / (0.828 × 覆蓋)），LOD0→1 喺約 12% 螢幕高度（1080p 約 120 px）、→2 喺 6%、→3 喺 3%；樹用樹冠闊度 3.3 m×scale，岩用 1.1 m×scale。結果：岩石 18–44／36–88／73–176 m，樹 27–50／53–100／106–199 m（舊版全部岩石固定 20／45 m，大岩喺 20 m 已佔 24% 螢幕高度所以太激進）。想再遲啲切換就調細 `render.lodBias`（例如 0.5 = 所有距離加倍）。全局旋鈕：`render.cullDistanceScale`（縮放所有 `cullDistance`，>1 = 更遠先剔除）同 `render.shadowDistance`（0 = Shadow Quality 預設範圍，>0 覆蓋，5–1000），Rendering 面板各有滑桿；Linux：`tests/cull_settings_test.cpp`。出生點前面有「強制」展示排：同一個物件分別鎖定 LOD0／1／2／3（松樹 z=11、岩石 z=19，x=−9／−3／3／9），唔使走動就可以並排比較。全部由 `python3 tools/gen_lod_test_assets.py` 生成（需要 numpy＋Pillow；ASCII FBX，Linux 只做咗閉合流形＋正體積＋索引／連接自我檢查，FBX SDK 匯入同貼圖載入要 Windows 驗收；如果貼圖冇出現，睇 `content_hot_reload.log`／Debug 輸出有冇 FBX 或 texture 載入錯誤）。
  - 行走驗證（testbed 冇 `GameScripts`，唔使自己編譯腳本）：`SamplePlayerScript`（WASD／左 stick 行、Space 跳、滑鼠轉鏡頭）而家同時編譯入 **Debug `IvyEngine`** 同 **Debug `IvyEngineRuntime`**（兩個 vcxproj 都有 `samples\scripts\SamplePlayerScript`，Release 冇；外部 `GameScripts.dll` 入面同名腳本會覆蓋佢，`ScriptFactory::Register` 係按名覆蓋）。重新 build Debug 編輯器後：File → Open Testbed → 打開 `scene_lod.json` → F2 Play 就可以行（`settings.camera.mode = "thirdPerson"`，TPS 鏡頭跟住 Player）。
  - 完全唔使腳本嘅後備：`testbed/rom/scene/scene_lod_fly.json`（同一個場地，冇 Player；`debugCamera` fly + `enabledOnPlay`，Play 時直接用自由飛行鏡頭：RMB + WASD、Shift 加速）。Release 編輯器或者未重新 build 時用呢個。Counters：Rendering 面板、View → Show FPS When Playing、F11 overlay 嘅 `lod` 行。
  - 驗收（Windows Debug＋Release，Terry）：行近行遠目視切換、counters 正確、影子冇跳變、`lodEnabled` A/B 頂點數／GPU ms、`animate` 物件冇回歸、Inspector 加減 LOD＋Undo／Redo＋保存重載、LOD mesh hot reload。
  - 未做：per-layer cull distance、自動減面、skinned LOD、per-clip bounds。
- [ ] **Occlusion culling（暫緩）**
  - 等 Release profiling 顯示 draw call／overdraw 係瓶頸、場景有大量遮擋先做。起點：D3D11 occlusion predicate（`D3D11_QUERY_OCCLUSION_PREDICATE`）或 Hi-Z（上一 frame depth pyramid）；baked PVS（似 Unity Umbra）需要 editor bake 流程，放最後。

### Culling／LOD 缺口盤點（2026-11-18 source audit，每項都對過代碼）

唔屬於上面 LOD v1 嘅範圍，建議每個主題獨立 PR（每個都要自己嘅 Windows A/B 證據）。順序按效益／風險。

- [x] **`DrawVisual` 先剔除再設定**（source 已做，等 Windows 驗收）：`GameObject::DrawVisual`（`engine/ecs/game_object.cpp`）而家先做距離／frustum／cascade 剔除，確定要畫先做 `SetEmission`／`SetShaderVariant`／`SetMaterialOverride` 同 model 姿態設定；被剔除嘅物件唔再付呢啲成本，亦唔會改 model 嘅 pose／override 狀態。剔除結果同 counters 不變。仍然冇空間分割，逐個物件 O(N) 遍歷。
- [x] **離屏／遠距動畫降頻或暫停**（source 已做，等 Windows 驗收）
  - `DrawVisual` 喺 Play 嘅 real-model pass 為每個 Visual 記低「最後被考慮」同「最後被任何 pass（主鏡頭或 shadow cascade）提交」嘅 frame serial（`Lod_GetFrameSerial`，`lod_state.{h,cpp}`）。下一幀 `VisualComponent::Update` 用 `anim_cull::ShouldSkipPose`（`engine/rendering/culling/anim_cull_math.h`）判斷：上一幀有被考慮但冇被畫 → 唔做 `UpdateAnimation`（跳過 bone 採樣、skinning、palette），只用新加嘅 `Model3D::AdvanceAnimationTime` 推進 clip 時間，所以返回畫面時相位連續。冇被考慮過嘅 Visual（新生成、父節點 inactive）照常更新，唔會喺第一次畫之前凍結。Shadow cascade 仍然畫到嘅 caster 會維持更新。
  - 涵蓋 frustum 剔除、`cullDistance` 剔除同兩者都冇畫嘅情況；Toggle：`render.animCullingEnabled`（預設 true；Rendering 面板、bridge `set_setting`）。Counters：Rendering 面板、collision-viz、FPS `ANIM u/s`、bridge `perf.anim.{updated,skipped}`（上一個 update pass）。
  - 已知限制：重新入畫面嗰一幀可能顯示舊 pose（一幀滯後）；腳本讀離屏角色 bone（`TryGetBoneLocalMatrix`）會拎到最後一次評估嘅 pose；未做降頻（只有「畫／唔畫」兩級）、物理／腳本唔受影響。
  - Linux：`tools/run_anim_cull_check.sh`（決策邏輯）。驗收場景：`testbed/rom/scene/scene_anim_cull_fly.json`（`python3 tools/gen_anim_cull_scene.py` 生成；fly 鏡頭，冇腳本）：前排 5、後排 5（鏡頭後 45 m）、左右 4、遠排 4（`cullDistance 40`）、鏡頭後 12 m 嘅 1 個 shadow caster，全部循環播 Idle。
  - 驗收（Windows Debug＋Release，Terry）：起始望前面 → `ANIM u` ≈ 前排＋可能嘅 shadow caster，`s` ≈ 後排＋左右＋遠排；Rendering 面板關 `Off-screen Animation Culling` 後 `s` 變 0、`u` 變全部；轉身 180° 後原本被跳過嘅後排同前排 pose 一致（相位連續，唔應該跳返 T-pose／第 0 幀）；`animCullingEnabled` ON vs OFF 同一視角截圖像素 diff 0；GPU／update ms A/B；shadow caster 嘅影子唔凍結；Edit／Play 切換同 scene 重載後冇凍結角色。
- [x] **燈光揀選**（source 已做，等 Windows 驗收）
  - `SceneLightingSyncFromNodes(roots, cameraPosition)`（`engine/scene/scene_lighting.cpp`）喺啟用燈光多過 `Lighting::MAX_LIGHTS`（4）時，用 `light_select::Select`（`engine/rendering/culling/light_select_math.h`）揀：directional 優先，然後 point／spot 按 camera 處影響力 `亮度 × range² / (range² + 距離²)`（亮度 = colour × intensity 最大分量）排序；上一幀揀中嘅燈有 1.25× 加成避免臨界閃爍；揀中嘅燈保持場景順序，所以 shadow caster 索引穩定。≤ 4 盞燈嘅場景行為完全不變。
  - Toggle：`render.lightSelectionEnabled`（預設 true；Rendering 面板、bridge `set_setting`）。關閉 = 舊行為（場景順序頭 4 盞）。Counters：Rendering 面板 `lights enabled / used`。
  - 未做：spot 錐形方向／物件包圍盒嘅影響力、per-object 燈光（forward+ 或 tiled）、燈光 fade-in/out、spot／point shadow。
  - Linux：`tools/run_light_select_check.sh`（選擇數學）。驗收場景：`testbed/rom/scene/scene_light_select_fly.json`（`python3 tools/gen_light_select_scene.py` 生成）：1 盞暗 directional ＋ 12 盞彩色 point light 排成一行，13 盞燈對 4 個 slot。
  - 驗收（Windows Debug＋Release，Terry）：Play 後沿住燈行飛，開住 Light Selection 時最近嘅 3 盞 point light 照亮地面，亮區跟住鏡頭移動，交界處冇明顯閃爍；`lights enabled 13 | used 4`；關掉之後永遠只係行頭嗰 3 盞（最左）亮；`≤ 4` 盞燈嘅現有場景（例如 `scene_lod.json`）畫面冇變。
- [x] **Billboard／粒子／Effekseer 剔除**（source 已做，等 Windows 驗收；粒子同 Effekseer 要有腳本／effect 資產嘅 content project 先驗到）
  - **Billboard**：`Billboard::Draw`（`engine/rendering/billboard/billboard.cpp`）按 view×proj 對清單做球體 frustum 剔除（半徑 = 四邊形半對角線，camera-facing 同 ground-aligned 都適用），用 `remove_if` 保持遠到近次序；腳本 `Billboard::Add` 同 `BillboardComponent` 都涵蓋。Toggle：`render.billboardCullingEnabled`。
  - **粒子**：`ParticleRenderer_RenderEmitter` 對每粒粒子做球體測試（半徑 0.75×size），被剔除嘅粒子唔入 instance buffer，全部被剔除就唔 Map／Draw。同一個 toggle。模擬、spawn、壽命完全唔受影響。
  - **Effekseer**：`EffectSystem` 追蹤每個 handle 嘅位置同縮放（`Play`／`SetLocation`／`SetScale`），`Draw3D` 前按 `effectCullRadius`（預設 20 m）× 最大縮放分量做球體測試，用 `Manager::SetAutoDrawing` 開關，所以 `Manager::Draw` 保持原有繪製次序；effect 照常模擬。Toggle：`render.effectCullingEnabled`、`render.effectCullRadius`（0.5–500）。Effect 冇 authored bounds，半徑係全局估算，大範圍 effect 要自己調大。
  - Counters：Rendering 面板（billboard／particle／effect 各 considered／culled）。未接 bridge `perf`；bridge `set_setting` 加 `billboardCullingEnabled`、`effectCullingEnabled`。
  - 未做：per-effect／per-billboard 剔除距離、Effekseer 自帶 culling world、粒子 emitter 級別剔除與距離縮減（`maxParticles` 按距離減）、粒子／effect 嘅 LOD。
  - Linux：`tools/run_fx_cull_check.sh`（半徑＋球體 frustum 數學）、`tests/cull_settings_test.cpp`（半徑 clamp）。驗收場景：`testbed/rom/scene/scene_billboard_cull_fly.json`（`python3 tools/gen_billboard_cull_scene.py` 生成：12×12 billboard 加 16 個地面標記，fly 鏡頭；Windows 已驗收：望天全部 160 個被剔除、只望一個時其餘約 150 個被剔除）同 `testbed/rom/scene/scene_particle_cull_fly.json`（Windows Debug 已驗收：望天全部粒子被剔除、只望一個 emitter 時約 100 粒未被剔除其餘全剔除；`python3 tools/gen_particle_cull_scene.py` 生成：三圈共 40 個 emitter，每個掛 `SampleParticleEmitterScript`（`samples/scripts/`，只編入 Debug editor／Debug runtime host，Release 冇粒子）；fly 鏡頭）。testbed 冇 Effekseer 資產，所以 effect 仍然要用有 effect 資產嘅 content project 驗。
  - 驗收（Windows Debug＋Release，Terry）：Billboard——`billboard considered 160 | culled N`（N 隨視角變，望向一邊時約一半）；開關 Billboard／Particle Culling 前後同一視角截圖像素 diff 0；邊緣冇閃現／消失；GPU ms A/B。粒子——開 `scene_particle_cull_fly.json`（Debug build）F2：`particle considered` 約 40 × 每 emitter 活粒子數；望住一個 emitter 時 `particle culled` 佔大部分，望天則接近全部；開關 Billboard／Particle Culling 前後同一視角畫面不變。Effekseer——`effect culled` > 0；effect 嘅 origin 離開畫面邊緣時有冇突然消失（有就調大 `effectCullRadius`）；返入畫面時 effect 進度連續（冇由頭播）。
- [x] **LOD 螢幕大小模式**（source 已做，等 Windows 驗收）
  - `visual.lodMode: "screenSize"`（預設 `"distance"`）＋ `lods[i].screenHeight`（0–1，視口高度比例；投影高度跌到呢個比例以下就切去呢級，由大到小排序，0 = 永不切換）。投影高度 = 基礎 mesh 世界 AABB 最長邊 /（2 × 距離 × tan(fov/2)）；`lod_select::ScreenHeightToDistance`（`lod_select_math.h`）將每個閾值換算成切換距離，之後照用原有距離選擇（hysteresis、`lodBias`、`cullDistance` 不變），所以接入點只係 `GameObject::DrawVisual` 同 `Lod_BeginFrame(pos, fov)`（新增 `Lod_GetTanHalfFov`）。Orthographic 相機保持 level 0。
  - Inspector：Visual 區 `Screen Size Mode` checkbox，每行改為 `%` 輸入；切換模式時會補預設值並重排。Save／load：兩個欄位有設就寫。
  - Linux：`tools/run_lod_check.sh`（換算公式、往返、同距離選擇配合）。驗收場景：`testbed/rom/scene/scene_lod_screen_fly.json`（`python3 tools/gen_lod_screen_scene.py` 生成；4 欄 boulder 縮放 1／2／4／8，全部用 12%／6%／3%）。
  - 驗收（Windows）：fly 遠離，細 boulder 好早變 LOD1，大 boulder 要遠好多先變，切換距離大約同尺寸成正比（細 boulder 喺 45° FOV 下約 11／22／44 m，×8 嗰欄約 8 倍；實際數字隨相機 FOV 變）；Rendering 面板 `lod L0 / L1 / L2+` 隨距離變；縮窄 FOV 時 LOD 保持得更遠；Inspector 開關模式、改 %、Undo 之外嘅保存重載；舊場景（distance 模式）畫面不變。
  - Debug 標籤：`render.lodDebugLabels`（預設 false；Rendering 面板 `LOD Debug Labels`；bridge `set_setting` 收）。`DrawVisual` 喺主 camera pass 為每個有 LOD／cullDistance 嘅 Visual 記低 `LodDebugLabel`（`lod_state`），`main.cpp` 喺 FPS overlay 之後將 AABB 頂部投影到螢幕，用 `hal::DebugText` 畫 `L<級> <距離>m <螢幕高度%>`（綠／黃／橙／紅 = L0／L1／L2／L3）；最近優先、上限 96 個。`scene_lod_screen_fly.json` 預設開。
  - 未做：以 bounding sphere 而非 AABB 最長邊計大小、per-camera（shadow 專用）螢幕大小、editor 視圖預覽。
- [ ] **LOD cross-fade／dither**：切換係硬切。
- [x] **陰影投影物用較低 LOD**（source 已做，等 Windows 驗收）
  - `render.shadowLodOffset`（預設 0，0–3；Rendering 面板 `Shadow LOD Offset`）：cascade pass 喺 `GameObject::DrawVisual` 用 `lod_select::ShadowCasterLevel(cameraLevel, offset, count)` 揀 mesh——相機級別往低細節方向推 `offset` 級，唔超過最後一級；`cullDistance` 剔除照舊，`animate: true` 嘅 Visual 只有 level 0 所以唔受影響。Hysteresis 記憶仍然記相機級別，唔會被 offset 污染。預設 0 = 行為不變。
  - Linux：`tools/run_lod_check.sh`（`ShadowCasterLevel`、clamp）。驗收場景：`testbed/rom/scene/scene_lod_fly.json` 嘅強制 LOD0 展示排（offset 3 時影子變 LOD3 形狀）。
  - 驗收（Windows）：offset 0 影子同之前一樣；offset 1／2／3 影子逐級變粗但位置唔飄；Rendering 面板 vertices／GPU ms A/B；遠距樹影冇跳變。
- [ ] **編輯器 LOD 工具**：編輯視圖用 proxy（`DrawVisual(false)`）所以唔行 LOD；冇強制指定某級嘅 debug 選項，冇顯示邊界、距離嘅 gizmo。
- [ ] **腳本同 bridge 可控性**（bridge 部分已做，等 Windows 驗收；腳本部分未做）
  - Bridge：`set_setting` 而家除布林外亦收數值 key（`lodBias`、`cullDistanceScale`、`shadowDistance`、`shadowLodOffset`、`effectCullRadius`；JSON number、必須有限；host 按場景載入嘅同一組 clamp 夾住，超範圍唔會報錯）。`tools/runtime_test_client.py` 同 `--value` 一併支援；Linux：`tools/run_runtime_bridge_offline_linux.sh`、`python3 -m unittest tools/runtime_test_client_check.py`。
  - 未做：`ScriptRuntimeApi` 嘅 LOD／culling／render 設定介面（要 append-only 並 bump 版本，目前仍然係 48）；bridge `perf` 未輸出 anim／fx／light 計數。
- [ ] **LOD／剔除嘅進階項**（暫緩）：HLOD／impostor、自動減面、per-clip skinned bounds、per-layer／per-tag cull distance。
- 唔算缺口：texture mip chain 已由 `model_util.cpp` 生成；engine 冇 tilemap 系統；screen-space UI 唔剔除係設計。

## 歷史 backlog（source audit 結果）

以下已對 `3f5afe9` 源碼同 git 核實。勾選代表 source review 完成，唔等於另一次 runtime acceptance。

- [x] **Trigger 系統**：Enter／Exit 已有。`1499a44`；`Collider::m_isTrigger`、`physics_system.cpp` 的 `DiffAndDispatchTriggerEvents`、script `OnTriggerEnter`／`OnTriggerExit`、Properties checkbox。`OnTriggerStay` 未實作；只有需要 Stay 先另開一項，唔好再掛「還債」。
- [x] **Prefab drag-drop Undo**：hierarchy 同 root drop zone 接受 `ASSET_PREFAB` 後呼叫 `EditorUndo_PushCreate`（`engine/core/engine_gui.cpp`）；`CreateObjectCommand::Undo` 會刪走個節點（`engine/editor/editor_undo.cpp`）。
- [x] **deploy.bat executable 命名**：bat 仍複製 `IvyEngineRuntime.exe`（`external_content_template/deploy/deploy.bat`）。Editor package 用 `SanitizeGameExeStem` 寫成 `<GameName>.exe`（`engine/editor/playable_package_builder.cpp`）。兩邊嘅分別已寫喺 `docs/ENGINE_USER_GUIDE.md` §17／§21。若要 bat 都輸出遊戲名，先另開窄項。
- [x] **Asset cache LRU／refcount**：`ModelAssetCache` 係 process-lifetime `std::map`（`engine/ecs/model_asset_cache.h`），同 README 寫法一致。冇 LRU、冇 refcount，亦冇「曾經做完再拆走」嘅 commit。有記憶體數字先重開。
- [x] **Mesh collider recursion crash**：`915dce7` 停止 Box／mesh／cylinder 互相呼叫直到 stack overflow。當時改為無碰撞並 log。接觸已喺 source 實作，Windows runtime 未驗收，見上面「Cylinder pair collision」。唔好再當未還原嘅舊故障。

PBR 球場已由引擎待辦移除：倉庫只有 PBR shader 同 `testbed/rom/scene/scene_pbr_showcase.json` 材質展示，冇球場內容；`FrisbeeArena` 屬已完成遊戲。

## 最近同步留下嘅驗證事項

- [x] **ImGuizmo／ImGui AddPolyline 參數順序**（Terry 喺 2026-11-13 喺 Windows editor 目視驗收：旋轉環 2px、旋轉扇形邊閉合、平移平面不變）
  - `libraries/imguizmo/ImGuizmo.cpp` 三個 `AddPolyline` 已改返 ImGui 1.92.5 簽名 `(flags, thickness)`。Linux：`python3 tools/check_imguizmo_addpolyline_order.py`。MSVC 仍保留 PR #7 對 `ImGuizmo.cpp` 嘅 C4244 suppression。
- [x] **PR #5 資產清理**：被刪嘅 `.ua/`、`rom/effects/`、`rom/skybox/001/`、`rom/model/character/YBot_white.fbx`、`rom/model/object/{tree,Wall,Cone_Yellow}.fbx`、`external_template/`、已刪 sample script，scene／prefab JSON 冇殘留引用。玩家模型其後喺 `testbed/rom/model/player/Y Bot_white.fbx`。`rom/effects/` 只剩 hot-reload 路徑前綴。當時 `samples/scenes` 的 2D 場景引用 `rom/texture/sprite_demo/`，呢批 PNG 唔喺 repo、亦唔係 PR #5 刪走。之後 testbed 用細 placeholder PNG 補咗四張圖（`adventurer_walk`、`test_sheet`、`hp_fill`、`hp_frame`），場景已移去 `testbed/rom/scene/`。
- [x] **In-repo testbed**：`testbed/` 係 Debug editor 喺冇最近專案可開時會打開嘅 content project。`samples/scenes` 已移入 `testbed/rom/scene/`。引擎預設其後搬去 repo 根 `resources/`，用 `rom/<rest>` → `resources/<rest>` fallback，唔再做 NTFS junction。Testbed 專用模型同 sprite placeholder 喺 `testbed/rom/`。Linux：`tools/content_path_check.cpp`、`python3 tools/check_testbed_assets.py`、`python3 tools/check_engine_resource_paths.py`。Windows editor 實測未在呢次做。
- [ ] **PR #7 視覺 regression**：Terry 確認合併前 Windows build 已成功。仍然開住嘅只係畫面：lighting、CSM、post-process、gizmo。Linux 不能代替呢次目視；ImGuizmo 線條見上一項。

> 勾選係 source review 或 Terry 對 build 嘅確認。畫面同玩法驗收仍然分開。

## 已完成項目

已完成嘅 Physics Material、primitive meshes、render thread、Inspector Undo 修補、IBL／資產補齊、angular dynamics v1、Cylinder recursion fix、Collider Auto-Fit/scaling，保留喺 [[Engine Changelog]] 嘅完成紀錄，唔再混入未完成清單。歷史 backlog 嘅勾選同證據留喺上面一節。

- [x] **Runtime test bridge（Phases 1–7）**：Runtime + Editor Play mode、scenario runner（PR #14–#21、#23、#24、#26、#29）。詳見 `docs/RUNTIME_TEST_BRIDGE.md`。
- [x] **Agent docs pack**：`docs/agent/` + `tools/export_agent_docs.py`（PR #30 合併 `025ea1b`）。入口：`docs/agent/README.md`。

## 建議優先次序

1. PR #7 視覺檢查（lighting、CSM、post-process、gizmo）喺 Windows 做（ImGuizmo AddPolyline 已驗收）。
2. 重做 Properties Panel／Inspector（先有 UI 草案；現有編輯同 Undo 要保住）。
   Culling／LOD 跟進（獨立 PR，順序：`DrawVisual` 先剔除、離屏動畫降頻 → 燈光揀選 → billboard／粒子／effect 剔除）見「Culling／LOD 缺口盤點」。
3. AI 套件：Waypoint → Perception → BT／Blackboard → EQS。NavMesh 繼續暫緩。

Box friction torque 同 World-space UI 未排入上面次序：前者只有想箱子傾側先做；後者驗收仍待設計。Culling 主線（3D frustum／shadow cascade／2D sprite）已入 master；距離／occlusion／tilemap／billboard／particle 未排入。

## 維護規則

- 之後新增／完成／暫緩 engine 待辦，集中更新本頁；Changelog 記做過嘅事同證據。
- 完成要附 commit／實測或 Terry 確認；source review 同 runtime acceptance 分開。
- 歷史項目核實前唔直接轉成 implementation prompt。

[[Engine Changelog]]
