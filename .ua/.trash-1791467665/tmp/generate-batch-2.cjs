const fs = require('fs');

function n(id, type, name, filePath, summary, tags, complexity, extra = {}) {
  const node = { id, type, name, summary, tags, complexity };
  if (filePath) node.filePath = filePath;
  Object.assign(node, extra);
  return node;
}
function e(source, target, type, weight) {
  return { source, target, type, direction: 'forward', weight };
}

const nodes = [];
const edges = [];

// ── File nodes ──────────────────────────────────────────────
nodes.push(
  n('file:engine/ecs/audio_component.h', 'file', 'audio_component.h', 'engine/ecs/audio_component.h',
    'Declares AudioComponent, a GameObject component that queues play/stop and spatial playback commands for the audio system.',
    ['component', 'audio', 'data-model', 'serialization'], 'simple',
    { languageNotes: 'C++ header parsed as C; lifecycle methods are declared and implemented in audio_component.cpp.' }),
  n('file:engine/ecs/audio_listener_component.cpp', 'file', 'audio_listener_component.cpp', 'engine/ecs/audio_listener_component.cpp',
    'Implements empty JSON serialize/deserialize for AudioListenerComponent; pose is applied by the listener sync system.',
    ['component', 'audio', 'serialization'], 'simple'),
  n('file:engine/ecs/audio_listener_component.h', 'file', 'audio_listener_component.h', 'engine/ecs/audio_listener_component.h',
    'Declares AudioListenerComponent, a marker component identifying the scene node that owns the 3D audio listener.',
    ['component', 'audio', 'data-model'], 'simple'),
  n('file:engine/ecs/billboard_component.h', 'file', 'billboard_component.h', 'engine/ecs/billboard_component.h',
    'Declares BillboardComponent for textured camera-facing quads, including scene-wide submit helpers separate from VisualComponent.',
    ['component', 'rendering', 'data-model', 'serialization'], 'moderate'),
  n('file:engine/ecs/collider_component.h', 'file', 'collider_component.h', 'engine/ecs/collider_component.h',
    'Declares ColliderComponent, serializable collider shape settings that apply to or clear a GameObject physics collider.',
    ['component', 'physics', 'data-model', 'serialization'], 'simple'),
  n('file:engine/ecs/component.h', 'file', 'component.h', 'engine/ecs/component.h',
    'Defines the Component base interface: owner pointer, lifecycle hooks, parameter injection, and JSON serialize/deserialize.',
    ['type-definition', 'component', 'data-model'], 'simple',
    { languageNotes: 'Abstract base with default no-op lifecycle methods; GetTypeName is pure virtual.' }),
  n('file:engine/ecs/component_factory.cpp', 'file', 'component_factory.cpp', 'engine/ecs/component_factory.cpp',
    'Implements the ComponentFactory singleton registry used to construct components by type-name string.',
    ['factory', 'singleton', 'component'], 'simple'),
  n('file:engine/ecs/component_factory.h', 'file', 'component_factory.h', 'engine/ecs/component_factory.h',
    'Declares the singleton ComponentFactory and REGISTER_COMPONENT macro for JSON-driven component instantiation.',
    ['factory', 'singleton', 'type-definition'], 'simple'),
  n('file:engine/ecs/directional_light_component.cpp', 'file', 'directional_light_component.cpp', 'engine/ecs/directional_light_component.cpp',
    'Serializes directional light properties and derives world-space shine direction via LightGizmo helpers.',
    ['component', 'lighting', 'serialization'], 'simple'),
  n('file:engine/ecs/directional_light_component.h', 'file', 'directional_light_component.h', 'engine/ecs/directional_light_component.h',
    'Declares DirectionalLightComponent with color, intensity, specular, and shadow-casting fields.',
    ['component', 'lighting', 'data-model'], 'simple'),
  n('file:engine/ecs/effect_component.h', 'file', 'effect_component.h', 'engine/ecs/effect_component.h',
    'Declares EffectComponent, which queues play/stop of particle or visual effects and syncs their transform on the main thread.',
    ['component', 'effects', 'data-model'], 'simple'),
  n('file:engine/ecs/material_component.cpp', 'file', 'material_component.cpp', 'engine/ecs/material_component.cpp',
    'Implements MaterialComponent JSON I/O and release of cached normal-map and toon-ramp shader resource views.',
    ['component', 'rendering', 'serialization'], 'moderate'),
  n('file:engine/ecs/material_component.h', 'file', 'material_component.h', 'engine/ecs/material_component.h',
    'Declares per-object lit/toon material parameters including normal maps, outline, shadow tint, and ramp textures.',
    ['component', 'rendering', 'data-model'], 'moderate'),
  n('file:engine/ecs/point_light_component.cpp', 'file', 'point_light_component.cpp', 'engine/ecs/point_light_component.cpp',
    'Serializes and deserializes PointLightComponent color, range, attenuation, and specular settings.',
    ['component', 'lighting', 'serialization'], 'simple'),
  n('file:engine/ecs/point_light_component.h', 'file', 'point_light_component.h', 'engine/ecs/point_light_component.h',
    'Declares PointLightComponent with omnidirectional light color, intensity, range, and attenuation.',
    ['component', 'lighting', 'data-model'], 'simple'),
  n('file:engine/ecs/prefab_serializer.cpp', 'file', 'prefab_serializer.cpp', 'engine/ecs/prefab_serializer.cpp',
    'Recursively serializes GameObject trees to versioned prefab JSON and reconstructs nodes via ComponentFactory and TagRegistry.',
    ['serialization', 'prefab', 'service'], 'moderate'),
  n('file:engine/ecs/rigidbody_component.cpp', 'file', 'rigidbody_component.cpp', 'engine/ecs/rigidbody_component.cpp',
    'Applies Rigidbody settings to the owner and serializes mass, drag, gravity, and kinematic flags.',
    ['component', 'physics', 'serialization'], 'simple'),
  n('file:engine/ecs/rigidbody_component.h', 'file', 'rigidbody_component.h', 'engine/ecs/rigidbody_component.h',
    'Declares RigidbodyComponent wrapping a physics Rigidbody with apply/reset and JSON persistence.',
    ['component', 'physics', 'data-model'], 'simple'),
  n('file:engine/ecs/spot_light_component.cpp', 'file', 'spot_light_component.cpp', 'engine/ecs/spot_light_component.cpp',
    'Serializes and deserializes SpotLightComponent cone angles, range, color, and optional shadow casting.',
    ['component', 'lighting', 'serialization'], 'simple'),
  n('file:engine/ecs/spot_light_component.h', 'file', 'spot_light_component.h', 'engine/ecs/spot_light_component.h',
    'Declares SpotLightComponent with cone inner/outer angles, range, attenuation, and shadow flag.',
    ['component', 'lighting', 'data-model'], 'simple'),
  n('file:engine/ecs/tag_registry.cpp', 'file', 'tag_registry.cpp', 'engine/ecs/tag_registry.cpp',
    'Implements the TagRegistry singleton: built-in tag slots, custom tag add/remove, and rom/tags.json persistence.',
    ['singleton', 'data-model', 'serialization'], 'moderate'),
  n('file:engine/ecs/tag_registry.h', 'file', 'tag_registry.h', 'engine/ecs/tag_registry.h',
    'Declares the project-wide GameObject tag catalog with stable built-in indices and custom tags persisted to JSON.',
    ['type-definition', 'singleton', 'data-model'], 'simple'),
  n('file:engine/editor/editor_undo.cpp', 'file', 'editor_undo.cpp', 'engine/editor/editor_undo.cpp',
    'Implements the editor undo stack: transform, component, hierarchy, and UI commands plus ImGui-aware edit sessions.',
    ['event-handler', 'editor', 'serialization', 'service'], 'complex',
    { languageNotes: 'Command pattern: anonymous-namespace EditorCommand subclasses plus a public EditorUndo_* C API.' }),
  n('file:engine/editor/light_gizmo.cpp', 'file', 'light_gizmo.cpp', 'engine/editor/light_gizmo.cpp',
    'Draws debug gizmos for directional, point, and spot lights and computes world-space light pose from Transform.',
    ['utility', 'lighting', 'editor', 'debug'], 'moderate'),
  n('file:engine/editor/light_gizmo.h', 'file', 'light_gizmo.h', 'engine/editor/light_gizmo.h',
    'Public LightGizmo API for light world pose helpers and debug-draw of scene light volumes.',
    ['type-definition', 'lighting', 'editor'], 'simple'),
  n('file:engine/scene/scene_lighting.cpp', 'file', 'scene_lighting.cpp', 'engine/scene/scene_lighting.cpp',
    'Walks scene nodes to push enabled light components into the Lighting/Shadow systems, with a default fallback rig.',
    ['service', 'lighting', 'scene'], 'moderate'),
  n('file:engine/scene/scene_loader.cpp', 'file', 'scene_loader.cpp', 'engine/scene/scene_loader.cpp',
    'Loads and saves scene JSON, registers ECS component types, manages script DLLs, and coordinates play-session teardown.',
    ['entry-point', 'serialization', 'scene', 'service'], 'complex'),
  n('file:engine/scene/scene_runtime.cpp', 'file', 'scene_runtime.cpp', 'engine/scene/scene_runtime.cpp',
    'Registers editor GameObject trees with the active SceneBase for physics and applies or clears collider components.',
    ['service', 'scene', 'physics'], 'moderate'),
  n('file:engine/scene/scene_runtime.h', 'file', 'scene_runtime.h', 'engine/scene/scene_runtime.h',
    'Declares the SceneRuntime singleton that syncs editor roots into the play/physics scene.',
    ['singleton', 'scene', 'type-definition'], 'simple')
);

// ── Class nodes ─────────────────────────────────────────────
nodes.push(
  n('class:engine/ecs/audio_component.h:AudioComponent', 'class', 'AudioComponent', 'engine/ecs/audio_component.h',
    'Clip playback component with bus, volume, loop, autoPlay, and spatial fields; play/stop commands flush on the main thread.',
    ['component', 'audio', 'serialization'], 'moderate', { lineRange: [8, 50] }),
  n('class:engine/ecs/audio_listener_component.h:AudioListenerComponent', 'class', 'AudioListenerComponent', 'engine/ecs/audio_listener_component.h',
    'Marker component naming the audio listener; serialization stores an empty JSON object.',
    ['component', 'audio', 'serialization'], 'simple', { lineRange: [7, 13] }),
  n('class:engine/ecs/billboard_component.h:BillboardComponent', 'class', 'BillboardComponent', 'engine/ecs/billboard_component.h',
    'Textured quad billboard with size, color, rotation, and ground-alignment; submits draw data for a scene tree.',
    ['component', 'rendering', 'serialization'], 'moderate', { lineRange: [32, 62] }),
  n('class:engine/ecs/collider_component.h:ColliderComponent', 'class', 'ColliderComponent', 'engine/ecs/collider_component.h',
    'Stores collider type, extents, trigger flag, and debug visibility; ApplyToOwner builds the physics shape.',
    ['component', 'physics', 'serialization'], 'simple', { lineRange: [7, 25] }),
  n('class:engine/ecs/component.h:Component', 'class', 'Component', 'engine/ecs/component.h',
    'Base type for all GameObject components, defining attach/detach/update/reset and JSON persistence hooks.',
    ['type-definition', 'component', 'lifecycle'], 'simple', { lineRange: [18, 48] }),
  n('class:engine/ecs/component_factory.h:ComponentFactory', 'class', 'ComponentFactory', 'engine/ecs/component_factory.h',
    'Singleton map from GetTypeName strings to creator lambdas used when loading prefabs and scenes.',
    ['factory', 'singleton', 'component'], 'simple', { lineRange: [25, 45] }),
  n('class:engine/ecs/directional_light_component.h:DirectionalLightComponent', 'class', 'DirectionalLightComponent', 'engine/ecs/directional_light_component.h',
    'Infinite-direction light with color, intensity, specular, and optional cascaded shadow casting.',
    ['component', 'lighting', 'serialization'], 'simple', { lineRange: [10, 25] }),
  n('class:engine/ecs/effect_component.h:EffectComponent', 'class', 'EffectComponent', 'engine/ecs/effect_component.h',
    'Effect playback component with autoPlay, lifetime, and main-thread flush of play/stop/transform commands.',
    ['component', 'effects', 'serialization'], 'moderate', { lineRange: [8, 49] }),
  n('class:engine/ecs/material_component.h:MaterialComponent', 'class', 'MaterialComponent', 'engine/ecs/material_component.h',
    'Per-mesh PBR/toon material with cached SRVs for normal maps and ramp textures.',
    ['component', 'rendering', 'serialization'], 'moderate', { lineRange: [28, 66] }),
  n('class:engine/ecs/point_light_component.h:PointLightComponent', 'class', 'PointLightComponent', 'engine/ecs/point_light_component.h',
    'Omnidirectional light with range and quadratic attenuation serialized as PointLight.',
    ['component', 'lighting', 'serialization'], 'simple', { lineRange: [6, 20] }),
  n('class:engine/ecs/rigidbody_component.h:RigidbodyComponent', 'class', 'RigidbodyComponent', 'engine/ecs/rigidbody_component.h',
    'Serializable wrapper around a physics Rigidbody that can apply settings onto its owner.',
    ['component', 'physics', 'serialization'], 'simple', { lineRange: [24, 39] }),
  n('class:engine/ecs/spot_light_component.h:SpotLightComponent', 'class', 'SpotLightComponent', 'engine/ecs/spot_light_component.h',
    'Cone light with inner/outer angles, range, attenuation, and optional shadows.',
    ['component', 'lighting', 'serialization'], 'simple', { lineRange: [6, 23] }),
  n('class:engine/ecs/tag_registry.h:TagRegistry', 'class', 'TagRegistry', 'engine/ecs/tag_registry.h',
    'Singleton catalog of built-in and custom GameObject tags with JSON load/save.',
    ['singleton', 'data-model', 'registry'], 'moderate', { lineRange: [9, 38] }),
  n('class:engine/scene/scene_runtime.h:SceneRuntime', 'class', 'SceneRuntime', 'engine/scene/scene_runtime.h',
    'Singleton that registers editor scene trees with SceneBase and rebuilds debug colliders.',
    ['singleton', 'scene', 'physics'], 'moderate', { lineRange: [9, 29] }),
  n('class:engine/editor/editor_undo.cpp:TransformCommand', 'class', 'TransformCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that reapplies before/after SceneTransform JSON onto a NodeRef.',
    ['command', 'editor', 'undo'], 'simple', { lineRange: [207, 237] }),
  n('class:engine/editor/editor_undo.cpp:ComponentEditCommand', 'class', 'ComponentEditCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that deserializes a component snapshot and runs post-deserialize apply hooks.',
    ['command', 'editor', 'undo'], 'moderate', { lineRange: [239, 270] }),
  n('class:engine/editor/editor_undo.cpp:CreateObjectCommand', 'class', 'CreateObjectCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that inserts or removes a prefab-serialized GameObject subtree.',
    ['command', 'editor', 'undo'], 'moderate', { lineRange: [272, 326] }),
  n('class:engine/editor/editor_undo.cpp:DeleteObjectCommand', 'class', 'DeleteObjectCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that restores a deleted subtree from JSON or deletes it again.',
    ['command', 'editor', 'undo'], 'moderate', { lineRange: [328, 382] }),
  n('class:engine/editor/editor_undo.cpp:RenameCommand', 'class', 'RenameCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that swaps a GameObject display name.',
    ['command', 'editor', 'undo'], 'simple', { lineRange: [384, 401] }),
  n('class:engine/editor/editor_undo.cpp:TagCommand', 'class', 'TagCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that restores a GameObject tag id.',
    ['command', 'editor', 'undo'], 'simple', { lineRange: [403, 422] }),
  n('class:engine/editor/editor_undo.cpp:ReparentCommand', 'class', 'ReparentCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that moves a node between parents and sibling indices.',
    ['command', 'editor', 'undo'], 'moderate', { lineRange: [424, 452] }),
  n('class:engine/editor/editor_undo.cpp:AddComponentCommand', 'class', 'AddComponentCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that creates a component from JSON or removes it by type name.',
    ['command', 'editor', 'undo'], 'simple', { lineRange: [482, 503] }),
  n('class:engine/editor/editor_undo.cpp:RemoveComponentCommand', 'class', 'RemoveComponentCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that removes a component and can restore it from serialized JSON.',
    ['command', 'editor', 'undo'], 'simple', { lineRange: [505, 526] }),
  n('class:engine/editor/editor_undo.cpp:CreateUiCommand', 'class', 'CreateUiCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that creates or deletes a canvas UI element from a snapshot.',
    ['command', 'editor', 'ui'], 'simple', { lineRange: [531, 558] }),
  n('class:engine/editor/editor_undo.cpp:DeleteUiCommand', 'class', 'DeleteUiCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that restores or removes a deleted UI element.',
    ['command', 'editor', 'ui'], 'simple', { lineRange: [560, 587] }),
  n('class:engine/editor/editor_undo.cpp:UiEditCommand', 'class', 'UiEditCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that reapplies before/after JSON onto a UI element.',
    ['command', 'editor', 'ui'], 'simple', { lineRange: [589, 615] }),
  n('class:engine/editor/editor_undo.cpp:UiReorderCommand', 'class', 'UiReorderCommand', 'engine/editor/editor_undo.cpp',
    'Undo command that restores a UI element sibling order.',
    ['command', 'editor', 'ui'], 'simple', { lineRange: [617, 637] })
);

// ── Function nodes ──────────────────────────────────────────
const fn = (path, name, start, end, summary, tags, complexity) =>
  n(`function:${path}:${name}`, 'function', name, path, summary, tags, complexity, { lineRange: [start, end] });

nodes.push(
  fn('engine/ecs/directional_light_component.cpp', 'Serialize', 10, 19,
    'Writes enabled, color, intensity, specular, and castShadows fields to JSON.',
    ['serialization', 'lighting'], 'simple'),
  fn('engine/ecs/directional_light_component.cpp', 'Deserialize', 21, 42,
    'Parses directional light JSON with fallbacks and logs parse errors.',
    ['serialization', 'lighting'], 'simple'),
  fn('engine/ecs/material_component.cpp', 'Serialize', 105, 123,
    'Dumps PBR/toon material fields including optional texture paths to JSON.',
    ['serialization', 'rendering'], 'simple'),
  fn('engine/ecs/material_component.cpp', 'Deserialize', 125, 149,
    'Reads material JSON, clamps unit intervals, and releases stale texture SRVs.',
    ['serialization', 'rendering'], 'simple'),
  fn('engine/ecs/point_light_component.cpp', 'Serialize', 5, 15,
    'Writes point-light color, intensity, range, attenuation, and specular to JSON.',
    ['serialization', 'lighting'], 'simple'),
  fn('engine/ecs/point_light_component.cpp', 'Deserialize', 17, 43,
    'Restores PointLightComponent fields from JSON with defaults on parse failure.',
    ['serialization', 'lighting'], 'simple'),
  fn('engine/ecs/prefab_serializer.cpp', 'SerializeNode', 24, 51,
    'Recursively encodes a GameObject name, tag, components, and children as JSON.',
    ['serialization', 'prefab'], 'moderate'),
  fn('engine/ecs/prefab_serializer.cpp', 'DeserializeNode', 53, 91,
    'Rebuilds a GameObject tree, creating components through ComponentFactory and applying tags.',
    ['serialization', 'prefab', 'factory'], 'moderate'),
  fn('engine/ecs/prefab_serializer.cpp', 'SaveToFile', 116, 130,
    'Writes a version-4 prefab wrapper with a single root node to disk.',
    ['serialization', 'prefab'], 'simple'),
  fn('engine/ecs/prefab_serializer.cpp', 'Deserialize', 132, 151,
    'Parses prefab JSON (nodes array, prefab object, or bare node) into a GameObject.',
    ['serialization', 'prefab'], 'simple'),
  fn('engine/ecs/prefab_serializer.cpp', 'LoadFromFile', 153, 164,
    'Reads a prefab file and deserializes its root GameObject.',
    ['serialization', 'prefab'], 'simple'),
  fn('engine/ecs/rigidbody_component.cpp', 'Serialize', 23, 32,
    'Persists gravity, kinematic, mass, drag, restitution, and friction settings.',
    ['serialization', 'physics'], 'simple'),
  fn('engine/ecs/rigidbody_component.cpp', 'Deserialize', 34, 48,
    'Restores Rigidbody fields from JSON and leaves ApplyToOwner to the caller.',
    ['serialization', 'physics'], 'simple'),
  fn('engine/ecs/spot_light_component.cpp', 'Serialize', 6, 19,
    'Writes spot-light color, cone angles, range, attenuation, and shadow flag.',
    ['serialization', 'lighting'], 'simple'),
  fn('engine/ecs/spot_light_component.cpp', 'Deserialize', 21, 50,
    'Parses SpotLight JSON including inner/outer cone degrees and attenuation.',
    ['serialization', 'lighting'], 'simple'),
  fn('engine/ecs/tag_registry.cpp', 'FindByName', 76, 85,
    'Looks up a tag id by case-insensitive trimmed name.',
    ['utility', 'registry'], 'simple'),
  fn('engine/ecs/tag_registry.cpp', 'AddTag', 87, 96,
    'Appends a unique custom tag and returns its new id, or -1 on failure.',
    ['registry', 'mutation'], 'simple'),
  fn('engine/ecs/tag_registry.cpp', 'LoadFromFile', 113, 147,
    'Reloads built-in names and appends custom tags from rom/tags.json.',
    ['serialization', 'registry'], 'moderate'),
  fn('engine/ecs/tag_registry.cpp', 'SaveToFile', 149, 158,
    'Writes the current tag name list as a JSON tags array.',
    ['serialization', 'registry'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'GetSiblingIndex', 88, 104,
    'Returns a node\'s index among its parent children or among scene roots.',
    ['utility', 'editor', 'hierarchy'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'InsertNodeAt', 106, 118,
    'Inserts a GameObject at a sibling index under a parent or as a scene root.',
    ['editor', 'hierarchy'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'DetachNode', 120, 140,
    'Removes a node from its parent or the scene-root list without destroying it.',
    ['editor', 'hierarchy'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'ApplyComponentPostDeserialize', 142, 164,
    'Reapplies collider, audio, effect, and billboard side effects after an undo deserialize.',
    ['editor', 'component', 'undo'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'ParseNodeNameFromJson', 166, 179,
    'Extracts a GameObject name from a prefab or scene-node JSON blob.',
    ['serialization', 'editor'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'RemoveComponentByTypeName', 454, 470,
    'Removes a typed component from a node using the editor type-name switch.',
    ['editor', 'component'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'EditorUndo_PushCreate', 818, 827,
    'Records a create-object undo command from a live node, parent, and insert index.',
    ['editor', 'undo', 'api'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'EditorUndo_EndComponentEdit', 849, 915,
    'Commits or opens an ImGui component-edit session when widgets deactivate after edits.',
    ['editor', 'undo', 'api'], 'complex'),
  fn('engine/editor/editor_undo.cpp', 'EditorUndo_PushAddComponent', 945, 957,
    'Pushes an add-component command after capturing the new component JSON.',
    ['editor', 'undo', 'api'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'EditorUndo_PushRemoveComponent', 959, 972,
    'Pushes a remove-component command after snapshotting the component JSON.',
    ['editor', 'undo', 'api'], 'simple'),
  fn('engine/editor/editor_undo.cpp', 'EditorUndo_EndUiEdit', 1013, 1056,
    'Commits an ImGui UI-element edit session into a UiEditCommand when values change.',
    ['editor', 'undo', 'ui'], 'moderate'),
  fn('engine/editor/light_gizmo.cpp', 'VisitDraw', 28, 53,
    'Recursively draws gizmos for enabled directional, point, and spot lights on a node.',
    ['lighting', 'debug', 'editor'], 'simple'),
  fn('engine/editor/light_gizmo.cpp', 'GetWorldPosition', 59, 65,
    'Returns a Transform world position used as point/spot light origin.',
    ['lighting', 'utility'], 'simple'),
  fn('engine/editor/light_gizmo.cpp', 'GetShineDirectionWS', 67, 79,
    'Computes the world-space outgoing shine direction from a light Transform.',
    ['lighting', 'utility'], 'simple'),
  fn('engine/editor/light_gizmo.cpp', 'DrawDirectional', 86, 135,
    'Draws a directional-light arrow gizmo using a DebugDrawer.',
    ['lighting', 'debug'], 'moderate'),
  fn('engine/editor/light_gizmo.cpp', 'DrawPoint', 137, 165,
    'Draws a point-light range sphere gizmo.',
    ['lighting', 'debug'], 'simple'),
  fn('engine/editor/light_gizmo.cpp', 'DrawSpot', 167, 219,
    'Draws a spot-light cone gizmo from inner/outer angles and range.',
    ['lighting', 'debug'], 'moderate'),
  fn('engine/scene/scene_lighting.cpp', 'VisitLights', 24, 39,
    'Collects enabled directional, point, and spot light components from a node tree.',
    ['lighting', 'scene'], 'simple'),
  fn('engine/scene/scene_lighting.cpp', 'ApplyFallbackLighting', 41, 52,
    'Installs a default directional light and ambient color when the scene has no lights.',
    ['lighting', 'scene'], 'simple'),
  fn('engine/scene/scene_lighting.cpp', 'SceneLightingSyncFromNodes', 64, 135,
    'Pushes collected scene lights into Lighting/Shadow, mapping poses through LightGizmo.',
    ['lighting', 'scene', 'service'], 'complex'),
  fn('engine/scene/scene_loader.cpp', 'ApplySceneSettings', 99, 143,
    'Applies render settings and debug-camera pose/mode from a scene JSON settings object.',
    ['scene', 'configuration'], 'moderate'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_RegisterComponentTypes', 159, 220,
    'Registers all built-in ECS component creators with ComponentFactory exactly once.',
    ['factory', 'component', 'entry-point'], 'moderate'),
  fn('engine/scene/scene_loader.cpp', 'RestoreEditorDebugColliderFlags', 236, 246,
    'Restores showDebugCollider flags after a play session tears down physics shapes.',
    ['scene', 'physics', 'editor'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_RefreshSceneAssets', 275, 315,
    'Scans the scene directory and rebuilds the in-memory scene-asset catalog.',
    ['scene', 'assets'], 'moderate'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_LoadDefaultSceneSetting', 317, 336,
    'Reads default_scene.txt to remember which scene file should load at startup.',
    ['scene', 'configuration'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_LoadScriptDlls', 348, 367,
    'Loads gameplay script DLLs from the runtime bin directory via ScriptFactory.',
    ['scripting', 'scene'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_ScanScriptDllsForChanges', 374, 400,
    'Detects script DLL changes and queues hot reloads through ScriptFactory.',
    ['scripting', 'hot-reload'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_Initialize', 408, 424,
    'Registers components, loads tags, starts the script watcher, and prepares scene assets.',
    ['entry-point', 'scene'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'CreateModelGameObject', 437, 446,
    'Builds a GameObject with Transform and VisualComponent for a legacy model block.',
    ['factory', 'scene'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'GameObjectFromLegacyBlock', 448, 491,
    'Converts a version-3 scene block into a GameObject with transform, visual, and collider.',
    ['serialization', 'scene', 'migration'], 'moderate'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_PrepareForSceneReload', 497, 509,
    'Flushes prefabs, tears down scripts, and clears SceneRuntime before loading a new scene.',
    ['scene', 'lifecycle'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_LoadFromFile', 511, 594,
    'Loads a scene JSON file: prefab instances, inline nodes, canvas, settings, then syncs runtime.',
    ['serialization', 'scene', 'entry-point'], 'complex'),
  fn('engine/scene/scene_loader.cpp', 'SceneLoader_SaveToFile', 600, 619,
    'Writes version-4 scene JSON with settings, non-spawn nodes, and canvas state.',
    ['serialization', 'scene'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'TeardownAllScripts', 628, 639,
    'Walks scene nodes and calls ScriptComponent::Teardown on each script.',
    ['scripting', 'lifecycle'], 'simple'),
  fn('engine/scene/scene_loader.cpp', 'TeardownScenePlaySession', 641, 659,
    'Stops play-only objects, scripts, and runtime registration when leaving play mode.',
    ['scene', 'lifecycle'], 'simple'),
  fn('engine/scene/scene_runtime.cpp', 'Clear', 12, 23,
    'Unregisters all tracked objects from SceneBase and optionally destroys colliders.',
    ['scene', 'physics'], 'simple'),
  fn('engine/scene/scene_runtime.cpp', 'ApplyCollidersTree', 25, 35,
    'Recursively syncs transforms and applies ColliderComponent shapes for debug draw.',
    ['scene', 'physics'], 'simple'),
  fn('engine/scene/scene_runtime.cpp', 'RegisterTree', 44, 59,
    'Applies colliders and adds objects that have physics shapes to the active scene.',
    ['scene', 'physics'], 'simple'),
  fn('engine/scene/scene_runtime.cpp', 'UnregisterTree', 70, 85,
    'Removes a subtree from SceneBase and optionally clears collider shapes.',
    ['scene', 'physics'], 'simple')
);

// ── contains + exports for classes/functions ───────────────
const exportedClasses = new Set([
  'class:engine/ecs/audio_component.h:AudioComponent',
  'class:engine/ecs/audio_listener_component.h:AudioListenerComponent',
  'class:engine/ecs/billboard_component.h:BillboardComponent',
  'class:engine/ecs/collider_component.h:ColliderComponent',
  'class:engine/ecs/component.h:Component',
  'class:engine/ecs/component_factory.h:ComponentFactory',
  'class:engine/ecs/directional_light_component.h:DirectionalLightComponent',
  'class:engine/ecs/effect_component.h:EffectComponent',
  'class:engine/ecs/material_component.h:MaterialComponent',
  'class:engine/ecs/point_light_component.h:PointLightComponent',
  'class:engine/ecs/rigidbody_component.h:RigidbodyComponent',
  'class:engine/ecs/spot_light_component.h:SpotLightComponent',
  'class:engine/ecs/tag_registry.h:TagRegistry',
  'class:engine/scene/scene_runtime.h:SceneRuntime'
]);

const exportedFunctions = new Set([
  'function:engine/ecs/prefab_serializer.cpp:SaveToFile',
  'function:engine/ecs/prefab_serializer.cpp:Deserialize',
  'function:engine/ecs/prefab_serializer.cpp:LoadFromFile',
  'function:engine/editor/light_gizmo.cpp:GetWorldPosition',
  'function:engine/editor/light_gizmo.cpp:GetShineDirectionWS',
  'function:engine/editor/light_gizmo.cpp:DrawDirectional',
  'function:engine/editor/light_gizmo.cpp:DrawPoint',
  'function:engine/editor/light_gizmo.cpp:DrawSpot',
  'function:engine/scene/scene_lighting.cpp:SceneLightingSyncFromNodes',
  'function:engine/scene/scene_loader.cpp:SceneLoader_RegisterComponentTypes',
  'function:engine/scene/scene_loader.cpp:SceneLoader_RefreshSceneAssets',
  'function:engine/scene/scene_loader.cpp:SceneLoader_LoadDefaultSceneSetting',
  'function:engine/scene/scene_loader.cpp:SceneLoader_LoadScriptDlls',
  'function:engine/scene/scene_loader.cpp:SceneLoader_ScanScriptDllsForChanges',
  'function:engine/scene/scene_loader.cpp:SceneLoader_Initialize',
  'function:engine/scene/scene_loader.cpp:GameObjectFromLegacyBlock',
  'function:engine/scene/scene_loader.cpp:SceneLoader_LoadFromFile',
  'function:engine/scene/scene_loader.cpp:SceneLoader_SaveToFile',
  'function:engine/scene/scene_loader.cpp:TeardownAllScripts',
  'function:engine/scene/scene_loader.cpp:TeardownScenePlaySession',
  'function:engine/editor/editor_undo.cpp:EditorUndo_PushCreate',
  'function:engine/editor/editor_undo.cpp:EditorUndo_EndComponentEdit',
  'function:engine/editor/editor_undo.cpp:EditorUndo_PushAddComponent',
  'function:engine/editor/editor_undo.cpp:EditorUndo_PushRemoveComponent',
  'function:engine/editor/editor_undo.cpp:EditorUndo_EndUiEdit'
]);

for (const node of nodes) {
  if (node.type === 'function' || node.type === 'class') {
    edges.push(e(`file:${node.filePath}`, node.id, 'contains', 1.0));
    if (exportedClasses.has(node.id) || exportedFunctions.has(node.id)) {
      edges.push(e(`file:${node.filePath}`, node.id, 'exports', 0.8));
    }
  }
}

// ── imports from batchImportData ───────────────────────────
const importData = JSON.parse(fs.readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-input-2.json', 'utf8')).batchImportData;
let importCount = 0;
for (const [from, targets] of Object.entries(importData)) {
  for (const to of targets) {
    edges.push(e(`file:${from}`, `file:${to}`, 'imports', 0.7));
    importCount++;
  }
}

// ── inherits ───────────────────────────────────────────────
const componentInheritSamePart = [
  'class:engine/ecs/audio_component.h:AudioComponent',
  'class:engine/ecs/audio_listener_component.h:AudioListenerComponent',
  'class:engine/ecs/billboard_component.h:BillboardComponent',
  'class:engine/ecs/collider_component.h:ColliderComponent'
];
for (const id of componentInheritSamePart) {
  edges.push(e(id, 'class:engine/ecs/component.h:Component', 'inherits', 0.9));
}
const componentInheritCrossPart = [
  'class:engine/ecs/directional_light_component.h:DirectionalLightComponent',
  'class:engine/ecs/effect_component.h:EffectComponent',
  'class:engine/ecs/material_component.h:MaterialComponent',
  'class:engine/ecs/point_light_component.h:PointLightComponent',
  'class:engine/ecs/rigidbody_component.h:RigidbodyComponent',
  'class:engine/ecs/spot_light_component.h:SpotLightComponent'
];
for (const id of componentInheritCrossPart) {
  edges.push(e(id, 'file:engine/ecs/component.h', 'inherits', 0.9));
}

const undoCommands = [
  'TransformCommand', 'ComponentEditCommand', 'CreateObjectCommand', 'DeleteObjectCommand',
  'RenameCommand', 'TagCommand', 'ReparentCommand', 'AddComponentCommand', 'RemoveComponentCommand',
  'CreateUiCommand', 'DeleteUiCommand', 'UiEditCommand', 'UiReorderCommand'
];
for (const name of undoCommands) {
  edges.push(e(`class:engine/editor/editor_undo.cpp:${name}`, 'class:engine/editor/editor_undo.h:EditorCommand', 'inherits', 0.9));
}

// ── calls (cross-file, validation-safe targets) ────────────
edges.push(
  e('file:engine/ecs/directional_light_component.cpp', 'file:engine/editor/light_gizmo.h', 'calls', 0.8),
  e('function:engine/ecs/prefab_serializer.cpp:DeserializeNode', 'file:engine/ecs/component_factory.h', 'calls', 0.8),
  e('function:engine/ecs/prefab_serializer.cpp:DeserializeNode', 'file:engine/ecs/tag_registry.h', 'calls', 0.8),
  e('function:engine/ecs/prefab_serializer.cpp:SerializeNode', 'file:engine/ecs/component.h', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_RegisterComponentTypes', 'file:engine/ecs/component_factory.h', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_LoadFromFile', 'function:engine/ecs/prefab_serializer.h:LoadFromFile', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_LoadFromFile', 'function:engine/ecs/prefab_serializer.h:Deserialize', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_LoadFromFile', 'file:engine/ecs/tag_registry.h', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_LoadFromFile', 'file:engine/scene/scene_runtime.h', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_SaveToFile', 'function:engine/ecs/prefab_serializer.h:Serialize', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_PrepareForSceneReload', 'function:engine/scene/scene_runtime.cpp:Clear', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_LoadScriptDlls', 'function:engine/scripting/script_factory.h:LoadScriptDll', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_ScanScriptDllsForChanges', 'class:engine/scripting/script_factory.h:ScriptFactory', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:TeardownAllScripts', 'function:engine/scripting/script_component.h:Teardown', 'calls', 0.8),
  e('function:engine/scene/scene_loader.cpp:SceneLoader_Initialize', 'file:engine/ecs/tag_registry.h', 'calls', 0.8),
  e('function:engine/scene/scene_runtime.cpp:Clear', 'file:engine/ecs/collider_component.h', 'calls', 0.8),
  e('function:engine/scene/scene_runtime.cpp:ApplyCollidersTree', 'file:engine/ecs/collider_component.h', 'calls', 0.8),
  e('function:engine/scene/scene_runtime.cpp:RegisterTree', 'file:engine/ecs/collider_component.h', 'calls', 0.8),
  e('function:engine/scene/scene_runtime.cpp:UnregisterTree', 'file:engine/ecs/collider_component.h', 'calls', 0.8),
  e('function:engine/editor/light_gizmo.cpp:VisitDraw', 'file:engine/ecs/directional_light_component.h', 'calls', 0.8),
  e('function:engine/editor/light_gizmo.cpp:VisitDraw', 'file:engine/ecs/point_light_component.h', 'calls', 0.8),
  e('function:engine/editor/light_gizmo.cpp:VisitDraw', 'file:engine/ecs/spot_light_component.h', 'calls', 0.8),
  e('function:engine/scene/scene_lighting.cpp:VisitLights', 'file:engine/ecs/directional_light_component.h', 'calls', 0.8),
  e('function:engine/scene/scene_lighting.cpp:VisitLights', 'file:engine/ecs/point_light_component.h', 'calls', 0.8),
  e('function:engine/scene/scene_lighting.cpp:VisitLights', 'file:engine/ecs/spot_light_component.h', 'calls', 0.8),
  e('function:engine/scene/scene_lighting.cpp:SceneLightingSyncFromNodes', 'file:engine/editor/light_gizmo.h', 'calls', 0.8),
  e('function:engine/editor/editor_undo.cpp:ApplyComponentPostDeserialize', 'file:engine/ecs/collider_component.h', 'calls', 0.8),
  e('function:engine/editor/editor_undo.cpp:ApplyComponentPostDeserialize', 'file:engine/ecs/audio_component.h', 'calls', 0.8),
  e('function:engine/editor/editor_undo.cpp:ApplyComponentPostDeserialize', 'file:engine/ecs/effect_component.h', 'calls', 0.8),
  e('function:engine/editor/editor_undo.cpp:ApplyComponentPostDeserialize', 'file:engine/ecs/billboard_component.h', 'calls', 0.8),
  e('class:engine/editor/editor_undo.cpp:CreateObjectCommand', 'function:engine/ecs/prefab_serializer.h:Deserialize', 'calls', 0.8),
  e('class:engine/editor/editor_undo.cpp:DeleteObjectCommand', 'function:engine/ecs/prefab_serializer.h:Deserialize', 'calls', 0.8),
  e('class:engine/editor/editor_undo.cpp:AddComponentCommand', 'file:engine/ecs/component_factory.h', 'calls', 0.8),
  e('function:engine/editor/editor_undo.cpp:RemoveComponentByTypeName', 'class:engine/ecs/transform.h:Transform', 'calls', 0.8)
);

// Self-check: no self-edges, no dup node ids
const ids = new Set();
for (const node of nodes) {
  if (ids.has(node.id)) throw new Error('duplicate node ' + node.id);
  ids.add(node.id);
}
for (const edge of edges) {
  if (edge.source === edge.target) throw new Error('self edge ' + edge.source);
}

const batchFiles = JSON.parse(fs.readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-input-2.json', 'utf8')).batchFiles;
const filePaths = batchFiles.map(f => f.path).sort();
const expectedImports = Object.values(importData).reduce((s, a) => s + a.length, 0);
const actualImports = edges.filter(x => x.type === 'imports').length;
if (actualImports !== expectedImports) {
  throw new Error(`imports ${actualImports} != expected ${expectedImports}`);
}

const nodeCount = nodes.length;
const edgeCount = edges.length;
console.log('totals', { nodeCount, edgeCount, importCount: actualImports, files: filePaths.length });

let parts = Math.max(1, Math.ceil(Math.max(nodeCount / 60, edgeCount / 120)));
function countPartNodes(p, chunkSize) {
  const max = [];
  for (let i = 0; i < p; i++) {
    const partFiles = filePaths.slice(i * chunkSize, (i + 1) * chunkSize);
    const partFileSet = new Set(partFiles);
    max.push(nodes.filter(nd => partFileSet.has(nd.filePath)).length);
  }
  return max;
}
while (true) {
  const chunkSize = Math.ceil(filePaths.length / parts);
  const counts = countPartNodes(parts, chunkSize);
  const edgeCounts = [];
  for (let i = 0; i < parts; i++) {
    const partFiles = filePaths.slice(i * chunkSize, (i + 1) * chunkSize);
    const partFileSet = new Set(partFiles);
    const partNodeIds = new Set(nodes.filter(nd => partFileSet.has(nd.filePath)).map(nd => nd.id));
    edgeCounts.push(edges.filter(ed => partNodeIds.has(ed.source)).length);
  }
  if (counts.every(c => c <= 60) && edgeCounts.every(c => c <= 120)) break;
  parts += 1;
  if (parts > filePaths.length) throw new Error('cannot split within limits');
}
const chunk = Math.ceil(filePaths.length / parts);
console.log('parts', parts, 'chunk', chunk);

const batchesJson = JSON.parse(fs.readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/intermediate/batches.json', 'utf8'));
const batch2 = batchesJson.batches.find(b => b.batchIndex === 2);
const neighborMap = batch2.neighborMap || {};
const neighborSymbols = new Map();
for (const [src, neighbors] of Object.entries(neighborMap)) {
  for (const nb of neighbors) {
    neighborSymbols.set(nb.path, new Set(nb.symbols || []));
  }
}
const importPaths = new Set();
for (const arr of Object.values(importData)) for (const p of arr) importPaths.add(p);
for (const p of Object.keys(importData)) importPaths.add(p);
for (const p of Object.keys(neighborMap)) importPaths.add(p);
for (const neighbors of Object.values(neighborMap)) for (const nb of neighbors) importPaths.add(nb.path);

function filePathOfId(id) {
  const m = id.match(/^(?:file|config|document|service|pipeline|schema|resource):(.+)$/);
  if (m) return m[1];
  const m2 = id.match(/^(?:function|class|table|endpoint):(.+):[^:]+$/);
  if (m2) return m2[1];
  return null;
}
function symbolOfId(id) {
  const m = id.match(/^(?:function|class):.+:([^:]+)$/);
  return m ? m[1] : null;
}
function isValidTarget(id, partNodeIds) {
  if (partNodeIds.has(id)) return true;
  if (id.startsWith('file:')) {
    const p = id.slice(5);
    return importPaths.has(p);
  }
  if (id.startsWith('function:') || id.startsWith('class:')) {
    const p = filePathOfId(id);
    const sym = symbolOfId(id);
    if (partNodeIds.has(id)) return true;
    const syms = neighborSymbols.get(p);
    if (syms && syms.has(sym)) return true;
    return false;
  }
  return false;
}

const outDir = 'C:/Users/terry/source/repos/ivy-engine/.ua/intermediate';
const written = [];
for (let i = 0; i < parts; i++) {
  const partFiles = filePaths.slice(i * chunk, (i + 1) * chunk);
  const partFileSet = new Set(partFiles);
  const partNodes = nodes.filter(nd => partFileSet.has(nd.filePath) || (nd.type === 'file' && partFileSet.has(nd.filePath)));
  const partNodeIds = new Set(partNodes.map(nd => nd.id));
  const partEdges = edges.filter(ed => partNodeIds.has(ed.source));
  const invalid = [];
  for (const ed of partEdges) {
    const srcOk = partNodeIds.has(ed.source) || isValidTarget(ed.source, partNodeIds);
    const tgtOk = isValidTarget(ed.target, partNodeIds);
    if (!srcOk || !tgtOk) invalid.push({ edge: ed, srcOk, tgtOk });
  }
  if (invalid.length) {
    console.error('INVALID part', i + 1, invalid.slice(0, 8));
    throw new Error(`part ${i + 1} failed validation (${invalid.length} edges)`);
  }
  const fname = parts === 1 ? 'batch-2.json' : `batch-2-part-${i + 1}.json`;
  const outPath = `${outDir}/${fname}`;
  fs.writeFileSync(outPath, JSON.stringify({ nodes: partNodes, edges: partEdges }, null, 2));
  written.push({ fname, nodes: partNodes.length, edges: partEdges.length, files: partFiles });
  console.log(fname, 'nodes', partNodes.length, 'edges', partEdges.length, 'files', partFiles.length);
}

console.log(JSON.stringify(written.map(w => ({ fname: w.fname, nodes: w.nodes, edges: w.edges, files: w.files })), null, 2));
