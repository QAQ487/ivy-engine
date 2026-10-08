const fs = require("fs");
const path = require("path");

const extract = JSON.parse(
  fs.readFileSync(
    "C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-extract-results-9.json",
    "utf8"
  )
);
const input = JSON.parse(
  fs.readFileSync(
    "C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-input-9.json",
    "utf8"
  )
);
const neighbors = JSON.parse(
  fs.readFileSync(
    "C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-neighbors-9.json",
    "utf8"
  )
);

const batchImportData = input.batchImportData;

const fileMeta = {
  "engine/audio/audio_manager.cpp": {
    summary:
      "Implements the XAudio2/X3DAudio singleton: WAV load, 2D/3D playback, bus volumes, GameObject-tracked emitters, and instance voices used by audio components.",
    tags: ["service", "singleton", "audio", "3d-audio"],
    complexity: "complex",
    languageNotes:
      "XAudio2 mastering voice plus X3DAudio listener/emitter DSP; named clips live beside numeric AudioPlaybackId instances.",
  },
  "engine/audio/audio_manager.h": {
    summary:
      "Declares AudioManager, AudioType, and the public load/play/stop/volume API for 2D, 3D, instance, and UI-click playback.",
    tags: ["type-definition", "audio", "singleton", "api-handler"],
    complexity: "moderate",
    languageNotes:
      "C++ singleton with deleted copy operations; private helpers operate on an internal Playback voice struct.",
  },
  "engine/core/debug_system.cpp": {
    summary:
      "Implements main-thread debug controls: collision visualization, time-scale cycling, freeze frame-step, hitstop pulses, and the on-screen overlay.",
    tags: ["debug", "service", "time-scale", "event-handler"],
    complexity: "moderate",
  },
  "engine/core/debug_system.h": {
    summary:
      "Public DebugSystem API for collision viz, time scale, frame stepping, overlay text, and play-session time-scale reset.",
    tags: ["type-definition", "debug", "time-scale", "api-handler"],
    complexity: "simple",
    languageNotes:
      "GAME_RUNTIME-safe free functions in a namespace — no ImGui dependency.",
  },
  "engine/debug_draw/debug_drawer.cpp": {
    summary:
      "Builds D3D11 wireframe primitives and draws boxes, spheres, capsules, cylinders, and batched lines, including deferred queues flushed in the Draw phase.",
    tags: ["debug", "rendering", "service", "gizmo"],
    complexity: "complex",
    languageNotes:
      "Static GPU buffers shared across instances; DrawLinesCached remaps a persistent dynamic VB instead of recreating it each frame.",
  },
  "engine/debug_draw/debug_drawer.h": {
    summary:
      "Declares DebugDrawer wireframe drawing, deferred sphere/line queues, and cached multi-line submission used by colliders and gizmos.",
    tags: ["type-definition", "debug", "rendering", "gizmo"],
    complexity: "moderate",
  },
  "engine/ecs/game_object.cpp": {
    summary:
      "Implements GameObject update, destructor cleanup, trigger callbacks, collision-ignore lists, transform sync, reparenting, and visual/collider draw.",
    tags: ["ecs", "data-model", "lifecycle", "component"],
    complexity: "complex",
  },
  "engine/editor/project_manager.cpp": {
    summary:
      "Implements project create/open/save, recent-list tracking, engine-asset copy, gravity/audio bus activation, and the Windows folder picker.",
    tags: ["editor", "service", "project", "asset-pipeline"],
    complexity: "complex",
    languageNotes:
      "Uses Win32 IFileDialog plus nlohmann::json project files; scaffolds a content .sln and build scripts for new projects.",
  },
  "engine/physics/collider/collider.cpp": {
    summary:
      "Implements box, sphere, capsule, cylinder, and mesh colliders: world poses, contact generation, resolution, ray tests, and debug wireframes.",
    tags: ["physics", "collision", "data-model", "debug"],
    complexity: "complex",
    languageNotes:
      "Per-shape CheckCollision/ResolveCollision overrides plus shared ResolveContact that writes velocity, friction, and contact flashes.",
  },
  "engine/physics/collider/collider.h": {
    summary:
      "Declares the Collider hierarchy (box, sphere, capsule, cylinder, mesh) with trigger flags, ray tests, and debug-draw hooks.",
    tags: ["type-definition", "physics", "collision", "data-model"],
    complexity: "complex",
    languageNotes:
      "Classic virtual collider interface; mesh stores local vertices/indices and a padded bounding sphere.",
  },
  "engine/physics/core/physics_system.cpp": {
    summary:
      "Runs the play-mode physics tick: position snapshots, gravity integrate, swept broad phase, trigger enter/exit, raycasts, and contact-flash debug draw.",
    tags: ["physics", "service", "collision", "debug"],
    complexity: "complex",
    languageNotes:
      "Swept-AABB broad phase plus sub-stepped narrow phase to reduce tunneling on fast movers.",
  },
  "engine/physics/core/physics_system.h": {
    summary:
      "Public physics tick, trigger-pair, raycast, and debug-stat API used by play mode, scripts, and scene changes.",
    tags: ["type-definition", "physics", "api-handler", "collision"],
    complexity: "moderate",
  },
  "engine/physics/core/rigidbody.cpp": {
    summary:
      "Implements project gravity load plus Rigidbody force accumulation, velocity clear, and semi-implicit Euler integration with drag.",
    tags: ["physics", "data-model", "integration", "service"],
    complexity: "moderate",
  },
  "engine/physics/core/rigidbody.h": {
    summary:
      "Declares Rigidbody mass/drag/restitution/friction, ForceMode, and the engine gravity getters used during integration.",
    tags: ["type-definition", "physics", "data-model", "integration"],
    complexity: "moderate",
  },
  "engine/scripting/script_runtime_api.cpp": {
    summary:
      "Host-side ScriptRuntimeApi table and ScriptApi_* thunks that let content DLLs drive audio, physics, camera, UI, particles, and prefabs.",
    tags: ["api-handler", "scripting", "bridge", "entry-point"],
    complexity: "complex",
    languageNotes:
      "C-style function-pointer table (version 42) plus thin ScriptApi_* wrappers that call through ScriptRuntimeApi_GetHost().",
  },
  "src/app/scene/scene_manager.cpp": {
    summary:
      "Implements the SceneManager singleton that swaps SceneBase instances, clears physics trigger pairs, and forwards Update/Draw.",
    tags: ["singleton", "scene", "lifecycle", "service"],
    complexity: "moderate",
  },
  "src/app/scene/scene_manager.h": {
    summary:
      "Declares SceneBase object lists and the SceneManager singleton used to change, update, and draw the active scene.",
    tags: ["type-definition", "scene", "singleton", "lifecycle"],
    complexity: "simple",
  },
};

const fnSummary = {
  // audio
  Playback: "Internal voice record holding WAV bytes, an XAudio2 source voice, and optional X3DAudio emitter state.",
  Initialize: "Creates the XAudio2 engine, mastering voice, and X3DAudio handle.",
  Clamp01: "Clamps a scalar volume or mix value into the 0–1 range.",
  DestroyPlaybackVoice: "Stops and destroys an XAudio2 source voice on a Playback.",
  VoiceHasFinished: "Returns whether a non-looping source voice has drained.",
  Finalize: "Stops every voice and tears down XAudio2 resources.",
  Update: "Ticks 3D emitters, listener tracking, finished-instance sweep, and debug draw.",
  LoadWav: "Reads a WAV file into Playback audio bytes and WAVEFORMATEX.",
  PlaySFX3D: "Starts or restarts a named 3D SFX at a world position.",
  PlaySFX: "Starts or restarts a named 2D sound-effect clip.",
  PlayBGM: "Starts or restarts looping background music by id or path.",
  PlayVoice: "Starts or restarts a named voice-over clip.",
  LoadBGM: "Preloads a BGM WAV without starting playback.",
  LoadSFX: "Preloads a 2D SFX WAV without starting playback.",
  LoadVoice: "Preloads a voice WAV without starting playback.",
  Load3DSFX: "Preloads a 3D SFX and prepares its emitter.",
  Apply3DAudio: "Runs X3DAudio and applies the output matrix to a 3D voice.",
  Stop: "Stops and releases the named playback id.",
  StopAll: "Stops every named source currently playing.",
  StopAllByType: "Stops all voices of BGM, SFX, or VOICE type.",
  IsPlaying: "Reports whether the named source still has an active voice.",
  SetEmitterPosition: "Moves a 3D source emitter to a world position.",
  SetEmitterDistanceScale: "Sets how quickly a 3D source attenuates with distance.",
  SetEmitter: "Binds a 3D source emitter to a GameObject transform.",
  SetListenerStereoBalance: "Toggles listener stereo-balance processing.",
  SetListener: "Sets the 3D listener from an explicit pose or a tracked GameObject.",
  OnGameObjectDestroyed: "Clears listener/emitter pointers that referenced a freed GameObject.",
  SetVolume: "Sets per-source base volume and reapplies bus mix.",
  UpdateVolume: "Writes the combined bus volume onto a source voice.",
  ReapplyAllVolumes: "Recomputes output volume for every live source.",
  SetMasterVolume: "Sets master bus volume and reapplies all voices.",
  SetBGMVolume: "Sets the BGM bus volume.",
  SetSFXVolume: "Sets the SFX bus volume.",
  SetVoiceVolume: "Sets the voice-over bus volume.",
  DrawDebug: "Draws 3D emitter spheres through DebugDrawer when debug draw is on.",
  GetDebugInfo: "Returns distance and L/R matrix volumes for a 3D source.",
  Setup3DEmitter: "Initializes X3DAudio emitter fields and distance curve.",
  SubmitAndStart: "Submits the WAV buffer and starts the source voice.",
  SweepFinishedInstances: "Destroys instance voices that have finished playing.",
  StartInstance: "Allocates a unique AudioPlaybackId and starts a component-owned voice.",
  StartInstanceFromLoaded: "Starts an instance from a previously loaded named clip.",
  SetUiClickSound: "Preloads the 2D SFX used by UI button clicks.",
  PlayUiClick: "Plays the preloaded UI click SFX.",
  StopInstance: "Stops one component-owned playback id.",
  IsInstancePlaying: "Returns whether an instance id still has an active voice.",
  SetInstancePosition: "Updates a 3D instance emitter position.",
  SetInstanceDistanceScale: "Updates attenuation scale for an instance voice.",
  SetInstanceVolume: "Updates volume on an instance voice.",
  StopAllInstances: "Stops every component-owned instance voice.",
  StopSceneInstances: "Stops scene instances while leaving persistAcrossScene voices running.",
  LoadBusVolumesFromProjectFile: "Reads master/BGM/SFX/voice bus levels from project.json.",
  TryLoadBusVolumesFromNearbyProject: "Looks for a nearby project.json and loads bus volumes.",

  // debug system
  TimeScalePulseState: "Tracks an active hitstop-style time-scale pulse and remaining real-time duration.",
  ClearPulse: "Cancels any in-flight time-scale pulse.",
  ApplyFreezeCursor: "Shows or restores the third-person camera cursor when entering or leaving freeze.",
  Init: "Resets debug overlay, time scale, and collision-viz state.",
  Shutdown: "Clears overlay text and restores cursor ownership.",
  ToggleCollisionViz: "Toggles F11 collider wireframe visualization.",
  CollisionVizEnabled: "Returns whether collider debug draw is enabled.",
  GetTimeScale: "Returns the effective time scale including an active pulse.",
  PulseTimeScale: "Starts a short real-time hitstop pulse unless debug time-scale is already overridden.",
  UpdateTimeScalePulse: "Advances pulse remaining time using unscaled wall-clock delta.",
  CycleTimeScale: "Cycles debug time scale 1 → 0.5 → 0.25 → freeze → 1.",
  IsFrozen: "True when the effective time scale is zero.",
  StepOneFrame: "Requests a single Update tick while the world is frozen.",
  FrameStepRequested: "Returns whether a freeze frame-step is pending.",
  StepAdvance: "Returns a 1/60 dt if a step is pending and clears the request.",
  ConsumeFrameStep: "Consumes a pending frame-step request and returns whether one was waiting.",
  SetOverlayText: "Replaces the debug overlay string.",
  GetOverlayColor: "Returns overlay tint (red while frozen).",
  RefreshOverlay: "Rebuilds overlay lines from time scale, freeze, and physics debug stats.",
  ResetTimeScale: "Clears freeze, pulses, and debug time-scale when play ends.",

  // debug drawer
  "~DebugDrawer": "Releases GPU debug buffers if this instance still owns them.",
  Release: "Destroys DebugDrawer vertex/index buffers, texture, and depth state.",
  DrawBox: "Draws a wireframe OBB using the shared box mesh.",
  DrawSphere: "Draws a wireframe sphere, optionally with depth testing disabled.",
  DrawCapsule: "Draws a wireframe capsule from radius, height, and rotation.",
  DrawCylinder: "Draws a wireframe Y-aligned cylinder after rotation.",
  DrawLine: "Draws a single world-space line segment.",
  DrawLines: "Batches many line segments into one GPU draw.",
  DrawLinesCached: "Uploads line verts into a persistent dynamic VB and draws them.",
  QueueSphere: "Enqueues a sphere wire for the Draw-phase flush.",
  FlushSphereQueue: "Draws queued spheres without depth test and clears the queue.",
  QueueLine: "Enqueues a line segment for the Draw-phase flush.",
  FlushLineQueue: "Draws queued lines and clears the deferred line queue.",

  // game object
  IsInsideUpdate: "True while any GameObject::Update is on the call stack.",
  Update_IsParallelSafe: "Reports whether all attached scripts allow parallel Update.",
  "~GameObject": "Notifies audio and physics, detaches components, and deletes host-owned colliders.",
  OnTriggerEnter: "Forwards trigger-enter to the object's Script component.",
  OnTriggerExit: "Forwards trigger-exit to the object's Script component.",
  AddIgnoreCollisionTag: "Adds a tag this object should not collide with.",
  RemoveIgnoreCollisionTag: "Removes a collision-ignore tag.",
  AddIgnoreCollisionObject: "Ignores collisions against a specific other GameObject.",
  RemoveIgnoreCollisionObject: "Stops ignoring a specific GameObject pair.",
  ClearIgnoreCollisionObjects: "Clears all per-object collision ignores.",
  IgnoresCollisionWith: "True if tags or object-ignore lists skip this pair.",
  SyncLegacyTransformFromComponents: "Copies Transform component pose into legacy GameObject fields.",
  SyncComponentsFromLegacyTransform: "Writes legacy pose back onto the Transform component.",
  ReparentChild: "Moves a child under a new parent and keeps world pose stable.",
  GetWorldPose: "Reads world position, rotation, and scale from Transform.",
  DrawVisual: "Draws the visual/model and optional debug collider for this object.",

  // project manager
  CopyDirectoryRecursive: "Recursively copies a directory tree, reporting filesystem errors.",
  ReadDefaultSceneFileName: "Reads the default scene filename from a project root.",
  WriteDefaultSceneFile: "Writes the default-scene marker file under a project root.",
  SanitizeProjectFolderName: "Strips invalid path characters from a project folder name.",
  WriteContentProjectSolution: "Writes the content-project Visual Studio solution scaffold.",
  WriteContentBuildScriptsBat: "Writes Windows build helper scripts into a new project.",
  Utf8FromWide: "Converts a wide Windows string to UTF-8.",
  WideFromUtf8: "Converts UTF-8 to a wide Windows string.",
  GetExecutableDirectory: "Returns the directory of the running editor executable.",
  FindEngineRepoRoot: "Walks upward from the exe to locate the engine repository root.",
  RecentListPath: "Returns the path of the recent-projects JSON list.",
  LoadRecentList: "Loads the recent-projects list from disk.",
  SaveRecentList: "Writes the recent-projects list to disk.",
  TouchRecent: "Moves a project root to the front of the recent list.",
  MakeDefaultData: "Builds default ProjectData for a newly created root.",
  LoadProjectFile: "Parses project.json into ProjectData.",
  WriteProjectFile: "Serializes ProjectData to project.json.",
  EnsureEngineAssets: "Copies required engine assets into the project if missing.",
  ActivateRoot: "Applies gravity, audio buses, UI click SFX, and recent-list state for an opened root.",
  OpenProject: "Opens an existing project root and activates it.",
  ScaffoldNewProjectTree: "Creates folders, default scene, and content project files.",
  CreateProject: "Creates a new project under a parent directory.",
  SaveProjectFile: "Writes the in-memory project file back to disk.",
  SetLastOpenedScene: "Records the last opened scene filename.",
  SetDefaultScene: "Records the default scene filename.",
  SetEditorUiZoomPercent: "Stores editor UI zoom in project data.",
  SetPhysicsGravity: "Writes gravity into project data and the live physics singleton.",
  SetAudioBuses: "Writes bus volumes into project data.",
  ResolveSceneToLoad: "Chooses last-opened vs default scene for startup.",
  BootstrapAtStartup: "Opens the last project or prompts so the editor can start.",
  CloseProject: "Clears the active project root.",
  UpdateWindowTitle: "Sets the editor window title from the open project name.",
  PickFolder: "Shows the Win32 folder picker and returns the chosen path.",

  // collider
  GetOwnerPose: "Reads an owner's world position, rotation, and scale for collider math.",
  CollisionPairIgnored: "True when either GameObject ignores the other via tags or object lists.",
  MeshLocalTestRadius: "Converts a world test radius into mesh-local space.",
  LocalPenetrationToWorld: "Transforms a local mesh penetration vector into world space.",
  MeshSizePaddingScale: "Computes extra mesh collision padding from local bounds and owner scale.",
  StabilizeFloorContactNormal: "Snaps near-up contact normals to world up to reduce jitter.",
  BuildCylinderWorldFrame: "Builds the orthonormal frame used for cylinder contact tests.",
  WorldPointToCylinderLocal: "Converts a world point into cylinder-local coordinates.",
  ClosestPointOnCylinderLocal: "Finds the closest point on a local cylinder surface.",
  SphereCylinder_Contact: "Computes sphere-vs-cylinder contact normal and penetration.",
  ClosestPointOnOBB_World: "Returns the closest point on a rotated box to a world point.",
  SphereOBB_Contact: "Computes sphere-vs-OBB contact normal and penetration.",
  GetWorldCenter: "Returns the collider center in world space including owner pose.",
  GetSize: "Returns the box collider's full world-space size.",
  RayIntersect: "Ray-tests the collider shape and returns the nearest hit.",
  RayIntersectSphereWorld: "Analytic ray-vs-sphere test used by sphere and capsule rays.",
  ResolveContact: "Applies positional correction, bounce, and friction for a solid contact.",
  CheckCollision: "Dispatches shape-vs-shape overlap tests for the collider pair.",
  ResolveCollision: "Resolves overlap for the collider pair, including triggers.",
  DebugDraw: "Draws the collider wireframe through DebugDrawer.",
  SetMeshData: "Replaces mesh-collider vertices and indices and rebuilds the bounds.",
  BuildFromModel: "Fills a mesh collider from a Model3D's triangles.",
  GetWorldAABB: "Computes the mesh collider's world-space AABB.",
  CalculateBoundingSphere: "Rebuilds the local bounding sphere from mesh vertices.",
  GetWorldBoundingSphereRadius: "Returns the mesh bounding radius scaled by the owner.",
  Raycast: "Raycasts the triangle mesh and returns hit point and normal.",
  RayIntersectsTriangle: "Möller–Trumbore ray-triangle intersection helper.",
  TriangleIntersectsSphere: "Tests a triangle against a sphere and returns contact data.",
  CylinderWorldFrame: "Orthonormal cylinder frame (center, axes, radius, half-height) for contact tests.",

  // physics system
  InsertTriggerPair: "Records an overlapping trigger pair for this resolution pass.",
  DiffAndDispatchTriggerEvents: "Diffs previous/current trigger sets and fires enter/exit.",
  CommitContactPairs: "Promotes this pass's solid contacts into the previous-pass set.",
  IsDynamic: "True when the object has a non-kinematic rigidbody.",
  GetApproxRadius: "Approximates a broad-phase radius from collider type and size.",
  BroadPhasePass: "Sphere-overlap broad phase using current centers.",
  BroadPhasePassSwept: "Swept-sphere broad phase along previous-to-current motion.",
  DoTriggerOverlapCheck: "Runs CheckCollision for a trigger pair and records it.",
  DoSweptNarrowPhase: "Sub-steps a fast mover and resolves collisions along the sweep.",
  PhysicsSystem_ClearTriggerPairs: "Clears trigger tracking on scene change.",
  PhysicsSystem_NotifyObjectDestroyed: "Fires leftover trigger-exit and drops pairs that referenced the object.",
  PhysicsSystem_SavePositions: "Snapshots dynamic object positions before script motion.",
  PhysicsSystem_ResolveCollisions: "Full detect/resolve pass with swept broad phase and triggers.",
  PhysicsSystem_Raycast: "Nearest-hit raycast against active scene colliders.",
  PhysicsSystem_Update: "Integrates rigidbodies then resolves collisions for a play-mode tick.",
  PhysicsSystem_GetDebugStats: "Returns collider/pair/contact/trigger counters for the overlay.",
  PhysicsSystem_NotifyContactResolved: "Increments the resolved-contact debug counter.",
  PhysicsSystem_WasContactPairKnown: "True if this solid pair already contacted last pass.",
  PhysicsSystem_QueueContactFlash: "Queues a short-lived contact-flash debug primitive.",
  PhysicsSystem_PrepareContactFlashes: "Re-queues active flashes so Draw can flush them.",
  PhysicsSystem_ClearDebugDrawQueue: "Clears pending physics debug primitives.",
  PhysicsSystem_FlushDebugDraw: "Draws collider wires and contact flashes when viz is enabled.",
  ContactFlash: "Short-lived debug flash at a new contact point and normal tip.",
  BroadPhaseEntry: "Sortable broad-phase record with collider, dynamics flag, and sweep center.",

  // rigidbody
  LoadGravityFromProjectFile: "Reads physics.gravity from a project.json path.",
  Physics_GetGravity: "Returns the current engine gravity magnitude.",
  Physics_SetGravity: "Overrides the live gravity used during integration.",
  Physics_TryLoadGravityFromNearbyProject: "Loads gravity from cwd or exe-adjacent project.json.",
  AddForce: "Accumulates Force/Acceleration/Impulse/VelocityChange into the rigidbody.",
  ClearAllForces: "Zeros the force accumulator.",
  ClearVelocity: "Zeros selected velocity axes using a clear-flag vector.",
  Integrate: "Applies gravity, accumulated forces, drag, and Euler-integrates position.",

  // scene manager
  SceneManager: "Constructs an empty SceneManager with no current scene.",
  "~SceneManager": "Destroys the current scene on singleton teardown.",
  DestroyCurrentScene: "Finalizes and optionally deletes the active SceneBase.",
  ChangeScene: "Clears physics triggers, destroys the current scene, and initializes the next.",
};

function scriptApiSummary(name) {
  const table = {
    ScriptApi_Visual_PlayClip: "Script export that plays a named clip on a VisualComponent.",
    ScriptApi_Visual_TryGetBoneWorld: "Script export that reads a bone's world position and rotation.",
    ScriptApi_Visual_PlayClipLoop: "Script export that plays a visual clip with an explicit loop flag.",
    ScriptApi_Visual_GetClipDuration: "Script export that returns the active visual clip duration.",
    ScriptApi_Visual_SetAnimSpeed: "Script export that sets visual animation playback speed.",
    ScriptApi_Effect_Play: "Script export that starts an EffectComponent.",
    ScriptApi_Audio_Play: "Script export that starts an AudioComponent.",
    ScriptApi_Audio_Stop: "Script export that stops an AudioComponent.",
    ScriptApi_AudioSetMasterVolume: "Script export that sets AudioManager master volume.",
    ScriptApi_AudioGetMasterVolume: "Script export that reads AudioManager master volume.",
    ScriptApi_MouseGetPosition: "Script export that writes the current mouse position.",
    ScriptApi_PostFX_Burst: "Script export that triggers a post-process RGB-shift burst.",
    ScriptApi_Physics_Raycast: "Script export that raycasts the physics world and returns a hit.",
    ScriptApi_Camera_AddTrauma: "Script export that adds camera shake trauma.",
    ScriptApi_Camera_SetSpeedFactor: "Script export that scales camera move speed.",
    ScriptApi_Debug_PulseTimeScale: "Script export that pulses DebugSystem time scale for hitstop.",
    ScriptApi_LineSprite_Draw: "Script export that draws a world-space line sprite.",
    ScriptApi_ApplyColliderComponent: "Script export that applies a ColliderComponent onto its owner.",
    ScriptApi_DestroyCollider: "Script export that destroys a GameObject's collider.",
    ScriptApi_GameObjectAddIgnoreCollisionTag: "Script export that adds a collision-ignore tag on a GameObject.",
    ScriptApi_GameObjectRemoveIgnoreCollisionTag: "Script export that removes a collision-ignore tag.",
    ScriptApi_GameObjectAddIgnoreCollisionObject: "Script export that ignores collisions with another object.",
    ScriptApi_GameObjectRemoveIgnoreCollisionObject: "Script export that stops ignoring a specific object pair.",
    ScriptApi_GameObjectClearIgnoreCollisionObjects: "Script export that clears per-object collision ignores.",
    ScriptApi_GetColliderWorldCenter: "Script export that returns a collider's world-space center.",
    ScriptApi_GetBoxColliderSize: "Script export that reads a box collider's size.",
    ScriptApi_SetBoxColliderSize: "Script export that writes a box collider's size.",
    ScriptApi_TryGetColliderFootprint: "Script export that returns a collider footprint center and size.",
    ScriptApi_EngineGUIStopPlay: "Script export that stops editor play mode.",
    ScriptApi_CameraSetPosition: "Script export that sets the active camera position.",
    ScriptApi_CameraSetTarget: "Script export that sets the active camera look target.",
    ScriptApi_CameraSetUp: "Script export that sets the active camera up vector.",
    ScriptApi_SetActiveCameraNodeName: "Script export that selects the active scene camera node.",
    ScriptApi_ActivateSceneCamera: "Script export that activates a named scene camera.",
    ScriptApi_GetBackBufferWidth: "Script export that returns backbuffer width.",
    ScriptApi_GetBackBufferHeight: "Script export that returns backbuffer height.",
    ScriptApi_SpriteDraw: "Script export that draws a 2D sprite with UV, rotation, and color.",
    ScriptApi_TextureGetWhiteTexture: "Script export that returns the shared white texture id.",
    ScriptApi_TextureGetWidth: "Script export that returns a texture's width.",
    ScriptApi_TextureGetHeight: "Script export that returns a texture's height.",
    ScriptApi_TextureRelease: "Script export that releases a loaded texture id.",
    ScriptApi_EngineGUILoadFromFile: "Script export that loads a scene file through EngineGUI.",
    ScriptApi_EngineGUIStartPlay: "Script export that starts editor play mode.",
    ScriptApi_GetSphereColliderRadius: "Script export that reads a sphere collider radius.",
    ScriptApi_GetCameraForward: "Script export that returns the camera forward vector.",
    ScriptApi_UICanvasRemoveElement: "Script export that removes a named element from a UI canvas.",
    ScriptApi_UISpriteSetTexture: "Script export that assigns a texture path to a UI sprite.",
    ScriptApi_UISpriteSetRadial360Fill: "Script export that applies radial-360 fill to a UI sprite.",
    ScriptApi_UISpriteSetFillNone: "Script export that clears UI sprite fill.",
    ScriptApi_UISpriteSetSourceRect: "Script export that sets a UI sprite source rectangle.",
    ScriptApi_TextureLoad: "Script export that loads a texture file and returns its id.",
    ScriptApi_BillboardAdd: "Script export that spawns a world billboard.",
    ScriptApi_BillboardAddUv: "Script export that spawns a billboard with explicit UVs.",
    ScriptApi_ParticleCreateEmitter: "Script export that creates a particle emitter.",
    ScriptApi_ParticleSetEmitterPosition: "Script export that moves a particle emitter.",
    ScriptApi_ParticleSetEmitterEmitting: "Script export that toggles emitter emission.",
    ScriptApi_ParticleStopEmitter: "Script export that stops an emitter, optionally destroying it when empty.",
    ScriptApi_ParticleDestroyEmitter: "Script export that destroys a particle emitter immediately.",
    ScriptApi_ParticleSpawnAt: "Script export that spawns a particle at a world position.",
    ScriptApi_GetRenderViewportWidth: "Script export that returns render viewport width.",
    ScriptApi_GetRenderViewportHeight: "Script export that returns render viewport height.",
    ScriptApi_GetCameraViewMatrix: "Script export that writes the camera view matrix.",
    ScriptApi_GetCameraProjMatrix: "Script export that writes the camera projection matrix.",
    ScriptApi_DebugDrawSphereWire: "Script export that queues a debug wireframe sphere.",
    ScriptApi_SceneCanvasSetTimerSeconds: "Script export that sets the scene-canvas timer.",
    ScriptApi_SceneCanvasRequestSceneFile: "Script export that requests loading another scene file.",
    ScriptApi_UITextSetText: "Script export that sets a UI text element's string.",
    ScriptApi_UIElementSetVisible: "Script export that shows or hides a UI element.",
    ScriptApi_PickupDebugSetMessage: "Script export that shows a timed pickup debug message.",
    ScriptApi_PrefabDestroy: "Script export that destroys a prefab instance.",
    ScriptApi_PrefabSetBeforeDestroyCallback: "Script export that registers a prefab pre-destroy callback.",
  };
  return table[name] || `Script-DLL host export ${name} that forwards to the runtime API table.`;
}

function tagsFor(filePath, name, kind) {
  if (name.startsWith("ScriptApi_")) {
    if (name.includes("Audio")) return ["scripting", "audio", "api-handler", "bridge"];
    if (name.includes("Particle")) return ["scripting", "particles", "api-handler"];
    if (name.includes("Camera") || name.includes("Camera")) return ["scripting", "camera", "api-handler"];
    if (name.includes("Collider") || name.includes("Physics") || name.includes("IgnoreCollision"))
      return ["scripting", "physics", "api-handler"];
    if (name.includes("UI") || name.includes("Sprite") || name.includes("Texture") || name.includes("Billboard"))
      return ["scripting", "ui", "api-handler"];
    if (name.includes("Visual") || name.includes("Effect")) return ["scripting", "animation", "api-handler"];
    if (name.includes("Prefab")) return ["scripting", "prefab", "api-handler"];
    if (name.includes("Debug") || name.includes("Draw")) return ["scripting", "debug", "api-handler"];
    return ["scripting", "api-handler", "bridge"];
  }
  if (filePath.includes("audio")) return ["audio", kind === "class" ? "data-model" : "service", "playback"];
  if (filePath.includes("debug_system")) return ["debug", "time-scale", "service"];
  if (filePath.includes("debug_drawer")) return ["debug", "rendering", "gizmo"];
  if (filePath.includes("game_object")) return ["ecs", "lifecycle", "component"];
  if (filePath.includes("project_manager")) return ["editor", "project", "service"];
  if (filePath.includes("collider")) return ["physics", "collision", kind === "class" ? "data-model" : "utility"];
  if (filePath.includes("physics_system")) return ["physics", "collision", "service"];
  if (filePath.includes("rigidbody")) return ["physics", "integration", "data-model"];
  if (filePath.includes("scene_manager")) return ["scene", "lifecycle", "singleton"];
  return ["utility", "service", "implementation"];
}

function complexityFromLen(len) {
  if (len > 200) return "complex";
  if (len >= 50) return "moderate";
  return "simple";
}

function summaryFor(filePath, name, kind) {
  if (fnSummary[name]) return fnSummary[name];
  if (name.startsWith("ScriptApi_")) return scriptApiSummary(name);
  if (filePath.includes("debug_drawer") && name === "Initialize")
    return "Creates wireframe GPU meshes, a debug texture, and the no-depth stencil state.";
  if (filePath.includes("debug_system") && name === "Initialize")
    return "Initializes debug overlay and time-scale state.";
  if (filePath.includes("game_object") && name === "Update")
    return "Updates components, runs LogicUpdate and physics integrate, then recurses to children.";
  if (filePath.includes("scene_manager") && name === "Update")
    return "Forwards elapsed time to the current SceneBase.";
  if (filePath.includes("scene_manager") && name === "Draw")
    return "Forwards Draw to the current SceneBase.";
  if (filePath.includes("project_manager") && name === "SetUiClickSound")
    return "Stores the UI click path on the project and pushes it to AudioManager.";
  return `${kind === "class" ? "Internal type" : "Implements"} ${name} in ${path.basename(filePath)}.`;
}

const nodes = [];
const edges = [];
const emittedIds = new Set();

function addNode(n) {
  if (emittedIds.has(n.id)) return;
  emittedIds.add(n.id);
  nodes.push(n);
}

function addEdge(e) {
  if (e.source === e.target) return;
  edges.push(e);
}

const resultByPath = new Map(extract.results.map((r) => [r.path, r]));

for (const file of input.batchFiles) {
  const meta = fileMeta[file.path];
  if (!meta) throw new Error("missing file meta " + file.path);
  const node = {
    id: "file:" + file.path,
    type: "file",
    name: path.basename(file.path),
    filePath: file.path,
    summary: meta.summary,
    tags: meta.tags,
    complexity: meta.complexity,
  };
  if (meta.languageNotes) node.languageNotes = meta.languageNotes;
  addNode(node);
}

for (const r of extract.results) {
  const exports = new Set((r.exports || []).map((e) => e.name));
  const byName = new Map();
  for (const f of r.functions || []) {
    const len = f.endLine - f.startLine + 1;
    const cur = byName.get(f.name);
    if (!cur || len > cur.len) {
      byName.set(f.name, {
        name: f.name,
        start: f.startLine,
        end: f.endLine,
        len,
        exported: exports.has(f.name),
      });
    } else if (exports.has(f.name)) {
      cur.exported = true;
    }
  }
  for (const f of byName.values()) {
    if (!(f.exported || f.len >= 10)) continue;
    const id = `function:${r.path}:${f.name}`;
    addNode({
      id,
      type: "function",
      name: f.name,
      filePath: r.path,
      lineRange: [f.start, f.end],
      summary: summaryFor(r.path, f.name, "function"),
      tags: tagsFor(r.path, f.name, "function"),
      complexity: complexityFromLen(f.len),
    });
    addEdge({
      source: "file:" + r.path,
      target: id,
      type: "contains",
      direction: "forward",
      weight: 1.0,
    });
    if (f.exported) {
      addEdge({
        source: "file:" + r.path,
        target: id,
        type: "exports",
        direction: "forward",
        weight: 0.8,
      });
    }
  }

  for (const c of r.classes || []) {
    const len = c.endLine - c.startLine + 1;
    const exported = exports.has(c.name);
    const significant = exported || (c.methods || []).length >= 2 || len >= 20;
    if (!significant) continue;
    const id = `class:${r.path}:${c.name}`;
    addNode({
      id,
      type: "class",
      name: c.name,
      filePath: r.path,
      lineRange: [c.startLine, c.endLine],
      summary: summaryFor(r.path, c.name, "class"),
      tags: tagsFor(r.path, c.name, "class"),
      complexity: complexityFromLen(len),
    });
    addEdge({
      source: "file:" + r.path,
      target: id,
      type: "contains",
      direction: "forward",
      weight: 1.0,
    });
    if (exported) {
      addEdge({
        source: "file:" + r.path,
        target: id,
        type: "exports",
        direction: "forward",
        weight: 0.8,
      });
    }
  }
}

// Import edges — 1:1 with batchImportData
let importCount = 0;
for (const [src, targets] of Object.entries(batchImportData)) {
  for (const t of targets) {
    addEdge({
      source: "file:" + src,
      target: "file:" + t,
      type: "imports",
      direction: "forward",
      weight: 0.7,
    });
    importCount++;
  }
}

const expectedImports = Object.values(batchImportData).reduce((s, v) => s + v.length, 0);
if (importCount !== expectedImports) {
  throw new Error(`import mismatch ${importCount} != ${expectedImports}`);
}

const calls = [
  ["function:engine/audio/audio_manager.cpp:DrawDebug", "function:engine/debug_draw/debug_drawer.cpp:DrawSphere"],
  ["function:engine/core/debug_system.cpp:RefreshOverlay", "function:engine/physics/core/physics_system.cpp:PhysicsSystem_GetDebugStats"],
  ["function:engine/ecs/game_object.cpp:~GameObject", "function:engine/audio/audio_manager.cpp:OnGameObjectDestroyed"],
  ["function:engine/ecs/game_object.cpp:~GameObject", "function:engine/physics/core/physics_system.cpp:PhysicsSystem_NotifyObjectDestroyed"],
  ["function:engine/ecs/game_object.cpp:Update_IsParallelSafe", "function:engine/scripting/script_component.h:GetScriptClass"],
  ["function:engine/ecs/game_object.cpp:GetWorldPose", "function:engine/ecs/transform.h:GetWorldPositionRotationScale"],
  ["function:engine/ecs/game_object.cpp:DrawVisual", "function:engine/ecs/transform.h:GetWorldPositionRotationScale"],
  ["function:engine/ecs/game_object.cpp:DrawVisual", "function:engine/physics/collider/collider.cpp:DebugDraw"],
  ["function:engine/editor/project_manager.cpp:FindEngineRepoRoot", "function:engine/editor/project_manager.h:GetExecutableDirectory"],
  ["function:engine/editor/project_manager.cpp:RecentListPath", "function:engine/editor/project_manager.h:GetExecutableDirectory"],
  ["function:engine/editor/project_manager.cpp:EnsureEngineAssets", "function:engine/editor/project_manager.h:GetExecutableDirectory"],
  ["function:engine/editor/project_manager.cpp:ActivateRoot", "function:engine/physics/core/rigidbody.cpp:Physics_SetGravity"],
  ["function:engine/editor/project_manager.cpp:ActivateRoot", "function:engine/audio/audio_manager.cpp:SetMasterVolume"],
  ["function:engine/editor/project_manager.cpp:ActivateRoot", "function:engine/audio/audio_manager.cpp:SetBGMVolume"],
  ["function:engine/editor/project_manager.cpp:ActivateRoot", "function:engine/audio/audio_manager.cpp:SetSFXVolume"],
  ["function:engine/editor/project_manager.cpp:ActivateRoot", "function:engine/audio/audio_manager.cpp:SetVoiceVolume"],
  ["function:engine/editor/project_manager.cpp:ActivateRoot", "function:engine/audio/audio_manager.cpp:SetUiClickSound"],
  ["function:engine/editor/project_manager.cpp:ActivateRoot", "function:engine/editor/project_manager.h:TouchRecent"],
  ["function:engine/editor/project_manager.cpp:SetPhysicsGravity", "function:engine/physics/core/rigidbody.cpp:Physics_SetGravity"],
  ["function:engine/editor/project_manager.cpp:SetUiClickSound", "function:engine/audio/audio_manager.cpp:SetUiClickSound"],
  ["function:engine/physics/collider/collider.cpp:CollisionPairIgnored", "function:engine/ecs/game_object.cpp:IgnoresCollisionWith"],
  ["function:engine/physics/collider/collider.cpp:ResolveContact", "function:engine/physics/core/physics_system.cpp:PhysicsSystem_NotifyContactResolved"],
  ["function:engine/physics/collider/collider.cpp:ResolveContact", "function:engine/physics/core/physics_system.cpp:PhysicsSystem_WasContactPairKnown"],
  ["function:engine/physics/collider/collider.cpp:ResolveContact", "function:engine/physics/core/physics_system.cpp:PhysicsSystem_QueueContactFlash"],
  ["function:engine/physics/collider/collider.cpp:DebugDraw", "function:engine/debug_draw/debug_drawer.cpp:DrawBox"],
  ["function:engine/physics/collider/collider.cpp:DebugDraw", "function:engine/debug_draw/debug_drawer.cpp:DrawSphere"],
  ["function:engine/physics/collider/collider.cpp:DebugDraw", "function:engine/debug_draw/debug_drawer.cpp:DrawCapsule"],
  ["function:engine/physics/collider/collider.cpp:DebugDraw", "function:engine/debug_draw/debug_drawer.cpp:DrawCylinder"],
  ["function:engine/physics/collider/collider.cpp:DebugDraw", "function:engine/debug_draw/debug_drawer.cpp:DrawLinesCached"],
  ["function:engine/physics/core/physics_system.cpp:DiffAndDispatchTriggerEvents", "function:engine/ecs/game_object.cpp:OnTriggerEnter"],
  ["function:engine/physics/core/physics_system.cpp:DiffAndDispatchTriggerEvents", "function:engine/ecs/game_object.cpp:OnTriggerExit"],
  ["function:engine/physics/core/physics_system.cpp:GetApproxRadius", "function:engine/physics/collider/collider.cpp:GetSize"],
  ["function:engine/physics/core/physics_system.cpp:GetApproxRadius", "function:engine/physics/collider/collider.cpp:GetWorldBoundingSphereRadius"],
  ["function:engine/physics/core/physics_system.cpp:BroadPhasePass", "function:engine/physics/collider/collider.cpp:GetWorldCenter"],
  ["function:engine/physics/core/physics_system.cpp:BroadPhasePassSwept", "function:engine/physics/collider/collider.cpp:GetWorldCenter"],
  ["function:engine/physics/core/physics_system.cpp:DoTriggerOverlapCheck", "function:engine/physics/collider/collider.cpp:CheckCollision"],
  ["function:engine/physics/core/physics_system.cpp:DoSweptNarrowPhase", "function:engine/physics/collider/collider.cpp:CheckCollision"],
  ["function:engine/physics/core/physics_system.cpp:DoSweptNarrowPhase", "function:engine/physics/collider/collider.cpp:ResolveCollision"],
  ["function:engine/physics/core/physics_system.cpp:PhysicsSystem_NotifyObjectDestroyed", "function:engine/ecs/game_object.cpp:OnTriggerExit"],
  ["function:engine/physics/core/physics_system.cpp:PhysicsSystem_ResolveCollisions", "function:engine/physics/collider/collider.cpp:GetWorldCenter"],
  ["function:engine/physics/core/physics_system.cpp:PhysicsSystem_Raycast", "function:engine/physics/collider/collider.cpp:GetWorldCenter"],
  ["function:engine/physics/core/physics_system.cpp:PhysicsSystem_Raycast", "function:engine/physics/collider/collider.cpp:RayIntersect"],
  ["function:engine/physics/core/physics_system.cpp:PhysicsSystem_Update", "function:engine/physics/core/rigidbody.cpp:Integrate"],
  ["function:engine/physics/core/physics_system.cpp:PhysicsSystem_FlushDebugDraw", "function:engine/physics/collider/collider.cpp:DebugDraw"],
  ["function:src/app/scene/scene_manager.cpp:ChangeScene", "function:engine/physics/core/physics_system.cpp:PhysicsSystem_ClearTriggerPairs"],
];

for (const [source, target] of calls) {
  addEdge({
    source,
    target,
    type: "calls",
    direction: "forward",
    weight: 0.8,
  });
}

// Split
const filesAlpha = input.batchFiles.map((f) => f.path).sort();
const nodeCount = nodes.length;
const edgeCount = edges.length;
console.log("totals", { nodeCount, edgeCount, importCount, files: filesAlpha.length });

const outDir = "C:/Users/terry/source/repos/ivy-engine/.ua/intermediate";
fs.mkdirSync(outDir, { recursive: true });

function writePart(filename, partNodes, partEdges) {
  const payload = { nodes: partNodes, edges: partEdges };
  const outPath = path.join(outDir, filename);
  fs.writeFileSync(outPath, JSON.stringify(payload, null, 2));
  return outPath;
}

const neighborFiles = new Set();
const neighborSymbols = new Set();
for (const list of Object.values(neighbors)) {
  for (const n of list) {
    neighborFiles.add(n.path);
    for (const s of n.symbols || []) neighborSymbols.add(`${n.path}:${s}`);
  }
}
for (const list of Object.values(batchImportData)) {
  for (const p of list) neighborFiles.add(p);
}

function edgeOk(e, partIds) {
  const ok = (id) => {
    if (partIds.has(id)) return true;
    if (id.startsWith("file:")) {
      const p = id.slice(5);
      return neighborFiles.has(p) || filesAlpha.includes(p);
    }
    const m = id.match(/^(function|class):(.+):(.+)$/);
    if (m) {
      const p = m[2];
      const sym = m[3];
      if (emittedIds.has(id)) return true;
      if (neighborSymbols.has(`${p}:${sym}`)) return true;
      if (neighborFiles.has(p)) return true;
    }
    return false;
  };
  return ok(e.source) && ok(e.target);
}

let partsWritten = [];
if (nodeCount <= 60 && edgeCount <= 120) {
  const p = writePart("batch-9.json", nodes, edges);
  partsWritten.push({ file: p, nodes: nodeCount, edges: edgeCount });
} else {
  const parts = Math.ceil(Math.max(nodeCount / 60, edgeCount / 120));
  const chunk = Math.ceil(filesAlpha.length / parts);
  console.log("splitting into", parts, "parts, chunk", chunk);
  for (let i = 0; i < parts; i++) {
    const slice = filesAlpha.slice(i * chunk, (i + 1) * chunk);
    if (!slice.length) continue;
    const sliceSet = new Set(slice);
    const partNodes = nodes.filter((n) => sliceSet.has(n.filePath));
    const partIds = new Set(partNodes.map((n) => n.id));
    const partEdges = edges.filter((e) => partIds.has(e.source));
    const bad = partEdges.filter((e) => !edgeOk(e, partIds));
    if (bad.length) {
      console.error("VALIDATION FAIL part", i + 1, bad);
      throw new Error("edge validation failed part " + (i + 1));
    }
    const fname = `batch-9-part-${i + 1}.json`;
    const p = writePart(fname, partNodes, partEdges);
    partsWritten.push({ file: fname, nodes: partNodes.length, edges: partEdges.length, files: slice });
  }
}

console.log(JSON.stringify(partsWritten, null, 2));
console.log("files skipped: none");
