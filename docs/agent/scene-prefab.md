# Scene & prefab JSON

## Scene file (version 4)

Saved by `api:SceneLoader_SaveToFile` as:

```json
{
  "version": 4,
  "settings": { },
  "nodes": [ ],
  "canvas": { }
}
```

| Key | Role |
|-----|------|
| `version` | Scene format version; saver writes `4` |
| `settings` | Execution + render + camera (below) |
| `nodes` | Root `GameObject` trees (each node via `api:PrefabSerializer`) |
| `canvas` | Scene UI canvas blob (`SceneCanvas_*`) |

### `settings`

From `api:SceneExecutionPolicy` + `api:RenderSettings_ToJson` / parse:

| Field | Values / notes |
|-------|----------------|
| `renderPath` | `"3d"` (default) or `"2d"` — 2D path disables 3D physics policy |
| `physicsMode` | With 3D path: `"3d"` (default) or `"none"`. With 2D path: `"2d"` or `"none"` (default `"none"`) |
| `render` | Nested render settings object |
| `render.frustumCullingEnabled` | bool — main-pass 3D mesh cull |
| `render.shadowCascadeCullingEnabled` | bool — shadow cascade caster cull |
| `render.spriteCullingEnabled` | bool — 2D ortho sprite view-rect cull |
| `camera.mode` | JSON strings from `PrepareSceneSettings` in `engine/scene/scene_loader.cpp`: `"thirdPerson"`, `"firstPerson"`, or default `"scene"` (→ `api:CameraMode` `SceneCamera`). C++ also has `CameraMode::Intro`; it is **not** a JSON mode string |
| `camera.node` | Preferred scene camera node name |
| `debugCamera` | Same `.cpp` parser: `mode` (`"orbit"` default / `"fly"`), `enabledOnPlay`, `yawDeg`, `pitchDeg`, optional `position` object `{x,y,z}` |

Other `render.*` keys (skybox, fog, shadows, clear color, …) match `api:RenderSettingsData` / `api:RenderSettings_ParseJson`.

## Registered components

Exact type names from `api:SceneLoader_RegisterComponentTypes`:

JSON type names come from each component's `GetTypeName()` (via `REGISTER_COMPONENT` / explicit `Register`):

| JSON type name | C++ type |
|----------------|----------|
| `SceneTransform` | `api:Transform` |
| `BoxCollider` | `api:BoxColliderComponent` |
| `SphereCollider` | `api:SphereColliderComponent` |
| `CapsuleCollider` | `api:CapsuleColliderComponent` |
| `CylinderCollider` | `api:CylinderColliderComponent` |
| `MeshCollider` | `api:MeshColliderComponent` |
| `Rigidbody2D` | `api:Rigidbody2DComponent` |
| `Collider2D` | `api:Collider2DComponent` |
| `SpriteAnimator` | `api:SpriteAnimatorComponent` |
| `SpriteRenderer` | `api:SpriteRendererComponent` |
| `visual` | `api:VisualComponent` |
| `Billboard` | `api:BillboardComponent` |
| `Material` | `api:MaterialComponent` |
| `Rigidbody` | `api:RigidbodyComponent` |
| `Script` | `api:ScriptComponent` (`scriptClass` + optional `properties`) |
| `Collider` | `api:ColliderComponent` (generic; shape variants above preferred) |
| `Effect` | `api:EffectComponent` |
| `DirectionalLight` | directional light component |
| `PointLight` | point light component |
| `SpotLight` | spot light component |
| `Camera` | `api:CameraComponent` |
| `Audio` | `api:AudioComponent` |
| `AudioListener` | `api:AudioListenerComponent` |

## Prefab formats (`api:PrefabSerializer`)

Node JSON (recursive), from `prefab_serializer.h`:

```json
{
  "name": "EnemyManager",
  "prefabAssetPath": "assets/Prefabs/EnemyManager.prefab.json",
  "components": {
    "SceneTransform": { },
    "visual": { "model": "…", "proxy_model": "…" }
  },
  "children": [ ]
}
```

Files written by `api:PrefabSerializer` `SaveToFile` use the scene-style wrapper:

```json
{ "version": 4, "nodes": [ { /* node JSON */ } ] }
```

`Load` / `Deserialize` still accept older bare-node prefab files.

Runtime spawn/destroy from scripts: `api:ScriptApi_PrefabInstantiate`, `api:ScriptApi_PrefabDestroy`.

## Bridge scenarios

Automated bridge plans: [`../schemas/runtime_bridge_scenario.v1.schema.json`](../schemas/runtime_bridge_scenario.v1.schema.json). Ops guide: engine `docs/RUNTIME_TEST_BRIDGE.md` (condensed: [`testing-bridge.md`](testing-bridge.md)).
