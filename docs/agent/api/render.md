# Render / visuals

Script-facing helpers from `api:ScriptRuntimeApi` / `script_runtime_api.h`. Scene render toggles: [`../scene-prefab.md`](../scene-prefab.md).

## Components

| Find / type | Notes |
|-------------|--------|
| `api:ScriptApi_FindVisualComponent` | Skinned/static mesh + clips (`api:VisualComponent`). Public fields also include the mesh LOD data `lods` (level `i+1` = `lods[i]`, `maxDistance` = switch-to distance, or `screenHeight` when `lodScreenSize` is true) and `cullDistance` (0 = never); see `scene-prefab.md`. LOD applies in Play / runtime only and is skipped while `playAnimation` is on |
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

## Runtime render settings (v49)

Change culling / LOD options of the running scene by name; nothing is written to the scene file, and a host older than v49 simply returns `false`. Numbers are clamped to the same ranges as the scene loader; unknown keys and non-finite numbers return `false` and change nothing.

- `bool ScriptApi_RenderSettingSetBool(const char* key, bool value)` / `bool ScriptApi_RenderSettingGetBool(const char* key, bool& outValue)` — `api:ScriptApi_RenderSettingSetBool` / `api:ScriptApi_RenderSettingGetBool`. Keys: `frustumCullingEnabled`, `shadowCascadeCullingEnabled`, `spriteCullingEnabled`, `lodEnabled`, `animCullingEnabled`, `lightSelectionEnabled`, `billboardCullingEnabled`, `effectCullingEnabled`, `lodDebugLabels`.
- `bool ScriptApi_RenderSettingSetNumber(const char* key, double value)` / `bool ScriptApi_RenderSettingGetNumber(const char* key, double& outValue)` — `api:ScriptApi_RenderSettingSetNumber` / `api:ScriptApi_RenderSettingGetNumber`. Keys: `lodBias` (0.1–10), `cullDistanceScale` (0.1–10), `shadowDistance` (0 = preset, otherwise 5–1000), `shadowLodOffset` (0–3), `effectCullRadius` (0.5–500), `lodForceLevel` (-1 = automatic, 0–7 forces that LOD level on every LOD mesh, limited to the levels the object has).
- Typical use: a quality menu that lowers `lodBias` / `cullDistanceScale`, or a debug key that toggles `lodDebugLabels`. The same keys work in the test bridge `set_setting` verb.

## Viewport / device

- `api:ScriptApi_GetRenderViewportWidth` / `api:ScriptApi_GetRenderViewportHeight`
- `void ScriptApi_GetGameViewportRect(float& x, float& y, float& width, float& height)` — `api:ScriptApi_GetGameViewportRect`
- `api:ScriptApi_GetBackBufferWidth` / `api:ScriptApi_GetBackBufferHeight`
- `api:ScriptApi_GetD3DDevice` / `api:ScriptApi_GetD3DDeviceContext`
- `void ScriptApi_GetCameraViewMatrix(DirectX::XMFLOAT4X4* out)` — `api:ScriptApi_GetCameraViewMatrix`
- `void ScriptApi_GetCameraProjMatrix(float width, float height, DirectX::XMFLOAT4X4* out)` — `api:ScriptApi_GetCameraProjMatrix`
