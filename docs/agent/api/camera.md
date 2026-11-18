# Camera

## `api:CameraMode` (`engine/core/engine_gui.h`)

C++ enum: `ThirdPerson = 0`, `SceneCamera = 1`, `Intro = 2`, `FirstPerson = 3`.

Scene JSON `settings.camera.mode` strings are parsed in `engine/scene/scene_loader.cpp` (`PrepareSceneSettings`): only `"thirdPerson"`, `"firstPerson"`, and default `"scene"` (→ `SceneCamera`). **`Intro` is not a JSON mode string** — scripts/UI set it via `EngineGUI_SetCameraMode` / host `engineGuiSetCameraMode`. Save maps non–third/first modes back to `"scene"`.

`settings.debugCamera` (same function): `mode` (`"orbit"` default vs `"fly"`), `enabledOnPlay`, `yawDeg`, `pitchDeg`, optional `position.{x,y,z}`. See [`../scene-prefab.md`](../scene-prefab.md).

Component `Camera` → `api:CameraComponent`.

## Scene / free camera (script wrappers)

- `void ScriptApi_CameraSetPosition(DirectX::XMFLOAT3 position)` — `api:ScriptApi_CameraSetPosition` (also feeds Intro pose via director)
- `void ScriptApi_CameraSetTarget(DirectX::XMFLOAT3 target)` — `api:ScriptApi_CameraSetTarget`
- `void ScriptApi_CameraSetUp(DirectX::XMFLOAT3 up)` — `api:ScriptApi_CameraSetUp`
- `void ScriptApi_SetActiveCameraNodeName(const char* name)` — `api:ScriptApi_SetActiveCameraNodeName`
- `void ScriptApi_ActivateSceneCamera(const char* name)` — `api:ScriptApi_ActivateSceneCamera`
- `DirectX::XMFLOAT3 ScriptApi_GetCameraForward()` — `api:ScriptApi_GetCameraForward`
- `void ScriptApi_GetCameraViewMatrix(DirectX::XMFLOAT4X4* out)` — `api:ScriptApi_GetCameraViewMatrix`
- `void ScriptApi_GetCameraProjMatrix(float width, float height, DirectX::XMFLOAT4X4* out)` — `api:ScriptApi_GetCameraProjMatrix`
- `void ScriptApi_Camera_AddTrauma(float amount01)` — `api:ScriptApi_Camera_AddTrauma`
- `void ScriptApi_Camera_SetSpeedFactor(float speed01)` — `api:ScriptApi_Camera_SetSpeedFactor`

## Third-person (`src/app/camera/third_person_camera.h`)

Also wired into `api:ScriptRuntimeApi` (`thirdPersonCamera*` pointers). Prefer these declarations for scripts:

- `void ThirdPersonCamera_Initialize()` — `api:ThirdPersonCamera_Initialize`
- `void ThirdPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr)` — `api:ThirdPersonCamera_SetTarget` (live pointers; `nullptr`/`nullptr` detaches; yaw in degrees)
- `void ThirdPersonCamera_SetDistance(float distance)` — `api:ThirdPersonCamera_SetDistance`
- `void ThirdPersonCamera_SetHeight(float height)` — `api:ThirdPersonCamera_SetHeight`
- `void ThirdPersonCamera_SetLookAtHeight(float height)` — `api:ThirdPersonCamera_SetLookAtHeight`
- `void ThirdPersonCamera_SetMouseSensitivity(float sensitivity)` / `void ThirdPersonCamera_SetControllerSensitivity(float sensitivity)` — `api:ThirdPersonCamera_SetMouseSensitivity` / `api:ThirdPersonCamera_SetControllerSensitivity`
- `void ThirdPersonCamera_SetPitchLimits(float minPitch, float maxPitch)` — `api:ThirdPersonCamera_SetPitchLimits`
- `void ThirdPersonCamera_ShowCursor(bool show)` / `bool ThirdPersonCamera_IsCursorVisible()` — `api:ThirdPersonCamera_ShowCursor` / `api:ThirdPersonCamera_IsCursorVisible`
- `void ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode mode)` / `void ThirdPersonCamera_SetMagnetTarget(const DirectX::XMFLOAT3& targetPos)` — `api:ThirdPersonCamera_SetMagnetMode` / `api:ThirdPersonCamera_SetMagnetTarget` (`api:ThirdPersonMagnetMode`)
- `void ThirdPersonCamera_AddTrauma(float amount01)` / `void ThirdPersonCamera_SetSpeedFactor(float speed01)` — `api:ThirdPersonCamera_AddTrauma` / `api:ThirdPersonCamera_SetSpeedFactor`

The camera **writes** the yaw pointer (mouse / right stick orbit) and reads the position pointer. Give it a camera-orbit yaw of its own, not the character's heading: if a script also overwrites that float with its movement direction, the camera swings with every strafe or backward step. Move relative to that yaw and turn the character separately.

Sample: `SamplePlayerScript` keeps `m_cameraYaw` (passed to `ThirdPersonCamera_SetTarget`) and `m_facingYaw` (character, turns toward the move direction at `turnSpeed` deg/s), calls these after `OnCreate`, and sets `CameraMode::ThirdPerson` via `api:ScriptRuntimeApi_GetHost`. WASD / left stick move relative to the camera; mouse / right stick orbit it; Space jumps.

## First-person (`src/app/camera/first_person_camera.h`)

- `void FirstPersonCamera_Initialize()` — `api:FirstPersonCamera_Initialize`
- `void FirstPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr)` — `api:FirstPersonCamera_SetTarget` (live pointers; yaw degrees; `nullptr`/`nullptr` detaches)
- `void FirstPersonCamera_SetEyeHeight(float eyeHeight)` — `api:FirstPersonCamera_SetEyeHeight`
- `void FirstPersonCamera_SetMouseSensitivity(float sensitivity)` / `void FirstPersonCamera_SetControllerSensitivity(float sensitivity)` — `api:FirstPersonCamera_SetMouseSensitivity` / `api:FirstPersonCamera_SetControllerSensitivity`
- `void FirstPersonCamera_SetPitchLimits(float minPitch, float maxPitch)` — `api:FirstPersonCamera_SetPitchLimits`
- `void FirstPersonCamera_ShowCursor(bool show)` / `bool FirstPersonCamera_IsCursorVisible()` — `api:FirstPersonCamera_ShowCursor` / `api:FirstPersonCamera_IsCursorVisible`
