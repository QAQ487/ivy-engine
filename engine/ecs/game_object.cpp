#include "engine/ecs/game_object.h"
#include "transform.h"
#include "visual_component.h"
#include "material_component.h"
#include "collider_component.h"
#include "engine/scene/render_settings.h"
#include "engine/rendering/d3d11/shader.h"

#include <DirectXMath.h>
#include <algorithm>

using namespace DirectX;

void GameObject::AddIgnoreCollisionTag(Tag tag) {
    // Must live in the EXE .cpp so /MT script DLLs do not allocate this vector
    // on the DLL heap (destroying the GameObject in the EXE would then assert).
    m_ignoreCollisionTags.push_back(tag);
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
        position = t->localPosition;
        rotation = t->localRotation;
        scale    = t->localScale;
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
            if (model) {
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

                model->SetPosition(t->localPosition);
                model->SetRotateEulerDeg(t->localRotation);
                model->SetScale(t->localScale);
                model->Draw();
                model->SetTextureOverride(nullptr);
                model->ClearMaterialOverride();
                model->SetEmission(prevEmission);
            }
        }
    }

    if (auto* cc = GetComponent<ColliderComponent>())
        m_debugCollider = cc->showDebugCollider;

    if (m_debugCollider && m_collider)
        m_collider->DebugDraw();

    for (const auto& child : m_children)
        if (child) child->DrawVisual(useRealModel);
}
