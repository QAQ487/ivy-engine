# Input actions: project.json defaults, player rebinding in a save slot

Scripts read raw devices (`KeyLogger_IsTrigger(KK_SPACE)`, `Controller_IsPressed(ControllerButton::A, 0)`), so every game hard-codes its keys and players cannot rebind them. Once games ship and players have rebinding saved, the binding text and where it is stored are hard to change, so they are fixed here.

**Decision**

- An **action** has a name (`[A-Za-z0-9_.-]{1,64}`), a type (`button` or `axis`) and a list of **bindings**. A button is pressed while any binding reads above 0.5. An axis reads the binding with the largest magnitude (-1..1).
- **Binding text** is the format in files and in the API: `Key.<name>` (names from `engine/input/key_names.h`, the same table as the test bridge), `Pad.A` … `Pad.DPadRight`, `Pad.LT` / `Pad.RT`, `Pad.LeftStickX` (full axis) or `Pad.LeftStickX+` / `-` (one direction), `Mouse.Left` / `.Right` / `.Middle`, and `A/B` pairs on axis actions (value = A − B).
- **Defaults** live in `project.json` → `input.actions`. They are edited in Project Settings → Input, which writes valid sets straight away (no undo, like the other Project Settings pages) and replaces `input.actions` wholesale so deleted actions do not survive a merge. Unreadable files are never overwritten (#51).
- **Player rebinding** is the difference from the defaults, stored as `{"version":1,"bindings":{action:[...]}}` under key `bindings` in the engine save slot `ivy_input` (ADR 0009). It is written only by `ScriptApi_InputSaveBindings`, so a game's options menu can offer Apply / Cancel. It is loaded on top of the defaults at start; actions that no longer exist and bindings that no longer parse are dropped with a Console line.
- Only **pad 0** is read. States update once per frame before scripts run. Raw input APIs stay as they are.
- Core logic is `engine/input/input_actions` (Linux-tested); `engine/input/input_action_system.cpp` samples devices and loads / saves. Scripts reach it through `ScriptRuntimeApi` v52.

**Considered Options**

- *Separate `rom/input_actions.json`*: easier to diff for long lists, but needs a packaging rule; `project.json` already carries project settings and is always packaged.
- *Editor page with undo*: the undo stack holds scene edits; mixing project settings into it would make scene undo confusing.
- *Auto-save every rebinding change*: no Cancel in an options menu.
- *Any connected pad / per-player pads*: deferred until a game needs local multiplayer.
