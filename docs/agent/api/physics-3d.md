# Physics 3D

Active when scene `physicsMode` is `"3d"` and `renderPath` is not `"2d"` (`api:SceneExecutionPolicy`).

## Rigidbody

`api:Rigidbody` on `GameObject` (JSON type `Rigidbody` → C++ `api:RigidbodyComponent`):

- `api:Physics_GetGravity`
- `api:ForceMode` (`Force`, `Impulse`, …)
- Common methods: `SetIsKinematic`, `SetUseGravity`, `SetDrag`, `SetMass`, `GetVelocity`, `AddForce`, `AddTorque`

Through the host (v50; preferred from DLL scripts, never adds a Rigidbody component, returns `false` without one or for a non-finite vector):

- `bool ScriptApi_RigidbodyGetVelocity(GameObject& object, DirectX::XMFLOAT3& outVelocity)` — `api:ScriptApi_RigidbodyGetVelocity`
- `bool ScriptApi_RigidbodySetVelocity(GameObject& object, DirectX::XMFLOAT3 velocity)` — `api:ScriptApi_RigidbodySetVelocity`
- `bool ScriptApi_RigidbodyGetAngularVelocity(GameObject& object, DirectX::XMFLOAT3& outVelocity)` / `bool ScriptApi_RigidbodySetAngularVelocity(GameObject& object, DirectX::XMFLOAT3 velocity)` — `api:ScriptApi_RigidbodyGetAngularVelocity` / `api:ScriptApi_RigidbodySetAngularVelocity`
- `bool ScriptApi_RigidbodyAddForce(GameObject& object, DirectX::XMFLOAT3 force, ForceMode mode = ForceMode::Force)` — `api:ScriptApi_RigidbodyAddForce`
- `bool ScriptApi_RigidbodyAddTorque(GameObject& object, DirectX::XMFLOAT3 torque, ForceMode mode = ForceMode::Force)` — `api:ScriptApi_RigidbodyAddTorque`
- Touch another object's body only from scripts that are not `Update_IsParallelSafe`.

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

## Terrain (v53)

A `Terrain` component (JSON in [`../scene-prefab.md`](../scene-prefab.md)) with `collision: true` gives its object a static heightfield collider (`api:TerrainCollider`, `ColliderType::TERRAIN`) unless that object has its own collider component. Bodies with Sphere / Capsule / Box / Cylinder / Mesh colliders land on it, and `api:ScriptApi_Physics_Raycast` hits it like any collider. The terrain object's rigidbody is forced kinematic.

- `bool ScriptApi_TerrainSampleHeight(float x, float z, float* outHeight, DirectX::XMFLOAT3* outNormal = nullptr)` — `api:ScriptApi_TerrainSampleHeight`: surface height (and up-facing normal) of the highest loaded, enabled terrain under world (x, z); `false` off the terrain or on a host older than v53. Cheap (one grid cell): fine every frame for placing objects or snapping to the ground.

## Raycast / debug

- `bool ScriptApi_Physics_Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance, DirectX::XMFLOAT3* outPoint, DirectX::XMFLOAT3* outNormal, float* outDistance)` — `api:ScriptApi_Physics_Raycast`
- `bool ScriptApi_CollisionVizEnabled()` — `api:ScriptApi_CollisionVizEnabled` (host F11)
- Trigger callbacks on scripts: `api:OnTriggerEnter` / `api:OnTriggerExit`
