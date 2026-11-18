# Scripting

Gameplay scripts are C++ classes that inherit `api:Script` (`engine/scripting/Script.h`) and register by class name for prefab/scene JSON (`"Script": { "scriptClass": "…" }`).

Prefer `ScriptApi_*` wrappers from `engine/scripting/script_runtime_api.h` for host access. The host ABI table is `api:ScriptRuntimeApi`; its `version` field is **49** (constant `kScriptRuntimeApiVersion` in `script_runtime_api.cpp`, not declared in a header — do not use an `api:` marker for it).

## Lifecycle (`api:Script`)

| Method | When |
|--------|------|
| `api:OnCreate` | Once after deserialize, before play updates |
| `api:OnUpdate` | Every frame while the owner is active (`dt` seconds) |
| `api:Update_IsParallelSafe` | Return `true` only after a parallel-safety audit (default `false`) |
| `api:OnDestroy` | Once before teardown |
| `api:OnTriggerEnter` / `api:OnTriggerExit` | Trigger volumes (`IsTrigger` colliders) |
| `api:Serialize` / `api:Deserialize` | Persist/restore fields (`Deserialize` runs before `OnCreate`) |

Optional: `api:GetRuntimePhysicsProxy` for editor play-mode collider overlays.

Serialized fields: declare with macros in `script_fields.h` (`SCRIPT_DECLARE_FIELD`, `SCRIPT_SERIALIZE_METHODS`).

## Registration

Place `api:REGISTER_SCRIPT` once in the script `.cpp` (e.g. `REGISTER_SCRIPT(MyScript)`).

| Header | Role |
|--------|------|
| `engine/scripting/script_dll_register.h` | **Preferred for game scripts** (samples use this). Under `SCRIPT_DLL_BUILD` / `_WINDLL` registers into the DLL export table; otherwise uses `api:ScriptFactory` |
| `engine/scripting/script_register.h` | Static-only registration with `api:ScriptFactory` |

UI element scripts use `api:UICanvasScript` + `api:REGISTER_UI_SCRIPT` (`ui_canvas_script_register.h`) — see [`api/ui.md`](api/ui.md).

## Minimal example

Patterned on `samples/scripts/SampleSpinScript`:

```cpp
// MySpinScript.h
#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

class MySpinScript : public Script {
public:
#define MY_SPIN_FIELDS(X) \
    X(float, degreesPerSecond, 45.0f)
    MY_SPIN_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(MY_SPIN_FIELDS)
    void OnUpdate(engine::GameObject& self, float dt) override;
    bool Update_IsParallelSafe() const override { return true; }
};
```

```cpp
// MySpinScript.cpp
#include "MySpinScript.h"
#include "engine/ecs/transform.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

REGISTER_SCRIPT(MySpinScript)

void MySpinScript::OnUpdate(engine::GameObject& self, float dt) {
    DirectX::XMFLOAT3 rot = self.GetRotation();
    rot.y += degreesPerSecond * dt;
    self.SetRotation(rot);
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localRotation = rot;
}
```

## Common `ScriptApi_*` (see also `api/*.md`)

| Area | APIs |
|------|------|
| Transform | `api:ScriptApi_FindTransform` |
| Prefab | `GameObject* ScriptApi_PrefabInstantiate(...)` / `ScriptApi_PrefabDestroy` — see [`api/assets.md`](api/assets.md) (`api:ScriptApi_PrefabInstantiate`, `api:ScriptApi_PrefabDestroy`) |
| Visual | `api:ScriptApi_FindVisualComponent`, `ScriptApi_Visual_PlayClip(VisualComponent*, const char*)` — [`api/render.md`](api/render.md) |
| Render settings (v49) | `api:ScriptApi_RenderSettingSetBool`, `api:ScriptApi_RenderSettingSetNumber`, `api:ScriptApi_RenderSettingGetBool`, `api:ScriptApi_RenderSettingGetNumber` — runtime culling / LOD options by name, [`api/render.md`](api/render.md) |
| Audio | `api:ScriptApi_FindAudioComponent`, `api:ScriptApi_Audio_Play`, `api:ScriptApi_Audio_Stop` |
| Physics 3D | `ScriptApi_Physics_Raycast(...)` — [`api/physics-3d.md`](api/physics-3d.md) |
| Physics 2D | full signatures in [`api/physics-2d.md`](api/physics-2d.md) |
| Sprite | `ScriptApi_SpritePlay(GameObject*, const char* clipName, bool restart = false)` — [`api/render.md`](api/render.md) |
| Input | `api:Keyboard_GetState`, `api:KeyLogger_IsTrigger`, `api:Controller_GetLeftStick`, `api:Controller_IsPressed`, `void ScriptApi_MouseGetPosition(int& x, int& y)` |

Camera follow: `void ThirdPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr)` / same shape for `FirstPersonCamera_SetTarget` — [`api/camera.md`](api/camera.md) (`api:ThirdPersonCamera_SetTarget`, `api:FirstPersonCamera_SetTarget`).
