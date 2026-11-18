// game_object.cpp
// ECS game object: hierarchy, components, lifecycle.

#include "engine/physics/2d/physics2d.h"
#include "sprite_animator_component.h"
#include "engine/scene/scene_loader.h"
#include "engine/ecs/game_object.h"
#include "transform.h"
#include "visual_component.h"
#include "material_component.h"
#include "collider_component_access.h"
#include "engine/scripting/script_component.h"
#include "engine/scene/render_settings.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/culling/frustum_cull.h"
#include "engine/rendering/culling/cull_settings_math.h"
#include "engine/rendering/culling/lod_select_math.h"
#include "engine/rendering/culling/lod_state.h"
#include "engine/rendering/culling/shadow_cascade_cull.h"
#include "engine/rendering/lighting/shadow.h"
#include "engine/audio/audio_manager.h"
#include "engine/physics/core/physics_system.h"
#include "engine/core/debug_system.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <atomic>

using namespace DirectX;

namespace {
std::atomic<int> g_gameObjectUpdateDepth{ 0 };
}

bool GameObject::IsInsideUpdate() {
    return g_gameObjectUpdateDepth.load(std::memory_order_relaxed) > 0;
}

void GameObject::Update(double elapsed_time) {
    if (!m_isActive) return;

    g_gameObjectUpdateDepth.fetch_add(1, std::memory_order_relaxed);

    for (auto& kv : m_components)
        if(kv.second->GetTypeName()!=std::string("SpriteAnimator")) kv.second->Update(elapsed_time);

    LogicUpdate(elapsed_time);
    if(auto* animator=GetComponent<SpriteAnimatorComponent>()) animator->Update(elapsed_time);
    if(SceneLoader_Uses3DPhysics()) IntegratePhysics(elapsed_time);

    for (auto& child : m_children)
        if (child) child->Update(elapsed_time);

    g_gameObjectUpdateDepth.fetch_sub(1, std::memory_order_relaxed);
}

bool GameObject::Update_IsParallelSafe() const {
    for (const auto& kv : m_components) {
        const ScriptComponent* sc =
            dynamic_cast<const ScriptComponent*>(kv.second.get());
        if (!sc) continue;
        const Script* script = sc->GetScript();
        if (!script) {
            // Script not created yet but class requested — treat as unsafe.
            if (!sc->GetScriptClass().empty())
                return false;
            continue;
        }
        if (!script->Update_IsParallelSafe())
            return false;
    }
    return true;
}

GameObject::~GameObject() {
    // Clears any AudioManager listener/emitter tracking pointers that still
    // reference this object, so a playing sound's next Update() can't
    // dereference a freed GameObject.
    AudioManager::Instance().OnGameObjectDestroyed(this);
    Physics2D_Remove(this);
    PhysicsSystem_NotifyObjectDestroyed(this);

    for (auto& kv : m_components)
        kv.second->OnDetach();
    // Colliders created via ScriptApi/Host must be freed in the EXE heap.
    if (m_collider && !m_colliderOwnedByHost)
        delete m_collider;
    m_collider = nullptr;
    m_colliderOwnedByHost = false;
}

void GameObject::OnTriggerEnter(GameObject* other) {
    if (ScriptComponent* sc = GetComponent<ScriptComponent>()) {
        if (Script* script = sc->GetScript())
            script->OnTriggerEnter(*this, other);
    }
}

void GameObject::OnTriggerExit(GameObject* other) {
    if (ScriptComponent* sc = GetComponent<ScriptComponent>()) {
        if (Script* script = sc->GetScript())
            script->OnTriggerExit(*this, other);
    }
}

void GameObject::AddIgnoreCollisionTag(Tag tag) {
    // Must live in the EXE .cpp so /MT script DLLs do not allocate this vector
    // on the DLL heap (destroying the GameObject in the EXE would then assert).
    if (ShouldIgnoreCollisionWith(tag))
        return;
    m_ignoreCollisionTags.push_back(tag);
}

void GameObject::RemoveIgnoreCollisionTag(Tag tag) {
    m_ignoreCollisionTags.erase(
        std::remove(m_ignoreCollisionTags.begin(), m_ignoreCollisionTags.end(), tag),
        m_ignoreCollisionTags.end());
}

void GameObject::AddIgnoreCollisionObject(GameObject* other) {
    if (!other || other == this)
        return;
    for (GameObject* existing : m_ignoreCollisionObjects) {
        if (existing == other)
            return;
    }
    m_ignoreCollisionObjects.push_back(other);
}

void GameObject::RemoveIgnoreCollisionObject(GameObject* other) {
    m_ignoreCollisionObjects.erase(
        std::remove(m_ignoreCollisionObjects.begin(), m_ignoreCollisionObjects.end(), other),
        m_ignoreCollisionObjects.end());
}

void GameObject::ClearIgnoreCollisionObjects() {
    m_ignoreCollisionObjects.clear();
}

bool GameObject::IgnoresCollisionWith(const GameObject* other) const {
    if (!other)
        return true;
    if (ShouldIgnoreCollisionWith(other->m_tag))
        return true;
    for (GameObject* existing : m_ignoreCollisionObjects) {
        if (existing == other)
            return true;
    }
    return false;
}

engine::Transform* GameObject::GetTransform() {
    return GetComponent<engine::Transform>();
}

const engine::Transform* GameObject::GetTransform() const {
    return GetComponent<engine::Transform>();
}

void GameObject::SyncLegacyTransformFromComponents() {
    if (const engine::Transform* t = GetTransform()) {
        m_position = t->localPosition;
        m_rotation = t->localRotation;
        m_scale    = t->localScale;
    }
}

void GameObject::SyncComponentsFromLegacyTransform() {
    if (engine::Transform* t = GetTransform()) {
        t->localPosition = m_position;
        t->localRotation = m_rotation;
        t->localScale    = m_scale;
    }
}

void GameObject::ReparentChild(GameObject* child, GameObject& newParent) {
    if (!child || child == &newParent) return;

    // Reject reparenting under one of the child's own descendants: that would
    // create a hierarchy cycle and infinite-recurse Update()/Draw().
    for (GameObject* p = &newParent; p != nullptr; p = p->GetParent()) {
        if (p == child) return;
    }

    GameObject* oldParent = child->GetParent();
    if (!oldParent) return;

    auto& siblings = oldParent->GetChildren();
    auto it = std::find_if(siblings.begin(), siblings.end(),
        [child](const std::unique_ptr<GameObject>& c) { return c.get() == child; });
    if (it == siblings.end()) return;

    std::unique_ptr<GameObject> owned = std::move(*it);
    siblings.erase(it);
    newParent.AddChild(std::move(owned));
}

void GameObject::GetWorldPose(XMFLOAT3& position,
                              XMFLOAT3& rotation,
                              XMFLOAT3& scale) const {
    if (const engine::Transform* t = GetTransform()) {
        t->GetWorldPositionRotationScale(position, rotation, scale);
    } else {
        position = m_position;
        rotation = m_rotation;
        scale    = m_scale;
    }
}

void GameObject::DrawVisual(bool useRealModel) {
    if (!m_isActive) return;

    SyncLegacyTransformFromComponents();

    if (const engine::Transform* t = GetTransform()) {
        if (auto* vc = GetComponent<VisualComponent>()) {
            Model3D* model = useRealModel ? vc->GetRealModel() : vc->GetModel();

            XMFLOAT3 worldPos, worldRot, worldScale;
            t->GetWorldPositionRotationScale(worldPos, worldRot, worldScale);

            // Distance-based mesh LOD (Play / runtime only). Level 0 is the base
            // mesh; levels > 0 are skipped while the Visual animates so the shared
            // skeleton / bind pose is never mixed with a different mesh.
            int lodLevel = 0;
            bool lodCulledByDistance = false;
            bool lodTracked = false;
            const bool lodCounting = FrustumCull_IsMainPassActive()
                && Shadow::GetRenderingLightIndex() < 0;
            if (model && useRealModel && Lod_IsFrameActive()
                && RenderSettings_GetConst().lodEnabled
                && (!vc->lods.empty() || vc->cullDistance > 0.0f)) {
                lodTracked = true;
                float distance = 0.0f;
                {
                    XMFLOAT3 center = worldPos;
                    if (model->HasLocalAabb()) {
                        frustum_cull::Aabb local{};
                        model->GetLocalAabb(local.min, local.max);
                        local.valid = true;
                        const frustum_cull::Aabb wb = frustum_cull::TransformAabbTRS(
                            local, worldPos, worldRot, worldScale);
                        center = XMFLOAT3(
                            0.5f * (wb.min.x + wb.max.x),
                            0.5f * (wb.min.y + wb.max.y),
                            0.5f * (wb.min.z + wb.max.z));
                    }
                    const XMFLOAT3& cam = Lod_GetCameraPosition();
                    const float dx = center.x - cam.x;
                    const float dy = center.y - cam.y;
                    const float dz = center.z - cam.z;
                    distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                }

                float thresholds[kLodMaxLevels - 1] = {};
                int thresholdCount = 0;
                if (!vc->playAnimation) {
                    thresholdCount = static_cast<int>(
                        std::min<size_t>(vc->lods.size(), kLodMaxLevels - 1));
                    for (int i = 0; i < thresholdCount; ++i)
                        thresholds[i] = vc->lods[static_cast<size_t>(i)].maxDistance;
                    lod_select::NormalizeThresholds(thresholds, thresholdCount);
                }
                lodLevel = lod_select::SelectLevel(
                    distance, thresholds, thresholdCount, vc->lodLastLevel,
                    lod_select::kDefaultHysteresis,
                    RenderSettings_GetConst().lodBias,
                    cull_settings::ScaledCullDistance(
                        vc->cullDistance, RenderSettings_GetConst().cullDistanceScale));
                if (lodLevel == lod_select::kCulled) {
                    lodCulledByDistance = true;
                    lodLevel = 0;
                } else {
                    vc->lodLastLevel = lodLevel;
                    if (lodLevel > 0) {
                        if (Model3D* lodModel = vc->GetLodModel(lodLevel))
                            model = lodModel;
                        else
                            lodLevel = 0;
                    }
                }
                if (lodCounting) {
                    Lod_RecordConsidered();
                    if (lodCulledByDistance)
                        Lod_RecordCulledByDistance();
                }
            }

            // Stamp for off-screen animation skipping (see anim_cull_math.h). Only the
            // real-model Play pass counts; shadow passes mark drawn below as well.
            const bool animStamp = useRealModel && Lod_IsFrameActive();
            if (animStamp)
                vc->lastConsideredSerial = Lod_GetFrameSerial();

            if (model) {
                // Main-camera frustum cull and/or per-cascade light-ortho caster cull.
                // Cascade cull uses open-near so casters between the light and the
                // cascade volume still cast; camera-frustum culling never gates shadows.
                bool submitDraw = !lodCulledByDistance;
                const auto& renderSettings = RenderSettings_GetConst();
                const bool frustumEnabled = renderSettings.frustumCullingEnabled;
                const bool shadowCullEnabled = renderSettings.shadowCascadeCullingEnabled;
                const bool mainPass = FrustumCull_IsMainPassActive()
                    && Shadow::GetRenderingLightIndex() < 0;
                const bool shadowPass = ShadowCascadeCull_IsCascadePassActive()
                    && Shadow::GetRenderingLightIndex() >= 0;

                frustum_cull::Aabb world{};
                bool haveWorldAabb = false;
                auto ensureWorldAabb = [&]() -> const frustum_cull::Aabb& {
                    if (!haveWorldAabb) {
                        frustum_cull::Aabb local{};
                        model->GetLocalAabb(local.min, local.max);
                        local.valid = true;
                        world = frustum_cull::TransformAabbTRS(
                            local, worldPos, worldRot, worldScale);
                        haveWorldAabb = true;
                    }
                    return world;
                };

                if (frustumEnabled && mainPass && model->HasLocalAabb()) {
                    FrustumCull_RecordConsidered();
                    if (!frustum_cull::AabbIntersectsFrustum(
                            ensureWorldAabb(), FrustumCull_GetPlanes())) {
                        FrustumCull_RecordCulled();
                        submitDraw = false;
                    }
                }

                if (submitDraw && shadowCullEnabled && shadowPass && model->HasLocalAabb()) {
                    ShadowCascadeCull_RecordConsidered();
                    if (!frustum_cull::AabbIntersectsFrustumOpenNear(
                            ensureWorldAabb(), ShadowCascadeCull_GetPlanes())) {
                        ShadowCascadeCull_RecordCulled();
                        submitDraw = false;
                    }
                }

                if (submitDraw) {
                    if (animStamp)
                        vc->lastDrawnSerial = Lod_GetFrameSerial();
                    const XMFLOAT4 prevEmission = model->GetEmission();
                    model->SetEmission(vc->emissionGlow);
                    Shader3DVariant variant = vc->shaderOverride.empty()
                        ? RenderSettings_GetDefaultShader()
                        : Shader3DVariantFromId(vc->shaderOverride);
                    model->SetShaderVariant(variant);

                    if (variant == Shader3DVariant::SurfaceFX) {
                        Shader_SetSurfaceFXParams(vc->hasSurfaceFx ? vc->surfaceFx : SURFACE_FX_PARAMS{});
                        model->SetTextureOverride(vc->GetSurfaceFxSRV());
                        model->ClearMaterialOverride();
                    } else {
                        model->SetTextureOverride(nullptr);

                        if (auto* mc = GetComponent<MaterialComponent>()) {
                            Model3D::MaterialOverride mo;
                            mo.enabled = true;
                            mo.color = mc->color;
                            mo.roughness = mc->roughness;
                            mo.metallic = mc->metallic;
                            mo.normalStrength = mc->normalStrength;
                            mo.normalSRV = mc->GetNormalMapSRV();
                            mo.shadeSteps = mc->shadeSteps;
                            mo.shadeSmoothness = mc->shadeSmoothness;
                            mo.specularThreshold = mc->specularThreshold;
                            mo.outlineWidth = mc->outlineWidth;
                            mo.outlineColor = mc->outlineColor;
                            mo.shadowTint = mc->shadowTint;
                            mo.rimStrength = mc->rimStrength;
                            mo.rampSRV = mc->GetRampTextureSRV();
                            model->SetMaterialOverride(mo);
                        } else {
                            model->ClearMaterialOverride();
                        }
                    }

                    model->SetPosition(worldPos);
                    model->SetRotateEulerDeg(worldRot);
                    model->SetScale(worldScale);

                    if (frustumEnabled && mainPass)
                        FrustumCull_RecordDrawn();
                    if (lodCounting && lodTracked)
                        Lod_RecordDrawn(lodLevel);
                    if (shadowCullEnabled && shadowPass)
                        ShadowCascadeCull_RecordDrawn();
                    model->Draw();
                    model->SetTextureOverride(nullptr);
                    model->ClearMaterialOverride();
                    model->SetEmission(prevEmission);
                }
            }
        }
    }

    if (auto* cc = FindColliderComponent(this))
        m_debugCollider = cc->showDebugCollider;

#if defined(GAME_RUNTIME)
    if (m_debugCollider && m_collider && DebugSystem::CollisionVizEnabled())
        m_collider->DebugDraw();
#else
    if (m_debugCollider && m_collider)
        m_collider->DebugDraw();
#endif

    for (const auto& child : m_children)
        if (child) child->DrawVisual(useRealModel);
}
