# UI

Scene canvas + element scripts. Canvas blob is the scene JSON `canvas` field.

## Canvas access

- `api:ScriptApi_SceneCanvasGetCanvas` → `api:UICanvas`
- `api:ScriptApi_SceneCanvasSetTimerSeconds`
- `api:ScriptApi_SceneCanvasRequestSceneFile` — load `rom/...` scene; optional play-on-load
- `api:ScriptApi_UICanvasGetElement` / `api:ScriptApi_UICanvasGetSprite`
- `api:ScriptApi_UICanvasAddSprite` / `api:ScriptApi_UICanvasRemoveElement`
- `api:ScriptApi_UITextSetText` (`api:UIText`)
- `api:ScriptApi_UIElementSetVisible` (`api:UIElement`)
- Sprite helpers: `api:ScriptApi_UISpriteSetTexture`, `api:ScriptApi_UISpriteSetRadial360Fill`, `api:ScriptApi_UISpriteSetFillNone`, `api:ScriptApi_UISpriteSetSourceRect` (`api:UISprite`, `api:UIAnchor`)

## `api:UICanvasScript`

Separate base for UI element scripts (`ui_canvas_script.h`):

| Method | Role |
|--------|------|
| `OnCreate` / `OnUpdate` / `OnDestroy` | Element lifecycle (`UIElement& owner`) |
| `OnClick` | Button (or similar) activation |
| `Serialize` / `Deserialize` | Persist fields |

Register with `api:REGISTER_UI_SCRIPT` from `ui_canvas_script_register.h` (DLL vs static paths mirror `api:REGISTER_SCRIPT`).
