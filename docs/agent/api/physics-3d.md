# Physics 3D

Active when scene `physicsMode` is `"3d"` and `renderPath` is not `"2d"` (`api:SceneExecutionPolicy`).

## Rigidbody

`api:Rigidbody` on `GameObject` (JSON type `Rigidbody` → C++ `api:RigidbodyComponent`):

- `api:Physics_GetGravity`
- `api:ForceMode` (`Force`, `Impulse`, …)
- Common methods: `SetIsKinematic`, `SetUseGravity`, `SetDrag`, `SetMass`, `GetVelocity`, `AddForce`, `AddTorque`

## Colliders

**JSON type names** (scene/prefab) differ from some C++ class names. Full table: [`../scene-prefab.md`](../scene-prefab.md).

| JSON name | C++ type |
|-----------|----------|
| `BoxCollider` | `api:BoxColliderComponent` |
| `SphereCollider` | `api:SphereColliderComponent` |
| `CapsuleCollider` | `api:CapsuleColliderComponent` |
| `CylinderCollider` | `api:CylinderColliderComponent` |
| `MeshCollider` | `api:MeshColliderComponent` |
| `Collider` | `api:ColliderComponent` (generic aggregate) |

Find / mutate via script API:

- `ColliderComponent* ScriptApi_FindColliderComponent(engine::GameObject& object)` — `api:ScriptApi_FindColliderComponent`
- `void ScriptApi_ApplyColliderComponent(ColliderComponent& component)` — `api:ScriptApi_ApplyColliderComponent`
- `BoxCollider* ScriptApi_CreateBoxCollider(engine::GameObject& object, DirectX::XMFLOAT3 size, DirectX::XMFLOAT3 center)` — `api:ScriptApi_CreateBoxCollider`
- `void ScriptApi_DestroyCollider(engine::GameObject& object)` — `api:ScriptApi_DestroyCollider`
- `DirectX::XMFLOAT3 ScriptApi_GetColliderWorldCenter(const Collider& collider)` — `api:ScriptApi_GetColliderWorldCenter`
- `DirectX::XMFLOAT3 ScriptApi_GetBoxColliderSize(const BoxCollider& collider)` / `void ScriptApi_SetBoxColliderSize(BoxCollider& collider, DirectX::XMFLOAT3 size)` — `api:ScriptApi_GetBoxColliderSize` / `api:ScriptApi_SetBoxColliderSize`
- `float ScriptApi_GetSphereColliderRadius(const SphereCollider& collider)` — `api:ScriptApi_GetSphereColliderRadius`
- `bool ScriptApi_TryGetColliderFootprint(const Collider& collider, DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize)` — `api:ScriptApi_TryGetColliderFootprint`
- Ignore rules: `api:ScriptApi_GameObjectAddIgnoreCollisionTag`, `api:ScriptApi_GameObjectRemoveIgnoreCollisionTag`, `api:ScriptApi_GameObjectAddIgnoreCollisionObject`, `api:ScriptApi_GameObjectRemoveIgnoreCollisionObject`, `api:ScriptApi_GameObjectClearIgnoreCollisionObjects`

## Raycast / debug

- `bool ScriptApi_Physics_Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, DirectX::XMFLOAT3* outPoint, DirectX::XMFLOAT3* outNormal, float* outDistance)` — `api:ScriptApi_Physics_Raycast`
- `bool ScriptApi_CollisionVizEnabled()` — `api:ScriptApi_CollisionVizEnabled` (host F11)
- Trigger callbacks on scripts: `api:OnTriggerEnter` / `api:OnTriggerExit`
