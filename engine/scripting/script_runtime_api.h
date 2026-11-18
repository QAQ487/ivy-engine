// script_runtime_api.h
// C API surface exposed to gameplay script DLLs.

#pragma once

#include <d3d11.h>

#include "scene_manager.h"
#include "engine/rendering/billboard/billboard.h"
#include "engine/rendering/particle/particle_system.h"
#include "engine/physics/collider/collider.h"
#include "engine/input/controller.h"
#include "engine/input/keyboard.h"
#include "engine/core/engine_gui.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/collider_component.h"
#include "engine/ecs/effect_component.h"
#include "engine/ecs/audio_component.h"
#include "engine/scene/scene_node.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/material_component.h"
#include "engine/ui/ui_canvas.h"
#include "engine/ui/ui_element.h"
#include "engine/ui/ui_sprite.h"
#include "engine/ui/ui_text.h"

#include <cstdint>
#include <string>
#include <vector>

struct ScriptRuntimeApi {
    std::uint32_t version;

    SceneBase* (*getCurrentScene)();

    const Keyboard_State* (*keyboardGetState)();
    bool (*keyLoggerIsTrigger)(Keyboard_Keys key);
    bool (*mouseIsLeftDown)();
    bool (*mouseIsMiddleDown)();
    bool (*mouseIsRightDown)();
    int (*mouseGetWheelDelta)();
    DirectX::XMFLOAT2 (*controllerGetLeftStick)(int padIndex);
    DirectX::XMFLOAT2 (*controllerGetRightStick)(int padIndex);
    float (*controllerGetLeftTrigger)(int padIndex);
    float (*controllerGetRightTrigger)(int padIndex);
    bool (*controllerIsPressed)(ControllerButton button, int padIndex);

    void (*thirdPersonCameraInitialize)();
    void (*thirdPersonCameraSetTarget)(DirectX::XMFLOAT3* positionPtr, float* yawPtr);
    void (*thirdPersonCameraSetMouseSensitivity)(float sensitivity);
    void (*thirdPersonCameraSetControllerSensitivity)(float sensitivity);
    void (*thirdPersonCameraSetPitchLimits)(float minPitch, float maxPitch);
    void (*thirdPersonCameraShowCursor)(bool show);
    bool (*thirdPersonCameraIsCursorVisible)();
    DirectX::XMFLOAT3 (*cameraGetForward)();

    bool (*engineGuiIsPlaying)();
    void (*engineGuiStopPlay)();
    void (*engineGuiSetCameraMode)(CameraMode mode);
    void (*engineGuiSetActiveCameraNodeName)(const char* name);
    void (*cameraSetPosition)(DirectX::XMFLOAT3 position);
    void (*cameraSetTarget)(DirectX::XMFLOAT3 target);
    void (*cameraSetUp)(DirectX::XMFLOAT3 up);
    int (*getBackBufferWidth)();
    int (*getBackBufferHeight)();
    void (*spriteDraw)(int texId,
                       float x,
                       float y,
                       float width,
                       float height,
                       int tx,
                       int ty,
                       int tw,
                       int th,
                       float angle,
                       DirectX::XMFLOAT4 color);
    int (*textureGetWhiteTexture)();
    unsigned int (*textureGetWidth)(int textureId);
    unsigned int (*textureGetHeight)(int textureId);
    void (*textureRelease)(int textureId);
    bool (*engineGuiLoadFromFile)(const char* filename);
    void (*engineGuiStartPlay)();

    UICanvas* (*sceneCanvasGetCanvas)();
    void (*sceneCanvasShowRedOverlay)();
    void (*sceneCanvasHideRedOverlay)();
    void (*sceneCanvasSetRedOverlayAlpha)(float alpha);
    void (*sceneCanvasSetTimerSeconds)(float seconds);
    void (*sceneCanvasRequestSceneFile)(const char* sceneFile, bool playOnLoad);

    UIElement* (*uiCanvasGetElement)(UICanvas* canvas, const char* name);
    void (*uiTextSetText)(UIText* text, const char* value);
    void (*uiElementSetVisible)(UIElement* element, bool visible);

    UISprite* (*uiCanvasAddSprite)(UICanvas* canvas,
                                   const char* name,
                                   DirectX::XMFLOAT2 position,
                                   UIAnchor anchor,
                                   float width,
                                   float height);
    void (*uiCanvasRemoveElement)(UICanvas* canvas, const char* name);
    void (*uiSpriteSetTexture)(UISprite* sprite, const wchar_t* texturePath);
    UISprite* (*uiCanvasGetSprite)(UICanvas* canvas, const char* name);
    void (*uiSpriteSetRadial360Fill)(UISprite* sprite, float fillAmount, bool clockwise);
    void (*uiSpriteSetFillNone)(UISprite* sprite);

    int (*textureLoad)(const wchar_t* fileName, bool mipmap);
    ID3D11ShaderResourceView* (*textureGetSRV)(int textureId);
    void (*billboardAdd)(DirectX::XMFLOAT3 position,
                         DirectX::XMFLOAT2 size,
                         DirectX::XMFLOAT4 color,
                         ID3D11ShaderResourceView* texture,
                         float rotation,
                         bool groundAligned);
    std::uint32_t (*particleCreateEmitter)(const engine::ParticleEmitterConfig* config,
                                           DirectX::XMFLOAT3 position);
    void (*particleSetEmitterPosition)(std::uint32_t emitterId,
                                       DirectX::XMFLOAT3 position);
    void (*particleSetEmitterEmitting)(std::uint32_t emitterId,
                                       bool emitting);
    void (*particleStopEmitter)(std::uint32_t emitterId,
                                bool destroyWhenEmpty);
    void (*particleDestroyEmitter)(std::uint32_t emitterId);
    void (*particleSpawnAt)(std::uint32_t emitterId,
                             DirectX::XMFLOAT3 position,
                             DirectX::XMFLOAT3 velocity,
                             DirectX::XMFLOAT4 color,
                             float size,
                             float lifetime);
    float (*getRenderViewportWidth)();
    float (*getRenderViewportHeight)();
    ID3D11Device* (*getD3DDevice)();
    ID3D11DeviceContext* (*getD3DDeviceContext)();
    void (*getCameraViewMatrix)(DirectX::XMFLOAT4X4* out);
    void (*getCameraProjMatrix)(float width, float height, DirectX::XMFLOAT4X4* out);
    void (*debugDrawSphereWire)(DirectX::XMFLOAT3 position,
                                DirectX::XMFLOAT3 center,
                                float radius,
                                DirectX::XMFLOAT3 rotation,
                                DirectX::XMFLOAT4 color);

    void (*colliderComponentApplyToOwner)(ColliderComponent* component);
    BoxCollider* (*gameObjectCreateBoxCollider)(GameObject* object,
                                                DirectX::XMFLOAT3 size,
                                                DirectX::XMFLOAT3 center);
    void (*gameObjectDestroyCollider)(GameObject* object);
    void (*gameObjectAddIgnoreCollisionTag)(GameObject* object, Tag tag);
    DirectX::XMFLOAT3 (*colliderGetWorldCenter)(const Collider* collider);
    DirectX::XMFLOAT3 (*boxColliderGetSize)(const BoxCollider* collider);
    void (*boxColliderSetSize)(BoxCollider* collider, DirectX::XMFLOAT3 size);
    float (*sphereColliderGetRadius)(const SphereCollider* collider);
    bool (*colliderTryGetFootprint)(const Collider* collider,
                                    DirectX::XMFLOAT3* outCenter,
                                    DirectX::XMFLOAT3* outSize);

    void (*pickupDebugSetMessage)(const char* message, float displaySeconds);

    GameObject* (*prefabInstantiate)(const char* prefabPath,
                                     DirectX::XMFLOAT3 position,
                                     DirectX::XMFLOAT3 rotation,
                                     GameObject* parent,
                                     bool startActive);
    void (*prefabDestroy)(GameObject* obj);
    void (*prefabSetBeforeDestroyCallback)(void (*callback)(GameObject* obj));

    void (*visualPlayClip)(VisualComponent* component, const char* clipName);
    void (*effectPlay)(EffectComponent* component);
    void (*uiSpriteSetSourceRect)(UISprite* sprite, int x, int y, int width, int height);

    void (*thirdPersonCameraSetMagnetMode)(int mode);
    void (*thirdPersonCameraSetMagnetTarget)(DirectX::XMFLOAT3 target);

    void (*billboardAddUv)(DirectX::XMFLOAT3 position,
                           DirectX::XMFLOAT2 size,
                           DirectX::XMFLOAT4 color,
                           ID3D11ShaderResourceView* texture,
                           float rotation,
                           bool groundAligned,
                           DirectX::XMFLOAT2 uvOffset,
                           DirectX::XMFLOAT2 uvScale);

    // Appended for ABI safety — always add new host hooks at the end.
    void (*gameObjectRemoveIgnoreCollisionTag)(GameObject* object, Tag tag);
    void (*audioPlay)(AudioComponent* component);
    void (*audioStop)(AudioComponent* component);
    bool (*visualTryGetBoneWorld)(VisualComponent* component,
                                  const char* boneName,
                                  DirectX::XMFLOAT3* outPos,
                                  DirectX::XMFLOAT3* outRotDeg);
    void (*visualPlayClipLoop)(VisualComponent* component, const char* clipName, int loop);
    float (*visualGetClipDuration)(VisualComponent* component);
    void (*visualSetAnimSpeed)(VisualComponent* component, float speed);
    void (*thirdPersonCameraSetDistance)(float distance);
    void (*thirdPersonCameraSetHeight)(float height);
    void (*thirdPersonCameraSetLookAtHeight)(float height);
    void (*gameObjectAddIgnoreCollisionObject)(GameObject* object, GameObject* other);
    void (*gameObjectRemoveIgnoreCollisionObject)(GameObject* object, GameObject* other);
    void (*gameObjectClearIgnoreCollisionObjects)(GameObject* object);
    void (*audioSetMasterVolume)(float volume);
    float (*audioGetMasterVolume)();
    void (*mouseGetPosition)(int* x, int* y);
    void (*postFxBurst)(float rgbShift, float durationSeconds);
    bool (*physicsRaycast)(const DirectX::XMFLOAT3* origin, const DirectX::XMFLOAT3* dir,
        float maxDistance, DirectX::XMFLOAT3* outPoint, DirectX::XMFLOAT3* outNormal,
        float* outDistance);
    void (*cameraAddTrauma)(float amount01);
    void (*cameraSetSpeedFactor)(float speed01);
    void (*debugPulseTimeScale)(float scale, unsigned durationMs);
    void (*lineSpriteDraw)(const DirectX::XMFLOAT3* a, const DirectX::XMFLOAT3* b,
        float thicknessWorld, const DirectX::XMFLOAT4* color);

    // v42 — first-person camera (append-only)
    void (*firstPersonCameraInitialize)();
    void (*firstPersonCameraSetTarget)(DirectX::XMFLOAT3* positionPtr, float* yawPtr);
    void (*firstPersonCameraSetEyeHeight)(float eyeHeight);
    void (*firstPersonCameraSetMouseSensitivity)(float sensitivity);
    void (*firstPersonCameraSetControllerSensitivity)(float sensitivity);
    void (*firstPersonCameraSetPitchLimits)(float minPitch, float maxPitch);
    void (*firstPersonCameraShowCursor)(bool show);
    bool (*firstPersonCameraIsCursorVisible)();

    // v43 — world sprite animation (append-only)
    bool (*spritePlay)(GameObject* object, const char* clipName, bool restart);
    void (*spritePause)(GameObject* object);
    void (*spriteStop)(GameObject* object);
    void (*spriteSetFlip)(GameObject* object, bool flipX, bool flipY);
    void (*spriteSetTint)(GameObject* object, DirectX::XMFLOAT4 tint);
    bool (*physics2DSetVelocity)(GameObject*, DirectX::XMFLOAT2);
    bool (*physics2DGetVelocity)(GameObject*, DirectX::XMFLOAT2*);
    bool (*physics2DMovePosition)(GameObject*, DirectX::XMFLOAT2);
    bool (*physics2DRaycast)(DirectX::XMFLOAT2, DirectX::XMFLOAT2, float, GameObject**, DirectX::XMFLOAT2*, DirectX::XMFLOAT2*, float*, std::uint32_t, bool);
    void (*getGameViewportRect)(float*, float*, float*, float*);

    // v46 — deferred debug geometry, queued during Update and flushed after scene drawing.
    void (*debugDrawLine)(DirectX::XMFLOAT3 start,
                          DirectX::XMFLOAT3 end,
                          DirectX::XMFLOAT4 color);

    // v47 — share the host F11 state with gameplay collision visualizers.
    bool (*collisionVizEnabled)();

    // v48 - deferred game exit, preserving the v42-v47 prefix.
    void (*requestExitGame)();

    // v49 - runtime render settings by name (see RenderSettings_SetBoolByName for the keys).
    // Return false for an unknown key or a non-finite number; numbers are clamped.
    bool (*renderSettingSetBool)(const char* key, bool value);
    bool (*renderSettingGetBool)(const char* key, bool* outValue);
    bool (*renderSettingSetNumber)(const char* key, double value);
    bool (*renderSettingGetNumber)(const char* key, double* outValue);

    // v50 - 3D rigidbody on an object's Rigidbody component (false when it has
    // none or a vector is not finite; force / torque modes are ForceMode values),
    // scene search and time.
    bool (*rigidbodyGetVelocity)(GameObject* object, DirectX::XMFLOAT3* outVelocity);
    bool (*rigidbodySetVelocity)(GameObject* object, DirectX::XMFLOAT3 velocity);
    bool (*rigidbodyGetAngularVelocity)(GameObject* object, DirectX::XMFLOAT3* outVelocity);
    bool (*rigidbodySetAngularVelocity)(GameObject* object, DirectX::XMFLOAT3 velocity);
    bool (*rigidbodyAddForce)(GameObject* object, DirectX::XMFLOAT3 force, int mode);
    bool (*rigidbodyAddTorque)(GameObject* object, DirectX::XMFLOAT3 torque, int mode);
    GameObject* (*findGameObjectByName)(const char* name, bool includeInactive);
    int (*findGameObjectsByTag)(Tag tag, GameObject** out, int capacity, bool includeInactive);
    float (*getTimeScale)();
    double (*getRealTimeSeconds)();

    // v51 - save data (engine/save/save_store.h): named slots of key-value
    // pairs, written only by saveCommit. Text getters copy into out (NUL
    // terminated, truncated to capacity) and return the full length in bytes,
    // or -1 when the key is missing / of another type. saveListSlots writes
    // slot names separated by '\n'.
    bool (*saveSetInt)(const char* slot, const char* key, long long value);
    bool (*saveGetInt)(const char* slot, const char* key, long long* outValue);
    bool (*saveSetFloat)(const char* slot, const char* key, double value);
    bool (*saveGetFloat)(const char* slot, const char* key, double* outValue);
    bool (*saveSetBool)(const char* slot, const char* key, bool value);
    bool (*saveGetBool)(const char* slot, const char* key, bool* outValue);
    bool (*saveSetString)(const char* slot, const char* key, const char* utf8Value);
    int (*saveGetString)(const char* slot, const char* key, char* out, int capacity);
    bool (*saveSetJson)(const char* slot, const char* key, const char* jsonText);
    int (*saveGetJson)(const char* slot, const char* key, char* out, int capacity);
    bool (*saveHasKey)(const char* slot, const char* key);
    bool (*saveDeleteKey)(const char* slot, const char* key);
    bool (*saveCommit)(const char* slot);
    bool (*saveSlotExists)(const char* slot);
    bool (*saveDeleteSlot)(const char* slot);
    int (*saveListSlots)(char* out, int capacity);
};

const ScriptRuntimeApi* ScriptRuntimeApi_GetHost();
bool ScriptApi_CollisionVizEnabled();

SceneBase* ScriptApi_GetCurrentScene();
engine::Transform* ScriptApi_FindTransform(engine::GameObject& object);
const engine::Transform* ScriptApi_FindTransform(const engine::GameObject& object);
ColliderComponent* ScriptApi_FindColliderComponent(engine::GameObject& object);
EffectComponent* ScriptApi_FindEffectComponent(engine::GameObject& object);
VisualComponent* ScriptApi_FindVisualComponent(engine::GameObject& object);
MaterialComponent* ScriptApi_FindMaterialComponent(engine::GameObject& object);
void ScriptApi_Visual_PlayClip(VisualComponent* component, const char* clipName);
bool ScriptApi_Visual_TryGetBoneWorld(VisualComponent* component,
                                      const char* boneName,
                                      DirectX::XMFLOAT3& outPos,
                                      DirectX::XMFLOAT3& outRotDeg);
void ScriptApi_Visual_PlayClipLoop(VisualComponent* component, const char* clipName, bool loop);
float ScriptApi_Visual_GetClipDuration(VisualComponent* component);
void ScriptApi_Visual_SetAnimSpeed(VisualComponent* component, float speed);
void ScriptApi_Effect_Play(EffectComponent* component);
AudioComponent* ScriptApi_FindAudioComponent(engine::GameObject& object);
void ScriptApi_Audio_Play(AudioComponent* component);
void ScriptApi_Audio_Stop(AudioComponent* component);
void ScriptApi_AudioSetMasterVolume(float volume);
float ScriptApi_AudioGetMasterVolume();
void ScriptApi_MouseGetPosition(int& x, int& y);
void ScriptApi_PostFX_Burst(float rgbShift, float durationSeconds);
bool ScriptApi_Physics_Raycast(const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& dir, float maxDistance,
    DirectX::XMFLOAT3* outPoint, DirectX::XMFLOAT3* outNormal,
    float* outDistance);
void ScriptApi_Camera_AddTrauma(float amount01);
void ScriptApi_Camera_SetSpeedFactor(float speed01);
void ScriptApi_Debug_PulseTimeScale(float scale, unsigned durationMs);
void ScriptApi_LineSprite_Draw(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b,
    float thicknessWorld, const DirectX::XMFLOAT4& color);

void ScriptApi_ApplyColliderComponent(ColliderComponent& component);
BoxCollider* ScriptApi_CreateBoxCollider(engine::GameObject& object,
                                         DirectX::XMFLOAT3 size,
                                         DirectX::XMFLOAT3 center);
void ScriptApi_DestroyCollider(engine::GameObject& object);
void ScriptApi_GameObjectAddIgnoreCollisionTag(engine::GameObject& object, Tag tag);
void ScriptApi_GameObjectRemoveIgnoreCollisionTag(engine::GameObject& object, Tag tag);
void ScriptApi_GameObjectAddIgnoreCollisionObject(engine::GameObject& object, engine::GameObject& other);
void ScriptApi_GameObjectRemoveIgnoreCollisionObject(engine::GameObject& object, engine::GameObject& other);
void ScriptApi_GameObjectClearIgnoreCollisionObjects(engine::GameObject& object);
DirectX::XMFLOAT3 ScriptApi_GetColliderWorldCenter(const Collider& collider);
DirectX::XMFLOAT3 ScriptApi_GetBoxColliderSize(const BoxCollider& collider);
void ScriptApi_SetBoxColliderSize(BoxCollider& collider, DirectX::XMFLOAT3 size);
bool ScriptApi_TryGetColliderFootprint(const Collider& collider,
                                       DirectX::XMFLOAT3& outCenter,
                                       DirectX::XMFLOAT3& outSize);
void ScriptApi_EngineGUIStopPlay();
bool ScriptApi_RequestExitGame();

// Runtime render settings (v49 host; older hosts return false). Toggles: frustumCullingEnabled,
// shadowCascadeCullingEnabled, spriteCullingEnabled, lodEnabled, animCullingEnabled,
// lightSelectionEnabled, billboardCullingEnabled, effectCullingEnabled, lodDebugLabels.
// Numbers (clamped like the scene loader): lodBias, cullDistanceScale, shadowDistance,
// shadowLodOffset, effectCullRadius, lodForceLevel (-1 automatic, 0..7 force a LOD level).
// Changes apply to the running scene only; they are not written to the scene file.
bool ScriptApi_RenderSettingSetBool(const char* key, bool value);
bool ScriptApi_RenderSettingGetBool(const char* key, bool& outValue);
bool ScriptApi_RenderSettingSetNumber(const char* key, double value);
bool ScriptApi_RenderSettingGetNumber(const char* key, double& outValue);

// 3D rigidbody (v50 host; older hosts return false). Works on an object with a
// Rigidbody component and never adds one. Kinematic bodies ignore forces.
// Call from the main thread (scripts that are not Update_IsParallelSafe) when
// the body belongs to another object.
bool ScriptApi_RigidbodyGetVelocity(GameObject& object, DirectX::XMFLOAT3& outVelocity);
bool ScriptApi_RigidbodySetVelocity(GameObject& object, DirectX::XMFLOAT3 velocity);
bool ScriptApi_RigidbodyGetAngularVelocity(GameObject& object, DirectX::XMFLOAT3& outVelocity);
bool ScriptApi_RigidbodySetAngularVelocity(GameObject& object, DirectX::XMFLOAT3 velocity);
bool ScriptApi_RigidbodyAddForce(GameObject& object, DirectX::XMFLOAT3 force,
                                 ForceMode mode = ForceMode::Force);
bool ScriptApi_RigidbodyAddTorque(GameObject& object, DirectX::XMFLOAT3 torque,
                                  ForceMode mode = ForceMode::Force);

// Scene search (v50 host; older hosts return nullptr / 0). Depth-first in scene
// order over the loaded scene, including runtime-spawned prefabs; objects
// waiting to be destroyed are skipped. Without includeInactive, an inactive
// object and its whole subtree are skipped. Name match is exact (case-sensitive).
GameObject* ScriptApi_FindGameObjectByName(const char* name, bool includeInactive = false);
// Writes up to capacity matches to out and returns the total number of matches.
int ScriptApi_FindGameObjectsByTag(Tag tag, GameObject** out, int capacity,
                                   bool includeInactive = false);

// Time (v50 host; older hosts return 1 / 0). Time scale is the multiplier the
// engine applies to dt (debug F7 scale and ScriptApi_Debug_PulseTimeScale);
// real time is wall-clock seconds since the engine started, not scaled.
float ScriptApi_GetTimeScale();
double ScriptApi_GetRealTimeSeconds();

// Save data (v51 host; older hosts: setters / Commit return false, getters the
// default). A slot is one file (name: 1-32 of A-Z a-z 0-9 _ -); keys are
// 1-128 bytes of UTF-8. Set* only changes memory: call ScriptApi_SaveCommit
// to write the slot (atomic, previous file kept as .bak). Saves live in
// <Saved Games>/<game name>/ for a packaged game and in
// <content project>/.ivy/saves/ in editor Play; Stop drops uncommitted values.
// GetInt reads integers only; GetFloat reads any number.
bool ScriptApi_SaveSetInt(const char* slot, const char* key, long long value);
long long ScriptApi_SaveGetInt(const char* slot, const char* key, long long defaultValue = 0);
bool ScriptApi_SaveSetFloat(const char* slot, const char* key, double value);
double ScriptApi_SaveGetFloat(const char* slot, const char* key, double defaultValue = 0.0);
bool ScriptApi_SaveSetBool(const char* slot, const char* key, bool value);
bool ScriptApi_SaveGetBool(const char* slot, const char* key, bool defaultValue = false);
bool ScriptApi_SaveSetString(const char* slot, const char* key, const char* utf8Value);
std::string ScriptApi_SaveGetString(const char* slot, const char* key, const char* defaultValue = "");
// Any JSON value as text, e.g. a nested object built with nlohmann::json::dump().
// SetJson returns false for text that does not parse; GetJson returns "" when missing.
bool ScriptApi_SaveSetJson(const char* slot, const char* key, const char* jsonText);
std::string ScriptApi_SaveGetJson(const char* slot, const char* key);
bool ScriptApi_SaveHasKey(const char* slot, const char* key);
bool ScriptApi_SaveDeleteKey(const char* slot, const char* key);
bool ScriptApi_SaveCommit(const char* slot);
// On disk (committed) only.
bool ScriptApi_SaveSlotExists(const char* slot);
bool ScriptApi_SaveDeleteSlot(const char* slot);
std::vector<std::string> ScriptApi_SaveListSlots();
void ScriptApi_CameraSetPosition(DirectX::XMFLOAT3 position);
void ScriptApi_CameraSetTarget(DirectX::XMFLOAT3 target);
void ScriptApi_CameraSetUp(DirectX::XMFLOAT3 up);
void ScriptApi_SetActiveCameraNodeName(const char* name);
void ScriptApi_ActivateSceneCamera(const char* name);
int ScriptApi_GetBackBufferWidth();
int ScriptApi_GetBackBufferHeight();
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
                          DirectX::XMFLOAT4 color);
int ScriptApi_TextureGetWhiteTexture();
unsigned int ScriptApi_TextureGetWidth(int textureId);
unsigned int ScriptApi_TextureGetHeight(int textureId);
void ScriptApi_TextureRelease(int textureId);
bool ScriptApi_EngineGUILoadFromFile(const char* filename);
void ScriptApi_EngineGUIStartPlay();
float ScriptApi_GetSphereColliderRadius(const SphereCollider& collider);

DirectX::XMFLOAT3 ScriptApi_GetCameraForward();
UISprite* ScriptApi_UICanvasAddSprite(UICanvas& canvas,
                                      const char* name,
                                      DirectX::XMFLOAT2 position,
                                      UIAnchor anchor,
                                      float width,
                                      float height);
void ScriptApi_UICanvasRemoveElement(UICanvas& canvas, const char* name);
void ScriptApi_UISpriteSetTexture(UISprite& sprite, const wchar_t* texturePath);
UISprite* ScriptApi_UICanvasGetSprite(UICanvas& canvas, const char* name);
void ScriptApi_UISpriteSetRadial360Fill(UISprite& sprite, float fillAmount, bool clockwise = true);
void ScriptApi_UISpriteSetFillNone(UISprite& sprite);
void ScriptApi_UISpriteSetSourceRect(UISprite& sprite, int x, int y, int width, int height);
int ScriptApi_TextureLoad(const wchar_t* fileName, bool mipmap = true);
ID3D11ShaderResourceView* ScriptApi_TextureGetSRV(int textureId);
void ScriptApi_BillboardAdd(DirectX::XMFLOAT3 position,
                            DirectX::XMFLOAT2 size,
                            DirectX::XMFLOAT4 color,
                            ID3D11ShaderResourceView* texture,
                            float rotation = 0.0f,
                            bool groundAligned = false);
void ScriptApi_BillboardAddUv(DirectX::XMFLOAT3 position,
                              DirectX::XMFLOAT2 size,
                              DirectX::XMFLOAT4 color,
                              ID3D11ShaderResourceView* texture,
                              float rotation,
                              bool groundAligned,
                              DirectX::XMFLOAT2 uvOffset,
                              DirectX::XMFLOAT2 uvScale);
std::uint32_t ScriptApi_ParticleCreateEmitter(const engine::ParticleEmitterConfig& config,
                                              DirectX::XMFLOAT3 position);
void ScriptApi_ParticleSetEmitterPosition(std::uint32_t emitterId,
                                          DirectX::XMFLOAT3 position);
void ScriptApi_ParticleSetEmitterEmitting(std::uint32_t emitterId,
                                          bool emitting);
void ScriptApi_ParticleStopEmitter(std::uint32_t emitterId,
                                   bool destroyWhenEmpty = true);
void ScriptApi_ParticleDestroyEmitter(std::uint32_t emitterId);
void ScriptApi_ParticleSpawnAt(std::uint32_t emitterId,
                               DirectX::XMFLOAT3 position,
                               DirectX::XMFLOAT3 velocity,
                               DirectX::XMFLOAT4 color,
                               float size,
                               float lifetime);
float ScriptApi_GetRenderViewportWidth();
float ScriptApi_GetRenderViewportHeight();
ID3D11Device* ScriptApi_GetD3DDevice();
ID3D11DeviceContext* ScriptApi_GetD3DDeviceContext();
void ScriptApi_GetCameraViewMatrix(DirectX::XMFLOAT4X4* out);
void ScriptApi_GetCameraProjMatrix(float width, float height, DirectX::XMFLOAT4X4* out);
void ScriptApi_DebugDrawSphereWire(DirectX::XMFLOAT3 position,
                                   DirectX::XMFLOAT3 center,
                                   float radius,
                                   DirectX::XMFLOAT3 rotation = {0.0f, 0.0f, 0.0f},
                                   DirectX::XMFLOAT4 color = {0.2f, 0.8f, 1.0f, 0.95f});
void ScriptApi_DebugDrawLine(DirectX::XMFLOAT3 start,
                             DirectX::XMFLOAT3 end,
                             DirectX::XMFLOAT4 color = {0.2f, 0.8f, 1.0f, 0.95f});

UICanvas* ScriptApi_SceneCanvasGetCanvas();
void ScriptApi_SceneCanvasSetTimerSeconds(float seconds);
void ScriptApi_SceneCanvasRequestSceneFile(const char* sceneFile, bool playOnLoad);
UIElement* ScriptApi_UICanvasGetElement(UICanvas& canvas, const char* name);
void ScriptApi_UITextSetText(UIText& text, const char* value);
void ScriptApi_UIElementSetVisible(UIElement& element, bool visible);
void ScriptApi_PickupDebugSetMessage(const char* message, float displaySeconds = 4.0f);

GameObject* ScriptApi_PrefabInstantiate(const char* prefabPath,
                                      DirectX::XMFLOAT3 position,
                                      DirectX::XMFLOAT3 rotation = {0.0f, 0.0f, 0.0f},
                                      GameObject* parent = nullptr,
                                      bool startActive = true);
void ScriptApi_PrefabDestroy(GameObject* obj);
void ScriptApi_PrefabSetBeforeDestroyCallback(void (*callback)(GameObject* obj));

// World sprite controls; missing objects/components safely return false or do nothing.
bool ScriptApi_SpritePlay(GameObject* object, const char* clipName, bool restart = false);
void ScriptApi_SpritePause(GameObject* object);
void ScriptApi_SpriteStop(GameObject* object);
void ScriptApi_SpriteSetFlip(GameObject* object, bool flipX, bool flipY);
void ScriptApi_SpriteSetTint(GameObject* object, DirectX::XMFLOAT4 tint);

bool ScriptApi_Physics2DSetVelocity(GameObject* object, DirectX::XMFLOAT2 velocity);

bool ScriptApi_Physics2DGetVelocity(GameObject* object, DirectX::XMFLOAT2* velocity);

bool ScriptApi_Physics2DMovePosition(GameObject* object, DirectX::XMFLOAT2 position);

bool ScriptApi_Physics2DRaycast(DirectX::XMFLOAT2 origin, DirectX::XMFLOAT2 direction, float distance, GameObject** outObject, DirectX::XMFLOAT2* outPoint, DirectX::XMFLOAT2* outNormal, float* outDistance, std::uint32_t mask = 0xffffffffu, bool includeTriggers = false);
void ScriptApi_GetGameViewportRect(float& x, float& y, float& width, float& height);
