// script_runtime_api.cpp
// C API surface exposed to gameplay script DLLs.

#include "script_runtime_api.h"
#include "engine/scripting/host_text.h"
#include "engine/ecs/collider_component_access.h"

#include "camera.h"
#include "camera_director.h"
#include "engine/ui/scene_canvas.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/sprite/sprite.h"
#include "engine/ui/ui_text.h"
#include "third_person_camera.h"
#include "first_person_camera.h"
#include "engine/rendering/billboard/billboard.h"
#include "engine/rendering/particle/particle_system.h"
#include "engine/input/key_logger.h"
#include "engine/input/mouse.h"
#include "engine/debug_draw/debug_drawer.h"
#include "engine/core/engine_gui.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/rendering/sprite/line_sprite.h"
#include "engine/physics/collider/collider.h"
#include "engine/ecs/prefab_runtime.h"
#include "engine/ecs/effect_component.h"
#include "engine/ecs/audio_component.h"
#include "engine/audio/audio_manager.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/material_component.h"
#if defined(SCRIPT_RUNTIME_API_HOST)
#include "engine/scene/render_settings.h"
#include "engine/ecs/physics2d_components.h"
#include "engine/physics/2d/physics2d.h"
#include "engine/ecs/sprite_animator_component.h"
#include "engine/ecs/sprite_renderer_component.h"
#include "engine/ecs/game_object_search.h"
#include "engine/ecs/rigidbody_component.h"
#include "engine/scene/scene_loader.h"
#include "engine/core/system_timer.h"
#include "engine/save/save_store.h"
#include "engine/save/save_system.h"
#endif
#include "engine/rendering/d3d11/post_process.h"
#include "engine/physics/core/physics_system.h"
#include "engine/core/debug_system.h"

#include <Windows.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t kScriptRuntimeApiVersion = 51;

template <typename T>
T* FindComponentByTypeName(GameObject& object, const char* typeName) {
    for (const auto& kv : object.GetComponents()) {
        Component* component = kv.second.get();
        if (component && std::string(component->GetTypeName()) == typeName)
            return static_cast<T*>(component);
    }
    return nullptr;
}

template <typename T>
const T* FindComponentByTypeName(const GameObject& object, const char* typeName) {
    for (const auto& kv : object.GetComponents()) {
        const Component* component = kv.second.get();
        if (component && std::string(component->GetTypeName()) == typeName)
            return static_cast<const T*>(component);
    }
    return nullptr;
}

#if defined(SCRIPT_RUNTIME_API_HOST)
bool Host_Physics2DSetVelocity(GameObject* object, DirectX::XMFLOAT2 velocity) {
    auto* body = object ? FindComponentByTypeName<Rigidbody2DComponent>(*object, "Rigidbody2D") : nullptr;
    if (!body || body->bodyType != BodyType2D::Dynamic || !std::isfinite(velocity.x) || !std::isfinite(velocity.y)) return false;
    body->velocity = velocity;
    return true;
}
bool Host_Physics2DGetVelocity(GameObject* object, DirectX::XMFLOAT2* velocity) {
    auto* body = object ? FindComponentByTypeName<Rigidbody2DComponent>(*object, "Rigidbody2D") : nullptr;
    if (!body || !velocity) return false;
    *velocity = body->velocity;
    return true;
}
bool Host_Physics2DMovePosition(GameObject* object, DirectX::XMFLOAT2 position) {
    auto* body = object ? FindComponentByTypeName<Rigidbody2DComponent>(*object, "Rigidbody2D") : nullptr;
    return body && body->bodyType == BodyType2D::Kinematic && body->MovePosition(position);
}
bool Host_Physics2DRaycast(DirectX::XMFLOAT2 origin, DirectX::XMFLOAT2 direction, float distance, GameObject** outObject, DirectX::XMFLOAT2* outPoint, DirectX::XMFLOAT2* outNormal, float* outDistance, std::uint32_t mask, bool includeTriggers) {
    Physics2DRayHit hit{};
    if (outObject) *outObject = nullptr;
    if (!Physics2D_Raycast(origin, direction, distance, hit, mask, includeTriggers)) return false;
    if (outObject) *outObject = hit.object;
    if (outPoint) *outPoint = hit.point;
    if (outNormal) *outNormal = hit.normal;
    if (outDistance) *outDistance = hit.distance;
    return true;
}
bool Host_SpritePlay(GameObject* object, const char* clipName, bool restart) {
    auto* animator = object ? FindComponentByTypeName<SpriteAnimatorComponent>(*object, "SpriteAnimator") : nullptr;
    return animator && clipName ? animator->Play(clipName, restart) : false;
}
void Host_SpritePause(GameObject* object) {
    auto* animator = object ? FindComponentByTypeName<SpriteAnimatorComponent>(*object, "SpriteAnimator") : nullptr;
    if (animator) animator->Pause();
}
void Host_SpriteStop(GameObject* object) {
    auto* animator = object ? FindComponentByTypeName<SpriteAnimatorComponent>(*object, "SpriteAnimator") : nullptr;
    if (animator) animator->Stop();
}
void Host_SpriteSetFlip(GameObject* object, bool flipX, bool flipY) {
    auto* sprite = object ? FindComponentByTypeName<SpriteRendererComponent>(*object, "SpriteRenderer") : nullptr;
    if (sprite) { sprite->flipX = flipX; sprite->flipY = flipY; }
}
void Host_SpriteSetTint(GameObject* object, DirectX::XMFLOAT4 tint) {
    auto* sprite = object ? FindComponentByTypeName<SpriteRendererComponent>(*object, "SpriteRenderer") : nullptr;
    if (sprite) sprite->tint = tint;
}
SceneBase* Host_GetCurrentScene() {
    return SceneManager::Instance().GetCurrentScene();
}

DirectX::XMFLOAT3 Host_GetCameraForward() {
    return GetCamera().GetForward();
}

UISprite* Host_UICanvasAddSprite(UICanvas* canvas,
                                 const char* name,
                                 DirectX::XMFLOAT2 position,
                                 UIAnchor anchor,
                                 float width,
                                 float height) {
    return canvas ? canvas->AddSprite(name ? name : "", position, anchor, width, height) : nullptr;
}

void Host_UICanvasRemoveElement(UICanvas* canvas, const char* name) {
    if (canvas && name)
        canvas->RemoveElement(name);
}

void Host_UISpriteSetTexture(UISprite* sprite, const wchar_t* texturePath) {
    if (sprite)
        sprite->SetTexture(texturePath);
}

UIElement* Host_UICanvasGetElement(UICanvas* canvas, const char* name) {
    return canvas && name ? canvas->GetElement(name) : nullptr;
}

UISprite* Host_UICanvasGetSprite(UICanvas* canvas, const char* name) {
    UIElement* element = Host_UICanvasGetElement(canvas, name);
    return element ? dynamic_cast<UISprite*>(element) : nullptr;
}

void Host_UISpriteSetRadial360Fill(UISprite* sprite, float fillAmount, bool clockwise) {
    if (!sprite)
        return;
    sprite->SetFillMethod(UISpriteFillMethod::RADIAL_360);
    sprite->SetFillOrigin(UISpriteFillOrigin::TOP);
    sprite->SetFillClockwise(clockwise);
    sprite->SetFillAmount(fillAmount);
}

void Host_UISpriteSetFillNone(UISprite* sprite) {
    if (!sprite)
        return;
    sprite->SetFillMethod(UISpriteFillMethod::NONE);
    sprite->SetFillAmount(1.0f);
}

void Host_UISpriteSetSourceRect(UISprite* sprite, int x, int y, int width, int height) {
    if (!sprite)
        return;
    sprite->SetSourceRect(x, y, width, height);
}

void Host_UITextSetText(UIText* text, const char* value) {
    if (text && value)
        text->SetText(value);
}

void Host_UIElementSetVisible(UIElement* element, bool visible) {
    if (element)
        element->SetVisible(visible);
}

void Host_BillboardAdd(DirectX::XMFLOAT3 position,
                       DirectX::XMFLOAT2 size,
                       DirectX::XMFLOAT4 color,
                       ID3D11ShaderResourceView* texture,
                       float rotation,
                       bool groundAligned) {
    Billboard::Add(position, size, color, texture, rotation, groundAligned);
}

void Host_BillboardAddUv(DirectX::XMFLOAT3 position,
                         DirectX::XMFLOAT2 size,
                         DirectX::XMFLOAT4 color,
                         ID3D11ShaderResourceView* texture,
                         float rotation,
                         bool groundAligned,
                         DirectX::XMFLOAT2 uvOffset,
                         DirectX::XMFLOAT2 uvScale) {
    Billboard::Add(position, size, color, texture, rotation, groundAligned, uvOffset, uvScale);
}

std::uint32_t Host_ParticleCreateEmitter(const engine::ParticleEmitterConfig* config,
                                         DirectX::XMFLOAT3 position) {
    return config ? engine::ParticleSystem::Instance().CreateEmitter(*config, position) : 0;
}

void Host_ParticleSetEmitterPosition(std::uint32_t emitterId, DirectX::XMFLOAT3 position) {
    engine::ParticleSystem::Instance().SetEmitterPosition(emitterId, position);
}

void Host_ParticleSetEmitterEmitting(std::uint32_t emitterId, bool emitting) {
    engine::ParticleSystem::Instance().SetEmitterEmitting(emitterId, emitting);
}

void Host_ParticleStopEmitter(std::uint32_t emitterId, bool destroyWhenEmpty) {
    engine::ParticleSystem::Instance().StopEmitter(emitterId, destroyWhenEmpty);
}

void Host_ParticleDestroyEmitter(std::uint32_t emitterId) {
    engine::ParticleSystem::Instance().DestroyEmitter(emitterId);
}

void Host_ParticleSpawnAt(std::uint32_t emitterId,
                          DirectX::XMFLOAT3 position,
                          DirectX::XMFLOAT3 velocity,
                          DirectX::XMFLOAT4 color,
                          float size,
                          float lifetime) {
    engine::ParticleSystem::Instance().SpawnParticle(
        emitterId, position, velocity, color, size, lifetime);
}

void Host_DebugDrawSphereWire(DirectX::XMFLOAT3 position,
                              DirectX::XMFLOAT3 center,
                              float radius,
                              DirectX::XMFLOAT3 rotation,
                              DirectX::XMFLOAT4 color) {
    DebugDrawer::QueueSphere(position, center, radius, rotation, color);
}

void Host_DebugDrawLine(DirectX::XMFLOAT3 start,
                        DirectX::XMFLOAT3 end,
                        DirectX::XMFLOAT4 color) {
    DebugDrawer::QueueLine(start, end, color);
}

float Host_GetRenderViewportWidth() {
    return GetRenderViewportWidth();
}

float Host_GetRenderViewportHeight() {
    return GetRenderViewportHeight();
}

void Host_GetGameViewportRect(float* x, float* y, float* width, float* height) {
    float viewportX = 0.0f;
    float viewportY = 0.0f;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    EngineGUI_GetGameViewportRect(viewportX, viewportY, viewportWidth, viewportHeight);
    if (x) *x = viewportX;
    if (y) *y = viewportY;
    if (width) *width = viewportWidth;
    if (height) *height = viewportHeight;
}

ID3D11Device* Host_GetD3DDevice() {
    return GetDevice();
}

ID3D11DeviceContext* Host_GetD3DDeviceContext() {
    return GetDeviceContext();
}

void Host_GetCameraViewMatrix(DirectX::XMFLOAT4X4* out) {
    if (!out) return;
    XMStoreFloat4x4(out, GetCamera().GetViewMatrix());
}

void Host_GetCameraProjMatrix(float width, float height, DirectX::XMFLOAT4X4* out) {
    if (!out) return;
    XMStoreFloat4x4(out, GetCamera().GetProjMatrix(width, height));
}

void Host_ColliderComponentApplyToOwner(ColliderComponent* component) {
    if (component)
        component->ApplyToOwner();
}

BoxCollider* Host_GameObjectCreateBoxCollider(GameObject* object,
                                              DirectX::XMFLOAT3 size,
                                              DirectX::XMFLOAT3 center) {
    if (!object)
        return nullptr;
    BoxCollider* box = object->CreateBoxCollider(size, center);
    object->m_colliderOwnedByHost = true;
    return box;
}

void Host_GameObjectDestroyCollider(GameObject* object) {
    if (!object)
        return;

    delete object->m_collider;
    object->m_collider = nullptr;
    object->m_colliderOwnedByHost = false;
}

void Host_GameObjectAddIgnoreCollisionTag(GameObject* object, Tag tag) {
    if (!object)
        return;
    object->AddIgnoreCollisionTag(tag);
}

void Host_GameObjectRemoveIgnoreCollisionTag(GameObject* object, Tag tag) {
    if (!object)
        return;
    object->RemoveIgnoreCollisionTag(tag);
}

void Host_GameObjectAddIgnoreCollisionObject(GameObject* object, GameObject* other) {
    if (!object || !other)
        return;
    object->AddIgnoreCollisionObject(other);
}

void Host_GameObjectRemoveIgnoreCollisionObject(GameObject* object, GameObject* other) {
    if (!object || !other)
        return;
    object->RemoveIgnoreCollisionObject(other);
}

void Host_GameObjectClearIgnoreCollisionObjects(GameObject* object) {
    if (!object)
        return;
    object->ClearIgnoreCollisionObjects();
}

DirectX::XMFLOAT3 Host_ColliderGetWorldCenter(const Collider* collider) {
    return collider ? collider->GetWorldCenter() : DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f};
}

DirectX::XMFLOAT3 Host_BoxColliderGetSize(const BoxCollider* collider) {
    return collider ? collider->GetSize() : DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f};
}

void Host_BoxColliderSetSize(BoxCollider* collider, DirectX::XMFLOAT3 size) {
    if (collider)
        collider->SetSize(size);
}

float Host_SphereColliderGetRadius(const SphereCollider* collider) {
    return collider ? collider->GetRadius() : 0.0f;
}

bool Host_ColliderTryGetFootprint(const Collider* collider,
                                  DirectX::XMFLOAT3* outCenter,
                                  DirectX::XMFLOAT3* outSize) {
    if (!collider || !outCenter || !outSize)
        return false;

    if (const auto* box = dynamic_cast<const BoxCollider*>(collider)) {
        *outCenter = box->GetWorldCenter();
        *outSize = box->GetSize();
        return true;
    }
    if (const auto* mesh = dynamic_cast<const MeshCollider*>(collider)) {
        mesh->GetWorldAABB(*outCenter, *outSize);
        return true;
    }
    if (const auto* cylinder = dynamic_cast<const CylinderCollider*>(collider)) {
        *outCenter = cylinder->GetWorldCenter();
        const float diameter = cylinder->GetRadius() * 2.0f;
        *outSize = { diameter, cylinder->GetHeight(), diameter };
        return true;
    }
    return false;
}

void Host_SceneCanvasRequestSceneFile(const char* sceneFile, bool playOnLoad) {
    if (sceneFile && sceneFile[0] != '\0')
        SceneCanvas_RequestSceneFile(sceneFile, playOnLoad);
}

void Host_PickupDebugSetMessage(const char* message, float displaySeconds) {
    (void)message;
    (void)displaySeconds;
}

GameObject* Host_PrefabInstantiate(const char* prefabPath,
                                   DirectX::XMFLOAT3 position,
                                   DirectX::XMFLOAT3 rotation,
                                   GameObject* parent,
                                   bool startActive) {
    return PrefabRuntime_Instantiate(prefabPath, position, rotation, parent, startActive);
}

void Host_PrefabDestroy(GameObject* obj) {
    PrefabRuntime_Destroy(obj);
}

void Host_PrefabSetBeforeDestroyCallback(void (*callback)(GameObject* obj)) {
    PrefabRuntime_SetBeforeDestroyCallback(callback);
}

void Host_VisualPlayClip(VisualComponent* component, const char* clipName) {
    if (component && clipName)
        component->PlayClip(clipName, true);
}

void Host_VisualPlayClipLoop(VisualComponent* component, const char* clipName, int loop) {
    if (component && clipName)
        component->PlayClip(clipName, loop != 0);
}

float Host_VisualGetClipDuration(VisualComponent* component) {
    return component ? component->GetActiveClipDuration() : 0.0f;
}

void Host_VisualSetAnimSpeed(VisualComponent* component, float speed) {
    if (component)
        component->SetAnimationSpeed(speed);
}

// Live owner transform / bone world. Safe for the calling object's own pose.
// Reading another object's live GameObject transform during a parallel Update
// batch is unsafe - host code should use FrameTransformSnapshot_Find instead.
// No by-name neighbor transform API exists; do not invent a new script surface.
bool Host_VisualTryGetBoneWorld(VisualComponent* component,
                                const char* boneName,
                                DirectX::XMFLOAT3* outPos,
                                DirectX::XMFLOAT3* outRotDeg) {
    if (!component || !boneName || !*boneName || !outPos || !outRotDeg)
        return false;

    Model3D* model = component->GetRealModel();
    if (!model)
        return false;

    DirectX::XMMATRIX boneLocal;
    if (!model->TryGetBoneLocalMatrix(boneName, boneLocal))
        return false;

    DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
    if (GameObject* owner = component->owner) {
        if (engine::Transform* t = owner->GetTransform())
            world = t->GetWorldMatrix();
        else {
            const DirectX::XMFLOAT3 p = owner->GetPosition();
            const DirectX::XMFLOAT3 r = owner->GetRotation();
            const DirectX::XMFLOAT3 s = owner->GetScale();
            const DirectX::XMMATRIX S = DirectX::XMMatrixScaling(s.x, s.y, s.z);
            const DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(
                DirectX::XMConvertToRadians(r.x),
                DirectX::XMConvertToRadians(r.y),
                DirectX::XMConvertToRadians(r.z));
            const DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(p.x, p.y, p.z);
            world = S * R * T;
        }
    }

    const DirectX::XMMATRIX boneWorld = boneLocal * world;
    DirectX::XMVECTOR scaleV, rotQuat, translation;
    if (!DirectX::XMMatrixDecompose(&scaleV, &rotQuat, &translation, boneWorld))
        return false;

    DirectX::XMStoreFloat3(outPos, translation);

    const DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationQuaternion(rotQuat);
    const float r00 = rotMat.r[0].m128_f32[0];
    const float r10 = rotMat.r[1].m128_f32[0];
    const float r20 = rotMat.r[2].m128_f32[0];
    const float r21 = rotMat.r[2].m128_f32[1];
    const float r22 = rotMat.r[2].m128_f32[2];
    const float r11 = rotMat.r[1].m128_f32[1];
    const float r12 = rotMat.r[1].m128_f32[2];
    const float sy = sqrtf(r00 * r00 + r10 * r10);
    float rotX, rotY, rotZ;
    if (sy > 1e-6f) {
        rotX = atan2f(r21, r22);
        rotY = atan2f(-r20, sy);
        rotZ = atan2f(r10, r00);
    } else {
        rotX = atan2f(-r12, r11);
        rotY = atan2f(-r20, sy);
        rotZ = 0.0f;
    }
    *outRotDeg = {
        DirectX::XMConvertToDegrees(rotX),
        DirectX::XMConvertToDegrees(rotY),
        DirectX::XMConvertToDegrees(rotZ)
    };
    return true;
}

void Host_EffectPlay(EffectComponent* component) {
    if (component)
        component->Play();
}

void Host_AudioPlay(AudioComponent* component) {
    if (component)
        component->Play();
}

void Host_AudioStop(AudioComponent* component) {
    if (component)
        component->Stop();
}

void Host_AudioSetMasterVolume(float volume) {
    AudioManager::Instance().SetMasterVolume(volume);
}

float Host_AudioGetMasterVolume() {
    return AudioManager::Instance().GetMasterVolume();
}

void Host_MouseGetPosition(int* x, int* y) {
    int px = 0;
    int py = 0;
    Mouse_GetPosition(px, py);
    if (x)
        *x = px;
    if (y)
        *y = py;
}

void Host_PostFxBurst(float rgbShift, float durationSeconds) {
    PostProcess_TriggerBurst(rgbShift, durationSeconds);
}

bool Host_PhysicsRaycast(const DirectX::XMFLOAT3* origin, const DirectX::XMFLOAT3* dir,
    float maxDistance, DirectX::XMFLOAT3* outPoint, DirectX::XMFLOAT3* outNormal,
    float* outDistance) {
    if (!origin || !dir) return false;
    RayHit hit{};
    if (!PhysicsSystem_Raycast(*origin, *dir, maxDistance, hit, false))
        return false;
    if (outPoint) *outPoint = hit.point;
    if (outNormal) *outNormal = hit.normal;
    if (outDistance) *outDistance = hit.distance;
    return true;
}

void Host_CameraAddTrauma(float amount01) {
    ThirdPersonCamera_AddTrauma(amount01);
}

void Host_CameraSetSpeedFactor(float speed01) {
    ThirdPersonCamera_SetSpeedFactor(speed01);
}

void Host_DebugPulseTimeScale(float scale, unsigned durationMs) {
    DebugSystem::PulseTimeScale(scale, durationMs);
}

void Host_LineSpriteDraw(const DirectX::XMFLOAT3* a, const DirectX::XMFLOAT3* b,
    float thicknessWorld, const DirectX::XMFLOAT4* color) {
    if (!a || !b || !color)
        return;
    LineSprite_Draw(*a, *b, thicknessWorld, *color);
}

void Host_ThirdPersonCameraSetMagnetMode(int mode) {
    ThirdPersonCamera_SetMagnetMode(static_cast<ThirdPersonMagnetMode>(mode));
}

void Host_ThirdPersonCameraSetMagnetTarget(DirectX::XMFLOAT3 target) {
    ThirdPersonCamera_SetMagnetTarget(target);
}

void Host_CameraSetPosition(DirectX::XMFLOAT3 position) {
    CameraDirector_SetIntroPosition(position);
}

void Host_CameraSetTarget(DirectX::XMFLOAT3 target) {
    CameraDirector_SetIntroTarget(target);
}

void Host_CameraSetUp(DirectX::XMFLOAT3 up) {
    CameraDirector_SetIntroUp(up);
}

void Host_EngineGUI_SetActiveCameraNodeName(const char* name) {
    EngineGUI_SetActiveCameraNodeName(name ? name : "");
}

int Host_GetBackBufferWidth() {
    return static_cast<int>(GetBackBufferWidth());
}

int Host_GetBackBufferHeight() {
    return static_cast<int>(GetBackBufferHeight());
}

void Host_SpriteDraw(int texId,
                     float x,
                     float y,
                     float width,
                     float height,
                     int tx,
                     int ty,
                     int tw,
                     int th,
                     float angle,
                     DirectX::XMFLOAT4 color) {
    Sprite_Draw(texId, x, y, width, height, tx, ty, tw, th, angle, color);
}

int Host_TextureGetWhiteTexture() {
    return TextureGetWhiteTexture();
}

unsigned int Host_TextureGetWidth(int textureId) {
    return TextureGetWidth(textureId);
}

unsigned int Host_TextureGetHeight(int textureId) {
    return TextureGetHeight(textureId);
}

void Host_TextureRelease(int textureId) {
    TextureSetTexture(textureId);
}

bool Host_EngineGUILoadFromFile(const char* filename) {
    return filename ? EngineGUI_LoadFromFile(filename) : false;
}

void Host_EngineGUIStartPlay() {
    EngineGUI_StartPlay();
}

bool Host_RenderSettingSetBool(const char* key, bool value) {
    return RenderSettings_SetBoolByName(key, value);
}

bool Host_RenderSettingGetBool(const char* key, bool* outValue) {
    return RenderSettings_GetBoolByName(key, outValue);
}

bool Host_RenderSettingSetNumber(const char* key, double value) {
    return RenderSettings_SetNumberByName(key, value);
}

bool Host_RenderSettingGetNumber(const char* key, double* outValue) {
    return RenderSettings_GetNumberByName(key, outValue);
}

// --- v50 ---------------------------------------------------------------------
Rigidbody* HostBody(GameObject* object) {
    RigidbodyComponent* rc = object ? object->GetComponent<RigidbodyComponent>() : nullptr;
    return rc ? &rc->Body() : nullptr;
}

bool IsFinite3(const DirectX::XMFLOAT3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool ForceModeFromInt(int mode, ForceMode* out) {
    if (mode < static_cast<int>(ForceMode::Force) || mode > static_cast<int>(ForceMode::VelocityChange))
        return false;
    *out = static_cast<ForceMode>(mode);
    return true;
}

bool Host_RigidbodyGetVelocity(GameObject* object, DirectX::XMFLOAT3* outVelocity) {
    Rigidbody* body = HostBody(object);
    if (!body || !outVelocity) return false;
    *outVelocity = body->GetVelocity();
    return true;
}

bool Host_RigidbodySetVelocity(GameObject* object, DirectX::XMFLOAT3 velocity) {
    Rigidbody* body = HostBody(object);
    if (!body || !IsFinite3(velocity)) return false;
    body->SetVelocity(velocity);
    return true;
}

bool Host_RigidbodyGetAngularVelocity(GameObject* object, DirectX::XMFLOAT3* outVelocity) {
    Rigidbody* body = HostBody(object);
    if (!body || !outVelocity) return false;
    *outVelocity = body->GetAngularVelocity();
    return true;
}

bool Host_RigidbodySetAngularVelocity(GameObject* object, DirectX::XMFLOAT3 velocity) {
    Rigidbody* body = HostBody(object);
    if (!body || !IsFinite3(velocity)) return false;
    body->SetAngularVelocity(velocity);
    return true;
}

bool Host_RigidbodyAddForce(GameObject* object, DirectX::XMFLOAT3 force, int mode) {
    Rigidbody* body = HostBody(object);
    ForceMode fm{};
    if (!body || !IsFinite3(force) || !ForceModeFromInt(mode, &fm)) return false;
    body->AddForce(force, fm);
    return true;
}

bool Host_RigidbodyAddTorque(GameObject* object, DirectX::XMFLOAT3 torque, int mode) {
    Rigidbody* body = HostBody(object);
    ForceMode fm{};
    if (!body || !IsFinite3(torque) || !ForceModeFromInt(mode, &fm)) return false;
    body->AddTorque(torque, fm);
    return true;
}

// Shared walk for the find functions: skips objects waiting to be destroyed
// and, without includeInactive, inactive subtrees.
template <class Fn>
void HostWalkScene(bool includeInactive, Fn onObject) {
    using game_object_search::Visit;
    game_object_search::WalkDepthFirst<GameObject>(
        SceneLoader_GetSceneNodes(),
        [](GameObject& node) -> const std::vector<std::unique_ptr<GameObject>>& {
            return node.GetChildren();
        },
        [&](GameObject& node) {
            if (PrefabRuntime_IsPendingDestroy(&node)) return Visit::SkipChildren;
            if (!includeInactive && !node.IsActive()) return Visit::SkipChildren;
            return onObject(node) ? Visit::Continue : Visit::Stop;
        });
}

GameObject* Host_FindGameObjectByName(const char* name, bool includeInactive) {
    if (!name || !*name) return nullptr;
    GameObject* found = nullptr;
    HostWalkScene(includeInactive, [&](GameObject& node) {
        if (node.GetName() == name) {
            found = &node;
            return false;
        }
        return true;
    });
    return found;
}

int Host_FindGameObjectsByTag(Tag tag, GameObject** out, int capacity, bool includeInactive) {
    int total = 0;
    HostWalkScene(includeInactive, [&](GameObject& node) {
        if (node.GetTag() == tag) {
            if (out && total < capacity)
                out[total] = &node;
            ++total;
        }
        return true;
    });
    return total;
}

float Host_GetTimeScale() {
    return DebugSystem::GetTimeScale();
}

double Host_GetRealTimeSeconds() {
    return SystemTimer_GetTime();
}

// --- v51 save data -------------------------------------------------------------
// Null slot / key pointers fail like invalid names.
const char* Str(const char* s) { return s ? s : ""; }

using host_text::CopyOut;

template <class Fn>
auto WithSaves(Fn fn) {
    std::lock_guard<std::mutex> lock(SaveSystem_Mutex());
    return fn(SaveSystem_Store());
}

bool Host_SaveSetInt(const char* slot, const char* key, long long value) {
    return WithSaves([&](save_store::Store& s) { return s.SetInt(Str(slot), Str(key), value); });
}
bool Host_SaveGetInt(const char* slot, const char* key, long long* outValue) {
    return WithSaves([&](save_store::Store& s) {
        std::int64_t v = 0;
        if (!outValue || !s.GetInt(Str(slot), Str(key), &v)) return false;
        *outValue = static_cast<long long>(v);
        return true;
    });
}
bool Host_SaveSetFloat(const char* slot, const char* key, double value) {
    if (!std::isfinite(value)) return false;
    return WithSaves([&](save_store::Store& s) { return s.SetDouble(Str(slot), Str(key), value); });
}
bool Host_SaveGetFloat(const char* slot, const char* key, double* outValue) {
    return WithSaves([&](save_store::Store& s) { return outValue && s.GetDouble(Str(slot), Str(key), outValue); });
}
bool Host_SaveSetBool(const char* slot, const char* key, bool value) {
    return WithSaves([&](save_store::Store& s) { return s.SetBool(Str(slot), Str(key), value); });
}
bool Host_SaveGetBool(const char* slot, const char* key, bool* outValue) {
    return WithSaves([&](save_store::Store& s) { return outValue && s.GetBool(Str(slot), Str(key), outValue); });
}
bool Host_SaveSetString(const char* slot, const char* key, const char* utf8Value) {
    if (!utf8Value) return false;
    return WithSaves([&](save_store::Store& s) { return s.SetString(Str(slot), Str(key), utf8Value); });
}
int Host_SaveGetString(const char* slot, const char* key, char* out, int capacity) {
    return WithSaves([&](save_store::Store& s) {
        std::string v;
        return s.GetString(Str(slot), Str(key), &v) ? CopyOut(v, out, capacity) : -1;
    });
}
bool Host_SaveSetJson(const char* slot, const char* key, const char* jsonText) {
    if (!jsonText) return false;
    return WithSaves([&](save_store::Store& s) { return s.SetJsonText(Str(slot), Str(key), jsonText); });
}
int Host_SaveGetJson(const char* slot, const char* key, char* out, int capacity) {
    return WithSaves([&](save_store::Store& s) {
        std::string v;
        return s.GetJsonText(Str(slot), Str(key), &v) ? CopyOut(v, out, capacity) : -1;
    });
}
bool Host_SaveHasKey(const char* slot, const char* key) {
    return WithSaves([&](save_store::Store& s) { return s.HasKey(Str(slot), Str(key)); });
}
bool Host_SaveDeleteKey(const char* slot, const char* key) {
    return WithSaves([&](save_store::Store& s) { return s.DeleteKey(Str(slot), Str(key)); });
}
bool Host_SaveCommit(const char* slot) {
    return WithSaves([&](save_store::Store& s) { return s.Commit(Str(slot)); });
}
bool Host_SaveSlotExists(const char* slot) {
    return WithSaves([&](save_store::Store& s) { return s.SlotExists(Str(slot)); });
}
bool Host_SaveDeleteSlot(const char* slot) {
    return WithSaves([&](save_store::Store& s) { return s.DeleteSlot(Str(slot)); });
}
int Host_SaveListSlots(char* out, int capacity) {
    return WithSaves([&](save_store::Store& s) {
        return CopyOut(host_text::JoinLines(s.ListSlots()), out, capacity);
    });
}

const ScriptRuntimeApi g_scriptRuntimeApi = {
    kScriptRuntimeApiVersion,
    &Host_GetCurrentScene,
    &Keyboard_GetState,
    &KeyLogger_IsTrigger,
    &Mouse_IsLeftDown,
    &Mouse_IsMiddleDown,
    &Mouse_IsRightDown,
    &Mouse_GetWheelDelta,
    &Controller_GetLeftStick,
    &Controller_GetRightStick,
    &Controller_GetLeftTrigger,
    &Controller_GetRightTrigger,
    &Controller_IsPressed,
    &ThirdPersonCamera_Initialize,
    &ThirdPersonCamera_SetTarget,
    &ThirdPersonCamera_SetMouseSensitivity,
    &ThirdPersonCamera_SetControllerSensitivity,
    &ThirdPersonCamera_SetPitchLimits,
    &ThirdPersonCamera_ShowCursor,
    &ThirdPersonCamera_IsCursorVisible,
    &Host_GetCameraForward,
    &EngineGUI_IsPlaying,
    &EngineGUI_StopPlay,
    &EngineGUI_SetCameraMode,
    &Host_EngineGUI_SetActiveCameraNodeName,
    &Host_CameraSetPosition,
    &Host_CameraSetTarget,
    &Host_CameraSetUp,
    &Host_GetBackBufferWidth,
    &Host_GetBackBufferHeight,
    &Host_SpriteDraw,
    &Host_TextureGetWhiteTexture,
    &Host_TextureGetWidth,
    &Host_TextureGetHeight,
    &Host_TextureRelease,
    &Host_EngineGUILoadFromFile,
    &Host_EngineGUIStartPlay,
    &SceneCanvas_GetCanvas,
    &SceneCanvas_ShowRedOverlay,
    &SceneCanvas_HideRedOverlay,
    &SceneCanvas_SetRedOverlayAlpha,
    &SceneCanvas_SetTimerSeconds,
    &Host_SceneCanvasRequestSceneFile,
    &Host_UICanvasGetElement,
    &Host_UITextSetText,
    &Host_UIElementSetVisible,
    &Host_UICanvasAddSprite,
    &Host_UICanvasRemoveElement,
    &Host_UISpriteSetTexture,
    &Host_UICanvasGetSprite,
    &Host_UISpriteSetRadial360Fill,
    &Host_UISpriteSetFillNone,
    &TextureLoad,
    &TextureGetSRV,
    &Host_BillboardAdd,
    &Host_ParticleCreateEmitter,
    &Host_ParticleSetEmitterPosition,
    &Host_ParticleSetEmitterEmitting,
    &Host_ParticleStopEmitter,
    &Host_ParticleDestroyEmitter,
    &Host_ParticleSpawnAt,
    &Host_GetRenderViewportWidth,
    &Host_GetRenderViewportHeight,
    &Host_GetD3DDevice,
    &Host_GetD3DDeviceContext,
    &Host_GetCameraViewMatrix,
    &Host_GetCameraProjMatrix,
    &Host_DebugDrawSphereWire,
    &Host_ColliderComponentApplyToOwner,
    &Host_GameObjectCreateBoxCollider,
    &Host_GameObjectDestroyCollider,
    &Host_GameObjectAddIgnoreCollisionTag,
    &Host_ColliderGetWorldCenter,
    &Host_BoxColliderGetSize,
    &Host_BoxColliderSetSize,
    &Host_SphereColliderGetRadius,
    &Host_ColliderTryGetFootprint,
    &Host_PickupDebugSetMessage,
    &Host_PrefabInstantiate,
    &Host_PrefabDestroy,
    &Host_PrefabSetBeforeDestroyCallback,
    &Host_VisualPlayClip,
    &Host_EffectPlay,
    &Host_UISpriteSetSourceRect,
    &Host_ThirdPersonCameraSetMagnetMode,
    &Host_ThirdPersonCameraSetMagnetTarget,
    &Host_BillboardAddUv,
    &Host_GameObjectRemoveIgnoreCollisionTag,
    &Host_AudioPlay,
    &Host_AudioStop,
    &Host_VisualTryGetBoneWorld,
    &Host_VisualPlayClipLoop,
    &Host_VisualGetClipDuration,
    &Host_VisualSetAnimSpeed,
    &ThirdPersonCamera_SetDistance,
    &ThirdPersonCamera_SetHeight,
    &ThirdPersonCamera_SetLookAtHeight,
    &Host_GameObjectAddIgnoreCollisionObject,
    &Host_GameObjectRemoveIgnoreCollisionObject,
    &Host_GameObjectClearIgnoreCollisionObjects,
    &Host_AudioSetMasterVolume,
    &Host_AudioGetMasterVolume,
    &Host_MouseGetPosition,
    &Host_PostFxBurst,
    &Host_PhysicsRaycast,
    &Host_CameraAddTrauma,
    &Host_CameraSetSpeedFactor,
    &Host_DebugPulseTimeScale,
    &Host_LineSpriteDraw,
    &FirstPersonCamera_Initialize,
    &FirstPersonCamera_SetTarget,
    &FirstPersonCamera_SetEyeHeight,
    &FirstPersonCamera_SetMouseSensitivity,
    &FirstPersonCamera_SetControllerSensitivity,
    &FirstPersonCamera_SetPitchLimits,
    &FirstPersonCamera_ShowCursor,
    &FirstPersonCamera_IsCursorVisible,
    &Host_SpritePlay,
    &Host_SpritePause,
    &Host_SpriteStop,
    &Host_SpriteSetFlip,
    &Host_SpriteSetTint,
    &Host_Physics2DSetVelocity,
    &Host_Physics2DGetVelocity,
    &Host_Physics2DMovePosition,
    &Host_Physics2DRaycast,
    &Host_GetGameViewportRect,
    &Host_DebugDrawLine,
    &DebugSystem::CollisionVizEnabled,
    &EngineGUI_RequestExitGame,
    &Host_RenderSettingSetBool,
    &Host_RenderSettingGetBool,
    &Host_RenderSettingSetNumber,
    &Host_RenderSettingGetNumber,
    &Host_RigidbodyGetVelocity,
    &Host_RigidbodySetVelocity,
    &Host_RigidbodyGetAngularVelocity,
    &Host_RigidbodySetAngularVelocity,
    &Host_RigidbodyAddForce,
    &Host_RigidbodyAddTorque,
    &Host_FindGameObjectByName,
    &Host_FindGameObjectsByTag,
    &Host_GetTimeScale,
    &Host_GetRealTimeSeconds,
    &Host_SaveSetInt,
    &Host_SaveGetInt,
    &Host_SaveSetFloat,
    &Host_SaveGetFloat,
    &Host_SaveSetBool,
    &Host_SaveGetBool,
    &Host_SaveSetString,
    &Host_SaveGetString,
    &Host_SaveSetJson,
    &Host_SaveGetJson,
    &Host_SaveHasKey,
    &Host_SaveDeleteKey,
    &Host_SaveCommit,
    &Host_SaveSlotExists,
    &Host_SaveDeleteSlot,
    &Host_SaveListSlots,
};
#else
using GetApiFn = const ScriptRuntimeApi* (*)();

const ScriptRuntimeApi* ResolveScriptRuntimeApi() {
    HMODULE module = GetModuleHandleA(nullptr);
    if (!module)
        return nullptr;

    auto getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV51"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV50"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV49"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV48"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV47"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV46"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV45"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV44"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_GetV43"));
    if (!getApi)
        getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "ScriptRuntimeApi_Get"));
    if (!getApi)
        return nullptr;

    const ScriptRuntimeApi* api = getApi();
    // v42 provides the stable prefix; sprite wrappers gate access to the v43 tail.
    return (api && api->version >= 42) ? api : nullptr;
}
#endif
} // namespace

#if defined(SCRIPT_RUNTIME_API_HOST)
extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_Get() {
    // Existing v42 DLLs require an exact version match. Keep their export stable.
    static const ScriptRuntimeApi legacyApi = [] {
        ScriptRuntimeApi api = g_scriptRuntimeApi;
        api.version = 42;
        return api;
    }();
    return &legacyApi;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV43() {
    static const ScriptRuntimeApi api = [] {
        ScriptRuntimeApi copy = g_scriptRuntimeApi;
        copy.version = 43;
        return copy;
    }();
    return &api;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV44() {
    static const ScriptRuntimeApi api = [] {
        ScriptRuntimeApi copy = g_scriptRuntimeApi;
        copy.version = 44;
        return copy;
    }();
    return &api;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV45() {
    static const ScriptRuntimeApi api = [] {
        ScriptRuntimeApi copy = g_scriptRuntimeApi;
        copy.version = 45;
        return copy;
    }();
    return &api;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV46() {
    static const ScriptRuntimeApi api = [] {
        ScriptRuntimeApi copy = g_scriptRuntimeApi;
        copy.version = 46;
        return copy;
    }();
    return &api;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV47() {
    static const ScriptRuntimeApi api = [] {
        ScriptRuntimeApi copy = g_scriptRuntimeApi;
        copy.version = 47;
        return copy;
    }();
    return &api;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV48() {
    static const ScriptRuntimeApi api = [] {
        ScriptRuntimeApi copy = g_scriptRuntimeApi;
        copy.version = 48;
        return copy;
    }();
    return &api;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV49() {
    static const ScriptRuntimeApi api = [] {
        ScriptRuntimeApi copy = g_scriptRuntimeApi;
        copy.version = 49;
        return copy;
    }();
    return &api;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV50() {
    static const ScriptRuntimeApi api = [] {
        ScriptRuntimeApi copy = g_scriptRuntimeApi;
        copy.version = 50;
        return copy;
    }();
    return &api;
}

extern "C" __declspec(dllexport)
const ScriptRuntimeApi* ScriptRuntimeApi_GetV51() {
    return &g_scriptRuntimeApi;
}

const ScriptRuntimeApi* ScriptRuntimeApi_GetHost() {
    return &g_scriptRuntimeApi;
}
#else
const ScriptRuntimeApi* ScriptRuntimeApi_GetHost() {
    static const ScriptRuntimeApi* s_api = ResolveScriptRuntimeApi();
    return s_api;
}

const Keyboard_State* Keyboard_GetState(void) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->keyboardGetState ? api->keyboardGetState() : nullptr;
}

bool KeyLogger_IsTrigger(Keyboard_Keys key) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->keyLoggerIsTrigger ? api->keyLoggerIsTrigger(key) : false;
}

bool Mouse_IsLeftDown() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->mouseIsLeftDown ? api->mouseIsLeftDown() : false;
}

bool Mouse_IsMiddleDown() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->mouseIsMiddleDown ? api->mouseIsMiddleDown() : false;
}

bool Mouse_IsRightDown() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->mouseIsRightDown ? api->mouseIsRightDown() : false;
}

int Mouse_GetWheelDelta() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->mouseGetWheelDelta ? api->mouseGetWheelDelta() : 0;
}

DirectX::XMFLOAT2 Controller_GetLeftStick(int padIndex) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->controllerGetLeftStick ? api->controllerGetLeftStick(padIndex) : DirectX::XMFLOAT2{0.0f, 0.0f};
}

DirectX::XMFLOAT2 Controller_GetRightStick(int padIndex) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->controllerGetRightStick ? api->controllerGetRightStick(padIndex) : DirectX::XMFLOAT2{0.0f, 0.0f};
}

float Controller_GetLeftTrigger(int padIndex) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->controllerGetLeftTrigger ? api->controllerGetLeftTrigger(padIndex) : 0.0f;
}

float Controller_GetRightTrigger(int padIndex) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->controllerGetRightTrigger ? api->controllerGetRightTrigger(padIndex) : 0.0f;
}

bool Controller_IsPressed(ControllerButton button, int padIndex) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->controllerIsPressed ? api->controllerIsPressed(button, padIndex) : false;
}

void ThirdPersonCamera_Initialize() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraInitialize)
        api->thirdPersonCameraInitialize();
}

void ThirdPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetTarget)
        api->thirdPersonCameraSetTarget(positionPtr, yawPtr);
}

void ThirdPersonCamera_SetMouseSensitivity(float sensitivity) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetMouseSensitivity)
        api->thirdPersonCameraSetMouseSensitivity(sensitivity);
}

void ThirdPersonCamera_SetControllerSensitivity(float sensitivity) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetControllerSensitivity)
        api->thirdPersonCameraSetControllerSensitivity(sensitivity);
}

void ThirdPersonCamera_SetPitchLimits(float minPitch, float maxPitch) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetPitchLimits)
        api->thirdPersonCameraSetPitchLimits(minPitch, maxPitch);
}

void ThirdPersonCamera_ShowCursor(bool show) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraShowCursor)
        api->thirdPersonCameraShowCursor(show);
}

bool ThirdPersonCamera_IsCursorVisible() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->thirdPersonCameraIsCursorVisible ? api->thirdPersonCameraIsCursorVisible() : true;
}

void ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode mode) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetMagnetMode)
        api->thirdPersonCameraSetMagnetMode(static_cast<int>(mode));
}

void ThirdPersonCamera_SetMagnetTarget(const DirectX::XMFLOAT3& targetPos) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetMagnetTarget)
        api->thirdPersonCameraSetMagnetTarget(targetPos);
}

void ThirdPersonCamera_SetDistance(float distance) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetDistance)
        api->thirdPersonCameraSetDistance(distance);
}

void ThirdPersonCamera_SetHeight(float height) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetHeight)
        api->thirdPersonCameraSetHeight(height);
}

void ThirdPersonCamera_SetLookAtHeight(float height) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->thirdPersonCameraSetLookAtHeight)
        api->thirdPersonCameraSetLookAtHeight(height);
}

void FirstPersonCamera_Initialize() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->firstPersonCameraInitialize)
        api->firstPersonCameraInitialize();
}

void FirstPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->firstPersonCameraSetTarget)
        api->firstPersonCameraSetTarget(positionPtr, yawPtr);
}

void FirstPersonCamera_SetEyeHeight(float eyeHeight) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->firstPersonCameraSetEyeHeight)
        api->firstPersonCameraSetEyeHeight(eyeHeight);
}

void FirstPersonCamera_SetMouseSensitivity(float sensitivity) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->firstPersonCameraSetMouseSensitivity)
        api->firstPersonCameraSetMouseSensitivity(sensitivity);
}

void FirstPersonCamera_SetControllerSensitivity(float sensitivity) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->firstPersonCameraSetControllerSensitivity)
        api->firstPersonCameraSetControllerSensitivity(sensitivity);
}

void FirstPersonCamera_SetPitchLimits(float minPitch, float maxPitch) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->firstPersonCameraSetPitchLimits)
        api->firstPersonCameraSetPitchLimits(minPitch, maxPitch);
}

void FirstPersonCamera_ShowCursor(bool show) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->firstPersonCameraShowCursor)
        api->firstPersonCameraShowCursor(show);
}

bool FirstPersonCamera_IsCursorVisible() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->firstPersonCameraIsCursorVisible ? api->firstPersonCameraIsCursorVisible() : true;
}

bool EngineGUI_IsPlaying() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->engineGuiIsPlaying ? api->engineGuiIsPlaying() : false;
}

void EngineGUI_SetCameraMode(CameraMode mode) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->engineGuiSetCameraMode)
        api->engineGuiSetCameraMode(mode);
}

void EngineGUI_SetActiveCameraNodeName(const std::string& name) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->engineGuiSetActiveCameraNodeName)
        api->engineGuiSetActiveCameraNodeName(name.c_str());
}

UICanvas* SceneCanvas_GetCanvas() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->sceneCanvasGetCanvas ? api->sceneCanvasGetCanvas() : nullptr;
}

void SceneCanvas_ShowRedOverlay() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->sceneCanvasShowRedOverlay)
        api->sceneCanvasShowRedOverlay();
}

void SceneCanvas_HideRedOverlay() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->sceneCanvasHideRedOverlay)
        api->sceneCanvasHideRedOverlay();
}

void SceneCanvas_SetRedOverlayAlpha(float alpha) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->sceneCanvasSetRedOverlayAlpha)
        api->sceneCanvasSetRedOverlayAlpha(alpha);
}

void SceneCanvas_RequestSceneFile(const std::string& sceneFile, bool playOnLoad) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->sceneCanvasRequestSceneFile)
        api->sceneCanvasRequestSceneFile(sceneFile.c_str(), playOnLoad);
}
#endif

SceneBase* ScriptApi_GetCurrentScene() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->getCurrentScene ? api->getCurrentScene() : nullptr;
}

engine::Transform* ScriptApi_FindTransform(engine::GameObject& object) {
    return FindComponentByTypeName<engine::Transform>(object, "SceneTransform");
}

const engine::Transform* ScriptApi_FindTransform(const engine::GameObject& object) {
    return FindComponentByTypeName<engine::Transform>(object, "SceneTransform");
}

ColliderComponent* ScriptApi_FindColliderComponent(engine::GameObject& object) {
    return FindColliderComponent(&object);
}

EffectComponent* ScriptApi_FindEffectComponent(engine::GameObject& object) {
    return FindComponentByTypeName<EffectComponent>(object, "Effect");
}

VisualComponent* ScriptApi_FindVisualComponent(engine::GameObject& object) {
    return FindComponentByTypeName<VisualComponent>(object, "visual");
}

MaterialComponent* ScriptApi_FindMaterialComponent(engine::GameObject& object) {
    return FindComponentByTypeName<MaterialComponent>(object, "Material");
}

void ScriptApi_Visual_PlayClip(VisualComponent* component, const char* clipName) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->visualPlayClip)
        api->visualPlayClip(component, clipName);
}

bool ScriptApi_Visual_TryGetBoneWorld(VisualComponent* component,
                                      const char* boneName,
                                      DirectX::XMFLOAT3& outPos,
                                      DirectX::XMFLOAT3& outRotDeg) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (!api || !api->visualTryGetBoneWorld)
        return false;
    return api->visualTryGetBoneWorld(component, boneName, &outPos, &outRotDeg);
}

void ScriptApi_Visual_PlayClipLoop(VisualComponent* component, const char* clipName, bool loop) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->visualPlayClipLoop)
        api->visualPlayClipLoop(component, clipName, loop ? 1 : 0);
    else
        ScriptApi_Visual_PlayClip(component, clipName);
}

float ScriptApi_Visual_GetClipDuration(VisualComponent* component) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->visualGetClipDuration)
        return api->visualGetClipDuration(component);
    return 0.0f;
}

void ScriptApi_Visual_SetAnimSpeed(VisualComponent* component, float speed) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->visualSetAnimSpeed)
        api->visualSetAnimSpeed(component, speed);
}

void ScriptApi_Effect_Play(EffectComponent* component) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->effectPlay)
        api->effectPlay(component);
}

AudioComponent* ScriptApi_FindAudioComponent(engine::GameObject& object) {
    return FindComponentByTypeName<AudioComponent>(object, "Audio");
}

void ScriptApi_Audio_Play(AudioComponent* component) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->audioPlay)
        api->audioPlay(component);
}

void ScriptApi_Audio_Stop(AudioComponent* component) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->audioStop)
        api->audioStop(component);
}

void ScriptApi_AudioSetMasterVolume(float volume) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->audioSetMasterVolume)
        api->audioSetMasterVolume(volume);
}

float ScriptApi_AudioGetMasterVolume() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->audioGetMasterVolume ? api->audioGetMasterVolume() : 1.0f;
}

void ScriptApi_MouseGetPosition(int& x, int& y) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->mouseGetPosition) {
        api->mouseGetPosition(&x, &y);
        return;
    }
    x = 0;
    y = 0;
}

void ScriptApi_PostFX_Burst(float rgbShift, float durationSeconds) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->postFxBurst)
        api->postFxBurst(rgbShift, durationSeconds);
}

bool ScriptApi_Physics_Raycast(const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& dir, float maxDistance,
    DirectX::XMFLOAT3* outPoint, DirectX::XMFLOAT3* outNormal,
    float* outDistance) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (!api || !api->physicsRaycast)
        return false;
    return api->physicsRaycast(&origin, &dir, maxDistance, outPoint, outNormal, outDistance);
}

void ScriptApi_Camera_AddTrauma(float amount01) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->cameraAddTrauma)
        api->cameraAddTrauma(amount01);
}

void ScriptApi_Camera_SetSpeedFactor(float speed01) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->cameraSetSpeedFactor)
        api->cameraSetSpeedFactor(speed01);
}

void ScriptApi_Debug_PulseTimeScale(float scale, unsigned durationMs) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->debugPulseTimeScale)
        api->debugPulseTimeScale(scale, durationMs);
}

void ScriptApi_LineSprite_Draw(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b,
    float thicknessWorld, const DirectX::XMFLOAT4& color) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->lineSpriteDraw)
        api->lineSpriteDraw(&a, &b, thicknessWorld, &color);
}

void ScriptApi_ApplyColliderComponent(ColliderComponent& component) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->colliderComponentApplyToOwner)
        api->colliderComponentApplyToOwner(&component);
}

BoxCollider* ScriptApi_CreateBoxCollider(engine::GameObject& object,
                                         DirectX::XMFLOAT3 size,
                                         DirectX::XMFLOAT3 center) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->gameObjectCreateBoxCollider
        ? api->gameObjectCreateBoxCollider(&object, size, center)
        : nullptr;
}

void ScriptApi_DestroyCollider(engine::GameObject& object) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->gameObjectDestroyCollider)
        api->gameObjectDestroyCollider(&object);
}

void ScriptApi_GameObjectAddIgnoreCollisionTag(engine::GameObject& object, Tag tag) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->gameObjectAddIgnoreCollisionTag)
        api->gameObjectAddIgnoreCollisionTag(&object, tag);
}

void ScriptApi_GameObjectRemoveIgnoreCollisionTag(engine::GameObject& object, Tag tag) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->gameObjectRemoveIgnoreCollisionTag)
        api->gameObjectRemoveIgnoreCollisionTag(&object, tag);
}

void ScriptApi_GameObjectAddIgnoreCollisionObject(engine::GameObject& object, engine::GameObject& other) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->gameObjectAddIgnoreCollisionObject)
        api->gameObjectAddIgnoreCollisionObject(&object, &other);
}

void ScriptApi_GameObjectRemoveIgnoreCollisionObject(engine::GameObject& object, engine::GameObject& other) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->gameObjectRemoveIgnoreCollisionObject)
        api->gameObjectRemoveIgnoreCollisionObject(&object, &other);
}

void ScriptApi_GameObjectClearIgnoreCollisionObjects(engine::GameObject& object) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->gameObjectClearIgnoreCollisionObjects)
        api->gameObjectClearIgnoreCollisionObjects(&object);
}

DirectX::XMFLOAT3 ScriptApi_GetColliderWorldCenter(const Collider& collider) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->colliderGetWorldCenter
        ? api->colliderGetWorldCenter(&collider)
        : DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f};
}

DirectX::XMFLOAT3 ScriptApi_GetBoxColliderSize(const BoxCollider& collider) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->boxColliderGetSize
        ? api->boxColliderGetSize(&collider)
        : DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f};
}

void ScriptApi_SetBoxColliderSize(BoxCollider& collider, DirectX::XMFLOAT3 size) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->boxColliderSetSize)
        api->boxColliderSetSize(&collider, size);
}

bool ScriptApi_TryGetColliderFootprint(const Collider& collider,
                                       DirectX::XMFLOAT3& outCenter,
                                       DirectX::XMFLOAT3& outSize) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->colliderTryGetFootprint
        && api->colliderTryGetFootprint(&collider, &outCenter, &outSize);
}

bool ScriptApi_RequestExitGame() {
    const auto* api = ScriptRuntimeApi_GetHost();
    if (!api || api->version < 48 || !api->requestExitGame) return false;
    api->requestExitGame();
    return true;
}

bool ScriptApi_RenderSettingSetBool(const char* key, bool value) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 49 && api->renderSettingSetBool
        && api->renderSettingSetBool(key, value);
}

bool ScriptApi_RenderSettingGetBool(const char* key, bool& outValue) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 49 && api->renderSettingGetBool
        && api->renderSettingGetBool(key, &outValue);
}

bool ScriptApi_RenderSettingSetNumber(const char* key, double value) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 49 && api->renderSettingSetNumber
        && api->renderSettingSetNumber(key, value);
}

bool ScriptApi_RenderSettingGetNumber(const char* key, double& outValue) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 49 && api->renderSettingGetNumber
        && api->renderSettingGetNumber(key, &outValue);
}

bool ScriptApi_RigidbodyGetVelocity(GameObject& object, DirectX::XMFLOAT3& outVelocity) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 50 && api->rigidbodyGetVelocity
        && api->rigidbodyGetVelocity(&object, &outVelocity);
}

bool ScriptApi_RigidbodySetVelocity(GameObject& object, DirectX::XMFLOAT3 velocity) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 50 && api->rigidbodySetVelocity
        && api->rigidbodySetVelocity(&object, velocity);
}

bool ScriptApi_RigidbodyGetAngularVelocity(GameObject& object, DirectX::XMFLOAT3& outVelocity) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 50 && api->rigidbodyGetAngularVelocity
        && api->rigidbodyGetAngularVelocity(&object, &outVelocity);
}

bool ScriptApi_RigidbodySetAngularVelocity(GameObject& object, DirectX::XMFLOAT3 velocity) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 50 && api->rigidbodySetAngularVelocity
        && api->rigidbodySetAngularVelocity(&object, velocity);
}

bool ScriptApi_RigidbodyAddForce(GameObject& object, DirectX::XMFLOAT3 force, ForceMode mode) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 50 && api->rigidbodyAddForce
        && api->rigidbodyAddForce(&object, force, static_cast<int>(mode));
}

bool ScriptApi_RigidbodyAddTorque(GameObject& object, DirectX::XMFLOAT3 torque, ForceMode mode) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 50 && api->rigidbodyAddTorque
        && api->rigidbodyAddTorque(&object, torque, static_cast<int>(mode));
}

GameObject* ScriptApi_FindGameObjectByName(const char* name, bool includeInactive) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return (api && api->version >= 50 && api->findGameObjectByName)
        ? api->findGameObjectByName(name, includeInactive) : nullptr;
}

int ScriptApi_FindGameObjectsByTag(Tag tag, GameObject** out, int capacity, bool includeInactive) {
    const auto* api = ScriptRuntimeApi_GetHost();
    if (capacity < 0) capacity = 0;
    return (api && api->version >= 50 && api->findGameObjectsByTag)
        ? api->findGameObjectsByTag(tag, out, capacity, includeInactive) : 0;
}

float ScriptApi_GetTimeScale() {
    const auto* api = ScriptRuntimeApi_GetHost();
    return (api && api->version >= 50 && api->getTimeScale) ? api->getTimeScale() : 1.0f;
}

double ScriptApi_GetRealTimeSeconds() {
    const auto* api = ScriptRuntimeApi_GetHost();
    return (api && api->version >= 50 && api->getRealTimeSeconds) ? api->getRealTimeSeconds() : 0.0;
}

namespace {
const ScriptRuntimeApi* SaveApi() {
    const auto* api = ScriptRuntimeApi_GetHost();
    return (api && api->version >= 51) ? api : nullptr;
}
using host_text::ReadText;
} // namespace

bool ScriptApi_SaveSetInt(const char* slot, const char* key, long long value) {
    const auto* api = SaveApi();
    return api && api->saveSetInt && api->saveSetInt(slot, key, value);
}
long long ScriptApi_SaveGetInt(const char* slot, const char* key, long long defaultValue) {
    const auto* api = SaveApi();
    long long v = 0;
    return (api && api->saveGetInt && api->saveGetInt(slot, key, &v)) ? v : defaultValue;
}
bool ScriptApi_SaveSetFloat(const char* slot, const char* key, double value) {
    const auto* api = SaveApi();
    return api && api->saveSetFloat && api->saveSetFloat(slot, key, value);
}
double ScriptApi_SaveGetFloat(const char* slot, const char* key, double defaultValue) {
    const auto* api = SaveApi();
    double v = 0.0;
    return (api && api->saveGetFloat && api->saveGetFloat(slot, key, &v)) ? v : defaultValue;
}
bool ScriptApi_SaveSetBool(const char* slot, const char* key, bool value) {
    const auto* api = SaveApi();
    return api && api->saveSetBool && api->saveSetBool(slot, key, value);
}
bool ScriptApi_SaveGetBool(const char* slot, const char* key, bool defaultValue) {
    const auto* api = SaveApi();
    bool v = false;
    return (api && api->saveGetBool && api->saveGetBool(slot, key, &v)) ? v : defaultValue;
}
bool ScriptApi_SaveSetString(const char* slot, const char* key, const char* utf8Value) {
    const auto* api = SaveApi();
    return api && api->saveSetString && api->saveSetString(slot, key, utf8Value);
}
std::string ScriptApi_SaveGetString(const char* slot, const char* key, const char* defaultValue) {
    const auto* api = SaveApi();
    std::string v;
    if (api && api->saveGetString &&
        ReadText([&](char* out, int cap) { return api->saveGetString(slot, key, out, cap); }, &v))
        return v;
    return defaultValue ? defaultValue : "";
}
bool ScriptApi_SaveSetJson(const char* slot, const char* key, const char* jsonText) {
    const auto* api = SaveApi();
    return api && api->saveSetJson && api->saveSetJson(slot, key, jsonText);
}
std::string ScriptApi_SaveGetJson(const char* slot, const char* key) {
    const auto* api = SaveApi();
    std::string v;
    if (api && api->saveGetJson &&
        ReadText([&](char* out, int cap) { return api->saveGetJson(slot, key, out, cap); }, &v))
        return v;
    return {};
}
bool ScriptApi_SaveHasKey(const char* slot, const char* key) {
    const auto* api = SaveApi();
    return api && api->saveHasKey && api->saveHasKey(slot, key);
}
bool ScriptApi_SaveDeleteKey(const char* slot, const char* key) {
    const auto* api = SaveApi();
    return api && api->saveDeleteKey && api->saveDeleteKey(slot, key);
}
bool ScriptApi_SaveCommit(const char* slot) {
    const auto* api = SaveApi();
    return api && api->saveCommit && api->saveCommit(slot);
}
bool ScriptApi_SaveSlotExists(const char* slot) {
    const auto* api = SaveApi();
    return api && api->saveSlotExists && api->saveSlotExists(slot);
}
bool ScriptApi_SaveDeleteSlot(const char* slot) {
    const auto* api = SaveApi();
    return api && api->saveDeleteSlot && api->saveDeleteSlot(slot);
}
std::vector<std::string> ScriptApi_SaveListSlots() {
    const auto* api = SaveApi();
    std::string joined;
    if (!api || !api->saveListSlots ||
        !ReadText([&](char* out, int cap) { return api->saveListSlots(out, cap); }, &joined))
        return {};
    return host_text::SplitLines(joined);
}

void ScriptApi_EngineGUIStopPlay() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->engineGuiStopPlay)
        api->engineGuiStopPlay();
}

void ScriptApi_CameraSetPosition(DirectX::XMFLOAT3 position) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->cameraSetPosition)
        api->cameraSetPosition(position);
}

void ScriptApi_CameraSetTarget(DirectX::XMFLOAT3 target) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->cameraSetTarget)
        api->cameraSetTarget(target);
}

void ScriptApi_CameraSetUp(DirectX::XMFLOAT3 up) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->cameraSetUp)
        api->cameraSetUp(up);
}

void ScriptApi_SetActiveCameraNodeName(const char* name) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->engineGuiSetActiveCameraNodeName)
        api->engineGuiSetActiveCameraNodeName(name ? name : "");
}

void ScriptApi_ActivateSceneCamera(const char* name) {
    ScriptApi_SetActiveCameraNodeName(name);
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->engineGuiSetCameraMode)
        api->engineGuiSetCameraMode(CameraMode::SceneCamera);
}

int ScriptApi_GetBackBufferWidth() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->getBackBufferWidth ? api->getBackBufferWidth() : 0;
}

int ScriptApi_GetBackBufferHeight() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->getBackBufferHeight ? api->getBackBufferHeight() : 0;
}

void ScriptApi_SpriteDraw(int texId,
                          float x,
                          float y,
                          float width,
                          float height,
                          int tx,
                          int ty,
                          int tw,
                          int th,
                          float angle,
                          DirectX::XMFLOAT4 color) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->spriteDraw)
        api->spriteDraw(texId, x, y, width, height, tx, ty, tw, th, angle, color);
}

int ScriptApi_TextureGetWhiteTexture() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->textureGetWhiteTexture ? api->textureGetWhiteTexture() : -1;
}

unsigned int ScriptApi_TextureGetWidth(int textureId) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->textureGetWidth ? api->textureGetWidth(textureId) : 0u;
}

unsigned int ScriptApi_TextureGetHeight(int textureId) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->textureGetHeight ? api->textureGetHeight(textureId) : 0u;
}

void ScriptApi_TextureRelease(int textureId) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->textureRelease)
        api->textureRelease(textureId);
}

bool ScriptApi_EngineGUILoadFromFile(const char* filename) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->engineGuiLoadFromFile ? api->engineGuiLoadFromFile(filename) : false;
}

void ScriptApi_EngineGUIStartPlay() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->engineGuiStartPlay)
        api->engineGuiStartPlay();
}

float ScriptApi_GetSphereColliderRadius(const SphereCollider& collider) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->sphereColliderGetRadius ? api->sphereColliderGetRadius(&collider) : 0.0f;
}

DirectX::XMFLOAT3 ScriptApi_GetCameraForward() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->cameraGetForward ? api->cameraGetForward() : DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f};
}

UISprite* ScriptApi_UICanvasAddSprite(UICanvas& canvas,
                                      const char* name,
                                      DirectX::XMFLOAT2 position,
                                      UIAnchor anchor,
                                      float width,
                                      float height) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->uiCanvasAddSprite
        ? api->uiCanvasAddSprite(&canvas, name, position, anchor, width, height)
        : nullptr;
}

void ScriptApi_UICanvasRemoveElement(UICanvas& canvas, const char* name) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->uiCanvasRemoveElement)
        api->uiCanvasRemoveElement(&canvas, name);
}

void ScriptApi_UISpriteSetTexture(UISprite& sprite, const wchar_t* texturePath) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->uiSpriteSetTexture)
        api->uiSpriteSetTexture(&sprite, texturePath);
}

UISprite* ScriptApi_UICanvasGetSprite(UICanvas& canvas, const char* name) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->uiCanvasGetSprite ? api->uiCanvasGetSprite(&canvas, name) : nullptr;
}

void ScriptApi_UISpriteSetRadial360Fill(UISprite& sprite, float fillAmount, bool clockwise) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->uiSpriteSetRadial360Fill)
        api->uiSpriteSetRadial360Fill(&sprite, fillAmount, clockwise);
}

void ScriptApi_UISpriteSetFillNone(UISprite& sprite) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->uiSpriteSetFillNone)
        api->uiSpriteSetFillNone(&sprite);
}

void ScriptApi_UISpriteSetSourceRect(UISprite& sprite, int x, int y, int width, int height) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->uiSpriteSetSourceRect)
        api->uiSpriteSetSourceRect(&sprite, x, y, width, height);
}

int ScriptApi_TextureLoad(const wchar_t* fileName, bool mipmap) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->textureLoad ? api->textureLoad(fileName, mipmap) : -1;
}

ID3D11ShaderResourceView* ScriptApi_TextureGetSRV(int textureId) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->textureGetSRV ? api->textureGetSRV(textureId) : nullptr;
}

void ScriptApi_BillboardAdd(DirectX::XMFLOAT3 position,
                            DirectX::XMFLOAT2 size,
                            DirectX::XMFLOAT4 color,
                            ID3D11ShaderResourceView* texture,
                            float rotation,
                            bool groundAligned) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->billboardAdd)
        api->billboardAdd(position, size, color, texture, rotation, groundAligned);
}

void ScriptApi_BillboardAddUv(DirectX::XMFLOAT3 position,
                              DirectX::XMFLOAT2 size,
                              DirectX::XMFLOAT4 color,
                              ID3D11ShaderResourceView* texture,
                              float rotation,
                              bool groundAligned,
                              DirectX::XMFLOAT2 uvOffset,
                              DirectX::XMFLOAT2 uvScale) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->billboardAddUv)
        api->billboardAddUv(position, size, color, texture, rotation, groundAligned, uvOffset, uvScale);
}

std::uint32_t ScriptApi_ParticleCreateEmitter(const engine::ParticleEmitterConfig& config,
                                              DirectX::XMFLOAT3 position) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->particleCreateEmitter
        ? api->particleCreateEmitter(&config, position)
        : 0;
}

void ScriptApi_ParticleSetEmitterPosition(std::uint32_t emitterId,
                                          DirectX::XMFLOAT3 position) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->particleSetEmitterPosition)
        api->particleSetEmitterPosition(emitterId, position);
}

void ScriptApi_ParticleSetEmitterEmitting(std::uint32_t emitterId,
                                          bool emitting) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->particleSetEmitterEmitting)
        api->particleSetEmitterEmitting(emitterId, emitting);
}

void ScriptApi_ParticleStopEmitter(std::uint32_t emitterId,
                                   bool destroyWhenEmpty) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->particleStopEmitter)
        api->particleStopEmitter(emitterId, destroyWhenEmpty);
}

void ScriptApi_ParticleDestroyEmitter(std::uint32_t emitterId) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->particleDestroyEmitter)
        api->particleDestroyEmitter(emitterId);
}

void ScriptApi_ParticleSpawnAt(std::uint32_t emitterId,
                               DirectX::XMFLOAT3 position,
                               DirectX::XMFLOAT3 velocity,
                               DirectX::XMFLOAT4 color,
                               float size,
                               float lifetime) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->particleSpawnAt)
        api->particleSpawnAt(emitterId, position, velocity, color, size, lifetime);
}

float ScriptApi_GetRenderViewportWidth() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return (api && api->getRenderViewportWidth) ? api->getRenderViewportWidth() : 0.0f;
}

float ScriptApi_GetRenderViewportHeight() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return (api && api->getRenderViewportHeight) ? api->getRenderViewportHeight() : 0.0f;
}

ID3D11Device* ScriptApi_GetD3DDevice() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return (api && api->getD3DDevice) ? api->getD3DDevice() : nullptr;
}

ID3D11DeviceContext* ScriptApi_GetD3DDeviceContext() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return (api && api->getD3DDeviceContext) ? api->getD3DDeviceContext() : nullptr;
}

void ScriptApi_GetCameraViewMatrix(DirectX::XMFLOAT4X4* out) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->getCameraViewMatrix)
        api->getCameraViewMatrix(out);
}

void ScriptApi_GetCameraProjMatrix(float width, float height, DirectX::XMFLOAT4X4* out) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->getCameraProjMatrix)
        api->getCameraProjMatrix(width, height, out);
}

void ScriptApi_DebugDrawSphereWire(DirectX::XMFLOAT3 position,
                                   DirectX::XMFLOAT3 center,
                                   float radius,
                                   DirectX::XMFLOAT3 rotation,
                                   DirectX::XMFLOAT4 color) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->debugDrawSphereWire)
        api->debugDrawSphereWire(position, center, radius, rotation, color);
}

bool ScriptApi_CollisionVizEnabled() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 47 && api->collisionVizEnabled && api->collisionVizEnabled();
}

void ScriptApi_DebugDrawLine(DirectX::XMFLOAT3 start,
                             DirectX::XMFLOAT3 end,
                             DirectX::XMFLOAT4 color) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->version >= 46 && api->debugDrawLine)
        api->debugDrawLine(start, end, color);
}

UICanvas* ScriptApi_SceneCanvasGetCanvas() {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->sceneCanvasGetCanvas ? api->sceneCanvasGetCanvas() : nullptr;
}

void ScriptApi_SceneCanvasSetTimerSeconds(float seconds) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->sceneCanvasSetTimerSeconds)
        api->sceneCanvasSetTimerSeconds(seconds);
}

void ScriptApi_SceneCanvasRequestSceneFile(const char* sceneFile, bool playOnLoad) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->sceneCanvasRequestSceneFile)
        api->sceneCanvasRequestSceneFile(sceneFile, playOnLoad);
}

UIElement* ScriptApi_UICanvasGetElement(UICanvas& canvas, const char* name) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->uiCanvasGetElement ? api->uiCanvasGetElement(&canvas, name) : nullptr;
}

void ScriptApi_UITextSetText(UIText& text, const char* value) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->uiTextSetText)
        api->uiTextSetText(&text, value);
}

void ScriptApi_UIElementSetVisible(UIElement& element, bool visible) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->uiElementSetVisible)
        api->uiElementSetVisible(&element, visible);
}

void ScriptApi_PickupDebugSetMessage(const char* message, float displaySeconds) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->pickupDebugSetMessage)
        api->pickupDebugSetMessage(message, displaySeconds);
}

GameObject* ScriptApi_PrefabInstantiate(const char* prefabPath,
                                        DirectX::XMFLOAT3 position,
                                        DirectX::XMFLOAT3 rotation,
                                        GameObject* parent,
                                        bool startActive) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    return api && api->prefabInstantiate
        ? api->prefabInstantiate(prefabPath, position, rotation, parent, startActive)
        : nullptr;
}

void ScriptApi_PrefabDestroy(GameObject* obj) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->prefabDestroy)
        api->prefabDestroy(obj);
}

void ScriptApi_PrefabSetBeforeDestroyCallback(void (*callback)(GameObject* obj)) {
    const ScriptRuntimeApi* api = ScriptRuntimeApi_GetHost();
    if (api && api->prefabSetBeforeDestroyCallback)
        api->prefabSetBeforeDestroyCallback(callback);
}

bool ScriptApi_SpritePlay(GameObject* object, const char* clipName, bool restart) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 43 && api->spritePlay ? api->spritePlay(object, clipName, restart) : false;
}
void ScriptApi_SpritePause(GameObject* object) {
    const auto* api = ScriptRuntimeApi_GetHost();
    if (api && api->version >= 43 && api->spritePause) api->spritePause(object);
}
void ScriptApi_SpriteStop(GameObject* object) {
    const auto* api = ScriptRuntimeApi_GetHost();
    if (api && api->version >= 43 && api->spriteStop) api->spriteStop(object);
}
void ScriptApi_SpriteSetFlip(GameObject* object, bool flipX, bool flipY) {
    const auto* api = ScriptRuntimeApi_GetHost();
    if (api && api->version >= 43 && api->spriteSetFlip) api->spriteSetFlip(object, flipX, flipY);
}
void ScriptApi_SpriteSetTint(GameObject* object, DirectX::XMFLOAT4 tint) {
    const auto* api = ScriptRuntimeApi_GetHost();
    if (api && api->version >= 43 && api->spriteSetTint) api->spriteSetTint(object, tint);
}

bool ScriptApi_Physics2DSetVelocity(GameObject* object, DirectX::XMFLOAT2 velocity) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 44 && api->physics2DSetVelocity && api->physics2DSetVelocity(object, velocity);
}

bool ScriptApi_Physics2DGetVelocity(GameObject* object, DirectX::XMFLOAT2* velocity) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 44 && api->physics2DGetVelocity && api->physics2DGetVelocity(object, velocity);
}

bool ScriptApi_Physics2DMovePosition(GameObject* object, DirectX::XMFLOAT2 position) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 44 && api->physics2DMovePosition && api->physics2DMovePosition(object, position);
}

bool ScriptApi_Physics2DRaycast(DirectX::XMFLOAT2 origin, DirectX::XMFLOAT2 direction, float distance, GameObject** outObject, DirectX::XMFLOAT2* outPoint, DirectX::XMFLOAT2* outNormal, float* outDistance, std::uint32_t mask, bool includeTriggers) {
    const auto* api = ScriptRuntimeApi_GetHost();
    return api && api->version >= 44 && api->physics2DRaycast && api->physics2DRaycast(origin, direction, distance, outObject, outPoint, outNormal, outDistance, mask, includeTriggers);
}

void ScriptApi_GetGameViewportRect(float& x, float& y, float& width, float& height) {
    x = y = width = height = 0.0f;
    const auto* api = ScriptRuntimeApi_GetHost();
    if (api && api->version >= 45 && api->getGameViewportRect)
        api->getGameViewportRect(&x, &y, &width, &height);
}
