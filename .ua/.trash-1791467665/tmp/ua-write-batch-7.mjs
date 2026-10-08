import { writeFileSync, mkdirSync, readFileSync } from 'node:fs';

const file = (path, name, summary, tags, complexity, extra = {}) => ({
  id: `file:${path}`,
  type: 'file',
  name,
  filePath: path,
  summary,
  tags,
  complexity,
  ...extra,
});

const fn = (path, name, lineRange, summary, tags, complexity, extra = {}) => ({
  id: `function:${path}:${name}`,
  type: 'function',
  name,
  filePath: path,
  lineRange,
  summary,
  tags,
  complexity,
  ...extra,
});

const cls = (path, name, lineRange, summary, tags, complexity, extra = {}) => ({
  id: `class:${path}:${name}`,
  type: 'class',
  name,
  filePath: path,
  lineRange,
  summary,
  tags,
  complexity,
  ...extra,
});

const edge = (source, target, type, weight) => ({
  source, target, type, direction: 'forward', weight,
});

const contains = (filePath, nodeId) => edge(`file:${filePath}`, nodeId, 'contains', 1.0);
const exportsE = (filePath, nodeId) => edge(`file:${filePath}`, nodeId, 'exports', 0.8);
const importsE = (from, to) => edge(`file:${from}`, `file:${to}`, 'imports', 0.7);
const callsE = (source, target) => edge(source, target, 'calls', 0.8);

const importData = {
  'engine/audio/audio_listener_sync.cpp': [
    'engine/audio/audio_listener_sync.h',
    'engine/audio/audio_manager.h',
    'engine/core/engine_gui.h',
    'engine/ecs/audio_listener_component.h',
    'engine/ecs/camera_component.h',
    'engine/ecs/game_object.h',
    'engine/ecs/transform.h',
  ],
  'engine/core/easing.cpp': ['engine/core/easing.h'],
  'engine/core/easing.h': [],
  'engine/core/engine_gui.h': [],
  'engine/core/engine_gui_internal.h': [
    'engine/core/engine_gui.h',
    'engine/scene/scene_loader.h',
  ],
  'engine/core/engine_gui_persistence.cpp': [
    'engine/core/engine_gui.h',
    'engine/core/engine_gui_internal.h',
    'engine/scene/scene_loader.h',
  ],
  'engine/core/engine_gui_runtime.cpp': [
    'engine/core/engine_gui.h',
    'engine/ecs/game_object.h',
    'engine/ecs/model_asset_cache.h',
    'engine/input/mouse.h',
    'engine/physics/core/physics_system.h',
    'engine/rendering/d3d11/renderer.h',
    'engine/scene/scene_loader.h',
    'engine/scripting/script_component.h',
    'src/app/camera/debug_camera.h',
    'src/app/camera/first_person_camera.h',
    'src/app/camera/third_person_camera.h',
  ],
  'engine/ecs/audio_component.cpp': [
    'engine/ecs/audio_component.h',
    'engine/ecs/audio_system.h',
    'engine/ecs/game_object.h',
    'engine/ecs/transform.h',
  ],
  'engine/ecs/camera_component.cpp': [
    'engine/ecs/camera_component.h',
    'engine/ecs/transform.h',
  ],
  'engine/ecs/camera_component.h': ['engine/ecs/component.h'],
  'engine/ecs/effect_component.cpp': [
    'engine/ecs/effect_component.h',
    'engine/ecs/effect_system_flush.h',
    'engine/ecs/game_object.h',
    'engine/ecs/transform.h',
    'engine/rendering/effect/effect_system.h',
  ],
  'engine/ecs/game_object.h': [],
  'engine/ecs/prefab_runtime.cpp': [
    'engine/ecs/prefab_runtime.h',
    'engine/ecs/prefab_serializer.h',
    'engine/ecs/transform.h',
    'engine/physics/core/physics_system.h',
    'engine/scene/scene_loader.h',
    'engine/scene/scene_runtime.h',
    'engine/scripting/script_component.h',
  ],
  'engine/ecs/prefab_runtime.h': ['engine/ecs/game_object.h'],
  'engine/ecs/prefab_serializer.h': ['engine/ecs/game_object.h'],
  'engine/ecs/transform.cpp': [
    'engine/ecs/game_object.h',
    'engine/ecs/transform.h',
  ],
  'engine/ecs/transform.h': ['engine/ecs/component.h'],
  'engine/ecs/visual_system.cpp': [
    'engine/ecs/game_object.h',
    'engine/ecs/visual_component.h',
    'engine/ecs/visual_system.h',
    'engine/scene/scene_loader.h',
  ],
  'engine/editor/camera_gizmo.cpp': [
    'engine/debug_draw/debug_drawer.h',
    'engine/ecs/camera_component.h',
    'engine/ecs/game_object.h',
    'engine/ecs/transform.h',
    'engine/editor/camera_gizmo.h',
  ],
  'engine/editor/editor_prefab_panel.cpp': [
    'engine/ecs/game_object.h',
    'engine/ecs/prefab_serializer.h',
    'engine/editor/editor_prefab_panel.h',
  ],
};

const nodes = [];
const edges = [];
const exportedByFile = new Map();

const addFile = (...args) => {
  nodes.push(file(...args));
};

const addFn = (path, name, lineRange, summary, tags, complexity, exported = false) => {
  const node = fn(path, name, lineRange, summary, tags, complexity);
  nodes.push(node);
  edges.push(contains(path, node.id));
  if (exported) {
    edges.push(exportsE(path, node.id));
    const list = exportedByFile.get(path) || [];
    list.push(name);
    exportedByFile.set(path, list);
  }
  return node.id;
};

const addCls = (path, name, lineRange, summary, tags, complexity, exported = true) => {
  const node = cls(path, name, lineRange, summary, tags, complexity);
  nodes.push(node);
  edges.push(contains(path, node.id));
  if (exported) edges.push(exportsE(path, node.id));
  return node.id;
};

// --- files ---
addFile(
  'engine/audio/audio_listener_sync.cpp',
  'audio_listener_sync.cpp',
  'Keeps the 3D audio listener pose aligned with the active camera, debug camera, or first AudioListenerComponent in the scene graph.',
  ['audio', 'listener-sync', 'service', 'camera'],
  'moderate',
);

addFile(
  'engine/core/easing.cpp',
  'easing.cpp',
  'Implements Robert Penner-style ease-in/out curves (sine through bounce) used by camera blends and other timed interpolations.',
  ['utility', 'easing', 'animation', 'interpolation'],
  'moderate',
  { languageNotes: 'Function bodies sit inside an unusual #ifndef PI guard, which hid them from tree-sitter extraction.' },
);

addFile(
  'engine/core/easing.h',
  'easing.h',
  'Declares the public easing API: 30 scalar ease-in, ease-out, and ease-in-out curves over normalized time t.',
  ['utility', 'easing', 'type-definition', 'api-handler'],
  'simple',
);

addFile(
  'engine/core/engine_gui.h',
  'engine_gui.h',
  'Public editor/runtime GUI API for play mode, scene load/save, camera mode, viewport mapping, and UI edit mode.',
  ['editor', 'api-handler', 'type-definition', 'entry-point'],
  'moderate',
);

addFile(
  'engine/core/engine_gui_internal.h',
  'engine_gui_internal.h',
  'Shared editor GUI internals: play-session teardown, queued scene loads, and time-sliced asset preload hooks.',
  ['editor', 'internal-api', 'type-definition', 'scene-load'],
  'simple',
);

addFile(
  'engine/core/engine_gui_persistence.cpp',
  'engine_gui_persistence.cpp',
  'Editor-side scene save/load wrappers that queue loads and gate them behind the unsaved-changes popup.',
  ['editor', 'persistence', 'scene-load', 'api-handler'],
  'simple',
);

addFile(
  'engine/core/engine_gui_runtime.cpp',
  'engine_gui_runtime.cpp',
  'Runtime (non-editor) EngineGUI implementation: starts play, ticks non-player scene nodes, and stubs editor-only UI hooks.',
  ['runtime', 'api-handler', 'play-mode', 'entry-point'],
  'moderate',
  { languageNotes: 'Same EngineGUI_* symbols as the editor build, compiled only into the game runtime host.' },
);

addFile(
  'engine/ecs/audio_component.cpp',
  'audio_component.cpp',
  'Implements AudioComponent lifecycle, JSON clip settings, and deferred play/stop/position commands flushed to AudioManager.',
  ['audio', 'ecs-component', 'serialization', 'service'],
  'moderate',
);

addFile(
  'engine/ecs/camera_component.cpp',
  'camera_component.cpp',
  'Implements CameraComponent world-space eye/basis math, global Camera application, and FOV/clip serialization.',
  ['camera', 'ecs-component', 'serialization', 'math'],
  'moderate',
);

addFile(
  'engine/ecs/camera_component.h',
  'camera_component.h',
  'Declares CameraComponent: enabled flag, lens parameters, and static helpers that derive eye and axes from a Transform.',
  ['camera', 'ecs-component', 'type-definition', 'data-model'],
  'simple',
);

addFile(
  'engine/ecs/effect_component.cpp',
  'effect_component.cpp',
  'Implements EffectComponent registration, cached effect loading, and deferred play/stop/transform sync against EffectSystem.',
  ['vfx', 'ecs-component', 'serialization', 'service'],
  'moderate',
);

addFile(
  'engine/ecs/game_object.h',
  'game_object.h',
  'Core scene-node type: typed component map, parent/child hierarchy, legacy pose fields, collision ignores, and collider factories.',
  ['ecs', 'data-model', 'scene-graph', 'entry-point'],
  'complex',
  { languageNotes: 'Header-heavy GameObject keeps script-DLL-safe pose setters inline; destructor and Update live in game_object.cpp.' },
);

addFile(
  'engine/ecs/prefab_runtime.cpp',
  'prefab_runtime.cpp',
  'Runtime prefab spawn/destroy service that defers Instantiates during Update and flushes pending graph mutations safely.',
  ['prefab', 'runtime', 'factory', 'scene-graph'],
  'complex',
);

addFile(
  'engine/ecs/prefab_runtime.h',
  'prefab_runtime.h',
  'Declares PrefabRuntime spawn-root helpers, deferred instantiate/destroy, and the before-destroy callback hook.',
  ['prefab', 'runtime', 'type-definition', 'factory'],
  'simple',
);

addFile(
  'engine/ecs/prefab_serializer.h',
  'prefab_serializer.h',
  'Declares PrefabSerializer static JSON save/load for a GameObject subtree, including the versioned scene-style wrapper.',
  ['prefab', 'serialization', 'type-definition', 'data-model'],
  'simple',
);

addFile(
  'engine/ecs/transform.cpp',
  'transform.cpp',
  'Implements engine::Transform local/world matrices, ZYX Euler decompose, start-state snapshots, and JSON pose fields.',
  ['transform', 'ecs-component', 'math', 'serialization'],
  'moderate',
);

addFile(
  'engine/ecs/transform.h',
  'transform.h',
  'Declares the SceneTransform component: local pose, world-matrix helpers, and start-state snapshot used by Return-to-Start.',
  ['transform', 'ecs-component', 'type-definition', 'data-model'],
  'moderate',
);

addFile(
  'engine/ecs/visual_system.cpp',
  'visual_system.cpp',
  'Walks scene-graph roots and uploads dirty skinned vertices for every VisualComponent model.',
  ['rendering', 'skinning', 'scene-graph', 'service'],
  'simple',
);

addFile(
  'engine/editor/camera_gizmo.cpp',
  'camera_gizmo.cpp',
  'Draws editor debug axes and a near-plane frustum wire for every enabled CameraComponent in the scene graph.',
  ['editor', 'gizmo', 'camera', 'debug-draw'],
  'moderate',
);

addFile(
  'engine/editor/editor_prefab_panel.cpp',
  'editor_prefab_panel.cpp',
  'ImGui prefab browser that lists JSON prefabs, instantiates on click, and starts ASSET_PREFAB drag-drop payloads.',
  ['editor', 'prefab', 'ui-panel', 'factory'],
  'moderate',
);

// --- audio_listener_sync ---
const pALS = 'engine/audio/audio_listener_sync.cpp';
addFn(pALS, 'FindNodeByNameInRoots', [27, 36], 'Searches scene roots by name and returns the first matching GameObject.', ['utility', 'scene-graph', 'lookup'], 'simple');
addFn(pALS, 'FindFirstListener', [38, 47], 'Depth-first search for the first node that already has an AudioListenerComponent.', ['audio', 'scene-graph', 'lookup'], 'simple');
addFn(pALS, 'ApplyPoseFromNode', [63, 77], 'Pushes a node eye/forward/up pose into AudioManager, falling back to the global camera if the node is null.', ['audio', 'listener-sync', 'camera'], 'simple');
addFn(pALS, 'AudioListenerSync_EnsureOnCamera', [81, 86], 'Attaches an AudioListenerComponent to a camera node when play mode is using that camera.', ['audio', 'ecs-component', 'factory'], 'simple', true);
addFn(pALS, 'AudioListenerSync_Tick', [88, 117], 'Each frame, chooses the active camera or fallback listener and writes its world pose to AudioManager.', ['audio', 'listener-sync', 'event-handler'], 'moderate', true);

// --- easing ---
const pEase = 'engine/core/easing.cpp';
const easing = [
  ['easeInSine', 18, 20, 'Sine ease-in: sin(pi/2 * t).'],
  ['easeOutSine', 22, 24, 'Sine ease-out ending at 1.'],
  ['easeInOutSine', 26, 28, 'Symmetric sine ease-in-out around t=0.5.'],
  ['easeInQuad', 30, 32, 'Quadratic ease-in (t squared).'],
  ['easeOutQuad', 34, 36, 'Quadratic ease-out.'],
  ['easeInOutQuad', 38, 40, 'Piecewise quadratic ease-in-out.'],
  ['easeInCubic', 42, 44, 'Cubic ease-in (t cubed).'],
  ['easeOutCubic', 46, 48, 'Cubic ease-out.'],
  ['easeInOutCubic', 50, 56, 'Cubic ease-in-out with a corrected ease-out half.'],
  ['easeInQuart', 58, 61, 'Quartic ease-in.'],
  ['easeOutQuart', 63, 66, 'Quartic ease-out.'],
  ['easeInOutQuart', 68, 77, 'Piecewise quartic ease-in-out.'],
  ['easeInQuint', 79, 82, 'Quintic ease-in.'],
  ['easeOutQuint', 84, 87, 'Quintic ease-out.'],
  ['easeInOutQuint', 89, 99, 'Piecewise quintic ease-in-out.'],
  ['easeInExpo', 101, 103, 'Exponential ease-in via pow(2, 8t).'],
  ['easeOutExpo', 105, 107, 'Exponential ease-out.'],
  ['easeInOutExpo', 109, 116, 'Piecewise exponential ease-in-out.'],
  ['easeInCirc', 118, 120, 'Circular ease-in using sqrt.'],
  ['easeOutCirc', 122, 124, 'Circular ease-out (sqrt(t)).'],
  ['easeInOutCirc', 126, 133, 'Piecewise circular ease-in-out.'],
  ['easeInBack', 135, 137, 'Back ease-in that overshoots below zero.'],
  ['easeOutBack', 139, 141, 'Back ease-out that overshoots past one.'],
  ['easeInOutBack', 143, 149, 'Piecewise back ease-in-out with overshoot.'],
  ['easeInElastic', 151, 155, 'Elastic ease-in using a decaying sine.'],
  ['easeOutElastic', 157, 160, 'Elastic ease-out using a decaying cosine.'],
  ['easeInOutElastic', 162, 175, 'Three-phase elastic ease-in-out with a mid sine plateau.'],
  ['easeInBounce', 177, 179, 'Bounce ease-in from exponentially decaying sines.'],
  ['easeOutBounce', 181, 183, 'Bounce ease-out from decaying cosines.'],
  ['easeInOutBounce', 185, 192, 'Piecewise bounce ease-in-out.'],
];
for (const [name, a, b, summary] of easing) {
  addFn(pEase, name, [a, b], summary, ['easing', 'utility', 'interpolation'], 'simple', true);
}

// --- engine_gui persistence ---
const pPers = 'engine/core/engine_gui_persistence.cpp';
addFn(pPers, 'EngineGUI_SaveToFile', [5, 7], 'Saves the current editor scene through SceneLoader_SaveToFile.', ['persistence', 'scene-load', 'api-handler'], 'simple', true);
addFn(pPers, 'EngineGUI_LoadFromFileCore', [9, 21], 'Queues a scene load and soft-stops play without the unsaved-changes gate.', ['persistence', 'scene-load', 'play-mode'], 'simple', true);
addFn(pPers, 'EngineGUI_LoadFromFile', [23, 33], 'Loads a scene, or opens the unsaved-changes popup when the editor scene is dirty.', ['persistence', 'scene-load', 'api-handler'], 'simple', true);

// --- engine_gui runtime ---
const pRun = 'engine/core/engine_gui_runtime.cpp';
addFn(pRun, 'HasThirdPersonPlayerNode', [19, 28], 'Recursively finds a TAG_PLAYER node used to decide whether play should hide the cursor.', ['scene-graph', 'lookup', 'camera'], 'simple');
addFn(pRun, 'EnterRuntimePlay', [38, 50], 'Snapshots start transforms, syncs runtime, seeds physics poses, and hides the cursor for follow cameras.', ['play-mode', 'runtime', 'camera'], 'simple');
addFn(pRun, 'EngineGUI_Initialize', [52, 57], 'Refreshes the model asset cache and initializes the scene loader for the runtime host.', ['entry-point', 'initialization', 'runtime'], 'simple', true);
addFn(pRun, 'EngineGUI_Finalize', [59, 63], 'Shuts down the scene loader and clears runtime play/camera state.', ['shutdown', 'runtime', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_Update', [65, 77], 'Ticks non-player scene roots while runtime play is active; player roots are left for GameScene.', ['play-mode', 'runtime', 'event-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_Draw', [79, 80], 'No-op draw hook so the runtime host matches the editor EngineGUI surface.', ['runtime', 'api-handler', 'rendering'], 'simple', true);
addFn(pRun, 'EngineGUI_OnBackbufferResized', [82, 83], 'No-op resize hook for the runtime EngineGUI build.', ['runtime', 'api-handler', 'viewport'], 'simple', true);
addFn(pRun, 'EngineGUI_DrawSelectionIndicator', [85, 86], 'No-op selection overlay in the runtime build.', ['runtime', 'api-handler', 'editor'], 'simple', true);
addFn(pRun, 'EngineGUI_IsPlaying', [88, 90], 'Returns whether the runtime host is currently in play mode.', ['play-mode', 'runtime', 'query'], 'simple', true);
addFn(pRun, 'EngineGUI_PreferPlayVisuals', [92, 94], 'Reports play visuals as active whenever runtime play is on.', ['play-mode', 'runtime', 'query'], 'simple', true);
addFn(pRun, 'EngineGUI_IsSceneAssetPreloadActive', [96, 98], 'Always false in the runtime build (no editor preload pipeline).', ['runtime', 'api-handler', 'preload'], 'simple', true);
addFn(pRun, 'EngineGUI_IsDebugCameraPlayInputActive', [100, 102], 'Always false in the runtime build.', ['runtime', 'api-handler', 'camera'], 'simple', true);
addFn(pRun, 'EngineGUI_IsOnStart', [104, 106], 'True when the runtime host is not playing.', ['play-mode', 'runtime', 'query'], 'simple', true);
addFn(pRun, 'EngineGUI_StartPlay', [108, 110], 'Enters runtime play via EnterRuntimePlay.', ['play-mode', 'runtime', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_StopPlay', [112, 117], 'Tears down the play session, disables the debug camera, and resyncs runtime.', ['play-mode', 'runtime', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_SetEnabled', [119, 120], 'No-op editor-enable toggle in the runtime build.', ['runtime', 'api-handler', 'editor'], 'simple', true);
addFn(pRun, 'EngineGUI_GetGameViewportRect', [122, 127], 'Reports the full backbuffer as the game viewport.', ['viewport', 'runtime', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_SaveToFile', [129, 131], 'Saves the loaded scene through SceneLoader_SaveToFile.', ['persistence', 'runtime', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_LoadFromFile', [133, 138], 'Loads a scene and immediately enters runtime play on success.', ['persistence', 'play-mode', 'runtime'], 'simple', true);
addFn(pRun, 'EngineGUI_GetDefaultScenePath', [140, 142], 'Forwards the default scene path from SceneLoader.', ['runtime', 'persistence', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_ResolveStartupScenePath', [144, 146], 'Returns an empty path so the runtime uses the default scene.', ['runtime', 'api-handler', 'persistence'], 'simple', true);
addFn(pRun, 'EngineGUI_GetCameraMode', [148, 150], 'Returns the runtime camera mode (defaults to ThirdPerson).', ['camera', 'runtime', 'query'], 'simple', true);
addFn(pRun, 'EngineGUI_SetCameraMode', [152, 154], 'Stores the requested runtime camera mode.', ['camera', 'runtime', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_GetActiveCameraNodeName', [156, 158], 'Returns the stored active scene-camera node name.', ['camera', 'runtime', 'query'], 'simple', true);
addFn(pRun, 'EngineGUI_SetActiveCameraNodeName', [160, 162], 'Records which scene camera node name is currently active.', ['camera', 'runtime', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_IsUiOverlayVisible', [164, 166], 'Always reports the UI overlay as visible in the runtime build.', ['runtime', 'ui', 'query'], 'simple', true);
addFn(pRun, 'EngineGUI_SetUiOverlayVisible', [168, 169], 'No-op UI overlay toggle in the runtime build.', ['runtime', 'ui', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_GetEditMode', [171, 173], 'Always returns Scene3D edit mode in the runtime build.', ['runtime', 'editor', 'query'], 'simple', true);
addFn(pRun, 'EngineGUI_SetEditMode', [175, 176], 'No-op edit-mode setter in the runtime build.', ['runtime', 'editor', 'api-handler'], 'simple', true);
addFn(pRun, 'EngineGUI_IsUiEditMode', [178, 180], 'Always false in the runtime build.', ['runtime', 'ui', 'query'], 'simple', true);
addFn(pRun, 'EngineGUI_MouseToViewportLocal', [182, 191], 'Maps the hardware mouse into backbuffer space and reports whether it is inside the viewport.', ['input', 'viewport', 'runtime'], 'simple', true);
addFn(pRun, 'EngineGUI_IsSceneGraphObject', [193, 195], 'Forwards scene-graph membership checks to SceneLoader_IsSceneGraphObject.', ['scene-graph', 'runtime', 'query'], 'simple', true);
addFn(pRun, 'DrawEditorSceneProxies', [197, 198], 'No-op editor proxy draw in the runtime build.', ['runtime', 'api-handler', 'rendering'], 'simple', true);

// --- audio component ---
const pAud = 'engine/ecs/audio_component.cpp';
addFn(pAud, 'OnAttach', [32, 34], 'Registers this AudioComponent with AudioSystem.', ['ecs-component', 'lifecycle', 'audio'], 'simple', true);
addFn(pAud, 'OnDetach', [36, 41], 'Unregisters the component, stops playback, and clears deferred commands.', ['ecs-component', 'lifecycle', 'audio'], 'simple', true);
addFn(pAud, 'Update', [43, 55], 'Auto-plays once if requested and flags finished-check plus spatial position sync.', ['ecs-component', 'event-handler', 'audio'], 'simple', true);
addFn(pAud, 'OnReset', [57, 60], 'Stops playback and clears the auto-play latch for Return-to-Start.', ['ecs-component', 'lifecycle', 'audio'], 'simple', true);
addFn(pAud, 'Serialize', [62, 72], 'Writes clip path, bus, volume, loop, autoPlay, spatial, and distanceScale to JSON.', ['serialization', 'audio', 'ecs-component'], 'simple', true);
addFn(pAud, 'Deserialize', [74, 90], 'Restores clip settings from JSON and stops any current instance.', ['serialization', 'audio', 'ecs-component'], 'simple', true);
addFn(pAud, 'Play', [92, 97], 'Queues stop-then-play and flushes immediately when not inside GameObject::Update.', ['audio', 'playback', 'api-handler'], 'simple', true);
addFn(pAud, 'Stop', [99, 104], 'Queues a stop and flushes immediately when it is safe to touch AudioManager.', ['audio', 'playback', 'api-handler'], 'simple', true);
addFn(pAud, 'IsPlaying', [106, 112], 'True if a play is queued or AudioManager still reports the playback id.', ['audio', 'query', 'playback'], 'simple', true);
addFn(pAud, 'SetClipPath', [114, 119], 'Stops current audio and replaces the clip path, resetting auto-play.', ['audio', 'api-handler', 'asset-load'], 'simple', true);
addFn(pAud, 'ApplyLiveSettings', [121, 125], 'Queues a live volume/distance update and flushes when not inside Update.', ['audio', 'api-handler', 'playback'], 'simple', true);
addFn(pAud, 'ApplyStopImmediate', [127, 131], 'Stops the current AudioManager instance and clears the playback id.', ['audio', 'playback', 'service'], 'simple', true);
addFn(pAud, 'FlushCommands', [133, 177], 'Applies queued stop/play/finished/settings/position commands to AudioManager.', ['audio', 'playback', 'service'], 'moderate', true);
addFn(pAud, 'OwnerPosition', [179, 184], 'Returns the owner world position from Transform, or legacy GameObject position.', ['audio', 'transform', 'utility'], 'simple', true);

// --- camera component ---
const pCamCpp = 'engine/ecs/camera_component.cpp';
addFn(pCamCpp, 'TransformAxisWS', [11, 21], 'Transforms a local axis by the world matrix and returns a normalized fallback when degenerate.', ['math', 'transform', 'utility'], 'simple');
addFn(pCamCpp, 'ComputeEyeWS', [25, 31], 'Reads the world-space camera eye from the Transform translation.', ['camera', 'math', 'transform'], 'simple', true);
addFn(pCamCpp, 'ComputeForwardWS', [33, 35], 'Returns the Transform local +Z axis in world space.', ['camera', 'math', 'transform'], 'simple', true);
addFn(pCamCpp, 'ComputeUpWS', [37, 39], 'Returns the Transform local +Y axis in world space.', ['camera', 'math', 'transform'], 'simple', true);
addFn(pCamCpp, 'ComputeRightWS', [41, 43], 'Returns the Transform local +X axis in world space.', ['camera', 'math', 'transform'], 'simple', true);
addFn(pCamCpp, 'ComputeViewUpWS', [45, 59], 'Builds a stable view-up from forward crossed with world Y, switching to +Z near vertical pitch.', ['camera', 'math', 'utility'], 'simple', true);
addFn(pCamCpp, 'ApplyToGlobalCamera', [61, 72], 'Writes eye, look target, up, FOV, and clip planes into the global Camera singleton.', ['camera', 'service', 'ecs-component'], 'simple', true);
addFn(pCamCpp, 'Serialize', [74, 81], 'Serializes enabled, fov, and clip planes to JSON.', ['serialization', 'camera', 'ecs-component'], 'simple', true);
addFn(pCamCpp, 'Deserialize', [83, 93], 'Restores lens parameters from JSON with defaults on parse failure.', ['serialization', 'camera', 'ecs-component'], 'simple', true);

const pCamH = 'engine/ecs/camera_component.h';
addCls(
  pCamH,
  'CameraComponent',
  [8, 28],
  'Component storing lens settings and helpers that derive a world-space view from an engine::Transform.',
  ['camera', 'ecs-component', 'data-model'],
  'simple',
);

// --- effect component ---
const pFx = 'engine/ecs/effect_component.cpp';
addFn(pFx, 'Utf8ToWide', [17, 39], 'Converts a UTF-8 (or ACP fallback) path to a wide string for EffectSystem loading.', ['utility', 'encoding', 'vfx'], 'moderate');
addFn(pFx, 'OnAttach', [43, 46], 'Registers with EffectSystem and loads the cached effect asset.', ['ecs-component', 'lifecycle', 'vfx'], 'simple', true);
addFn(pFx, 'OnDetach', [48, 53], 'Unregisters, stops the instance, and clears deferred commands.', ['ecs-component', 'lifecycle', 'vfx'], 'simple', true);
addFn(pFx, 'Update', [55, 67], 'Auto-plays once and flags existence checks plus transform sync while an instance is live.', ['ecs-component', 'event-handler', 'vfx'], 'simple', true);
addFn(pFx, 'OnReset', [69, 73], 'Stops playback, clears auto-play, and reloads the effect for Return-to-Start.', ['ecs-component', 'lifecycle', 'vfx'], 'simple', true);
addFn(pFx, 'Serialize', [75, 84], 'Writes effect path, autoPlay/destroy, speed, lifetime, and random offset to JSON.', ['serialization', 'vfx', 'ecs-component'], 'simple', true);
addFn(pFx, 'Deserialize', [86, 102], 'Restores effect settings from JSON and invalidates the cached effect id.', ['serialization', 'vfx', 'ecs-component'], 'simple', true);
addFn(pFx, 'Play', [104, 109], 'Queues stop-then-play and flushes immediately when not inside Update.', ['vfx', 'playback', 'api-handler'], 'simple', true);
addFn(pFx, 'Stop', [111, 116], 'Queues a stop and flushes immediately when safe.', ['vfx', 'playback', 'api-handler'], 'simple', true);
addFn(pFx, 'IsPlaying', [118, 122], 'True if play is queued or EffectSystem still has the handle.', ['vfx', 'query', 'playback'], 'simple', true);
addFn(pFx, 'SetEffectPath', [124, 132], 'Stops, replaces the effect path, and reloads the cached effect.', ['vfx', 'api-handler', 'asset-load'], 'simple', true);
addFn(pFx, 'ApplyStopImmediate', [134, 139], 'Stops the EffectSystem instance and clears the handle.', ['vfx', 'playback', 'service'], 'simple', true);
addFn(pFx, 'FlushCommands', [141, 184], 'Applies queued stop/exist/play/sync/auto-destroy commands to EffectSystem.', ['vfx', 'playback', 'service'], 'moderate', true);
addFn(pFx, 'LoadEffect', [186, 200], 'Loads and caches the effect asset from a wide path if not already loaded.', ['vfx', 'asset-load', 'service'], 'simple', true);
addFn(pFx, 'SyncTransformImmediate', [202, 217], 'Pushes the owner world pose and scale onto the live EffectSystem instance.', ['vfx', 'transform', 'service'], 'simple', true);

// --- game object ---
const pGO = 'engine/ecs/game_object.h';
addCls(
  pGO,
  'GameObject',
  [59, 446],
  'Scene node that owns components and children, keeps a legacy pose, and exposes collision, rigidbody, and collider factories.',
  ['ecs', 'scene-graph', 'data-model', 'entry-point'],
  'complex',
);
addFn(pGO, 'AddComponent', [102, 112], 'Constructs a component in place, attaches it, and replaces any existing component of the same type.', ['ecs', 'factory', 'component-system'], 'simple');
addFn(pGO, 'IntegratePhysics', [205, 223], 'Integrates a non-kinematic rigidbody when the object has no scene collider (avoids double integration).', ['physics', 'ecs', 'lifecycle'], 'simple');
addFn(pGO, 'Draw', [228, 241], 'Optionally debug-draws the collider, then recursively draws children.', ['rendering', 'debug-draw', 'scene-graph'], 'simple');
addFn(pGO, 'GetRigidbody', [303, 324], 'Lazy-adds a RigidbodyComponent, or returns a kinematic fallback from the const overload path.', ['physics', 'ecs-component', 'factory'], 'simple');
addFn(pGO, 'CreateBoxCollider', [359, 370], 'Replaces the owned collider with a BoxCollider of the given size and center.', ['physics', 'factory', 'collision'], 'simple');
addFn(pGO, 'CreateSphereCollider', [372, 383], 'Replaces the owned collider with a SphereCollider.', ['physics', 'factory', 'collision'], 'simple');
addFn(pGO, 'CreateCapsuleCollider', [385, 397], 'Replaces the owned collider with a CapsuleCollider.', ['physics', 'factory', 'collision'], 'simple');
addFn(pGO, 'CreateCylinderCollider', [399, 411], 'Replaces the owned collider with a CylinderCollider.', ['physics', 'factory', 'collision'], 'simple');
addFn(pGO, 'CreateMeshCollider', [416, 429], 'Builds a MeshCollider from an explicit Model3D and attaches it to this object.', ['physics', 'factory', 'collision'], 'simple');

// --- prefab runtime ---
const pPR = 'engine/ecs/prefab_runtime.cpp';
addFn(pPR, 'ExtractFromSceneGraph', [43, 68], 'Detaches a GameObject from scene roots or its parent and returns ownership of that subtree.', ['prefab', 'scene-graph', 'ownership'], 'moderate');
addFn(pPR, 'IsInSceneGraph', [75, 94], 'Returns whether an object is still reachable from SceneLoader roots.', ['prefab', 'scene-graph', 'query'], 'simple');
addFn(pPR, 'PrefabRuntime_SetBeforeDestroyCallback', [98, 100], 'Installs a host callback invoked just before a spawned object is destroyed.', ['prefab', 'lifecycle', 'callback'], 'simple', true);
addFn(pPR, 'PrefabRuntime_IsSpawnRoot', [102, 104], 'True when the object is the reserved __RuntimeSpawned root.', ['prefab', 'query', 'scene-graph'], 'simple', true);
addFn(pPR, 'PrefabRuntime_IsRuntimeSpawned', [106, 112], 'True when the object lives under the runtime spawn root.', ['prefab', 'query', 'scene-graph'], 'simple', true);
addFn(pPR, 'PrefabRuntime_EnsureSpawnRoot', [114, 134], 'Finds or creates the __RuntimeSpawned root under the current scene nodes.', ['prefab', 'factory', 'scene-graph'], 'simple', true);
addFn(pPR, 'PrefabRuntime_InstantiateImmediate', [136, 167], 'Loads a prefab, applies world pose, parents it, and registers the subtree with SceneRuntime.', ['prefab', 'factory', 'scene-graph'], 'moderate');
addFn(pPR, 'PrefabRuntime_Instantiate', [169, 194], 'Defers instantiate into a mutex-guarded queue when called from GameObject::Update; otherwise spawns immediately.', ['prefab', 'factory', 'concurrency'], 'moderate', true);
addFn(pPR, 'PrefabRuntime_FlushSpawned', [196, 210], 'Drains the pending-spawn queue and instantiates each request immediately.', ['prefab', 'factory', 'event-handler'], 'simple', true);
addFn(pPR, 'PrefabRuntime_Destroy', [212, 225], 'Deactivates an object, fires the destroy callback, and queues it for FlushDestroyed.', ['prefab', 'lifecycle', 'concurrency'], 'simple', true);
addFn(pPR, 'PrefabRuntime_DestroyAllSpawnedChildren', [227, 243], 'Destroys every child of the runtime spawn root and flushes the destroy queue.', ['prefab', 'lifecycle', 'scene-graph'], 'simple', true);
addFn(pPR, 'PrefabRuntime_ResetSessionState', [245, 254], 'Clears spawn-root, callbacks, and pending spawn/destroy queues for a new play session.', ['prefab', 'lifecycle', 'session'], 'simple', true);
addFn(pPR, 'PrefabRuntime_FlushDestroyed', [256, 297], 'Unregisters, tears down scripts, notifies physics, and deletes queued spawned objects.', ['prefab', 'lifecycle', 'scene-graph'], 'moderate', true);

const pPRH = 'engine/ecs/prefab_serializer.h';
addCls(
  pPRH,
  'PrefabSerializer',
  [32, 42],
  'Static JSON serializer for a GameObject subtree, supporting both bare-node and versioned scene wrappers.',
  ['prefab', 'serialization', 'factory'],
  'simple',
);

// --- transform ---
const pXCpp = 'engine/ecs/transform.cpp';
addFn(pXCpp, 'EulerZyxDegreesFromRotationMatrix', [25, 51], 'Extracts ZYX Euler degrees from a rotation matrix, with a gimbal-lock fallback.', ['math', 'transform', 'utility'], 'moderate');
addFn(pXCpp, 'OnAttach', [55, 57], 'Snapshots the deserialized local pose as the Return-to-Start state.', ['ecs-component', 'lifecycle', 'transform'], 'simple', true);
addFn(pXCpp, 'OnReset', [59, 63], 'Restores local pose from the last snapshotted start state.', ['ecs-component', 'lifecycle', 'transform'], 'simple', true);
addFn(pXCpp, 'SnapshotStartState', [65, 69], 'Copies current local pose into the start-state fields used by OnReset.', ['transform', 'lifecycle', 'ecs-component'], 'simple', true);
addFn(pXCpp, 'GetLocalMatrix', [71, 79], 'Builds S * Rzyx * T from local scale, Euler degrees, and position.', ['transform', 'math', 'ecs-component'], 'simple', true);
addFn(pXCpp, 'GetWorldMatrix', [81, 92], 'Composes local matrices up the parent Transform chain.', ['transform', 'math', 'scene-graph'], 'simple', true);
addFn(pXCpp, 'GetWorldPosition', [94, 98], 'Reads world translation from the composed world matrix.', ['transform', 'math', 'ecs-component'], 'simple', true);
addFn(pXCpp, 'GetWorldPositionRotationScale', [100, 119], 'Returns world pose, skipping decompose when there is no parent Transform.', ['transform', 'math', 'ecs-component'], 'simple', true);
addFn(pXCpp, 'SetFromLocalMatrix', [121, 130], 'Decomposes a matrix back into local position, ZYX Euler, and scale.', ['transform', 'math', 'ecs-component'], 'simple', true);
addFn(pXCpp, 'Serialize', [132, 144], 'Writes local position, rotation, and scale axes to JSON.', ['serialization', 'transform', 'ecs-component'], 'simple', true);
addFn(pXCpp, 'Deserialize', [146, 162], 'Restores local pose from JSON and snapshots that pose as start state.', ['serialization', 'transform', 'ecs-component'], 'simple', true);

const pXH = 'engine/ecs/transform.h';
addCls(
  pXH,
  'Transform',
  [24, 66],
  'SceneTransform component storing local pose and deriving world matrices from the parent GameObject chain.',
  ['transform', 'ecs-component', 'data-model'],
  'moderate',
);

// --- visual system ---
const pVS = 'engine/ecs/visual_system.cpp';
addFn(pVS, 'UploadObjectTree', [13, 23], 'Recursively uploads dirty skinned vertices for VisualComponents under a node.', ['rendering', 'skinning', 'scene-graph'], 'simple');
addFn(pVS, 'VisualSystem_UploadSkinning', [27, 32], 'Uploads skinning for every scene-graph root, covering authored and runtime-spawned objects.', ['rendering', 'skinning', 'service'], 'simple', true);

// --- camera gizmo ---
const pGiz = 'engine/editor/camera_gizmo.cpp';
addFn(pGiz, 'VisitDraw', [22, 34], 'Walks a subtree and draws an enabled CameraComponent gizmo at each node.', ['editor', 'gizmo', 'scene-graph'], 'simple');
addFn(pGiz, 'DrawCamera', [40, 93], 'Draws RGB axes and a FOV near-plane frustum for one camera Transform.', ['editor', 'gizmo', 'camera', 'debug-draw'], 'moderate', true);
addFn(pGiz, 'DrawSceneCameras', [95, 98], 'Draws camera gizmos for every enabled camera under the given scene roots.', ['editor', 'gizmo', 'camera'], 'simple', true);

// --- prefab panel ---
const pPanel = 'engine/editor/editor_prefab_panel.cpp';
addFn(pPanel, 'PrefabPanel', [23, 85], 'Renders a refreshable prefab grid, instantiates on click, and starts ASSET_PREFAB drag-drop.', ['editor', 'prefab', 'ui-panel', 'factory'], 'moderate', true);

// --- imports ---
let importCount = 0;
for (const [from, tos] of Object.entries(importData)) {
  for (const to of tos) {
    edges.push(importsE(from, to));
    importCount += 1;
  }
}

// --- calls ---
const F = (path, name) => `function:${path}:${name}`;
const C = (path, name) => `class:${path}:${name}`;

edges.push(callsE(F(pALS, 'ApplyPoseFromNode'), F(pCamCpp, 'ComputeEyeWS')));
edges.push(callsE(F(pALS, 'ApplyPoseFromNode'), F(pCamCpp, 'ComputeForwardWS')));
edges.push(callsE(F(pALS, 'ApplyPoseFromNode'), F(pCamCpp, 'ComputeViewUpWS')));
edges.push(callsE(F(pALS, 'AudioListenerSync_Tick'), F(pALS, 'ApplyPoseFromNode')));
edges.push(callsE(F(pALS, 'AudioListenerSync_Tick'), F(pALS, 'AudioListenerSync_EnsureOnCamera')));
edges.push(callsE(F(pALS, 'AudioListenerSync_Tick'), F(pALS, 'FindNodeByNameInRoots')));
edges.push(callsE(F(pALS, 'AudioListenerSync_Tick'), F(pALS, 'FindFirstListener')));
edges.push(callsE(F(pALS, 'AudioListenerSync_Tick'), F(pRun, 'EngineGUI_GetCameraMode')));
edges.push(callsE(F(pALS, 'AudioListenerSync_Tick'), F(pRun, 'EngineGUI_GetActiveCameraNodeName')));
edges.push(callsE(F(pALS, 'AudioListenerSync_Tick'), F(pRun, 'EngineGUI_IsPlaying')));
edges.push(callsE(F(pALS, 'AudioListenerSync_Tick'), F('src/app/camera/debug_camera.cpp', 'DebugCamera_IsEnabled')));

edges.push(callsE(F(pPers, 'EngineGUI_LoadFromFileCore'), F('engine/core/engine_gui.cpp', 'SoftStopPlayForSceneLoad')));
edges.push(callsE(F(pPers, 'EngineGUI_LoadFromFile'), F(pPers, 'EngineGUI_LoadFromFileCore')));
edges.push(callsE(F(pPers, 'EngineGUI_LoadFromFile'), F('engine/core/engine_gui.cpp', 'EngineGUI_IsSceneDirty')));
edges.push(callsE(F(pPers, 'EngineGUI_LoadFromFile'), F('engine/core/engine_gui.cpp', 'EngineGUI_QueueUnsavedLoad')));
edges.push(callsE(F(pPers, 'EngineGUI_LoadFromFile'), F('engine/core/engine_gui.cpp', 'SoftStopPlayForSceneLoad')));

edges.push(callsE(F(pRun, 'EngineGUI_StartPlay'), F(pRun, 'EnterRuntimePlay')));
edges.push(callsE(F(pRun, 'EngineGUI_LoadFromFile'), F(pRun, 'EnterRuntimePlay')));
edges.push(callsE(F(pRun, 'EnterRuntimePlay'), F(pRun, 'HasThirdPersonPlayerNode')));
edges.push(callsE(F(pRun, 'EnterRuntimePlay'), F('src/app/camera/debug_camera.cpp', 'DebugCamera_SetEnabled')));
edges.push(callsE(F(pRun, 'EnterRuntimePlay'), F('src/app/camera/third_person_camera.cpp', 'ThirdPersonCamera_ShowCursor')));
edges.push(callsE(F(pRun, 'EngineGUI_StopPlay'), F('src/app/camera/debug_camera.cpp', 'DebugCamera_SetEnabled')));

edges.push(callsE(F(pAud, 'OnDetach'), F(pAud, 'ApplyStopImmediate')));
edges.push(callsE(F(pAud, 'Update'), F(pAud, 'Play')));
edges.push(callsE(F(pAud, 'OnReset'), F(pAud, 'Stop')));
edges.push(callsE(F(pAud, 'Deserialize'), F(pAud, 'Stop')));
edges.push(callsE(F(pAud, 'Play'), F(pAud, 'FlushCommands')));
edges.push(callsE(F(pAud, 'Stop'), F(pAud, 'FlushCommands')));
edges.push(callsE(F(pAud, 'SetClipPath'), F(pAud, 'Stop')));
edges.push(callsE(F(pAud, 'ApplyLiveSettings'), F(pAud, 'FlushCommands')));
edges.push(callsE(F(pAud, 'FlushCommands'), F(pAud, 'ApplyStopImmediate')));
edges.push(callsE(F(pAud, 'FlushCommands'), F(pAud, 'OwnerPosition')));
edges.push(callsE(F(pAud, 'OwnerPosition'), F(pXCpp, 'GetWorldPosition')));

edges.push(callsE(F(pCamCpp, 'ComputeForwardWS'), F(pCamCpp, 'TransformAxisWS')));
edges.push(callsE(F(pCamCpp, 'ComputeUpWS'), F(pCamCpp, 'TransformAxisWS')));
edges.push(callsE(F(pCamCpp, 'ComputeRightWS'), F(pCamCpp, 'TransformAxisWS')));
edges.push(callsE(F(pCamCpp, 'TransformAxisWS'), F(pXCpp, 'GetWorldMatrix')));
edges.push(callsE(F(pCamCpp, 'ComputeEyeWS'), F(pXCpp, 'GetWorldMatrix')));
edges.push(callsE(F(pCamCpp, 'ApplyToGlobalCamera'), F(pCamCpp, 'ComputeEyeWS')));
edges.push(callsE(F(pCamCpp, 'ApplyToGlobalCamera'), F(pCamCpp, 'ComputeForwardWS')));
edges.push(callsE(F(pCamCpp, 'ApplyToGlobalCamera'), F(pCamCpp, 'ComputeViewUpWS')));

edges.push(callsE(F(pFx, 'OnAttach'), F(pFx, 'LoadEffect')));
edges.push(callsE(F(pFx, 'OnDetach'), F(pFx, 'ApplyStopImmediate')));
edges.push(callsE(F(pFx, 'Update'), F(pFx, 'Play')));
edges.push(callsE(F(pFx, 'OnReset'), F(pFx, 'Stop')));
edges.push(callsE(F(pFx, 'OnReset'), F(pFx, 'LoadEffect')));
edges.push(callsE(F(pFx, 'Deserialize'), F(pFx, 'Stop')));
edges.push(callsE(F(pFx, 'Play'), F(pFx, 'FlushCommands')));
edges.push(callsE(F(pFx, 'Stop'), F(pFx, 'FlushCommands')));
edges.push(callsE(F(pFx, 'SetEffectPath'), F(pFx, 'Stop')));
edges.push(callsE(F(pFx, 'SetEffectPath'), F(pFx, 'LoadEffect')));
edges.push(callsE(F(pFx, 'FlushCommands'), F(pFx, 'ApplyStopImmediate')));
edges.push(callsE(F(pFx, 'FlushCommands'), F(pFx, 'LoadEffect')));
edges.push(callsE(F(pFx, 'FlushCommands'), F(pFx, 'SyncTransformImmediate')));
edges.push(callsE(F(pFx, 'LoadEffect'), F(pFx, 'Utf8ToWide')));
edges.push(callsE(F(pFx, 'SyncTransformImmediate'), F(pXCpp, 'GetWorldPositionRotationScale')));

edges.push(callsE(F(pPR, 'PrefabRuntime_IsRuntimeSpawned'), F(pPR, 'PrefabRuntime_IsSpawnRoot')));
edges.push(callsE(F(pPR, 'PrefabRuntime_EnsureSpawnRoot'), F(pPR, 'IsInSceneGraph')));
edges.push(callsE(F(pPR, 'PrefabRuntime_EnsureSpawnRoot'), F(pPR, 'PrefabRuntime_IsSpawnRoot')));
edges.push(callsE(F(pPR, 'PrefabRuntime_InstantiateImmediate'), F(pPR, 'PrefabRuntime_EnsureSpawnRoot')));
edges.push(callsE(F(pPR, 'PrefabRuntime_InstantiateImmediate'), F('engine/ecs/prefab_serializer.cpp', 'LoadFromFile')));
edges.push(callsE(F(pPR, 'PrefabRuntime_InstantiateImmediate'), F('engine/scene/scene_runtime.h', 'RegisterSubtree')));
edges.push(callsE(F(pPR, 'PrefabRuntime_Instantiate'), F(pPR, 'PrefabRuntime_InstantiateImmediate')));
edges.push(callsE(F(pPR, 'PrefabRuntime_FlushSpawned'), F(pPR, 'PrefabRuntime_InstantiateImmediate')));
edges.push(callsE(F(pPR, 'PrefabRuntime_DestroyAllSpawnedChildren'), F(pPR, 'PrefabRuntime_IsSpawnRoot')));
edges.push(callsE(F(pPR, 'PrefabRuntime_DestroyAllSpawnedChildren'), F(pPR, 'PrefabRuntime_Destroy')));
edges.push(callsE(F(pPR, 'PrefabRuntime_DestroyAllSpawnedChildren'), F(pPR, 'PrefabRuntime_FlushDestroyed')));
edges.push(callsE(F(pPR, 'PrefabRuntime_FlushDestroyed'), F(pPR, 'ExtractFromSceneGraph')));
edges.push(callsE(F(pPR, 'PrefabRuntime_FlushDestroyed'), F('engine/scripting/script_component.h', 'Teardown')));
edges.push(callsE(F(pPR, 'PrefabRuntime_FlushDestroyed'), F('engine/scene/scene_runtime.h', 'UnregisterSubtree')));

edges.push(callsE(F(pXCpp, 'OnAttach'), F(pXCpp, 'SnapshotStartState')));
edges.push(callsE(F(pXCpp, 'GetWorldMatrix'), F(pXCpp, 'GetLocalMatrix')));
edges.push(callsE(F(pXCpp, 'GetWorldPosition'), F(pXCpp, 'GetWorldMatrix')));
edges.push(callsE(F(pXCpp, 'GetWorldPositionRotationScale'), F(pXCpp, 'GetWorldMatrix')));
edges.push(callsE(F(pXCpp, 'GetWorldPositionRotationScale'), F(pXCpp, 'EulerZyxDegreesFromRotationMatrix')));
edges.push(callsE(F(pXCpp, 'SetFromLocalMatrix'), F(pXCpp, 'EulerZyxDegreesFromRotationMatrix')));
edges.push(callsE(F(pXCpp, 'Deserialize'), F(pXCpp, 'SnapshotStartState')));

edges.push(callsE(F(pVS, 'VisualSystem_UploadSkinning'), F(pVS, 'UploadObjectTree')));

edges.push(callsE(F(pGiz, 'VisitDraw'), F(pGiz, 'DrawCamera')));
edges.push(callsE(F(pGiz, 'DrawSceneCameras'), F(pGiz, 'VisitDraw')));
edges.push(callsE(F(pGiz, 'DrawCamera'), F(pCamCpp, 'ComputeEyeWS')));
edges.push(callsE(F(pGiz, 'DrawCamera'), F(pCamCpp, 'ComputeForwardWS')));
edges.push(callsE(F(pGiz, 'DrawCamera'), F(pCamCpp, 'ComputeUpWS')));
edges.push(callsE(F(pGiz, 'DrawCamera'), F(pCamCpp, 'ComputeRightWS')));

edges.push(callsE(F(pPanel, 'PrefabPanel'), F('engine/ecs/prefab_serializer.cpp', 'LoadFromFile')));

// class contains methods living in the header
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'AddComponent'), 'contains', 1.0));
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'IntegratePhysics'), 'contains', 1.0));
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'Draw'), 'contains', 1.0));
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'GetRigidbody'), 'contains', 1.0));
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'CreateBoxCollider'), 'contains', 1.0));
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'CreateSphereCollider'), 'contains', 1.0));
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'CreateCapsuleCollider'), 'contains', 1.0));
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'CreateCylinderCollider'), 'contains', 1.0));
edges.push(edge(C(pGO, 'GameObject'), F(pGO, 'CreateMeshCollider'), 'contains', 1.0));

// GameObject methods that are "exported" as the public class API
for (const name of [
  'AddComponent', 'IntegratePhysics', 'Draw', 'GetRigidbody',
  'CreateBoxCollider', 'CreateSphereCollider', 'CreateCapsuleCollider',
  'CreateCylinderCollider', 'CreateMeshCollider',
]) {
  edges.push(exportsE(pGO, F(pGO, name)));
}

const neighborMap = JSON.parse(
  readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-neighbor-7.json', 'utf8'),
);

const neighborFiles = new Set();
const neighborSymbols = new Map();
for (const neighbors of Object.values(neighborMap)) {
  for (const n of neighbors) {
    neighborFiles.add(n.path);
    const set = neighborSymbols.get(n.path) || new Set();
    for (const s of n.symbols || []) set.add(s);
    neighborSymbols.set(n.path, set);
  }
}
const importTargets = new Set(Object.values(importData).flat());
const batchFiles = Object.keys(importData);

const nodeIds = new Set(nodes.map((n) => n.id));

function targetOk(target) {
  if (nodeIds.has(target)) return true;
  if (target.startsWith('file:')) {
    const p = target.slice(5);
    return neighborFiles.has(p) || importTargets.has(p) || batchFiles.includes(p);
  }
  const m = target.match(/^(?:function|class):(.+):(.+)$/);
  if (!m) return false;
  const [, path, symbol] = m;
  if (nodeIds.has(target)) return true;
  const syms = neighborSymbols.get(path);
  return !!(syms && syms.has(symbol));
}

const bad = edges.filter((e) => !targetOk(e.target) || (!nodeIds.has(e.source) && !targetOk(e.source)));
if (bad.length) {
  console.error('INVALID EDGES', bad.length);
  for (const e of bad.slice(0, 30)) console.error(e);
  process.exit(1);
}

const expectedImports = Object.values(importData).reduce((s, a) => s + a.length, 0);
if (importCount !== expectedImports) {
  console.error('import mismatch', importCount, expectedImports);
  process.exit(1);
}

const shortTags = nodes.filter((n) => !n.tags || n.tags.length < 3 || n.tags.length > 5 || !n.summary);
if (shortTags.length) {
  console.error('BAD NODE FIELDS', shortTags.map((n) => [n.id, n.tags?.length, !!n.summary]));
  process.exit(1);
}

const ids = nodes.map((n) => n.id);
if (new Set(ids).size !== ids.length) {
  const seen = new Set();
  for (const id of ids) {
    if (seen.has(id)) console.error('DUP', id);
    seen.add(id);
  }
  process.exit(1);
}

const filePaths = [...new Set(nodes.filter((n) => n.filePath).map((n) => n.filePath))].sort();
const nodeCount = nodes.length;
const edgeCount = edges.length;

function nodesAndEdgesFor(group) {
  const groupSet = new Set(group);
  const partNodes = nodes.filter((n) => groupSet.has(n.filePath));
  const partIds = new Set(partNodes.map((n) => n.id));
  const partEdges = edges.filter((e) => partIds.has(e.source));
  return { partNodes, partEdges };
}

// Sequential alphabetical packing that stays within per-part limits.
const partitioned = [];
let current = [];
for (const fp of filePaths) {
  const trial = [...current, fp];
  const { partNodes, partEdges } = nodesAndEdgesFor(trial);
  if (current.length && (partNodes.length > 60 || partEdges.length > 120)) {
    const packed = nodesAndEdgesFor(current);
    partitioned.push({ group: current, ...packed });
    current = [fp];
  } else {
    current = trial;
  }
}
if (current.length) {
  const packed = nodesAndEdgesFor(current);
  partitioned.push({ group: current, ...packed });
}

function partTargetOk(target, partNodeIds) {
  if (partNodeIds.has(target)) return true;
  if (target.startsWith('file:')) {
    const p = target.slice(5);
    return neighborFiles.has(p) || importTargets.has(p) || batchFiles.includes(p);
  }
  const m = target.match(/^(?:function|class):(.+):(.+)$/);
  if (!m) return false;
  const [, path, symbol] = m;
  const syms = neighborSymbols.get(path);
  return !!(syms && syms.has(symbol));
}

for (const p of partitioned) {
  const partIds = new Set(p.partNodes.map((n) => n.id));
  p.partEdges = p.partEdges.filter((e) => partTargetOk(e.target, partIds));
}

const outDir = 'C:/Users/terry/source/repos/ivy-engine/.ua/intermediate';
mkdirSync(outDir, { recursive: true });

if (partitioned.length === 1) {
  writeFileSync(`${outDir}/batch-7.json`, JSON.stringify({ nodes, edges }, null, 2));
  console.log('wrote batch-7.json', nodeCount, edgeCount);
} else {
  let writtenNodes = 0;
  let writtenEdges = 0;
  partitioned.forEach((p, i) => {
    const dest = `${outDir}/batch-7-part-${i + 1}.json`;
    writeFileSync(dest, JSON.stringify({ nodes: p.partNodes, edges: p.partEdges }, null, 2));
    writtenNodes += p.partNodes.length;
    writtenEdges += p.partEdges.length;
    console.log(`wrote ${dest} nodes=${p.partNodes.length} edges=${p.partEdges.length} files=${p.group.length}`);
  });
  console.log('totals', writtenNodes, writtenEdges, 'parts', partitioned.length, 'imports', importCount);
}
