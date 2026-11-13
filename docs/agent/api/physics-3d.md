# Physics 3D

Active when scene `physicsMode` is `"3d"` and `renderPath` is not `"2d"` (`api:SceneExecutionPolicy`).

## Rigidbody

`api:Rigidbody` on `GameObject` (component JSON type `Rigidbody` → `api:RigidbodyComponent`):

- `api:Physics_GetGravity`
- `api:ForceMode` (`Force`, `Impulse`, …)
- Common methods: `SetIsKinematic`, `SetUseGravity`, `SetDrag`, `SetMass`, `GetVelocity`, `AddForce`, `AddTorque`

## Colliders

Find / mutate via script API:

- `api:ScriptApi_FindColliderComponent` → `api:ColliderComponent`
- `api:ScriptApi_ApplyColliderComponent`
- `api:ScriptApi_CreateBoxCollider` / `api:ScriptApi_DestroyCollider`
- `api:ScriptApi_GetColliderWorldCenter`, `api:ScriptApi_GetBoxColliderSize`, `api:ScriptApi_SetBoxColliderSize`
- `api:ScriptApi_GetSphereColliderRadius`, `api:ScriptApi_TryGetColliderFootprint`
- Ignore rules: `api:ScriptApi_GameObjectAddIgnoreCollisionTag`, `api:ScriptApi_GameObjectRemoveIgnoreCollisionTag`, `api:ScriptApi_GameObjectAddIgnoreCollisionObject`, `api:ScriptApi_GameObjectRemoveIgnoreCollisionObject`, `api:ScriptApi_GameObjectClearIgnoreCollisionObjects`

Registered collider component type names: `BoxColliderComponent`, `SphereColliderComponent`, `CapsuleColliderComponent`, `CylinderColliderComponent`, `MeshColliderComponent`, plus aggregate `Collider`.

## Raycast / debug

- `api:ScriptApi_Physics_Raycast` — origin, direction, max distance → optional hit point/normal/distance
- `api:ScriptApi_CollisionVizEnabled` — host F11 collision viz
- Trigger callbacks on scripts: `api:OnTriggerEnter` / `api:OnTriggerExit`
