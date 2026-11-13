# Input

Scripts typically call input headers directly (samples do). The same entry points are also exposed on the `api:ScriptRuntimeApi` host table for DLL use.

## Keyboard

- `api:Keyboard_GetState` / `api:Keyboard_GetStateOld` → `api:Keyboard_State`
- `api:KeyLogger_IsTrigger` — true only on the press frame (`api:Keyboard_Keys`, e.g. sample uses `KK_SPACE`, `KK_R`)

## Mouse

- `api:Mouse_IsLeftDown` / `api:Mouse_IsMiddleDown` / `api:Mouse_IsRightDown`
- `api:Mouse_GetWheelDelta`
- `api:Mouse_GetPosition` or wrapper `api:ScriptApi_MouseGetPosition`

## Controller

- `api:Controller_GetLeftStick` / `api:Controller_GetRightStick`
- `api:Controller_GetLeftTrigger` / `api:Controller_GetRightTrigger`
- `api:Controller_IsPressed` with `api:ControllerButton`

Automated injection uses the Debug test bridge (`keyboard` / `mouse` / `controller` verbs) — see [`../testing-bridge.md`](../testing-bridge.md).
