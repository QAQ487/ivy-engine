# UI

Scene canvas + element scripts. Canvas blob is the scene JSON `canvas` field.

## Canvas access

- `UICanvas* ScriptApi_SceneCanvasGetCanvas()` — `api:ScriptApi_SceneCanvasGetCanvas` → `api:UICanvas`
- `void ScriptApi_SceneCanvasSetTimerSeconds(float seconds)` — `api:ScriptApi_SceneCanvasSetTimerSeconds`
- `void ScriptApi_SceneCanvasRequestSceneFile(const char* sceneFile, bool playOnLoad)` — `api:ScriptApi_SceneCanvasRequestSceneFile`
- `UIElement* ScriptApi_UICanvasGetElement(UICanvas& canvas, const char* name)` / `UISprite* ScriptApi_UICanvasGetSprite(UICanvas& canvas, const char* name)` — `api:ScriptApi_UICanvasGetElement` / `api:ScriptApi_UICanvasGetSprite`
- `api:ScriptApi_UICanvasAddSprite` / `void ScriptApi_UICanvasRemoveElement(UICanvas& canvas, const char* name)` — `api:ScriptApi_UICanvasRemoveElement`
- `void ScriptApi_UITextSetText(UIText& text, const char* value)` — `api:ScriptApi_UITextSetText` (`api:UIText`)
- `void ScriptApi_UIElementSetVisible(UIElement& element, bool visible)` — `api:ScriptApi_UIElementSetVisible` (`api:UIElement`)
- Sprite helpers: `api:ScriptApi_UISpriteSetTexture`, `api:ScriptApi_UISpriteSetRadial360Fill`, `api:ScriptApi_UISpriteSetFillNone`, `api:ScriptApi_UISpriteSetSourceRect` (`api:UISprite`, `api:UIAnchor`)

## `api:UICanvasScript`

Separate base for UI element scripts (`ui_canvas_script.h`):

| Method | Role |
|--------|------|
| `OnCreate` / `OnUpdate` / `OnDestroy` | Element lifecycle (`UIElement& owner`) |
| `OnClick` | Button (or similar) activation |
| `Serialize` / `Deserialize` | Persist fields |

Register with `api:REGISTER_UI_SCRIPT` from `ui_canvas_script_register.h` (DLL vs static paths mirror `api:REGISTER_SCRIPT`).
