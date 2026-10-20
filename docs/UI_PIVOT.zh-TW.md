# UI Pivot

UI Sprite、Button、Bar、Text 與世界 Sprite 共用 Pivot 下拉選單：Center、Top Left、Top Center、Top Right、Middle Left、Middle Right、Bottom Left、Bottom Center、Bottom Right、Custom。

- Anchor：畫布上的定位基準。
- Position：Pivot 相對於 Anchor 的位移，UI 座標仍是 X 向右、Y 向下。
- Pivot：元素自身的定位原點；與世界 Sprite 一樣使用左下 `(0,0)`、左上 `(0,1)`、中心 `(0.5,0.5)`。

例如 Anchor=Center、Position=(0,0)、Pivot=Center，會將元素置中於畫布。改為 Pivot=Top Left，元素左上角就位於畫布中心；此操作保留 Position，讓元素相對原點移動。

UI Gizmo 的移動箭頭位於 Pivot。選取外框仍顯示元素範圍；縮放控制點位於外框邊角，拖曳縮放時會補償 Position。Bar 的 Pivot 以 bar 本體為準，標籤仍可顯示在其上方。

UI Sprite 的旋轉會繞明確設定的 Pivot 進行，選取範圍涵蓋旋轉後外框。已旋轉的 UI Sprite 使用 Inspector 的 Width／Height 調整尺寸，避免以軸對齊外接框反推尺寸。

Text 的 Pivot 作用於整個文字區塊，包括多行高度。設定 Pivot 後，舊的 Alignment 選項會停用，改由 Pivot 決定文字區塊定位。

相容性：舊場景沒有 `pivot` 欄位時保留原有排版及文字 Alignment；只有明確設定 Pivot 才使用新排版。Pivot 儲存在 UI 共用資料的 `pivot: {x, y}`，沿用場景儲存及 Undo／Redo。Pivot 使用 sidecar 資料，不改變既有 UI 類別的物件大小或虛擬函式順序。

驗證：Editor/runtime Debug x64 建置通過；`tools/ui_pivot_check.cpp` 驗證九個位置、多種 viewport 縮放與縮放時的定位補償。實際 GUI 拖曳、文字顯示與 Undo／Redo 尚待操作驗收。
