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

- `api:ScriptApi_Visual_PlayClip`
- `api:ScriptApi_Visual_PlayClipLoop`
- `api:ScriptApi_Visual_GetClipDuration`
- `api:ScriptApi_Visual_SetAnimSpeed`
- `api:ScriptApi_Visual_TryGetBoneWorld`

## World sprites (2D)

- `api:ScriptApi_SpritePlay` / `api:ScriptApi_SpritePause` / `api:ScriptApi_SpriteStop`
- `api:ScriptApi_SpriteSetFlip` / `api:ScriptApi_SpriteSetTint`

## Immediate / FX

- `api:ScriptApi_SpriteDraw` — screen-space textured quad
- `api:ScriptApi_BillboardAdd` / `api:ScriptApi_BillboardAddUv`
- `api:ScriptApi_Effect_Play`
- `api:ScriptApi_PostFX_Burst`
- `api:ScriptApi_LineSprite_Draw`
- `api:ScriptApi_DebugDrawSphereWire` / `api:ScriptApi_DebugDrawLine`
- Particles: `api:ScriptApi_ParticleCreateEmitter`, `api:ScriptApi_ParticleSetEmitterPosition`, `api:ScriptApi_ParticleSetEmitterEmitting`, `api:ScriptApi_ParticleStopEmitter`, `api:ScriptApi_ParticleDestroyEmitter`, `api:ScriptApi_ParticleSpawnAt`

## Viewport / device

- `api:ScriptApi_GetRenderViewportWidth` / `api:ScriptApi_GetRenderViewportHeight`
- `api:ScriptApi_GetGameViewportRect`
- `api:ScriptApi_GetBackBufferWidth` / `api:ScriptApi_GetBackBufferHeight`
- `api:ScriptApi_GetD3DDevice` / `api:ScriptApi_GetD3DDeviceContext`
- `api:ScriptApi_GetCameraViewMatrix` / `api:ScriptApi_GetCameraProjMatrix`
