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
| `render.lodEnabled` | bool (default `true`) — distance-based mesh LOD and `visual.cullDistance` (Play / runtime only) |
| `render.animCullingEnabled` | bool (default `true`) — animated Visuals that no pass (main camera or shadow cascade) drew last frame skip bone sampling / skinning; clip time still advances (Play / runtime only) |
| `render.lightSelectionEnabled` | bool (default `true`) — only 4 lights reach the GPU. With more enabled lights, directional lights first, then point / spot lights by influence at the camera (brightness × range² / (range² + distance²), with hysteresis); `false` uses the first 4 in scene order. Scenes with ≤ 4 lights are unaffected |
| `render.billboardCullingEnabled` | bool (default `true`) — frustum-cull camera-facing / ground-aligned billboards and individual particles at draw time (simulation unaffected) |
| `render.effectCullingEnabled` | bool (default `true`) — skip drawing Effekseer effects whose bounding sphere is outside the camera frustum; they keep simulating |
| `render.effectCullRadius` | number (default `20`, clamped 0.5–500) — assumed bounding radius in world units of every Effekseer effect (effects carry no bounds); multiplied by the effect's largest scale component. Raise it if large effects pop at the screen edge |
| `render.lodBias` | number (default `1`, clamped 0.1–10) — scales distance for LOD choice; >1 switches to lower detail sooner |
| `render.cullDistanceScale` | number (default `1`, clamped 0.1–10) — multiplies every `visual.cullDistance` (`0` / absent stays "never"); >1 keeps objects visible further out, <1 culls sooner. Needs `lodEnabled` |
| `render.shadowDistance` | number (default `0`, clamped 5–1000 when positive) — shadow range in world units; `0` uses the Shadow Quality preset (Low 40 / Medium 70 / High 90). Map size and cascade count still come from the preset |
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

`visual` LOD fields (optional; absent = no LOD):

```json
"visual": {
  "model": "rom/model/x.fbx",
  "lods": [
    { "model": "rom/model/x_lod1.fbx", "maxDistance": 30 },
    { "model": "rom/model/x_lod2.fbx", "maxDistance": 70 }
  ],
  "cullDistance": 120
}
```

- Level 0 is `model`; `lods[i]` is level `i+1`. Each `maxDistance` is the camera distance at which rendering switches **to** that level (loaded ascending; at most 7 LOD levels). Distance is camera → world-AABB centre of the base mesh, with 5% hysteresis.
- Shadow passes use the same level as the main camera pass.
- Ignored while `animate` is `true` (levels > 0 are skipped); `cullDistance` still applies. LOD only applies in Play / runtime, never to `proxy_model` edit view. A missing LOD mesh falls back to level 0.
- LOD meshes should share the base mesh pivot and scale, or the switch will visibly jump.

Files written by `api:PrefabSerializer` `SaveToFile` use the scene-style wrapper:

```json
{ "version": 4, "nodes": [ { /* node JSON */ } ] }
```

`Load` / `Deserialize` still accept older bare-node prefab files.

Runtime spawn/destroy from scripts: `api:ScriptApi_PrefabInstantiate`, `api:ScriptApi_PrefabDestroy`.

## Bridge scenarios

Automated bridge plans: [`../schemas/runtime_bridge_scenario.v1.schema.json`](../schemas/runtime_bridge_scenario.v1.schema.json). Ops guide: engine `docs/RUNTIME_TEST_BRIDGE.md` (condensed: [`testing-bridge.md`](testing-bridge.md)).
