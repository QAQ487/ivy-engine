# Render / visuals

Script-facing helpers from `api:ScriptRuntimeApi` / `script_runtime_api.h`. Scene render toggles: [`../scene-prefab.md`](../scene-prefab.md).

## Components

| Find / type | Notes |
|-------------|--------|
| `api:ScriptApi_FindVisualComponent` | Skinned/static mesh + clips (`api:VisualComponent`) |
| `api:ScriptApi_FindMaterialComponent` | `api:MaterialComponent` |
| `api:ScriptApi_FindEffectComponent` | Effekseer-style `api:EffectComponent` |
| Scene types `visual`, `Billboard`, `SpriteRenderer`, `SpriteAnimator`, lights | Registered in `api:SceneLoader_RegisterComponentTypes` |

## Visual clips

- `void ScriptApi_Visual_PlayClip(VisualComponent* component, const char* clipName)` — `api:ScriptApi_Visual_PlayClip`
- `void ScriptApi_Visual_PlayClipLoop(VisualComponent* component, const char* clipName, bool loop)` — `api:ScriptApi_Visual_PlayClipLoop`
- `float ScriptApi_Visual_GetClipDuration(VisualComponent* component)` — `api:ScriptApi_Visual_GetClipDuration`
- `void ScriptApi_Visual_SetAnimSpeed(VisualComponent* component, float speed)` — `api:ScriptApi_Visual_SetAnimSpeed`
- `bool ScriptApi_Visual_TryGetBoneWorld(VisualComponent* component, const char* boneName, DirectX::XMFLOAT3& outPos, DirectX::XMFLOAT3& outRotDeg)` — `api:ScriptApi_Visual_TryGetBoneWorld`

## World sprites (2D)

- `bool ScriptApi_SpritePlay(GameObject* object, const char* clipName, bool restart = false)` — `api:ScriptApi_SpritePlay`
- `void ScriptApi_SpritePause(GameObject* object)` / `void ScriptApi_SpriteStop(GameObject* object)` — `api:ScriptApi_SpritePause` / `api:ScriptApi_SpriteStop`
- `void ScriptApi_SpriteSetFlip(GameObject* object, bool flipX, bool flipY)` — `api:ScriptApi_SpriteSetFlip`
- `void ScriptApi_SpriteSetTint(GameObject* object, DirectX::XMFLOAT4 tint)` — `api:ScriptApi_SpriteSetTint`

## Immediate / FX

- `api:ScriptApi_SpriteDraw` — screen-space textured quad (`texId`, x/y/width/height, source rect, angle, color)
- `api:ScriptApi_BillboardAdd` / `api:ScriptApi_BillboardAddUv`
- `void ScriptApi_Effect_Play(EffectComponent* component)` — `api:ScriptApi_Effect_Play`
- `void ScriptApi_PostFX_Burst(float rgbShift, float durationSeconds)` — `api:ScriptApi_PostFX_Burst`
- `void ScriptApi_LineSprite_Draw(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float thicknessWorld, const DirectX::XMFLOAT4& color)` — `api:ScriptApi_LineSprite_Draw`
- `api:ScriptApi_DebugDrawSphereWire` / `api:ScriptApi_DebugDrawLine`
- Particles: `api:ScriptApi_ParticleCreateEmitter`, `api:ScriptApi_ParticleSetEmitterPosition`, `api:ScriptApi_ParticleSetEmitterEmitting`, `api:ScriptApi_ParticleStopEmitter`, `api:ScriptApi_ParticleDestroyEmitter`, `api:ScriptApi_ParticleSpawnAt`

## Viewport / device

- `api:ScriptApi_GetRenderViewportWidth` / `api:ScriptApi_GetRenderViewportHeight`
- `void ScriptApi_GetGameViewportRect(float& x, float& y, float& width, float& height)` — `api:ScriptApi_GetGameViewportRect`
- `api:ScriptApi_GetBackBufferWidth` / `api:ScriptApi_GetBackBufferHeight`
- `api:ScriptApi_GetD3DDevice` / `api:ScriptApi_GetD3DDeviceContext`
- `void ScriptApi_GetCameraViewMatrix(DirectX::XMFLOAT4X4* out)` — `api:ScriptApi_GetCameraViewMatrix`
- `void ScriptApi_GetCameraProjMatrix(float width, float height, DirectX::XMFLOAT4X4* out)` — `api:ScriptApi_GetCameraProjMatrix`
