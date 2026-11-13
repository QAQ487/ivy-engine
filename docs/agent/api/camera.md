# Camera

## Scene / free camera

Script wrappers:

- `api:ScriptApi_CameraSetPosition` / `api:ScriptApi_CameraSetTarget` / `api:ScriptApi_CameraSetUp`
- `api:ScriptApi_SetActiveCameraNodeName` / `api:ScriptApi_ActivateSceneCamera`
- `api:ScriptApi_GetCameraForward`
- `api:ScriptApi_GetCameraViewMatrix` / `api:ScriptApi_GetCameraProjMatrix`
- Trauma / speed (third-person shake helpers): `api:ScriptApi_Camera_AddTrauma`, `api:ScriptApi_Camera_SetSpeedFactor`

Scene settings `camera.mode` / `camera.node`: see [`../scene-prefab.md`](../scene-prefab.md). Types: `api:CameraMode`, component `Camera` → `api:CameraComponent`.

## Third-person (`src/app/camera/third_person_camera.h`)

Direct host APIs (also wired into `api:ScriptRuntimeApi`):

- `api:ThirdPersonCamera_Initialize`
- `api:ThirdPersonCamera_SetTarget`
- `api:ThirdPersonCamera_SetDistance` / `api:ThirdPersonCamera_SetHeight` / `api:ThirdPersonCamera_SetLookAtHeight`
- `api:ThirdPersonCamera_SetMouseSensitivity` / `api:ThirdPersonCamera_SetControllerSensitivity`
- `api:ThirdPersonCamera_SetPitchLimits`
- `api:ThirdPersonCamera_ShowCursor` / `api:ThirdPersonCamera_IsCursorVisible`
- `api:ThirdPersonCamera_SetMagnetMode` / `api:ThirdPersonCamera_SetMagnetTarget` (`api:ThirdPersonMagnetMode`)
- `api:ThirdPersonCamera_AddTrauma` / `api:ThirdPersonCamera_SetSpeedFactor`

Sample: `SamplePlayerScript` calls these after `OnCreate` and sets `CameraMode::ThirdPerson` via `api:ScriptRuntimeApi_GetHost`.

## First-person (`src/app/camera/first_person_camera.h`)

- `api:FirstPersonCamera_Initialize`
- `api:FirstPersonCamera_SetTarget`
- `api:FirstPersonCamera_SetEyeHeight`
- `api:FirstPersonCamera_SetMouseSensitivity` / `api:FirstPersonCamera_SetControllerSensitivity`
- `api:FirstPersonCamera_SetPitchLimits`
- `api:FirstPersonCamera_ShowCursor` / `api:FirstPersonCamera_IsCursorVisible`
