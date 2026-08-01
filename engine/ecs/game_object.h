/**===========================================================================
 File   :  [game_object.h]
 Author : Hayato Otsuki  (original)
          Terry           (Component system integration)
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
    // ── Public physics / rendering state ─────────────────────────────────
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

    // ── Constructor / Destructor ──────────────────────────────────────────
    explicit GameObject(std::string name = "GameObject")
        : m_name(std::move(name)) {}

    virtual ~GameObject() {
        for (auto& kv : m_components)
            kv.second->OnDetach();
        // Colliders created via ScriptApi/Host must be freed in the EXE heap.
        if (m_collider && !m_colliderOwnedByHost)
            delete m_collider;
        m_collider = nullptr;
        m_colliderOwnedByHost = false;
    }

    // ── Identity ──────────────────────────────────────────────────────────
    const std::string& GetName()  const { return m_name; }
    void SetName(const std::string& n)  { m_name = n; }

    // Path to the source prefab JSON, empty for non-prefab objects.
    const std::string& GetPrefabAssetPath() const { return m_prefabAssetPath; }
    void SetPrefabAssetPath(const std::string& p)  { m_prefabAssetPath = p; }

    // ── Component system ──────────────────────────────────────────────────

    // Construct a component of type T in-place and attach it to this object.
    // Returns a raw pointer to the new component (owned by this GameObject).
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        auto comp  = std::make_unique<T>(std::forward<Args>(args)...);
        comp->owner = this;
        comp->OnAttach();
        T* ptr = comp.get();
        m_components[std::type_index(typeid(T))] = std::move(comp);
        return ptr;
    }

    // Attach an already-constructed component (used by ComponentFactory /
    // PrefabSerializer when reconstructing objects from JSON).
    void AddComponentRaw(std::unique_ptr<Component> comp) {
        if (!comp) return;
        std::type_index key(typeid(*comp));
        comp->owner = this;
        comp->OnAttach();
        m_components[key] = std::move(comp);
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

    // ── Scene hierarchy ───────────────────────────────────────────────────
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

    // ── Lifecycle ─────────────────────────────────────────────────────────
    virtual void Initialize() {}
    virtual void Finalize()   {}

    // Per-frame update: ticks all components, then integrates physics,
    // then recurses into children.
    // NOTE: LogicUpdate() is kept for backwards compatibility with existing
    // subclasses.  New code should use Component::Update() instead.
    void Update(double elapsed_time) {
        if (!m_isActive) return;

        // Tick attached components first.
        for (auto& kv : m_components)
            kv.second->Update(elapsed_time);

        // Subclass per-frame logic (legacy hook).
        LogicUpdate(elapsed_time);

        // Physics integration.
        IntegratePhysics(elapsed_time);

        // Recurse into children.
        for (auto& child : m_children)
            if (child) child->Update(elapsed_time);
    }

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

        DirectX::XMFLOAT3 pos = m_position;
        rb.Integrate(pos, elapsed_time);
        SetPosition(pos);
    }

    // Draw this object and all children.
    // Subclasses (e.g. MapObjectGameObject) override this to use the
    // MapManager model cache instead of a local Model3D.
    virtual void Draw() {
        if (!m_isActive) return;

        if (m_debugCollider && m_collider)
            m_collider->DebugDraw();

        for (auto& child : m_children)
            if (child) child->Draw();
    }

    // ── Collision ─────────────────────────────────────────────────────────
    virtual void OnCollision(GameObject* /*other*/)     {}
    virtual void OnCollisionExit(GameObject* /*other*/) {}

    virtual bool CanCollideWith(GameObject* other) const {
        if (!other) return false;
        if (ShouldIgnoreCollisionWith(other->m_tag)) return false;
        if (other->ShouldIgnoreCollisionWith(m_tag)) return false;
        return true;
    }

    // ── Transform helpers ─────────────────────────────────────────────────
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
        // Keep Transform in lockstep — colliders/GetWorldPose read it; scripts
        // call SetPosition from the DLL so this must stay header-inline.
        if (engine::Transform* t = GetComponent<engine::Transform>())
            t->localPosition = p;
    }
    DirectX::XMFLOAT3 GetPosition() const         { return m_position; }

    void SetRotation(const DirectX::XMFLOAT3& r) {
        m_rotation = r;
        if (engine::Transform* t = GetComponent<engine::Transform>())
            t->localRotation = r;
    }
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
        static const Rigidbody s_fallback;
        return s_fallback;
    }

    Collider*       GetCollider()       { return m_collider; }
    const Collider* GetCollider() const { return m_collider; }

    // ── Transform component shortcut (Unity-style) ─────────────────────────
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

    // ── Collision ignore ──────────────────────────────────────────────────
    void AddIgnoreCollisionTag(Tag tag);
    void RemoveIgnoreCollisionTag(Tag tag);
    bool ShouldIgnoreCollisionWith(Tag otherTag) const {
        for (const auto& t : m_ignoreCollisionTags)
            if (t == otherTag) return true;
        return false;
    }

    // ── Collider factories ────────────────────────────────────────────────
    BoxCollider* CreateBoxCollider(
        const DirectX::XMFLOAT3& size,
        const DirectX::XMFLOAT3& center = {0.0f, 0.0f, 0.0f})
    {
        delete m_collider;
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
        delete m_collider;
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
        delete m_collider;
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
        delete m_collider;
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
        delete m_collider;
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
