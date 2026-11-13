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

- `api:ScriptApi_Physics2DSetVelocity`
- `api:ScriptApi_Physics2DGetVelocity`
- `api:ScriptApi_Physics2DMovePosition`
- `api:ScriptApi_Physics2DRaycast` — origin, direction, distance; optional hit object/point/normal/distance; `mask` (default all bits); `includeTriggers`

## Sprites (usually paired)

- `api:ScriptApi_SpritePlay` / `api:ScriptApi_SpritePause` / `api:ScriptApi_SpriteStop`
- `api:ScriptApi_SpriteSetFlip` / `api:ScriptApi_SpriteSetTint`
