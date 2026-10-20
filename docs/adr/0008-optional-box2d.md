# ADR 0008：可選 2D 物理採 Box2D 3.1.1

狀態：已採納及實作；使用者已確認第六階段與首版 2D 計畫完成（2026-10-20）。

選擇官方 Box2D 3.1.1 的 C17 API，以獨立 C++ adapter 連接 GameObject。相較自行維護 Box／Circle 解算、連續碰撞與接觸生命週期，成熟解算器減少維護成本。MIT 授權適合隨引擎散發；授權檔隨原始碼保留。

固定版本直接加入既有 MSBuild 專案，不新增套件管理器或執行時 DLL。使用 scalar 編譯，避免新增 AVX 硬件需求。3D 引擎與物理路徑沿用原有實作，只有 physicsMode 2d 建立 Box2D world。

首版只公開 Box／Circle、三種 body、Trigger、篩選及 Raycast。上游包含其他功能，但引擎不公開 joints 或任意 polygon。動態／kinematic 為語意根節點，固定 scale，物理負責回寫 transform。事件透過獨立 listener，保持既有 Script 的虛擬介面。

來源：
- https://github.com/erincatto/box2d/releases/tag/v3.1.1
- https://box2d.org/documentation/md_simulation.html
