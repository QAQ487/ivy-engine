# Physics 2D

Active when `renderPath` is `"2d"` and `physicsMode` is `"2d"` (`api:SceneExecutionPolicy`).

## Components

Registered names:

- `Rigidbody2D` → `api:Rigidbody2DComponent`
- `Collider2D` → `api:Collider2DComponent`
- `SpriteRenderer` → `api:SpriteRendererComponent`
- `SpriteAnimator` → `api:SpriteAnimatorComponent`

Events: `api:Physics2DEvent` (`engine/physics/2d/physics2d.h`). Sample scripts may override a physics-2D event hook on the script class (see `SamplePhysics2DPlayerScript`).

## Script API

- `bool ScriptApi_Physics2DSetVelocity(GameObject* object, DirectX::XMFLOAT2 velocity)` — `api:ScriptApi_Physics2DSetVelocity`
- `bool ScriptApi_Physics2DGetVelocity(GameObject* object, DirectX::XMFLOAT2* velocity)` — `api:ScriptApi_Physics2DGetVelocity`
- `bool ScriptApi_Physics2DMovePosition(GameObject* object, DirectX::XMFLOAT2 position)` — `api:ScriptApi_Physics2DMovePosition`
- `bool ScriptApi_Physics2DRaycast(DirectX::XMFLOAT2 origin, DirectX::XMFLOAT2 direction, float distance, GameObject** outObject, DirectX::XMFLOAT2* outPoint, DirectX::XMFLOAT2* outNormal, float* outDistance, std::uint32_t mask = 0xffffffffu, bool includeTriggers = false)` — `api:ScriptApi_Physics2DRaycast`

## Sprites (usually paired)

- `bool ScriptApi_SpritePlay(GameObject* object, const char* clipName, bool restart = false)` — `api:ScriptApi_SpritePlay`
- `void ScriptApi_SpritePause(GameObject* object)` / `void ScriptApi_SpriteStop(GameObject* object)` — `api:ScriptApi_SpritePause` / `api:ScriptApi_SpriteStop`
- `void ScriptApi_SpriteSetFlip(GameObject* object, bool flipX, bool flipY)` — `api:ScriptApi_SpriteSetFlip`
- `void ScriptApi_SpriteSetTint(GameObject* object, DirectX::XMFLOAT4 tint)` — `api:ScriptApi_SpriteSetTint`
