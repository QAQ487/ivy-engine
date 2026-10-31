/**===========================================================================
 File   :  [game_object.h]
 Author : Terry Yeung
 Date   : 2025/10/29  →  2026-03-13 (refactor)
 -----------------------------------------------------------------------------
 CHANGES (2026-03-13)
   - Added Component system (m_components, AddComponent<T>, GetComponent<T>, …)
     ported from engine::GameObject (scene_node.h).
   - Added m_prefabAssetPath for round-trip Prefab serialization.
   - Added m_name for editor / serializer identification.
   - Update() now ticks all attached components before doing physics.
   - Reset() added: restores runtime state to pre-play defaults via
     Component::OnReset() (mirrors engine::GameObject::Reset()).

 MIGRATION
   - engine::GameObject (scene_node.h) is now a thin alias for ::GameObject.
     All Prefab / Script code that used engine::GameObject continues to compile
     without changes.
   - MapObjectGameObject no longer needs a separate model field; it attaches a
     VisualComponent instead (see map_manager.h).
 ===========================================================================**/
#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include "../rendering/model/model.h"
#include "../physics/core/rigidbody.h"
#include "scene_manager.h"
#include "../physics/collider/collider.h"
#include "component.h"
#include "rigidbody_component.h"
#include "transform.h"
#include "engine/core/debug_system.h"

namespace engine { class Transform; }

#include <algorithm>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

enum class Tag {
    TAG_NONE,

    TAG_SHADOW,
    TAG_GROUND,
    TAG_OBSTACLES,  // temp
    TAG_WALL,

    TAG_PLAYER,
    TAG_ENEMY,
    TAG_BULLET,
    TAG_SCLAP,

    TAG_MAX
};

class GameObject {
public:
    // --- Public physics / rendering state ---
    // Kept public for backwards compatibility with existing gameplay code.
    DirectX::XMFLOAT3 m_position         = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_previousPosition = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_rotation         = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_scale            = {1.0f, 1.0f, 1.0f};
    bool              m_isActive         = true;
    Tag               m_tag              = Tag::TAG_NONE;
    Collider*         m_collider         = nullptr;
    bool              m_colliderOwnedByHost = false;
    bool              m_debugCollider    = false;

    std::vector<Tag>  m_ignoreCollisionTags;
    std::vector<GameObject*> m_ignoreCollisionObjects;

    // --- Constructor / Destructor ---
    explicit GameObject(std::string name = "GameObject")
        : m_name(std::move(name)) {}

    // Defined out-of-line in game_object.cpp (EXE-only translation unit) so
    // it can safely notify EXE-only singletons like AudioManager. Must NOT
    // move back into this header: /MT script DLLs also compile this header,
    // and a copy of the destructor body living in the DLL could end up
    // touching a separate AudioManager instance (or the DLL heap) instead of
    // the EXE's, the same class of bug AddIgnoreCollisionTag was split out
    // to avoid.
    virtual ~GameObject();

    // --- Identity ---
    const std::string& GetName()  const { return m_name; }
    void SetName(const std::string& n)  { m_name = n; }

    // Path to the source prefab JSON, empty for non-prefab objects.
    const std::string& GetPrefabAssetPath() const { return m_prefabAssetPath; }
    void SetPrefabAssetPath(const std::string& p)  { m_prefabAssetPath = p; }

    // --- Component system ---

    // Construct a component of type T in-place and attach it to this object.
    // Returns a raw pointer to the new component (owned by this GameObject).
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        auto comp  = std::make_unique<T>(std::forward<Args>(args)...);
        comp->owner = this;
        comp->OnAttach();
        T* ptr = comp.get();
        std::type_index key(typeid(T));
        auto it = m_components.find(key);
        if (it != m_components.end()) it->second->OnDetach();
        m_components[key] = std::move(comp);
        return ptr;
    }

    // Attach an already-constructed component (used by ComponentFactory /
    // PrefabSerializer when reconstructing objects from JSON).
    void AddComponentRaw(std::unique_ptr<Component> comp) {
        if (!comp) return;
        std::type_index key(typeid(*comp));
        comp->owner = this;
        comp->OnAttach();
        auto it = m_components.find(key);
        if (it != m_components.end()) it->second->OnDetach();
        m_components[key] = std::move(comp);
    }

    // Store prepared data without registering audio/effects in the live world.
    void AddPreparedComponent(std::unique_ptr<Component> comp) {
        if (!comp) return;
        comp->owner = this;
        const std::type_index key(typeid(*comp));
        m_components[key] = std::move(comp);
    }
    void AttachPreparedComponents() {
        for (auto& entry : m_components) entry.second->OnAttach();
        for (auto& child : m_children) child->AttachPreparedComponents();
    }

    // Return a pointer to the first component of type T, or nullptr.
    template<typename T>
    T* GetComponent() {
        auto it = m_components.find(std::type_index(typeid(T)));
        return (it != m_components.end())
               ? static_cast<T*>(it->second.get()) : nullptr;
    }

    template<typename T>
    const T* GetComponent() const {
        auto it = m_components.find(std::type_index(typeid(T)));
        return (it != m_components.end())
               ? static_cast<const T*>(it->second.get()) : nullptr;
    }

    template<typename T>
    bool HasComponent() const {
        return m_components.count(std::type_index(typeid(T))) > 0;
    }

    template<typename T>
    void RemoveComponent() {
        auto it = m_components.find(std::type_index(typeid(T)));
        if (it != m_components.end()) {
            it->second->OnDetach();
            m_components.erase(it);
        }
    }

    // Read-only access to all components (used by PrefabSerializer).
    const std::unordered_map<std::type_index, std::unique_ptr<Component>>&
    GetComponents() const { return m_components; }

    // --- Scene hierarchy ---
    // A GameObject can be a root node or a child of another GameObject.
    // Ownership of children belongs to the parent.

    GameObject* GetParent() const { return m_parent; }
    void SetParent(GameObject* parent) { m_parent = parent; }

    void AddChild(std::unique_ptr<GameObject> child) {
        if (!child) return;
        child->m_parent = this;
        m_children.push_back(std::move(child));
    }

    const std::vector<std::unique_ptr<GameObject>>& GetChildren() const { return m_children; }
          std::vector<std::unique_ptr<GameObject>>& GetChildren()       { return m_children; }

    // --- Lifecycle ---
    virtual void Initialize() {}
    virtual void Finalize()   {}

    // Per-frame update: ticks all components, then integrates physics,
    // Tick attached components, LogicUpdate, physics, then children.
    // NOTE: LogicUpdate() is kept for backwards compatibility with existing
    // subclasses.  New code should use Component::Update() instead.
    void Update(double elapsed_time);

    // True while any GameObject::Update is on the call stack (any thread).
    static bool IsInsideUpdate();

    // Worker-safe Update: true when every attached Script returns
    // Update_IsParallelSafe (or there is no Script). Default scripts are false.
    bool Update_IsParallelSafe() const;

    // Restore runtime state to pre-play defaults.
    // Calls OnReset() on all components, then recurses into children.
    void Reset() {
        for (auto& kv : m_components)
            kv.second->OnReset();
        for (auto& child : m_children)
            if (child) child->Reset();
    }

    // Override in subclasses for per-frame logic (legacy pattern).
    // Prefer AddComponent<MyScriptComponent>() for new code.
    virtual void LogicUpdate(double /*elapsed_time*/) {}

    virtual void IntegratePhysics(double elapsed_time) {
        RigidbodyComponent* rc = GetComponent<RigidbodyComponent>();
        if (!rc)
            return;

        Rigidbody& rb = rc->Body();
        if (rb.IsKinematic())
            return;

        // PhysicsSystem_Update already integrates dynamic objects that have a
        // collider (registered in the scene). Integrating again here doubled
        // movement once SetPosition kept Transform in sync with m_position.
        if (m_collider)
            return;

        rb.SetLocalInverseInertia(Rigidbody_InvInertiaUnitCube(rb.GetMass()));
        DirectX::XMFLOAT3 pos = m_position;
        DirectX::XMFLOAT3 rot = m_rotation;
        rb.Integrate(pos, rot, elapsed_time);
        SetPosition(pos);
        SetRotation(rot);
    }

    // Draw this object and all children.
    // Subclasses (e.g. MapObjectGameObject) override this to use the
    // MapManager model cache instead of a local Model3D.
    virtual void Draw() {
        if (!m_isActive) return;

#if defined(GAME_RUNTIME)
        if (m_debugCollider && m_collider && DebugSystem::CollisionVizEnabled())
            m_collider->DebugDraw();
#else
        if (m_debugCollider && m_collider)
            m_collider->DebugDraw();
#endif

        for (auto& child : m_children)
            if (child) child->Draw();
    }

    // --- Collision ---
    virtual void OnCollision(GameObject* /*other*/)     {}
    virtual void OnCollisionExit(GameObject* /*other*/) {}
    virtual void OnTriggerEnter(GameObject* other);
    virtual void OnTriggerExit(GameObject* other);

    virtual bool CanCollideWith(GameObject* other) const {
        if (!other) return false;
        if (IgnoresCollisionWith(other)) return false;
        if (other->IgnoresCollisionWith(this)) return false;
        return true;
    }

    // --- Transform helpers ---
    void Translate(const DirectX::XMFLOAT3& offset) {
        m_position.x += offset.x;
        m_position.y += offset.y;
        m_position.z += offset.z;
    }

    void SetActive(bool active) { m_isActive = active; }
    bool IsActive() const       { return m_isActive; }

    void SetTag(Tag tag) { m_tag = tag; }
    void SetTag(int tagId) {
        if (tagId >= 0)
            m_tag = static_cast<Tag>(tagId);
    }
    Tag GetTag() const { return m_tag; }
    int GetTagId() const { return static_cast<int>(m_tag); }

    void SetPosition(const DirectX::XMFLOAT3& p) {
        m_position = p;
        // Keep Transform in lockstep ? colliders/GetWorldPose read it; scripts
        // call SetPosition from the DLL so this must stay header-inline.
        if (engine::Transform* t = GetComponent<engine::Transform>())
            t->localPosition = p;
    }
    // Live pose. Own-object reads are fine during Update. Neighbor/other-object
    // reads during a parallel Update batch are unsafe - use
    // FrameTransformSnapshot_Find from host C++ instead (no new script API).
    DirectX::XMFLOAT3 GetPosition() const         { return m_position; }

    void SetRotation(const DirectX::XMFLOAT3& r) {
        m_rotation = r;
        if (engine::Transform* t = GetComponent<engine::Transform>())
            t->localRotation = r;
    }
    // See GetPosition - same parallel-batch neighbor-read caveat.
    DirectX::XMFLOAT3 GetRotation() const         { return m_rotation; }

    void SetScale(const DirectX::XMFLOAT3& s) {
        m_scale = s;
        if (engine::Transform* t = GetComponent<engine::Transform>())
            t->localScale = s;
    }
    DirectX::XMFLOAT3 GetScale() const         { return m_scale; }

    // Ensures a RigidbodyComponent exists (lazy-add). Prefer attaching
    // Rigidbody in the editor so settings serialize into the scene.
    Rigidbody& GetRigidbody() {
        RigidbodyComponent* rc = GetComponent<RigidbodyComponent>();
        if (!rc)
            rc = AddComponent<RigidbodyComponent>();
        return rc->Body();
    }
    const Rigidbody& GetRigidbody() const {
        if (const RigidbodyComponent* rc = GetComponent<RigidbodyComponent>())
            return rc->Body();
        // Objects with a collider but no explicit RigidbodyComponent (e.g.
        // script-created colliders that never called the non-const
        // GetRigidbody()) should read as static/kinematic by default, to
        // match ColliderComponent::ApplyToOwner()'s auto-added rigidbody
        // convention. Rigidbody defaults to non-kinematic/mass=1, which would
        // otherwise make these objects look "dynamic" to IsDynamic() checks.
        static const Rigidbody s_fallback = [] {
            Rigidbody rb;
            rb.SetIsKinematic(true);
            return rb;
        }();
        return s_fallback;
    }

    Collider*       GetCollider()       { return m_collider; }
    const Collider* GetCollider() const { return m_collider; }

    // --- Transform component shortcut ---
    engine::Transform* GetTransform();
    const engine::Transform* GetTransform() const;

    void SyncLegacyTransformFromComponents();
    void SyncComponentsFromLegacyTransform();

    void ReparentChild(GameObject* child, GameObject& newParent);

    void DrawVisual(bool useRealModel);

    // World pose for rendering / debug (Transform component when present).
    void GetWorldPose(DirectX::XMFLOAT3& position,
                      DirectX::XMFLOAT3& rotation,
                      DirectX::XMFLOAT3& scale) const;

    // --- Collision ignore ---
    void AddIgnoreCollisionTag(Tag tag);
    void RemoveIgnoreCollisionTag(Tag tag);
    void AddIgnoreCollisionObject(GameObject* other);
    void RemoveIgnoreCollisionObject(GameObject* other);
    void ClearIgnoreCollisionObjects();
    bool ShouldIgnoreCollisionWith(Tag otherTag) const {
        for (const auto& t : m_ignoreCollisionTags)
            if (t == otherTag) return true;
        return false;
    }
    bool IgnoresCollisionWith(const GameObject* other) const;

    // --- Collider factories ---
    BoxCollider* CreateBoxCollider(
        const DirectX::XMFLOAT3& size,
        const DirectX::XMFLOAT3& center = {0.0f, 0.0f, 0.0f})
    {
        if (m_collider && !m_colliderOwnedByHost) delete m_collider;
        m_colliderOwnedByHost = false;
        auto* box = new BoxCollider(this);
        box->SetSize(size);
        box->SetCenter(center);
        m_collider = box;
        return box;
    }

    SphereCollider* CreateSphereCollider(
        float radius,
        const DirectX::XMFLOAT3& center = {0.0f, 0.0f, 0.0f})
    {
        if (m_collider && !m_colliderOwnedByHost) delete m_collider;
        m_colliderOwnedByHost = false;
        auto* sphere = new SphereCollider(this);
        sphere->SetRadius(radius);
        sphere->SetCenter(center);
        m_collider = sphere;
        return sphere;
    }

    CapsuleCollider* CreateCapsuleCollider(
        float radius, float height,
        const DirectX::XMFLOAT3& center = {0.0f, 0.0f, 0.0f})
    {
        if (m_collider && !m_colliderOwnedByHost) delete m_collider;
        m_colliderOwnedByHost = false;
        auto* capsule = new CapsuleCollider(this);
        capsule->SetRadius(radius);
        capsule->SetHeight(height);
        capsule->SetCenter(center);
        m_collider = capsule;
        return capsule;
    }

    CylinderCollider* CreateCylinderCollider(
        float radius, float height,
        const DirectX::XMFLOAT3& center = {0.0f, 0.0f, 0.0f})
    {
        if (m_collider && !m_colliderOwnedByHost) delete m_collider;
        m_colliderOwnedByHost = false;
        auto* cylinder = new CylinderCollider(this);
        cylinder->SetRadius(radius);
        cylinder->SetHeight(height);
        cylinder->SetCenter(center);
        m_collider = cylinder;
        return cylinder;
    }

    // NOTE: CreateMeshCollider now requires the caller to pass a Model3D
    // explicitly, because GameObject no longer owns m_model directly.
    // MapObjectGameObject passes the cached model from MapManager.
    MeshCollider* CreateMeshCollider(
        Model3D& sourceModel,
        const DirectX::XMFLOAT3& center = {0.0f, 0.0f, 0.0f},
        float sizePadding = 0.0f)
    {
        if (m_collider && !m_colliderOwnedByHost) delete m_collider;
        m_colliderOwnedByHost = false;
        auto* mesh = new MeshCollider(this);
        mesh->SetCenter(center);
        mesh->SetSizePadding(sizePadding);
        mesh->BuildFromModel(sourceModel);
        m_collider = mesh;
        return mesh;
    }

    void RefreshColliderData(Model3D& sourceModel) {
        if (m_collider && m_collider->GetType() == ColliderType::MESH) {
            auto* meshCol = static_cast<MeshCollider*>(m_collider);
            meshCol->BuildFromModel(sourceModel);
        }
    }

private:
    std::string m_name;
    std::string m_prefabAssetPath;

    GameObject* m_parent = nullptr;
    std::vector<std::unique_ptr<GameObject>> m_children;

    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_components;
};

#endif // GAME_OBJECT_H
