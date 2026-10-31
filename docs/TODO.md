# Ivy Engine — To-do

> 引擎待辦集中入口。來源：[[Engine Changelog]]「留咗做」及 ivy-engine skill 歷史 backlog。
> 整理基準：remote `3f5afe9`。下面係已記錄嘅計劃，唔代表已逐項對當前源碼重新 audit；實作前要核實現有 hooks，避免重做已有系統。
> 已完成遊戲（MagneticField／FrisbeeArena／GravitySwing）嘅舊遊戲任務唔列入引擎待辦。未編優先次序，清單順序唔等於排期。

## 已列入目前 backlog（尚未驗收完成）

- [ ] **重做 Properties Panel／Inspector**
  - 先盤點 component/property 編輯、序列化、Undo/Redo、選取流程，提出 UI 草案同明確 scope，再實作。
  - 目標：分區／欄位清晰、編輯穩定，保留現有功能。
  - 驗收：常用欄位編輯／保存／重載正常；Undo/Redo 有效；新增／移除 component、資產選擇無回歸。
- [ ] **World-space UI 編輯器支援**
  - 可以建立、擺位同編輯 world-space UI；與 screen-space canvas 分開。
  - 交互同具體驗收範圍待設計。

## AI 系統套件（規劃，逐項實作狀態待 audit）

- [ ] **Blackboard**：key-value 短期記憶；先 audit 現有 script/component/data APIs，定最小 scope。
- [ ] **BehaviorTree runner**：Selector／Sequence／Task／Decorator＋Blackboard condition；abort 機制後補。
- [ ] **Perception**：sight cone＋LOS raycast；LoseSightRadius > SightRadius 防 flicker，結果寫入 Blackboard。
- [ ] **EQS 式 utility scoring**：候選位置按距離／方向／路徑成本加權。
- [ ] **AI debug visualization**：BT current node、perception cone、path，接現有 DebugSystem。
- [ ] **Waypoint component＋PatrolRoute**：editor 邏輯 marker（位置＋可選 tag）、路線陣列／current index、到達後 next。

### 暫緩

- [ ] **NavMesh／AI pathfinding**：暫緩，之後按遊戲需要排期。
  - 既有規劃包含 grid-based pathfinding＋path smoothing；與本項合併管理，唔當兩份獨立必做任務。
  - 無障礙 arena 可先用 steering，唔預設一定要 NavMesh。

## 歷史 backlog：待重新核實，唔當已確認缺失

以下只係 skill 留低嘅舊記錄；可能已完成、部分完成或已唔適用。先 audit，再決定保留／勾選／移除。

- [ ] Trigger 系統還債：舊項目未寫清欠邊部分，要先還原原本 scope。
- [ ] PBR 球場：舊展示／內容任務，先確認仲需唔需要。
- [ ] Asset cache LRU／refcount：先檢查現有生命週期同快取策略。
- [ ] Prefab drag-drop Undo：核實目前覆蓋範圍。
- [ ] deploy.bat executable 命名統一：已有部署命名歷史改動，唔預設仍未修。
- [ ] Mesh collider 修復：舊故障紀錄，須先重現及核實最新 collision dispatch。

## 最近同步留下嘅驗證事項（唔係已確認 bug）

- [ ] ImGuizmo／ImGui AddPolyline flags-thickness 版本相容：PR #7 只 suppress 單檔 C4244，唔等於已修行為；先查目前 API 同實際 gizmo。
- [ ] PR #5 資產清理後 clean checkout／package regression：確認 sample／runtime 真正需要嘅 assets 都有齊。
- [ ] PR #7 build／shader 視覺 regression：本次同步無重新 build/check/runtime，唔當 warning-free 或視覺等價已驗收。

> 呢幾項係本次 review 嘅驗證跟進，唔代表 Terry 從未測過，亦唔自動升格成新功能排期。

## 已完成項目

已完成嘅 Physics Material、primitive meshes、render thread、Inspector Undo 修補、IBL／資產補齊、angular dynamics v1、Cylinder recursion fix、Collider Auto-Fit/scaling，保留喺 [[Engine Changelog]] 嘅完成紀錄，唔再混入未完成清單。

## 維護規則

- 之後新增／完成／暫緩 engine 待辦，集中更新本頁；Changelog 記做過嘅事同證據。
- 完成要附 commit／實測或 Terry 確認；source review 同 runtime acceptance 分開。
- 歷史項目核實前唔直接轉成 implementation prompt。

[[Engine Changelog]]
