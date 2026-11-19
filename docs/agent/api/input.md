# Input

Prefer **input actions** for gameplay: named actions with player-rebindable bindings (v52). Raw device calls below still work and are exposed on the `api:ScriptRuntimeApi` host table for DLL use.

## Input actions (v52)

Defaults live in `project.json` → `input.actions` (editor: Project Settings → Input). Decision record: engine `docs/adr/0010-input-actions.md`.

```json
"input": { "actions": {
  "Jump":  { "type": "button", "bindings": ["Key.SPACE", "Pad.A"] },
  "MoveX": { "type": "axis",   "bindings": ["Key.D/Key.A", "Pad.LeftStickX"] }
}}
```

Binding text: `Key.<NAME>` (`mod:key_names`, e.g. `Key.SPACE`, `Key.W`, `Key.0`), `Pad.A`/`B`/`X`/`Y`/`LB`/`RB`/`Start`/`Back`/`LeftThumb`/`RightThumb`/`DPadUp`…, `Pad.LT`/`Pad.RT` (0..1), `Pad.LeftStickX`/`LeftStickY`/`RightStickX`/`RightStickY` (-1..1) or with `+`/`-` (one direction), `Mouse.Left`/`Right`/`Middle`, and `A/B` pairs on axis actions (A − B). Only pad 0 is read.

| Purpose | Wrapper |
|---------|---------|
| Read | `api:ScriptApi_InputIsPressed`, `api:ScriptApi_InputIsTriggered` (pressed this frame), `api:ScriptApi_InputIsReleased`, `float ScriptApi_InputGetAxis(const char* action)` (`api:ScriptApi_InputGetAxis`; button actions read 0 / 1), `api:ScriptApi_InputHasAction` |
| Bindings | `std::vector<std::string> ScriptApi_InputGetBindings(const char* action)` / `bool ScriptApi_InputSetBindings(const char* action, const std::vector<std::string>& bindings)` — `api:ScriptApi_InputGetBindings`, `api:ScriptApi_InputSetBindings` |
| Press-a-key rebinding | `bool ScriptApi_InputCaptureStart(const char* action, int index, ScriptInputPart part = ScriptInputPart::Whole, bool includeMouse = true)` (`api:ScriptApi_InputCaptureStart`); poll `api:ScriptApi_InputCaptureState` each frame (`api:ScriptInputCaptureState` Done → applied); `api:ScriptApi_InputCaptureCancel`. Esc cancels; `index == count` appends; `api:ScriptInputPart` Positive / Negative edit one side of an axis pair |
| Defaults / persist | `api:ScriptApi_InputResetBindings` (one action, or all with null), `api:ScriptApi_InputSaveBindings` — writes the differences to save slot `ivy_input`; loaded automatically at start |

Unknown action names read false / 0 and log once in the Console. Reference: engine `samples/scripts/SampleRebindScript.cpp` (testbed `scene_input_rebind_demo.json`) and `SamplePlayerScript.cpp` (MoveX / MoveY / Jump with a raw-input fallback).

## Keyboard

- `api:Keyboard_GetState` / `api:Keyboard_GetStateOld` → `api:Keyboard_State`
- `api:KeyLogger_IsTrigger` — true only on the press frame (`api:Keyboard_Keys`, e.g. sample uses `KK_SPACE`, `KK_R`)

## Mouse

- `api:Mouse_IsLeftDown` / `api:Mouse_IsMiddleDown` / `api:Mouse_IsRightDown`
- `api:Mouse_GetWheelDelta`
- `api:Mouse_GetPosition` or `void ScriptApi_MouseGetPosition(int& x, int& y)` — `api:ScriptApi_MouseGetPosition`

## Controller

- `DirectX::XMFLOAT2 Controller_GetLeftStick(int padIndex = 0)` / `Controller_GetRightStick` — `api:Controller_GetLeftStick` / `api:Controller_GetRightStick`
- `float Controller_GetLeftTrigger(int padIndex = 0)` / `Controller_GetRightTrigger` — `api:Controller_GetLeftTrigger` / `api:Controller_GetRightTrigger`
- `bool Controller_IsPressed(ControllerButton button, int padIndex = 0)` — `api:Controller_IsPressed` with `api:ControllerButton`

Automated injection uses the Debug test bridge (`keyboard` / `mouse` / `controller` verbs) — see [`../testing-bridge.md`](../testing-bridge.md).
