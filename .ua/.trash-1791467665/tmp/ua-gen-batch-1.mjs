import fs from "fs";

const UA = "C:/Users/terry/source/repos/ivy-engine/.ua";
const extract = JSON.parse(fs.readFileSync(`${UA}/tmp/ua-file-extract-results-1.json`, "utf8"));
const input = JSON.parse(fs.readFileSync(`${UA}/tmp/ua-file-analyzer-input-1.json`, "utf8"));
const neighborMap = JSON.parse(fs.readFileSync(`${UA}/tmp/ua-file-analyzer-neighbor-1.json`, "utf8"));

const FWD_DECL = new Set(["DebugDrawer", "GameObject", "Transform", "CameraComponent", "HWND__"]);

const FILE_META = {
  "engine/core/engine_gui.cpp": {
    summary: "ImGui editor shell that owns scene hierarchy, inspector, play-mode sessions, project workflows, and viewport picking for Ivy Engine.",
    tags: ["entry-point", "editor", "imgui", "scene-graph"],
    complexity: "complex",
    languageNotes: "Single large C++ translation unit: public EngineGUI_* API plus internal ImGui windows, ray picking, and play-session teardown.",
  },
  "engine/core/game_object.h": {
    summary: "Compatibility header that forwards to the ECS GameObject definition so app-layer code can include a core path.",
    tags: ["barrel", "data-model", "compatibility"],
    complexity: "simple",
  },
  "engine/ecs/collider_component.cpp": {
    summary: "Applies serialized collider shapes onto a GameObject, creating physics colliders from boxes, spheres, capsules, cylinders, or cached mesh data.",
    tags: ["ecs", "physics", "serialization", "component"],
    complexity: "moderate",
  },
  "engine/ecs/model_asset_cache.cpp": {
    summary: "Singleton cache that indexes rom/model assets and lazily loads shared Model3D GPU resources for visuals and colliders.",
    tags: ["cache", "asset-pipeline", "singleton", "service"],
    complexity: "moderate",
  },
  "engine/ecs/model_asset_cache.h": {
    summary: "Declares ModelAssetCache, the shared FBX/model directory index and GPU model lookup used by VisualComponent and the editor.",
    tags: ["type-definition", "cache", "ecs", "asset-pipeline"],
    complexity: "simple",
  },
  "engine/ecs/visual_component.cpp": {
    summary: "Implements VisualComponent animation, proxy/real model lookup, SurfaceFX textures, and JSON serialize/deserialize for mesh instances.",
    tags: ["ecs", "component", "animation", "serialization"],
    complexity: "moderate",
  },
  "engine/ecs/visual_component.h": {
    summary: "ECS visual component that binds a GameObject to a cached Model3D, optional proxy mesh, animation clips, and per-instance shader overrides.",
    tags: ["ecs", "component", "type-definition", "rendering"],
    complexity: "moderate",
    languageNotes: "Inherits Component; GetModel() is an inline cache lookup so editor and play paths share ModelAssetCache.",
  },
  "engine/editor/camera_gizmo.h": {
    summary: "Declares CameraGizmo debug-draw helpers that render camera frustums for selected and scene camera nodes.",
    tags: ["editor", "gizmo", "debug-draw", "type-definition"],
    complexity: "simple",
  },
  "engine/editor/editor_project_browser.h": {
    summary: "Declares the MapEditor project-browser panel entry used by the ImGui editor to pick and spawn project assets.",
    tags: ["editor", "ui", "type-definition", "assets"],
    complexity: "simple",
  },
  "engine/editor/playable_package_builder.cpp": {
    summary: "Editor build pipeline that finds MSBuild, compiles the content project, and copies a playable game package to a user destination.",
    tags: ["editor", "build-system", "packaging", "service"],
    complexity: "complex",
    languageNotes: "Win32 process spawn plus recursive file copy; build runs asynchronously with a polling log.",
  },
  "engine/editor/playable_package_builder.h": {
    summary: "Public PlayablePackage API for configuration prefs, destination checks, and async playable-package builds from the editor.",
    tags: ["editor", "type-definition", "packaging", "build-system"],
    complexity: "simple",
  },
  "engine/editor/project_asset_index.cpp": {
    summary: "Walks the open project's rom/ tree, classifies files by kind, and rebuilds the folder index used by the project browser.",
    tags: ["editor", "assets", "index", "service"],
    complexity: "moderate",
  },
  "engine/editor/project_asset_index.h": {
    summary: "Singleton project asset index that classifies rom/ files (models, scenes, prefabs, scripts) and exposes folder contents to the editor.",
    tags: ["editor", "type-definition", "assets", "singleton"],
    complexity: "simple",
  },
  "engine/editor/project_asset_ops.cpp": {
    summary: "Filesystem operations for editor assets: rename, recycle-bin delete, folder/scene creation, Explorer reveal, and scene-reference search.",
    tags: ["editor", "assets", "filesystem", "service"],
    complexity: "moderate",
    languageNotes: "Uses Windows recycle-bin deletion and path normalization so scene files are not deleted while open.",
  },
  "engine/editor/project_asset_ops.h": {
    summary: "Static ProjectAssetOps surface for safe rename/delete/create of project files from the editor browser.",
    tags: ["editor", "type-definition", "assets", "filesystem"],
    complexity: "simple",
  },
  "engine/editor/project_manager.h": {
    summary: "Singleton that opens Unity-style content projects (project.json, rom/, game/scripts), tracks recents, and stores editor/physics/audio settings.",
    tags: ["editor", "singleton", "configuration", "type-definition"],
    complexity: "moderate",
  },
  "engine/editor/transform_gizmo.h": {
    summary: "Declares the TransformGizmo used to move, rotate, and scale the selected GameObject in the editor viewport.",
    tags: ["editor", "gizmo", "type-definition", "transform"],
    complexity: "simple",
  },
  "engine/rendering/d3d11/shader.h": {
    summary: "D3D11 shader API: 3D variants (standard, toon, PBR, lights), skinning palette, SurfaceFX params, and cascaded shadow matrix binds.",
    tags: ["rendering", "d3d11", "type-definition", "shaders"],
    complexity: "moderate",
    languageNotes: "SURFACE_FX_PARAMS is a CPU mirror of constant buffer b4 and must stay 16-byte aligned.",
  },
  "engine/rendering/lighting/shadow.cpp": {
    summary: "Cascaded directional shadow-map implementation: camera-fitted ortho volumes, texel snap, and per-cascade depth-pass begin/end.",
    tags: ["rendering", "shadows", "lighting", "d3d11"],
    complexity: "complex",
    languageNotes: "Fits each cascade to frustum corners in light space, then snaps the ortho projection to shadow-map texels.",
  },
  "engine/rendering/lighting/shadow.h": {
    summary: "Public Shadow namespace for cascaded shadow maps: init/reconfigure, light direction, cascade matrices, and depth-pass control.",
    tags: ["rendering", "shadows", "type-definition", "lighting"],
    complexity: "simple",
  },
  "engine/rendering/model/model.cpp": {
    summary: "Model3D loader and runtime: OBJ/FBX import, GPU/CPU skinning, clip baking, and D3D11 draw with material and shadow binds.",
    tags: ["rendering", "assets", "animation", "fbx"],
    complexity: "complex",
    languageNotes: "Non-copyable because it owns D3D and FBX pointers; animation instances are cloned via CreateAnimInstanceFrom.",
  },
  "engine/rendering/model/model.h": {
    summary: "Declares Model3D, the engine's GPU mesh/animation object with FBX bones, clip playback, and per-draw material overrides.",
    tags: ["rendering", "type-definition", "animation", "fbx"],
    complexity: "moderate",
  },
  "engine/rendering/model/model_util.h": {
    summary: "Shared mesh/texture utilities for OBJ/FBX import: path helpers, WIC texture load, and vertex-key hashing used by Model3D.",
    tags: ["utility", "rendering", "assets", "type-definition"],
    complexity: "moderate",
  },
  "engine/scene/render_settings.cpp": {
    summary: "Loads and applies scene render settings (skybox, fog, default shader, shadow quality) and rebuilds shadow maps when quality changes.",
    tags: ["configuration", "rendering", "serialization", "scene"],
    complexity: "moderate",
  },
  "engine/scene/render_settings.h": {
    summary: "Scene-wide render settings data and accessors for skybox, fog, default shader variant, and shadow-quality presets.",
    tags: ["configuration", "type-definition", "rendering", "scene"],
    complexity: "simple",
  },
  "engine/scene/scene_asset_preload.cpp": {
    summary: "Time-sliced preloader that walks GameObjects or scene JSON, then warms ModelAssetCache models and animation clips on the main thread.",
    tags: ["assets", "scene", "preload", "service"],
    complexity: "moderate",
    languageNotes: "D3D11/LoadFBX work is stepped one item per Tick so the editor can show a progress bar.",
  },
  "engine/scene/scene_asset_preload.h": {
    summary: "Declares SceneAssetPreloader and task-list builders used by the editor and GAME_RUNTIME boot to warm models before play.",
    tags: ["assets", "type-definition", "scene", "preload"],
    complexity: "simple",
  },
  "engine/scripting/script_watcher.cpp": {
    summary: "Win32 directory watcher that detects GameScripts.dll changes on a worker thread and invokes a hot-reload callback.",
    tags: ["scripting", "hot-reload", "filesystem", "service"],
    complexity: "moderate",
    languageNotes: "Uses ReadDirectoryChangesW with an overlapped event and a stop HANDLE to join the watch thread.",
  },
  "engine/scripting/script_watcher.h": {
    summary: "ScriptWatcher class that monitors a folder for DLL changes so the editor can hot-reload GameScripts.",
    tags: ["scripting", "hot-reload", "type-definition", "service"],
    complexity: "simple",
  },
  "src/app/rendering/grid.cpp": {
    summary: "Builds and draws the editor ground grid as a line mesh GameObject using the default 3D shader and scene camera.",
    tags: ["rendering", "editor", "debug-draw", "component"],
    complexity: "moderate",
  },
  "src/app/rendering/grid.h": {
    summary: "Grid GameObject used as the editor ground plane, with C helpers to initialize, finalize, and draw the singleton instance.",
    tags: ["rendering", "editor", "type-definition", "component"],
    complexity: "simple",
    languageNotes: "Inherits GameObject so the grid participates in the scene object's initialize/draw lifecycle.",
  },
};

const SYMBOL_SUMMARIES = {
  "engine/core/engine_gui.cpp:EditorLayoutMetrics": "Cached pixel metrics for the editor chrome: menu bar, toolbar, and pane rectangles used to lay out ImGui windows.",
  "engine/core/engine_gui.cpp:SyncImGuiInputAcceptance": "Enables or blocks ImGui input queues when the editor should not consume mouse and keyboard events.",
  "engine/core/engine_gui.cpp:SaveUiPrefs": "Writes editor UI zoom and related prefs to the on-disk ui_prefs JSON file.",
  "engine/core/engine_gui.cpp:LoadUiPrefs": "Reads saved UI zoom prefs and applies a valid zoom percent at editor startup.",
  "engine/core/engine_gui.cpp:ApplyUiZoom": "Scales ImGui style sizes from a zoom percent so the editor UI stays readable.",
  "engine/core/engine_gui.cpp:EngineGUI_IsSceneDirty": "Returns whether the open scene has unsaved editor changes.",
  "engine/core/engine_gui.cpp:EngineGUI_MarkSceneDirty": "Marks the open scene dirty so save prompts and title-bar state update.",
  "engine/core/engine_gui.cpp:EngineGUI_ClearSceneDirty": "Clears the unsaved-scene flag after a successful save or discard.",
  "engine/core/engine_gui.cpp:EngineGUI_QueueUnsavedLoad": "Queues a scene load that must first confirm discarding unsaved edits.",
  "engine/core/engine_gui.cpp:EngineGUI_QueueUnsavedExit": "Queues editor exit so an unsaved-changes dialog can run first.",
  "engine/core/engine_gui.cpp:RemapNodeTagsAfterDelete": "Rewrites tag IDs on a subtree after a node is deleted so remaining tags stay consistent.",
  "engine/core/engine_gui.cpp:NormalizeSceneFileName": "Normalizes a scene name into a safe filename used under rom/scene.",
  "engine/core/engine_gui.cpp:EngineGUI_IsSceneGraphObject": "Returns true when a GameObject is a real scene-graph node rather than an editor helper.",
  "engine/core/engine_gui.cpp:CreateModelGameObject": "Spawns a GameObject with a VisualComponent bound to the named cached model.",
  "engine/core/engine_gui.cpp:ComputeEditorLayout": "Computes pane rectangles for hierarchy, viewport, and inspector from the current window size.",
  "engine/core/engine_gui.cpp:SpawnRootGameObject": "Inserts a new root node into the editor scene graph and marks the scene dirty.",
  "engine/core/engine_gui.cpp:SpawnChildGameObject": "Parents a newly created GameObject under the selected node.",
  "engine/core/engine_gui.cpp:SpawnDuplicate": "Deep-duplicates a GameObject (and children) into the scene with unique names.",
  "engine/core/engine_gui.cpp:DeleteGameObject": "Removes a node from the scene, remaps tags, and updates selection and undo state.",
  "engine/core/engine_gui.cpp:ClearScene": "Destroys all editor scene roots and resets selection for a new or loaded scene.",
  "engine/core/engine_gui.cpp:ScrollValue": "Applies mouse-wheel scrolling to a numeric inspector field.",
  "engine/core/engine_gui.cpp:AcceptAssetPathDrop": "Accepts a dragged asset path if its extension is in the allowed list.",
  "engine/core/engine_gui.cpp:DrawScriptPropertyField": "Draws an ImGui field for one reflected GameScripts property on the selected node.",
  "engine/core/engine_gui.cpp:DrawPostFxShowcaseWindow": "ImGui window for toggling and previewing post-process showcase effects.",
  "engine/core/engine_gui.cpp:DrawShortcutHintsWindow": "ImGui cheatsheet of editor keyboard shortcuts.",
  "engine/core/engine_gui.cpp:DrawRenderingSettingsWindow": "ImGui window that edits skybox, fog, default shader, and shadow-quality render settings.",
  "engine/core/engine_gui.cpp:ApplyProjectSessionState": "Applies opened-project paths, zoom, and audio/physics settings to the running editor session.",
  "engine/core/engine_gui.cpp:SwitchToOpenedProject": "Switches the editor onto a newly opened ProjectManager root and refreshes asset indexes.",
  "engine/core/engine_gui.cpp:RequestOpenProject": "Begins the open-project flow for a chosen folder path.",
  "engine/core/engine_gui.cpp:CloseOpenedProject": "Closes the current content project and returns editor state to the start screen.",
  "engine/core/engine_gui.cpp:RequestCloseProject": "Requests project close, prompting if the scene is dirty.",
  "engine/core/engine_gui.cpp:OpenBuildPreflight": "Opens the playable-package build dialog with destination and configuration prefs.",
  "engine/core/engine_gui.cpp:RequestBuildCurrentProject": "Starts an async PlayablePackage build of the open content project.",
  "engine/core/engine_gui.cpp:BeginNewProjectFlow": "Starts the new-project folder picker and name dialog.",
  "engine/core/engine_gui.cpp:SaveProject": "Writes project.json and the current scene so the content project stays in sync.",
  "engine/core/engine_gui.cpp:ConsumePendingFolderPick": "Applies a completed Win32 folder-picker result to open/create-project flows.",
  "engine/core/engine_gui.cpp:DrawEngineStartScreen": "Renders the no-project start screen with recent projects and new/open actions.",
  "engine/core/engine_gui.cpp:DrawEditorMenuBar": "Draws the File/Edit/Play menu bar that queues editor actions for the next frame.",
  "engine/core/engine_gui.cpp:ExecutePendingEditorAction": "Dispatches the queued menu action (save, play, build, project switch) after the ImGui frame.",
  "engine/core/engine_gui.cpp:DrawEditorPopups": "Draws modal dialogs for unsaved changes, new/open project, build preflight, and scene name prompts.",
  "engine/core/engine_gui.cpp:SaveSceneToFile": "Serializes the editor scene graph to a rom/scene JSON file.",
  "engine/core/engine_gui.cpp:NewScene": "Clears the current scene and starts an empty untitled scene.",
  "engine/core/engine_gui.cpp:TeardownPlaySession": "Stops play mode, restores editor runtime state, and optionally resets selection.",
  "engine/core/engine_gui.cpp:SoftStopPlayForSceneLoad": "Stops play without a full editor reset so a pending scene load can proceed.",
  "engine/core/engine_gui.cpp:ReturnToStart": "Leaves the editor session and returns to the project start screen.",
  "engine/core/engine_gui.cpp:HasThirdPersonPlayerNode": "Walks a subtree looking for a third-person player script/node.",
  "engine/core/engine_gui.cpp:HasCameraComponentNode": "Walks a subtree looking for a CameraComponent.",
  "engine/core/engine_gui.cpp:EngineGUI_IsDebugCameraPlayInputActive": "True when play mode is using the debug fly camera instead of a scene camera.",
  "engine/core/engine_gui.cpp:StartPlay": "Enters play mode: syncs SceneRuntime, starts scripts, and chooses debug vs scene camera.",
  "engine/core/engine_gui.cpp:EngineGUI_QueueSceneLoad": "Queues a scene filename to load after the current frame and any preload.",
  "engine/core/engine_gui.cpp:EngineGUI_BeginSceneAssetPreload": "Starts time-sliced model/animation warmup for the pending scene load.",
  "engine/core/engine_gui.cpp:EngineGUI_FlushSceneLoadPending": "Finishes preload and actually loads the queued scene into the editor graph.",
  "engine/core/engine_gui.cpp:EngineGUI_TickSceneAssetPreload": "Advances one preload step and returns whether warmup is still running.",
  "engine/core/engine_gui.cpp:EngineGUI_IsSceneAssetPreloadActive": "Returns whether a scene asset preload is currently in progress.",
  "engine/core/engine_gui.cpp:EngineGUI_Initialize": "Creates ImGui, gizmos, asset indexes, and editor session state after D3D11 is ready.",
  "engine/core/engine_gui.cpp:EngineGUI_Finalize": "Shuts down ImGui, watchers, and editor subsystems on engine exit.",
  "engine/core/engine_gui.cpp:EngineGUI_IsPlaying": "Returns whether the editor is currently in play mode.",
  "engine/core/engine_gui.cpp:EngineGUI_PreferPlayVisuals": "True when draw paths should use play-mode (real) models instead of editor proxies.",
  "engine/core/engine_gui.cpp:EngineGUI_IsOnStart": "True while the start screen is showing and no project session is active.",
  "engine/core/engine_gui.cpp:EngineGUI_StartPlay": "Public wrapper that starts a play session from menus or shortcuts.",
  "engine/core/engine_gui.cpp:EngineGUI_StopPlay": "Public wrapper that stops play and restores the editor session.",
  "engine/core/engine_gui.cpp:EngineGUI_GetCameraMode": "Returns the current editor/play camera mode enum.",
  "engine/core/engine_gui.cpp:EngineGUI_SetCameraMode": "Sets the editor/play camera mode (scene camera vs debug camera).",
  "engine/core/engine_gui.cpp:EngineGUI_SetActiveCameraNodeName": "Pins play-mode rendering to a named CameraComponent node.",
  "engine/core/engine_gui.cpp:EngineGUI_SetEnabled": "Enables or disables the entire ImGui editor overlay.",
  "engine/core/engine_gui.cpp:EngineGUI_ResolveStartupScenePath": "Resolves the project default/last-opened scene path for editor boot.",
  "engine/core/engine_gui.cpp:EngineGUI_IsUiOverlayVisible": "Returns whether in-game UI overlay editing is visible.",
  "engine/core/engine_gui.cpp:EngineGUI_SetUiOverlayVisible": "Shows or hides the UI overlay editor layer.",
  "engine/core/engine_gui.cpp:EngineGUI_GetEditMode": "Returns the current editor edit mode (scene vs UI).",
  "engine/core/engine_gui.cpp:EngineGUI_SetEditMode": "Switches between scene-object editing and UI-canvas editing.",
  "engine/core/engine_gui.cpp:EngineGUI_IsUiEditMode": "True when the editor is editing UI canvas elements instead of 3D nodes.",
  "engine/core/engine_gui.cpp:EngineGUI_GetGameViewportRect": "Returns the game viewport rectangle inside the editor layout for picking and rendering.",
  "engine/core/engine_gui.cpp:EngineGUI_MouseToViewportLocal": "Converts a screen mouse position into viewport-local coordinates.",
  "engine/core/engine_gui.cpp:RaySphere": "Ray-sphere intersection used for light/camera gizmo picking.",
  "engine/core/engine_gui.cpp:RayIntersectsTriangle": "Möller–Trumbore ray-triangle test for mesh picking.",
  "engine/core/engine_gui.cpp:RayPickModelMesh": "Transforms a model's CPU vertices and finds the closest ray hit on its triangles.",
  "engine/core/engine_gui.cpp:BuildPickRay": "Builds a world-space pick ray from viewport mouse coordinates and the active camera.",
  "engine/core/engine_gui.cpp:DrawMeshWireframeHighlight": "Draws a wireframe overlay of a picked mesh via DebugDrawer.",
  "engine/core/engine_gui.cpp:DrawSelectionColliderHighlight": "Draws the selected node's collider volume in the viewport.",
  "engine/core/engine_gui.cpp:DrawSelectionHighlight": "Draws mesh/collider/light highlights for the current editor selection.",
  "engine/core/engine_gui.cpp:EngineGUI_DrawSelectionIndicator": "Public hook that draws the current selection highlight during the editor frame.",
  "engine/core/engine_gui.cpp:FocusSelectedObjectWithCamera": "Frames the editor camera on the selected object's world bounds.",
  "engine/core/engine_gui.cpp:EngineGUI_Update": "Per-frame editor update: shortcuts, gizmos, play/stop, preload, and pending actions.",
  "engine/core/engine_gui.cpp:DrawEditorSceneProxies": "Draws editor-only proxies (lights, cameras, colliders) when not in play visuals.",
  "engine/core/engine_gui.cpp:MoveNodeInList": "Reorders a node among its siblings by a signed offset.",
  "engine/core/engine_gui.cpp:GetNodeSiblingIndexLocal": "Returns a node's index in its parent's child list.",
  "engine/core/engine_gui.cpp:UndoableReorderNode": "Records an undoable hierarchy reorder for the selected node.",
  "engine/core/engine_gui.cpp:DrawSceneNodeHierarchy": "Recursively draws the scene-tree ImGui rows with drag-reorder and context actions.",
  "engine/core/engine_gui.cpp:EngineGUI_OnBackbufferResized": "Notifies the editor layout that the D3D backbuffer size changed.",
  "engine/core/engine_gui.cpp:EngineGUI_Draw": "Draws the entire editor UI for one frame: layout, hierarchy, inspector, viewport tools, and popups.",

  "engine/ecs/collider_component.cpp:ClearFromOwner": "Removes any physics collider currently attached to the owning GameObject.",
  "engine/ecs/collider_component.cpp:ApplyToOwner": "Builds a box/sphere/capsule/cylinder/mesh collider on the owner from serialized shape data and the cached model.",
  "engine/ecs/collider_component.cpp:Serialize": "Writes collider type, size, trigger, and offset fields to JSON.",
  "engine/ecs/collider_component.cpp:Deserialize": "Parses collider JSON and stores shape parameters for a later ApplyToOwner.",

  "engine/ecs/model_asset_cache.cpp:ToLowerString": "Lowercases a UTF-8 string with the classic locale for asset-key comparisons.",
  "engine/ecs/model_asset_cache.cpp:NormalizeAssetKey": "Normalizes slashes and case so model paths share one cache key.",
  "engine/ecs/model_asset_cache.cpp:StemKey": "Returns the lowercase filename stem used when looking up models by short name.",
  "engine/ecs/model_asset_cache.cpp:ResolveModelWidePath": "Resolves a model name or relative path to a wide filesystem path for LoadFBX.",
  "engine/ecs/model_asset_cache.cpp:SortFolderTree": "Recursively sorts model-folder nodes alphabetically for the editor browser.",
  "engine/ecs/model_asset_cache.cpp:Initialize": "Initializes the singleton cache and scans rom/model.",
  "engine/ecs/model_asset_cache.cpp:Finalize": "Releases cached Model3D instances and marks the cache finalized.",
  "engine/ecs/model_asset_cache.cpp:RefreshAssetDirectory": "Rescans rom/model and rebuilds ObjectDef entries for every FBX/OBJ.",
  "engine/ecs/model_asset_cache.cpp:RebuildFolderTree": "Rebuilds the hierarchical folder tree from indexed object definitions.",
  "engine/ecs/model_asset_cache.cpp:FolderPathForDef": "Returns the folder-relative path for one ObjectDef.",
  "engine/ecs/model_asset_cache.cpp:GetObjectDefinitionsInFolder": "Lists model definitions that live directly in a rom/model subfolder.",

  "engine/ecs/visual_component.cpp:ReadFloat4": "Reads an XMFLOAT4 from JSON with a fallback when the array is missing.",
  "engine/ecs/visual_component.cpp:ReadFloat2": "Reads an XMFLOAT2 from JSON with a fallback when the array is missing.",
  "engine/ecs/visual_component.cpp:ToJson": "Serializes a DirectX float vector to a JSON array.",
  "engine/ecs/visual_component.cpp:EnsureAnimInstance": "Creates a per-instance Model3D clone when the object needs exclusive animation state.",
  "engine/ecs/visual_component.cpp:ReleaseSurfaceFxTexture": "Releases the loaded SurfaceFX shader-resource view.",
  "engine/ecs/visual_component.cpp:OnDetach": "Cleans animation instance and SurfaceFX resources when the component is removed.",
  "engine/ecs/visual_component.cpp:OnReset": "Restores play-mode visual state after play stops or the object resets.",
  "engine/ecs/visual_component.cpp:EnsureAnimationsLoaded": "Loads named animation FBX clips onto the instance via ModelAssetCache / Model3D.",
  "engine/ecs/visual_component.cpp:PlayClip": "Starts a named animation clip, optionally looping, on the instance model.",
  "engine/ecs/visual_component.cpp:GetActiveClipDuration": "Returns the duration in seconds of the currently playing clip.",
  "engine/ecs/visual_component.cpp:SetAnimationSpeed": "Sets playback speed on the active animation instance.",
  "engine/ecs/visual_component.cpp:Update": "Advances animation time each frame when playAnimation is enabled.",
  "engine/ecs/visual_component.cpp:Serialize": "Writes model paths, clips, emission, shader override, and SurfaceFX to JSON.",
  "engine/ecs/visual_component.cpp:Deserialize": "Restores VisualComponent fields from scene/prefab JSON.",
  "engine/ecs/visual_component.h:VisualComponent": "Component that instances a cached model with proxy mesh, clips, emission glow, and optional SurfaceFX.",

  "engine/editor/playable_package_builder.cpp:WideFromUtf8": "Converts a UTF-8 path to a UTF-16 wstring for Win32 APIs.",
  "engine/editor/playable_package_builder.cpp:Utf8FromWide": "Converts a UTF-16 wstring back to UTF-8.",
  "engine/editor/playable_package_builder.cpp:TrimCopy": "Returns a copy of a string with leading and trailing whitespace removed.",
  "engine/editor/playable_package_builder.cpp:SanitizeGameExeStem": "Strips invalid filename characters from the exported game .exe stem.",
  "engine/editor/playable_package_builder.cpp:NormalizeForCompare": "Canonicalizes a path for equality checks across slash and case differences.",
  "engine/editor/playable_package_builder.cpp:PathsEqual": "True when two filesystem paths refer to the same location.",
  "engine/editor/playable_package_builder.cpp:IsUnderOrEqual": "True when a candidate path is the root or a descendant of it.",
  "engine/editor/playable_package_builder.cpp:PrefsKey": "Builds the prefs map key for a content-project root.",
  "engine/editor/playable_package_builder.cpp:PrefsFilePath": "Returns the on-disk path of playable-package builder prefs JSON.",
  "engine/editor/playable_package_builder.cpp:AppendLog": "Appends text to the in-memory build log shown in the editor dialog.",
  "engine/editor/playable_package_builder.cpp:AppendLogLine": "Appends a single line to the build log.",
  "engine/editor/playable_package_builder.cpp:FindMsBuild": "Locates MSBuild.exe via vswhere and well-known Visual Studio install paths.",
  "engine/editor/playable_package_builder.cpp:RunProcess": "Runs a command line in a working directory and captures output into the build log.",
  "engine/editor/playable_package_builder.cpp:CopyOneFile": "Copies one file into the package directory, reporting errors.",
  "engine/editor/playable_package_builder.cpp:CopyDirectoryRecursive": "Recursively copies a directory tree into the playable package.",
  "engine/editor/playable_package_builder.cpp:AssemblePackage": "Copies the built exe, rom assets, and runtime files into the destination package folder.",
  "engine/editor/playable_package_builder.cpp:MsBuildProject": "Invokes MSBuild on the content project's vcxproj with the chosen configuration.",
  "engine/editor/playable_package_builder.cpp:RunBuild": "Full build job: locate MSBuild, compile, assemble the package, and record success/failure.",
  "engine/editor/playable_package_builder.cpp:TryParseConfiguration": "Parses a Debug/Release configuration name into the Configuration enum.",
  "engine/editor/playable_package_builder.cpp:DefaultDestination": "Suggests a default export folder next to the content project.",
  "engine/editor/playable_package_builder.cpp:LoadPrefs": "Loads last-used destination and configuration for a content root.",
  "engine/editor/playable_package_builder.cpp:SavePrefs": "Persists destination and configuration prefs for the next build dialog.",
  "engine/editor/playable_package_builder.cpp:DestinationRefusalReason": "Rejects destinations that would overwrite the project or engine repo.",
  "engine/editor/playable_package_builder.cpp:IsBuildRunning": "True while an async package build thread is still working.",
  "engine/editor/playable_package_builder.cpp:StartBuild": "Starts RunBuild on a worker thread if no build is already running.",
  "engine/editor/playable_package_builder.cpp:CopyLog": "Copies the current build log into a caller string for the ImGui dialog.",
  "engine/editor/playable_package_builder.cpp:IsBuildFinished": "True when the async build thread has completed.",
  "engine/editor/playable_package_builder.cpp:LastBuildSucceeded": "True when the last finished build assembled a package without error.",
  "engine/editor/playable_package_builder.cpp:LastBuildResultMessage": "Human-readable success or error string from the last build.",
  "engine/editor/playable_package_builder.h:Prefs": "Saved destination folder and Debug/Release configuration for playable-package builds.",
  "engine/editor/playable_package_builder.h:Request": "Input payload for StartBuild: content root, destination, game name, and configuration.",

  "engine/editor/project_asset_index.cpp:ToLowerString": "Lowercases a string for path-prefix and classification comparisons.",
  "engine/editor/project_asset_index.cpp:StartsWithPath": "True when a relative path is under a folder prefix.",
  "engine/editor/project_asset_index.cpp:ShouldSkipIndexedPath": "Skips generated or hidden paths that should not appear in the project browser.",
  "engine/editor/project_asset_index.cpp:AddFolderPath": "Ensures a folder node exists in the tree for a relative rom/ path.",
  "engine/editor/project_asset_index.cpp:SortFolderTree": "Alphabetically sorts project folder nodes for the browser.",
  "engine/editor/project_asset_index.cpp:ClassifyFile": "Maps a relative path to Kind (model, scene, prefab, script, texture, and others).",
  "engine/editor/project_asset_index.cpp:RebuildFolderTree": "Rebuilds the full folder tree and per-folder file lists from a scan.",
  "engine/editor/project_asset_index.cpp:Refresh": "Walks rom/ and refreshes the singleton ProjectAssetIndex.",
  "engine/editor/project_asset_index.cpp:GetFolderContents": "Returns classified entries in one folder for the project browser list.",
  "engine/editor/project_asset_index.cpp:GetFilesOfKind": "Collects every indexed file of a given Kind across the project.",
  "engine/editor/project_asset_index.h:ProjectAssetIndex": "Singleton index of the open project's rom/ tree with folder contents and kind filters.",

  "engine/editor/project_asset_ops.cpp:ToLowerCopy": "Returns a lowercase copy of a path fragment.",
  "engine/editor/project_asset_ops.cpp:NormalizeSlashes": "Rewrites path separators to forward slashes.",
  "engine/editor/project_asset_ops.cpp:ParentRel": "Returns the parent relative path of a rom/ entry.",
  "engine/editor/project_asset_ops.cpp:LeafName": "Returns the final path component of a relative asset path.",
  "engine/editor/project_asset_ops.cpp:Utf8FromWide": "Converts a wide filesystem path to UTF-8.",
  "engine/editor/project_asset_ops.cpp:PathFromUtf8": "Builds a filesystem path from a UTF-8 string.",
  "engine/editor/project_asset_ops.cpp:Utf8FromPath": "Converts a filesystem path to UTF-8.",
  "engine/editor/project_asset_ops.cpp:ProjectRootPath": "Returns the absolute root of the open ProjectManager project.",
  "engine/editor/project_asset_ops.cpp:GenericLower": "Lowercases a path for comparisons.",
  "engine/editor/project_asset_ops.cpp:MakeAbsoluteNormalized": "Resolves and normalizes an absolute path.",
  "engine/editor/project_asset_ops.cpp:CurrentSceneAbsoluteNormalized": "Absolute normalized path of the scene currently open in the editor.",
  "engine/editor/project_asset_ops.cpp:RecyclePath": "Sends a file or folder to the Windows Recycle Bin instead of permanently deleting it.",
  "engine/editor/project_asset_ops.cpp:IsScriptsPath": "True when a relative path is under game/scripts and must be treated carefully.",
  "engine/editor/project_asset_ops.cpp:IsValidLeafName": "Validates a new file or folder name and reports a reason when it is illegal.",
  "engine/editor/project_asset_ops.cpp:AssetPathFromRelative": "Converts a browser-relative path into the rom/-style asset path used by components.",
  "engine/editor/project_asset_ops.cpp:AbsolutePathFromRelative": "Resolves a project-relative path against the open project root.",
  "engine/editor/project_asset_ops.cpp:IsCurrentSceneOpen": "True when the given relative path is the scene currently loaded in the editor.",
  "engine/editor/project_asset_ops.cpp:FolderContainsCurrentScene": "True when deleting a folder would also delete the open scene.",
  "engine/editor/project_asset_ops.cpp:FindSceneReferences": "Scans scene JSON files for references to an asset path before delete/rename.",
  "engine/editor/project_asset_ops.cpp:RenameAsset": "Renames a project file or folder and updates the asset index.",
  "engine/editor/project_asset_ops.cpp:DeleteAssetToRecycleBin": "Deletes an asset via the Recycle Bin after blocking deletes of the open scene.",
  "engine/editor/project_asset_ops.cpp:CreateFolder": "Creates a new folder under a parent rom/ path.",
  "engine/editor/project_asset_ops.cpp:CreateMinimalScene": "Writes a new empty scene JSON under the chosen folder.",
  "engine/editor/project_asset_ops.cpp:ShowInExplorer": "Opens Explorer with the asset selected or its folder opened.",
  "engine/editor/project_asset_ops.h:ProjectAssetOps": "Static helpers for rename, recycle-bin delete, folder/scene create, and Explorer reveal.",

  "engine/editor/project_manager.h:ProjectManager": "Singleton that opens content projects, persists project.json settings, and tracks recent roots.",
  "engine/editor/project_manager.h:IsOpen": "True when a content project root is currently open.",
  "engine/editor/project_manager.h:GetEditorUiZoomPercent": "Returns the project's saved ImGui zoom percent.",
  "engine/editor/project_manager.h:GetPhysicsGravity": "Returns the project's saved physics gravity.",

  "engine/rendering/lighting/shadow.cpp:CascadeMap": "Per-cascade D3D resources and light view/proj used while rendering one shadow split.",
  "engine/rendering/lighting/shadow.cpp:ShadowConfig": "Shadow-map format, size, and depth-bias knobs applied when creating cascade textures.",
  "engine/rendering/lighting/shadow.cpp:SafeNormalize3": "Normalizes a 3-vector, falling back when the length is near zero.",
  "engine/rendering/lighting/shadow.cpp:BuildFrustumCorners": "Computes world-space frustum corners between a near and far plane for cascade fitting.",
  "engine/rendering/lighting/shadow.cpp:BuildLightView": "Builds a look-at view matrix from a focus point along the directional light.",
  "engine/rendering/lighting/shadow.cpp:FitOrthoCascade": "Fits an ortho projection to frustum corners in light space and snaps it to texels.",
  "engine/rendering/lighting/shadow.cpp:Initialize": "Creates cascade depth textures, DSVs, and SRVs for the requested map size.",
  "engine/rendering/lighting/shadow.cpp:Reconfigure": "Rebuilds shadow maps when quality (size or cascade count) changes.",
  "engine/rendering/lighting/shadow.cpp:Finalize": "Releases all cascade textures and views.",
  "engine/rendering/lighting/shadow.cpp:SetLightDirection": "Sets the directional light vector used to build cascade views.",
  "engine/rendering/lighting/shadow.cpp:GetLightDirection": "Returns the current directional light vector.",
  "engine/rendering/lighting/shadow.cpp:GetCascadeCount": "Returns how many cascades are active.",
  "engine/rendering/lighting/shadow.cpp:GetLightCount": "Legacy alias for cascade count used by depth-pass loops.",
  "engine/rendering/lighting/shadow.cpp:GetMaxCasters": "Returns the maximum supported directional shadow casters (currently one).",
  "engine/rendering/lighting/shadow.cpp:GetCascadeSplits": "Returns view-space far-Z splits packed as an XMFLOAT4.",
  "engine/rendering/lighting/shadow.cpp:GetCascadeBlend": "Returns the cascade blend width used by the shadow shader.",
  "engine/rendering/lighting/shadow.cpp:UpdateForCamera": "Rebuilds all cascade view/proj matrices from the active camera and max shadow distance.",
  "engine/rendering/lighting/shadow.cpp:GetLightView": "Returns the light-space view matrix for one cascade.",
  "engine/rendering/lighting/shadow.cpp:GetLightProj": "Returns the light-space projection matrix for one cascade.",
  "engine/rendering/lighting/shadow.cpp:GetLightViewProj": "Returns the combined light view-projection for one cascade.",
  "engine/rendering/lighting/shadow.cpp:Begin": "Binds a cascade depth target and begins the shadow depth pass.",
  "engine/rendering/lighting/shadow.cpp:End": "Ends the shadow depth pass and restores the previous render target.",
  "engine/rendering/lighting/shadow.cpp:GetShadowMapSize": "Returns the current shadow-map resolution.",
  "engine/rendering/lighting/shadow.cpp:GetReceiverBias": "Returns the receiver-side depth bias passed to shaders.",
  "engine/rendering/lighting/shadow.cpp:GetTexelSize": "Returns 1/mapSize for shader shadow sampling.",
  "engine/rendering/lighting/shadow.cpp:GetRenderingLightIndex": "Cascade index currently being rendered, or -1 outside a shadow pass.",
  "engine/rendering/lighting/shadow.cpp:SetBackfaceDepth": "Enables rendering backfaces into the shadow map to reduce acne.",
  "engine/rendering/lighting/shadow.cpp:IsBackfaceDepthEnabled": "True when shadow depth is rendered from back faces.",

  "engine/rendering/model/model.cpp:Model3D": "Constructs an empty Model3D with no GPU buffers until LoadOBJ/LoadFBX.",
  "engine/rendering/model/model.cpp:~Model3D": "Destructor that releases D3D and FBX resources owned by the model.",
  "engine/rendering/model/model.cpp:Release": "Releases vertex/index buffers, textures, FBX scene, and baked animation data.",
  "engine/rendering/model/model.cpp:LoadOBJ": "Imports an OBJ mesh and optional MTL textures into CPU vertices and D3D buffers.",
  "engine/rendering/model/model.cpp:LoadFBX": "Imports an FBX mesh, materials, bones, and base animation using the FBX SDK.",
  "engine/rendering/model/model.cpp:TryGetBoneLocalMatrix": "Looks up a bone's current model-space matrix for attachments or scripts.",
  "engine/rendering/model/model.cpp:ScaleBoneAndChildren": "Scales an FBX bone node and its children, used for editor/runtime bone tweaks.",
  "engine/rendering/model/model.cpp:SetPosition": "Sets the model's world translation used on the next Draw.",
  "engine/rendering/model/model.cpp:SetRotateEulerDeg": "Sets Euler-degree rotation applied when drawing the model.",
  "engine/rendering/model/model.cpp:SetScale": "Sets the model's world scale.",
  "engine/rendering/model/model.cpp:ApplyMaterialOverride": "Copies per-instance material override fields onto a GPU material.",
  "engine/rendering/model/model.cpp:BindNormalMapSlot": "Binds a normal-map SRV for the upcoming draw.",
  "engine/rendering/model/model.cpp:UnbindNormalMapSlot": "Clears the normal-map shader slot.",
  "engine/rendering/model/model.cpp:BindRampSlot": "Binds a toon-ramp texture SRV.",
  "engine/rendering/model/model.cpp:UnbindRampSlot": "Clears the toon-ramp shader slot.",
  "engine/rendering/model/model.cpp:InitMaterialDefaults": "Fills default albedo, specular, and emission for a newly imported material.",
  "engine/rendering/model/model.cpp:Draw": "Issues D3D11 draw calls for all submeshes with the current world transform and skin palette.",
  "engine/rendering/model/model.cpp:ApplyGeoPosition": "Writes a translation onto an FBX geometry node during import.",
  "engine/rendering/model/model.cpp:FbxToXM": "Converts an FbxAMatrix to an XMMATRIX.",
  "engine/rendering/model/model.cpp:CanGpuSkin": "True when bone weights and a skin palette are ready for GPU skinning.",
  "engine/rendering/model/model.cpp:RecreateVertexBufferFromCPU": "Rebuilds the D3D vertex buffer from CPU vertex data after a CPU-skin update.",
  "engine/rendering/model/model.cpp:BakeVertexBoneWeights": "Packs up to four bone indices/weights per vertex for GPU skinning.",
  "engine/rendering/model/model.cpp:FillBindPoseScratch": "Fills bind-pose bone matrices used to build the skin palette.",
  "engine/rendering/model/model.cpp:UpdateGpuSkinPalette": "Updates the 128-bone palette constant buffer from the current clip pose.",
  "engine/rendering/model/model.cpp:PublishSkinPalette": "Pushes the current skin palette to the shader.",
  "engine/rendering/model/model.cpp:UploadSkinnedVerticesIfDirty": "Uploads CPU-skinned vertices when GPU skinning is unavailable.",
  "engine/rendering/model/model.cpp:markVertexUploadDirty": "Flags CPU vertex data so the next draw uploads a new buffer.",
  "engine/rendering/model/model.cpp:GetAnimStackSpan": "Reads start time and duration from an FBX animation stack.",
  "engine/rendering/model/model.cpp:ScoreStackBoneMotion": "Scores an FBX anim stack by how much a sample bone moves, to pick the best clip.",
  "engine/rendering/model/model.cpp:BakeSkinningData": "Bakes bind pose, cluster weights, and palette layout after FBX import.",
  "engine/rendering/model/model.cpp:BakeRigidMeshData": "Prepares non-skinned mesh data when the FBX has no deformers.",
  "engine/rendering/model/model.cpp:InitAnimationInfo": "Initializes clip metadata after a successful FBX load.",
  "engine/rendering/model/model.cpp:LerpFbxAMatrix": "Linearly interpolates two FBX matrices for baked-clip sampling.",
  "engine/rendering/model/model.cpp:EnsureClipBaked": "Bakes a named animation clip into sampled bone matrices if not already cached.",
  "engine/rendering/model/model.cpp:SampleBakedClip": "Samples the baked clip at the current time into bone local matrices.",
  "engine/rendering/model/model.cpp:SkinFromScratch": "CPU-skins vertices from bind pose and the current bone palette.",
  "engine/rendering/model/model.cpp:UpdateAnimation": "Advances clip time and updates GPU or CPU skinning for this frame.",
  "engine/rendering/model/model.cpp:HasAnimation": "True when a named clip is available on this model.",
  "engine/rendering/model/model.cpp:MergeAnimScenesFrom": "Merges animation stacks from a shared source model into this instance.",
  "engine/rendering/model/model.cpp:EnsureAnimation": "Ensures a named clip is loaded, loading an external FBX if needed.",
  "engine/rendering/model/model.cpp:AddAnimation": "Loads an additional animation-only FBX and registers it under a clip name.",
  "engine/rendering/model/model.cpp:PlayAnimation": "Starts playback of a named clip, resetting time and loop state.",
  "engine/rendering/model/model.cpp:updateDynamicVBFromCP": "Uploads control-point positions into the dynamic vertex buffer.",
  "engine/rendering/model/model.cpp:uploadVertexBufferFromCPU": "Creates or updates the GPU vertex buffer from m_verticesCPU.",
  "engine/rendering/model/model.cpp:CreateAnimInstanceFrom": "Clones GPU buffers from a shared model and gives the instance its own animation state.",

  "engine/scene/render_settings.cpp:RenderSettings_ResetToDefaults": "Restores skybox, fog, shader, and shadow-quality fields to engine defaults.",
  "engine/scene/render_settings.cpp:RenderSettings_GetShadowPreset": "Returns map size, cascade count, and max distance for a shadow-quality enum.",
  "engine/scene/render_settings.cpp:RenderSettings_ShadowQualityFromId": "Parses a scene-JSON shadow-quality id string into the ShadowQuality enum.",
  "engine/scene/render_settings.cpp:RenderSettings_ApplyShadowQuality": "Reconfigures the Shadow system from the current quality preset and restores light direction.",
  "engine/scene/render_settings.cpp:RenderSettings_ApplyFromJson": "Applies a scene file's render object onto the live RenderSettingsData.",
  "engine/scene/render_settings.cpp:RenderSettings_ToJson": "Serializes live render settings for scene save.",
  "engine/scene/render_settings.cpp:RenderSettings_ReloadSkybox": "Reloads the skybox cubemap from the path stored in render settings.",
  "engine/scene/render_settings.cpp:RenderSettings_GetDefaultShader": "Maps the defaultShader id string to a Shader3DVariant.",
  "engine/scene/render_settings.h:ShadowQualityPreset": "Numeric shadow-map size, cascade count, and max distance for one quality tier.",
  "engine/scene/render_settings.h:RenderSettingsData": "Scene-wide skybox, clear color, default shader, fog, and shadow-quality settings.",

  "engine/scene/scene_asset_preload.cpp:ToLowerAscii": "ASCII-lowercases a path key for preload dedupe.",
  "engine/scene/scene_asset_preload.cpp:NormalizePathKey": "Normalizes slashes and case for model-path identity.",
  "engine/scene/scene_asset_preload.cpp:FilenameLabel": "Returns a filename label shown on the preload progress UI.",
  "engine/scene/scene_asset_preload.cpp:MergeVisualPaths": "Merges a model path plus clip paths into a deduped preload task list.",
  "engine/scene/scene_asset_preload.cpp:CollectVisual": "Collects VisualComponent model and animation paths from one GameObject.",
  "engine/scene/scene_asset_preload.cpp:IsProjectRelativeRomPath": "True when a path looks like a project-relative rom/ asset.",
  "engine/scene/scene_asset_preload.cpp:TryReadJsonFile": "Reads and parses a JSON file, returning false on I/O or parse errors.",
  "engine/scene/scene_asset_preload.cpp:ScanPrefabFile": "Scans a prefab JSON for nested visual/prefab references to preload.",
  "engine/scene/scene_asset_preload.cpp:CollectVisualFromJson": "Extracts model and animation paths from a visual JSON object.",
  "engine/scene/scene_asset_preload.cpp:WalkJsonNode": "Recursively walks a scene/prefab JSON node collecting visuals and prefab refs.",
  "engine/scene/scene_asset_preload.cpp:SceneAssetPreload_BuildTaskList": "Builds preload tasks from loaded editor GameObject roots.",
  "engine/scene/scene_asset_preload.cpp:SceneAssetPreload_BuildTaskListFromSceneFiles": "Builds preload tasks by scanning scene JSON files on disk (runtime boot).",
  "engine/scene/scene_asset_preload.cpp:Begin": "Queues flattened model/animation steps and starts a preload run.",
  "engine/scene/scene_asset_preload.cpp:Cancel": "Aborts an in-flight preload and clears remaining steps.",
  "engine/scene/scene_asset_preload.cpp:Progress01": "Returns 0–1 progress for the editor preload bar.",
  "engine/scene/scene_asset_preload.cpp:RebuildLabel": "Updates the current-file label shown during preload.",
  "engine/scene/scene_asset_preload.cpp:RunOneStep": "Loads one model or animation clip through ModelAssetCache / Model3D.",
  "engine/scene/scene_asset_preload.cpp:Tick": "Advances preload by one step per frame until the queue is empty.",

  "engine/scripting/script_watcher.cpp:Start": "Starts a worker thread that watches watchDir for DLL changes and fires onChange.",
  "engine/scripting/script_watcher.cpp:Stop": "Signals the watch thread to exit and joins it.",
  "engine/scripting/script_watcher.h:~ScriptWatcher": "Destructor that stops the directory-watch thread.",
  "engine/scripting/script_watcher.h:ScriptWatcher": "Background watcher that notifies the editor when GameScripts.dll is rebuilt.",

  "src/app/rendering/grid.cpp:Initialize": "Builds grid line vertices and D3D buffers, then registers the grid GameObject in the scene.",
  "src/app/rendering/grid.cpp:Finalize": "Removes the grid from the scene and releases its buffers.",
  "src/app/rendering/grid.cpp:LogicUpdate": "No-op per-frame update; the grid is static.",
  "src/app/rendering/grid.cpp:Draw": "Draws the grid lines with the default 3D shader and depth testing.",
  "src/app/rendering/grid.cpp:Grid_Initialize": "C helper that initializes the process-wide editor grid.",
  "src/app/rendering/grid.cpp:Grid_Finalize": "C helper that tears down the process-wide editor grid.",
  "src/app/rendering/grid.cpp:Grid_Draw": "C helper that draws the process-wide editor grid.",
  "src/app/rendering/grid.h:SetVertexBuffer": "Overrides the grid's vertex buffer pointer.",
  "src/app/rendering/grid.h:SetTexture": "Overrides the grid's texture SRV (usually the solid-white helper).",
  "src/app/rendering/grid.h:Grid": "GameObject subclass that owns the editor ground-grid mesh and draw path.",
};

function fnLines(fn) {
  return fn.endLine - fn.startLine + 1;
}

function complexityFromLines(n) {
  if (n > 200) return "complex";
  if (n >= 50) return "moderate";
  return "simple";
}

function tagsForSymbol(name, path, kind) {
  const n = name.toLowerCase();
  const tags = [];
  if (kind === "class") tags.push("data-model");
  if (n.includes("serialize") || n.includes("deserialize") || n.includes("tojson") || n.includes("fromjson")) tags.push("serialization");
  if (n.includes("draw") || n.includes("imgui") || n.includes("window") || n.includes("menu")) tags.push("ui");
  if (n.startsWith("enginegui") || path.includes("engine_gui")) tags.push("editor");
  if (n.includes("shadow") || n.includes("cascade") || n.includes("light")) tags.push("lighting");
  if (n.includes("anim") || n.includes("clip") || n.includes("skin") || n.includes("bone")) tags.push("animation");
  if (n.includes("load") || n.includes("fbx") || n.includes("obj") || n.includes("asset") || n.includes("preload")) tags.push("assets");
  if (n.includes("init") || n.includes("final") || n.includes("create") || n.includes("spawn")) tags.push("lifecycle");
  if (n.includes("pick") || n.includes("ray")) tags.push("picking");
  if (n.includes("build") || n.includes("msbuild") || n.includes("package")) tags.push("packaging");
  if (n.includes("path") || n.includes("folder") || n.includes("file")) tags.push("filesystem");
  if (n.includes("update") || n.includes("tick") || n.includes("play")) tags.push("runtime");
  if (kind === "function") tags.push("function");
  const uniq = [...new Set(tags)];
  while (uniq.length < 3) {
    if (!uniq.includes("engine")) uniq.push("engine");
    else if (!uniq.includes("cpp")) uniq.push("cpp");
    else uniq.push("ivy");
  }
  return uniq.slice(0, 5);
}

function summaryFor(path, name, kind) {
  if (kind === "function") {
    if (path === "engine/editor/project_asset_index.h" && name === "ProjectAssetIndex") {
      return "Private default constructor; use Instance() for the process-wide index.";
    }
    if (path === "engine/editor/project_manager.h" && name === "ProjectManager") {
      return "Private default constructor; use Instance() for the process-wide manager.";
    }
  }
  const key = `${path}:${name}`;
  if (SYMBOL_SUMMARIES[key]) return SYMBOL_SUMMARIES[key];
  const pretty = name.replace(/_/g, " ");
  if (kind === "class") return `${name} type defined in ${path}.`;
  return `${pretty} implemented in ${path}.`;
}

function isFwdClass(cls) {
  const span = cls.endLine - cls.startLine + 1;
  return span <= 2 && (cls.methods || []).length === 0 && FWD_DECL.has(cls.name);
}

const nodes = [];
const edges = [];
const nodeIds = new Set();
const functionsByFile = new Map();

function addNode(n) {
  if (nodeIds.has(n.id)) return;
  nodeIds.add(n.id);
  nodes.push(n);
}

for (const r of extract.results) {
  const meta = FILE_META[r.path];
  if (!meta) throw new Error("missing FILE_META for " + r.path);
  addNode({
    id: `file:${r.path}`,
    type: "file",
    name: r.path.split("/").pop(),
    filePath: r.path,
    summary: meta.summary,
    tags: meta.tags,
    complexity: meta.complexity,
    ...(meta.languageNotes ? { languageNotes: meta.languageNotes } : {}),
  });
}

for (const r of extract.results) {
  const exp = new Set((r.exports || []).map((e) => e.name));
  const seenFn = new Map();
  for (const fn of r.functions || []) {
    const exported = exp.has(fn.name);
    if (!exported && fnLines(fn) < 10) continue;
    if (!seenFn.has(fn.name)) {
      seenFn.set(fn.name, { ...fn, exported });
    } else {
      const prev = seenFn.get(fn.name);
      prev.startLine = Math.min(prev.startLine, fn.startLine);
      prev.endLine = Math.max(prev.endLine, fn.endLine);
      prev.exported = prev.exported || exported;
    }
  }
  functionsByFile.set(r.path, seenFn);

  for (const [name, fn] of seenFn) {
    const span = fnLines(fn);
    addNode({
      id: `function:${r.path}:${name}`,
      type: "function",
      name,
      filePath: r.path,
      lineRange: [fn.startLine, fn.endLine],
      summary: summaryFor(r.path, name, "function"),
      tags: tagsForSymbol(name, r.path, "function"),
      complexity: complexityFromLines(span),
    });
    edges.push({
      source: `file:${r.path}`,
      target: `function:${r.path}:${name}`,
      type: "contains",
      direction: "forward",
      weight: 1.0,
    });
    if (fn.exported) {
      edges.push({
        source: `file:${r.path}`,
        target: `function:${r.path}:${name}`,
        type: "exports",
        direction: "forward",
        weight: 0.8,
      });
    }
  }

  for (const cls of r.classes || []) {
    if (isFwdClass(cls)) continue;
    const span = cls.endLine - cls.startLine + 1;
    const exported = exp.has(cls.name);
    const significant = exported || (cls.methods || []).length >= 2 || span >= 20;
    if (!significant) continue;
    addNode({
      id: `class:${r.path}:${cls.name}`,
      type: "class",
      name: cls.name,
      filePath: r.path,
      lineRange: [cls.startLine, cls.endLine],
      summary: summaryFor(r.path, cls.name, "class"),
      tags: tagsForSymbol(cls.name, r.path, "class"),
      complexity: complexityFromLines(span),
    });
    edges.push({
      source: `file:${r.path}`,
      target: `class:${r.path}:${cls.name}`,
      type: "contains",
      direction: "forward",
      weight: 1.0,
    });
    if (exported) {
      edges.push({
        source: `file:${r.path}`,
        target: `class:${r.path}:${cls.name}`,
        type: "exports",
        direction: "forward",
        weight: 0.8,
      });
    }
  }
}

// imports: 1:1 with batchImportData
let importCount = 0;
for (const [filePath, imports] of Object.entries(input.batchImportData)) {
  for (const target of imports) {
    edges.push({
      source: `file:${filePath}`,
      target: `file:${target}`,
      type: "imports",
      direction: "forward",
      weight: 0.7,
    });
    importCount++;
  }
}

// inherits VisualComponent -> Component
if (nodeIds.has("class:engine/ecs/visual_component.h:VisualComponent")) {
  edges.push({
    source: "class:engine/ecs/visual_component.h:VisualComponent",
    target: "class:engine/ecs/component.h:Component",
    type: "inherits",
    direction: "forward",
    weight: 0.9,
  });
}

// High-confidence cross-file calls
const importSet = new Map();
for (const [filePath, imports] of Object.entries(input.batchImportData)) {
  importSet.set(filePath, new Set(imports));
}

const neighborSymbolsByFile = new Map();
for (const [file, neighbors] of Object.entries(neighborMap)) {
  const m = neighborSymbolsByFile.get(file) || [];
  for (const n of neighbors) m.push(n);
  neighborSymbolsByFile.set(file, m);
}

const inBatchByName = new Map();
for (const n of nodes) {
  if (n.type === "function" || n.type === "class") {
    const name = n.name;
    if (!inBatchByName.has(name)) inBatchByName.set(name, []);
    inBatchByName.get(name).push(n);
  }
}

const AMBIGUOUS = new Set([
  "Update", "Initialize", "Finalize", "Draw", "Serialize", "Deserialize",
  "OnDetach", "OnReset", "OnAttach", "Play", "Stop", "Start", "Refresh",
  "ToLowerString", "SortFolderTree", "RebuildFolderTree",
]);

const callSeen = new Set();
function emitCall(srcId, dstId) {
  const key = srcId + "->" + dstId;
  if (srcId === dstId) return;
  if (callSeen.has(key)) return;
  if (!nodeIds.has(srcId)) return;
  callSeen.add(key);
  edges.push({
    source: srcId,
    target: dstId,
    type: "calls",
    direction: "forward",
    weight: 0.8,
  });
}

function resolveCallee(callerFile, calleeRaw) {
  const callee = calleeRaw.replace(/<.*>/, "").split("::").pop();
  if (AMBIGUOUS.has(callee)) return null;
  const imported = importSet.get(callerFile) || new Set();

  const inBatch = (inBatchByName.get(callee) || []).filter((n) => n.filePath !== callerFile);
  const inBatchImported = inBatch.filter((n) => imported.has(n.filePath));
  if (inBatchImported.length === 1) return inBatchImported[0].id;
  if (inBatch.length === 1 && imported.has(inBatch[0].filePath)) return inBatch[0].id;

  const neighbors = neighborSymbolsByFile.get(callerFile) || [];
  const hits = [];
  for (const n of neighbors) {
    if ((n.symbols || []).includes(callee)) {
      const kind = /^[A-Z]/.test(callee) && !callee.includes("_") ? "class" : "function";
      // Prefer class if name looks like a type and file is a header
      if ((n.symbols || []).includes(callee)) {
        hits.push({ path: n.path, symbols: n.symbols });
      }
    }
  }
  const uniquePaths = [...new Set(hits.map((h) => h.path))];
  if (uniquePaths.length !== 1) return null;
  const path = uniquePaths[0];
  const symbols = hits[0].symbols;
  // Prefer class: for known types
  if (callee.endsWith("Component") || callee === "Component" || callee === "Transform" || callee === "Script") {
    return `class:${path}:${callee}`;
  }
  return `function:${path}:${callee}`;
}

for (const r of extract.results) {
  const fns = functionsByFile.get(r.path) || new Map();
  for (const call of r.callGraph || []) {
    if (!fns.has(call.caller)) continue;
    const dst = resolveCallee(r.path, call.callee);
    if (!dst) continue;
    emitCall(`function:${r.path}:${call.caller}`, dst);
  }
}

// Explicit high-value calls the resolver may miss
const extraCalls = [
  ["function:engine/ecs/collider_component.cpp:ApplyToOwner", "function:engine/ecs/collider_component.cpp:ClearFromOwner"],
  ["function:engine/scene/render_settings.cpp:RenderSettings_ApplyShadowQuality", "function:engine/rendering/lighting/shadow.cpp:Reconfigure"],
  ["function:engine/scene/render_settings.cpp:RenderSettings_ApplyShadowQuality", "function:engine/rendering/lighting/shadow.cpp:GetLightDirection"],
  ["function:engine/scene/render_settings.cpp:RenderSettings_ApplyShadowQuality", "function:engine/rendering/lighting/shadow.cpp:IsBackfaceDepthEnabled"],
  ["function:engine/scene/render_settings.cpp:RenderSettings_ApplyShadowQuality", "function:engine/rendering/lighting/shadow.cpp:SetLightDirection"],
  ["function:engine/scene/render_settings.cpp:RenderSettings_ApplyShadowQuality", "function:engine/rendering/lighting/shadow.cpp:SetBackfaceDepth"],
  ["function:engine/core/engine_gui.cpp:EngineGUI_BeginSceneAssetPreload", "function:engine/scene/scene_asset_preload.cpp:Begin"],
  ["function:engine/core/engine_gui.cpp:EngineGUI_TickSceneAssetPreload", "function:engine/scene/scene_asset_preload.cpp:Tick"],
  ["function:engine/core/engine_gui.cpp:EngineGUI_FlushSceneLoadPending", "function:engine/scene/scene_asset_preload.cpp:SceneAssetPreload_BuildTaskList"],
  ["function:src/app/rendering/grid.cpp:Draw", "function:engine/scene/render_settings.cpp:RenderSettings_GetDefaultShader"],
  ["function:engine/scripting/script_watcher.h:~ScriptWatcher", "function:engine/scripting/script_watcher.cpp:Stop"],
];
for (const [s, t] of extraCalls) {
  if (s === t) continue;
  emitCall(s, t);
}

const batchFiles = input.batchFiles.map((f) => f.path).sort();
const neighborPaths = new Set();
const neighborSymbolOk = new Set();
for (const neighbors of Object.values(neighborMap)) {
  for (const n of neighbors) {
    neighborPaths.add(n.path);
    for (const sym of n.symbols || []) {
      neighborSymbolOk.add(`function:${n.path}:${sym}`);
      neighborSymbolOk.add(`class:${n.path}:${sym}`);
    }
  }
}
const importPaths = new Set();
for (const arr of Object.values(input.batchImportData)) {
  for (const p of arr) importPaths.add(p);
}

function edgeTargetOk(id, partNodeIds) {
  if (partNodeIds.has(id)) return true;
  if (id.startsWith("file:")) {
    const p = id.slice("file:".length);
    return neighborPaths.has(p) || importPaths.has(p) || batchFiles.includes(p);
  }
  if (id.startsWith("function:") || id.startsWith("class:")) {
    if (neighborSymbolOk.has(id)) return true;
    // in-batch symbol living in another part
    if (nodeIds.has(id)) return true;
    return false;
  }
  return false;
}

const nodeCount = nodes.length;
const edgeCount = edges.length;
console.log("import edges", importCount, "expected", Object.values(input.batchImportData).reduce((a, b) => a + b.length, 0));
console.log("nodes", nodeCount, "edges", edgeCount);

const outDir = `${UA}/intermediate`;
fs.mkdirSync(outDir, { recursive: true });

function writeJson(name, obj) {
  const p = `${outDir}/${name}`;
  fs.writeFileSync(p, JSON.stringify(obj, null, 2) + "\n");
  return p;
}

if (nodeCount <= 60 && edgeCount <= 120) {
  const p = writeJson("batch-1.json", { nodes, edges });
  console.log("WROTE", p);
} else {
  const parts = Math.ceil(Math.max(nodeCount / 60, edgeCount / 120));
  const groupSize = Math.ceil(batchFiles.length / parts);
  console.log("SPLIT parts", parts, "groupSize", groupSize);
  const written = [];
  for (let k = 0; k < parts; k++) {
    const slice = batchFiles.slice(k * groupSize, (k + 1) * groupSize);
    if (slice.length === 0) continue;
    const sliceSet = new Set(slice);
    const partNodes = nodes.filter((n) => sliceSet.has(n.filePath));
    const partIds = new Set(partNodes.map((n) => n.id));
    const partEdges = edges.filter((e) => partIds.has(e.source));
    const bad = [];
    for (const e of partEdges) {
      if (!partIds.has(e.source)) bad.push(["src", e]);
      if (!edgeTargetOk(e.target, partIds)) bad.push(["tgt", e]);
    }
    if (bad.length) {
      console.error("VALIDATION FAIL part", k + 1);
      for (const [why, e] of bad.slice(0, 20)) {
        console.error(why, e.type, e.source, "->", e.target);
      }
      process.exit(2);
    }
    const name = `batch-1-part-${k + 1}.json`;
    writeJson(name, { nodes: partNodes, edges: partEdges });
    written.push({ name, nodes: partNodes.length, edges: partEdges.length, files: slice });
  }
  console.log(JSON.stringify(written, null, 2));
}
