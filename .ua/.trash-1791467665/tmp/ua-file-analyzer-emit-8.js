const fs = require("fs");
const path = require("path");

function n(id, type, name, filePath, summary, tags, complexity, extra) {
  const o = { id, type, name, summary, tags, complexity };
  if (filePath) o.filePath = filePath;
  if (extra) Object.assign(o, extra);
  return o;
}
function e(source, target, type, weight) {
  return { source, target, type, direction: "forward", weight };
}
function contains(file, child) {
  return e(file, child, "contains", 1.0);
}
function exportsE(file, child) {
  return e(file, child, "exports", 0.8);
}
function importsE(from, to) {
  return e("file:" + from, "file:" + to, "imports", 0.7);
}
function callsE(from, to) {
  return e(from, to, "calls", 0.8);
}

const nodes = [];
const edges = [];

nodes.push(
  n(
    "file:engine/editor/editor_prefab_panel.h",
    "file",
    "editor_prefab_panel.h",
    "engine/editor/editor_prefab_panel.h",
    "Declares MapEditor::PrefabPanel, the editor UI entry that lists prefab files and returns a spawned GameObject when the user instantiates one.",
    ["editor", "prefab", "ui-panel", "type-definition"],
    "simple"
  )
);
nodes.push(
  n(
    "file:engine/editor/editor_project_browser.cpp",
    "file",
    "editor_project_browser.cpp",
    "engine/editor/editor_project_browser.cpp",
    "Implements the ImGui project browser: folder tree, asset grid, drag-and-drop, and modal flows for rename, delete, new folder, and new scene.",
    ["editor", "asset-browser", "ui-panel", "event-handler"],
    "complex"
  )
);
nodes.push(
  n(
    "file:engine/editor/transform_gizmo.cpp",
    "file",
    "transform_gizmo.cpp",
    "engine/editor/transform_gizmo.cpp",
    "ImGuizmo-based translate/rotate/scale gizmo that projects the editor camera, applies world or local TRS to selected GameObjects, and records undo snapshots.",
    ["editor", "gizmo", "transform", "event-handler"],
    "complex",
    {
      languageNotes:
        "Builds ImGuizmo view/projection from the global Camera and converts between column-major ImGuizmo matrices and DirectXMath XMMATRIX.",
    }
  )
);
nodes.push(
  n(
    "file:engine/input/mouse.h",
    "file",
    "mouse.h",
    "engine/input/mouse.h",
    "C API for per-frame mouse delta, button state, wheel, and both back-buffer and HWND client coordinates used by gizmos and cameras.",
    ["input", "type-definition", "api-handler"],
    "simple"
  )
);
nodes.push(
  n(
    "file:engine/scene/scene_camera.cpp",
    "file",
    "scene_camera.cpp",
    "engine/scene/scene_camera.cpp",
    "Copies the active named CameraComponent node into the global Camera when editor CameraMode is SceneCamera.",
    ["camera", "scene", "synchronization", "service"],
    "simple"
  )
);
nodes.push(
  n(
    "file:engine/scene/scene_camera.h",
    "file",
    "scene_camera.h",
    "engine/scene/scene_camera.h",
    "Declares SceneCameraSyncFromActiveNode, the scene-camera producer that writes a CameraComponent pose into the global Camera.",
    ["camera", "scene", "type-definition"],
    "simple"
  )
);
nodes.push(
  n(
    "file:engine/scene/scene_loader.h",
    "file",
    "scene_loader.h",
    "engine/scene/scene_loader.h",
    "Public scene-load API: initialize/finalize, JSON load/save, script-DLL hot reload, play-session teardown, and editor scene-asset browser helpers.",
    ["scene", "serialization", "type-definition", "entry-point"],
    "moderate"
  )
);
nodes.push(
  n(
    "file:src/app/camera/camera.cpp",
    "file",
    "camera.cpp",
    "src/app/camera/camera.cpp",
    "Defines the global Camera singleton and LookAt/perspective matrix helpers used by rendering and every camera controller.",
    ["camera", "singleton", "data-model"],
    "moderate",
    {
      languageNotes:
        "Stores one process-wide Camera in a translation-unit static and exposes it through GetCamera/SetCamera.",
    }
  )
);
nodes.push(
  n(
    "file:src/app/camera/camera.h",
    "file",
    "camera.h",
    "src/app/camera/camera.h",
    "Look-at camera value type with position, target, up, FOV, and clip planes plus accessors for the process-wide Camera instance.",
    ["camera", "data-model", "type-definition"],
    "simple"
  )
);
nodes.push(
  n(
    "file:src/app/camera/camera_director.cpp",
    "file",
    "camera_director.cpp",
    "src/app/camera/camera_director.cpp",
    "Sole writer of the global Camera: samples Follow, first-person, scene-node, or intro poses and eases between them unless the debug camera is enabled.",
    ["camera", "service", "director", "blending"],
    "moderate"
  )
);
nodes.push(
  n(
    "file:src/app/camera/camera_director.h",
    "file",
    "camera_director.h",
    "src/app/camera/camera_director.h",
    "Declares CameraDirectorSource and the director API that blends Follow, SceneNode, Intro, and FirstPerson producers into the global Camera.",
    ["camera", "type-definition", "director"],
    "simple"
  )
);
nodes.push(
  n(
    "file:src/app/camera/camera_pose.cpp",
    "file",
    "camera_pose.cpp",
    "src/app/camera/camera_pose.cpp",
    "Converts between global Camera, CameraComponent/Transform nodes, and CameraPose, including spherical look-direction blends.",
    ["camera", "serialization", "utility", "math"],
    "moderate",
    {
      languageNotes:
        "Slerps look direction via an orthonormal axis when vectors are nearly opposite, then rebuilds a local basis for Transform write-back.",
    }
  )
);
nodes.push(
  n(
    "file:src/app/camera/camera_pose.h",
    "file",
    "camera_pose.h",
    "src/app/camera/camera_pose.h",
    "Shared CameraPose struct (eye, lookAt, up, FOV, clip) and conversion helpers used by the director and gameplay cameras.",
    ["camera", "data-model", "type-definition"],
    "simple"
  )
);
nodes.push(
  n(
    "file:src/app/camera/debug_camera.cpp",
    "file",
    "debug_camera.cpp",
    "src/app/camera/debug_camera.cpp",
    "Editor debug camera with Maya-style Alt+mouse orbit/pan/dolly and Unity-style RMB+WASD fly, toggled with F1.",
    ["camera", "editor", "debug", "event-handler"],
    "complex"
  )
);
nodes.push(
  n(
    "file:src/app/camera/debug_camera.h",
    "file",
    "debug_camera.h",
    "src/app/camera/debug_camera.h",
    "Public debug-camera API: enable/toggle, Orbit vs Fly mode, sensitivity knobs, focal point, and fly-view seeding.",
    ["camera", "editor", "type-definition"],
    "simple"
  )
);
nodes.push(
  n(
    "file:src/app/camera/first_person_camera.cpp",
    "file",
    "first_person_camera.cpp",
    "src/app/camera/first_person_camera.cpp",
    "First-person eye camera that follows live player position/yaw pointers, recenters the cursor, and writes pose onto a hidden CameraComponent node.",
    ["camera", "gameplay", "event-handler", "service"],
    "moderate"
  )
);
nodes.push(
  n(
    "file:src/app/camera/first_person_camera.h",
    "file",
    "first_person_camera.h",
    "src/app/camera/first_person_camera.h",
    "First-person camera API mirroring the third-person attach/look seams so the director can sample an FPS pose.",
    ["camera", "type-definition", "gameplay"],
    "simple"
  )
);
nodes.push(
  n(
    "file:src/app/camera/third_person_camera.cpp",
    "file",
    "third_person_camera.cpp",
    "src/app/camera/third_person_camera.cpp",
    "Third-person follow camera with magnet Push/Pull/Orbit, exponential smoothing, trauma shake, and speed-based FOV boost.",
    ["camera", "gameplay", "event-handler", "service"],
    "complex",
    {
      languageNotes:
        "Magnet modes change desired distance/FOV and follow sharpness; trauma applies a decaying offset plus a roll around the view axis.",
    }
  )
);
nodes.push(
  n(
    "file:src/app/camera/third_person_camera.h",
    "file",
    "third_person_camera.h",
    "src/app/camera/third_person_camera.h",
    "Third-person follow-camera API including magnet modes, trauma, speed factor, and access to the hidden follow-cam GameObject.",
    ["camera", "type-definition", "gameplay"],
    "simple"
  )
);

const imports = {
  "engine/editor/editor_prefab_panel.h": [],
  "engine/editor/editor_project_browser.cpp": [
    "engine/core/engine_gui.h",
    "engine/ecs/collider_component.h",
    "engine/ecs/game_object.h",
    "engine/ecs/prefab_serializer.h",
    "engine/ecs/transform.h",
    "engine/ecs/visual_component.h",
    "engine/editor/editor_project_browser.h",
    "engine/editor/project_asset_index.h",
    "engine/editor/project_asset_ops.h",
    "engine/scene/scene_loader.h",
  ],
  "engine/editor/transform_gizmo.cpp": [
    "engine/core/engine_gui.h",
    "engine/ecs/game_object.h",
    "engine/ecs/transform.h",
    "engine/editor/editor_undo.h",
    "engine/editor/transform_gizmo.h",
    "engine/input/mouse.h",
  ],
  "engine/input/mouse.h": [],
  "engine/scene/scene_camera.cpp": [
    "engine/core/engine_gui.h",
    "engine/ecs/camera_component.h",
    "engine/ecs/game_object.h",
    "engine/ecs/transform.h",
    "engine/scene/scene_camera.h",
  ],
  "engine/scene/scene_camera.h": [],
  "engine/scene/scene_loader.h": ["engine/ecs/game_object.h"],
  "src/app/camera/camera.cpp": ["src/app/camera/camera.h"],
  "src/app/camera/camera.h": [],
  "src/app/camera/camera_director.cpp": [
    "engine/core/easing.h",
    "engine/core/engine_gui.h",
    "engine/ecs/camera_component.h",
    "engine/ecs/game_object.h",
    "engine/ecs/transform.h",
    "src/app/camera/camera.h",
    "src/app/camera/camera_director.h",
    "src/app/camera/debug_camera.h",
    "src/app/camera/first_person_camera.h",
    "src/app/camera/third_person_camera.h",
  ],
  "src/app/camera/camera_director.h": [],
  "src/app/camera/camera_pose.cpp": [
    "engine/ecs/camera_component.h",
    "engine/ecs/transform.h",
    "src/app/camera/camera.h",
    "src/app/camera/camera_pose.h",
  ],
  "src/app/camera/camera_pose.h": [],
  "src/app/camera/debug_camera.cpp": [
    "engine/core/engine_gui.h",
    "engine/input/key_logger.h",
    "engine/input/keyboard.h",
    "engine/input/mouse.h",
    "src/app/camera/camera.h",
    "src/app/camera/debug_camera.h",
  ],
  "src/app/camera/debug_camera.h": [],
  "src/app/camera/first_person_camera.cpp": [
    "engine/ecs/audio_listener_component.h",
    "engine/ecs/camera_component.h",
    "engine/ecs/game_object.h",
    "engine/ecs/transform.h",
    "engine/input/controller.h",
    "engine/input/mouse.h",
    "engine/rendering/d3d11/renderer.h",
    "src/app/camera/camera_pose.h",
    "src/app/camera/first_person_camera.h",
    "src/app/camera/third_person_camera.h",
  ],
  "src/app/camera/first_person_camera.h": [],
  "src/app/camera/third_person_camera.cpp": [
    "engine/core/easing.h",
    "engine/ecs/audio_listener_component.h",
    "engine/ecs/camera_component.h",
    "engine/ecs/game_object.h",
    "engine/ecs/transform.h",
    "engine/input/controller.h",
    "engine/input/mouse.h",
    "engine/rendering/d3d11/renderer.h",
    "src/app/camera/camera_pose.h",
    "src/app/camera/third_person_camera.h",
  ],
  "src/app/camera/third_person_camera.h": [],
};

let importCount = 0;
for (const [from, tos] of Object.entries(imports)) {
  for (const to of tos) {
    edges.push(importsE(from, to));
    importCount++;
  }
}

function addFn(file, name, start, end, summary, tags, complexity, exported) {
  const id = "function:" + file + ":" + name;
  const tagList = Array.isArray(tags) ? tags : [tags, "utility", "accessor"];
  nodes.push(
    n(id, "function", name, file, summary, tagList, complexity, {
      lineRange: [start, end],
    })
  );
  edges.push(contains("file:" + file, id));
  if (exported) edges.push(exportsE("file:" + file, id));
  return id;
}

const pb = "engine/editor/editor_project_browser.cpp";
addFn(
  pb,
  "CreateModelGameObject",
  51,
  63,
  "Builds a GameObject with Transform, VisualComponent, and ColliderComponent for a model asset dropped or opened from the browser.",
  ["factory", "prefab", "ecs"],
  "simple",
  false
);
addFn(
  pb,
  "AdjustSelectionAfterRename",
  123,
  137,
  "Rewrites the selected folder path when a renamed folder is an ancestor of the current selection.",
  ["utility", "asset-browser", "path"],
  "simple",
  false
);
addFn(
  pb,
  "AdjustSelectionAfterDelete",
  139,
  149,
  "Moves folder selection to the parent when the deleted path contains the current folder.",
  ["utility", "asset-browser", "path"],
  "simple",
  false
);
addFn(
  pb,
  "FolderTreeLabel",
  151,
  163,
  "Builds an ImGui tree-node label that preserves expansion state via a stable storage id.",
  ["ui-panel", "utility", "imgui"],
  "simple",
  false
);
addFn(
  pb,
  "TryOpenOrUseAsset",
  178,
  201,
  "Opens a scene, or instantiates a model or prefab GameObject into the scene graph from a browser entry.",
  ["factory", "asset-browser", "serialization"],
  "moderate",
  false
);
addFn(
  pb,
  "OpenRenameModal",
  203,
  217,
  "Resets modal state and pre-fills the rename buffer with a folder name or file stem.",
  ["ui-panel", "event-handler", "asset-browser"],
  "simple",
  false
);
addFn(
  pb,
  "OpenDeleteModal",
  219,
  232,
  "Opens the delete confirmation modal and scans scene references when the asset kind requires it.",
  ["ui-panel", "validation", "asset-browser"],
  "simple",
  false
);
addFn(
  pb,
  "DrawFolderContextMenu",
  248,
  271,
  "Folder right-click menu for new scene/folder, reveal in Explorer, rename, and delete (scripts path guarded).",
  ["ui-panel", "event-handler", "asset-browser"],
  "moderate",
  false
);
addFn(
  pb,
  "DrawAssetContextMenu",
  273,
  306,
  "Asset right-click menu for open/use, Explorer, rename, delete, and clipboard path copy.",
  ["ui-panel", "event-handler", "asset-browser"],
  "moderate",
  false
);
addFn(
  pb,
  "DrawFolderTreeNode",
  308,
  357,
  "Recursively draws the project folder tree, updates selection on click, and attaches folder context menus.",
  ["ui-panel", "component", "asset-browser"],
  "moderate",
  false
);
addFn(
  pb,
  "DrawBreadcrumb",
  359,
  384,
  "Renders clickable path breadcrumbs that jump the browser back to an ancestor folder.",
  ["ui-panel", "navigation", "asset-browser"],
  "moderate",
  false
);
addFn(
  pb,
  "DrawBrowserModals",
  399,
  588,
  "Draws rename, delete, new-scene, and new-folder popups and commits changes through ProjectAssetOps.",
  ["ui-panel", "event-handler", "validation"],
  "complex",
  false
);
addFn(
  pb,
  "ProjectBrowserPanel",
  592,
  699,
  "Main project-browser panel: refresh, search, folder tree, asset grid, prefab drag-drop, and modal ownership.",
  ["entry-point", "ui-panel", "asset-browser"],
  "complex",
  true
);

const tg = "engine/editor/transform_gizmo.cpp";
addFn(
  tg,
  "BuildImGuizmoLookAt",
  45,
  73,
  "Constructs a left-handed look-at view matrix in ImGuizmo column-major layout from eye, at, and up.",
  ["utility", "math", "gizmo"],
  "moderate",
  false
);
addFn(
  tg,
  "BuildImGuizmoPerspective",
  75,
  88,
  "Fills an ImGuizmo-compatible perspective projection from FOV, aspect, and clip planes.",
  ["utility", "math", "gizmo"],
  "simple",
  false
);
addFn(
  tg,
  "BuildCameraMatrices",
  96,
  106,
  "Derives ImGuizmo view and projection matrices from the current global Camera and viewport size.",
  ["camera", "gizmo", "utility"],
  "simple",
  false
);
addFn(
  tg,
  "EulerZyxDegreesFromRotationMatrix",
  108,
  134,
  "Extracts ZYX Euler degrees from a rotation matrix, handling the pitch singularity case.",
  ["math", "transform", "utility"],
  "moderate",
  false
);
addFn(
  tg,
  "ExtractWorldTRS",
  136,
  145,
  "Decomposes a Transform world matrix into ImGuizmo translation, Euler rotation, and scale.",
  ["transform", "gizmo", "serialization"],
  "simple",
  false
);
addFn(
  tg,
  "ApplyMatrixToTransform",
  188,
  220,
  "Writes an ImGuizmo matrix back to a Transform in local or world mode, converting parent space when needed.",
  ["transform", "gizmo", "serialization"],
  "moderate",
  false
);
addFn(
  tg,
  "BeginFrame",
  236,
  238,
  "Starts an ImGuizmo frame so later Draw calls can submit manipulators.",
  ["gizmo", "entry-point", "editor"],
  "simple",
  true
);
addFn(
  tg,
  "IsInteracting",
  240,
  242,
  "Reports whether the mouse is over or currently dragging the transform gizmo.",
  ["gizmo", "event-handler", "editor"],
  "simple",
  true
);
addFn(
  tg,
  "DrawToolbarControls",
  244,
  278,
  "ImGui radio buttons for translate/rotate/scale and local vs world space with tooltips.",
  ["ui-panel", "gizmo", "editor"],
  "moderate",
  true
);
addFn(
  tg,
  "HandleHotkeys",
  280,
  299,
  "W/E/R and Q/G hotkeys that switch gizmo operation and space when the mouse is in the game viewport.",
  ["event-handler", "gizmo", "editor"],
  "moderate",
  true
);
addFn(
  tg,
  "SetVisible",
  301,
  303,
  "Shows or hides the viewport transform gizmo.",
  ["gizmo", "utility", "editor"],
  "simple",
  true
);
addFn(
  tg,
  "IsVisible",
  305,
  307,
  "Returns whether the transform gizmo is currently drawn.",
  ["gizmo", "utility", "editor"],
  "simple",
  true
);
addFn(
  tg,
  "Draw",
  309,
  368,
  "Draws ImGuizmo on the selected object, applies manipulated TRS, marks the scene dirty, and pushes a transform undo command.",
  ["gizmo", "event-handler", "transform"],
  "complex",
  true
);

addFn(
  "engine/scene/scene_camera.cpp",
  "SceneCameraSyncFromActiveNode",
  32,
  49,
  "Finds the named active CameraComponent in the scene graph and applies its world pose to the global Camera.",
  ["camera", "synchronization", "scene"],
  "simple",
  true
);

nodes.push(
  n(
    "class:src/app/camera/camera.h:Camera",
    "class",
    "Camera",
    "src/app/camera/camera.h",
    "Look-at camera holding position, target, up, FOV, and clip distances, and producing view/projection matrices.",
    ["camera", "data-model", "singleton"],
    "simple",
    { lineRange: [16, 36] }
  )
);
edges.push(
  contains("file:src/app/camera/camera.h", "class:src/app/camera/camera.h:Camera")
);
edges.push(
  exportsE("file:src/app/camera/camera.h", "class:src/app/camera/camera.h:Camera")
);

const cam = "src/app/camera/camera.cpp";
addFn(
  cam,
  "GetCamera",
  15,
  17,
  "Returns the process-wide Camera singleton used by rendering and all camera controllers.",
  ["camera", "singleton", "entry-point"],
  "simple",
  true
);
addFn(
  cam,
  "SetCamera",
  19,
  21,
  "Replaces the process-wide Camera singleton with a copied Camera value.",
  ["camera", "singleton", "utility"],
  "simple",
  true
);
addFn(
  cam,
  "Camera",
  23,
  32,
  "Default constructor placing the camera at the origin looking down +Z with 45-degree FOV.",
  ["camera", "factory", "data-model"],
  "simple",
  true
);
addFn(cam, "SetPosition", 34, 34, "Sets the camera eye position.", ["camera", "utility", "accessor"], "simple", true);
addFn(cam, "SetTarget", 35, 35, "Sets the look-at target point.", ["camera", "utility", "accessor"], "simple", true);
addFn(cam, "SetUp", 36, 36, "Sets the camera up vector.", ["camera", "utility", "accessor"], "simple", true);
addFn(cam, "SetFov", 37, 37, "Sets vertical field of view in degrees.", ["camera", "utility", "accessor"], "simple", true);
addFn(cam, "SetClip", 38, 38, "Sets near and far clip distances.", ["camera", "utility", "accessor"], "simple", true);
addFn(
  cam,
  "GetViewMatrix",
  40,
  45,
  "Builds a left-handed look-at view matrix from position, target, and up.",
  ["camera", "math", "utility"],
  "simple",
  true
);
addFn(
  cam,
  "GetProjMatrix",
  47,
  53,
  "Builds a left-handed perspective projection from FOV, aspect, and clip planes.",
  ["camera", "math", "utility"],
  "simple",
  true
);
addFn(
  cam,
  "GetForward",
  55,
  67,
  "Returns the normalized vector from camera position toward the look-at target.",
  ["camera", "math", "utility"],
  "simple",
  true
);

const cd = "src/app/camera/camera_director.cpp";
addFn(
  cd,
  "TrySampleSceneNode",
  57,
  72,
  "Reads CameraPose from the editor-named active CameraComponent node if it is enabled.",
  ["camera", "scene", "sampling"],
  "simple",
  false
);
addFn(
  cd,
  "TrySampleActiveSource",
  74,
  91,
  "Samples Follow, first-person, or scene-node pose depending on the current director source.",
  ["camera", "director", "sampling"],
  "moderate",
  false
);
addFn(
  cd,
  "CameraDirector_Initialize",
  101,
  113,
  "Seeds director state from the current global Camera and default blend duration.",
  ["camera", "director", "entry-point"],
  "simple",
  true
);
addFn(
  cd,
  "CameraDirector_SetSource",
  115,
  123,
  "Switches the active producer and starts a blend from the current global pose.",
  ["camera", "director", "blending"],
  "simple",
  true
);
addFn(
  cd,
  "CameraDirector_GetSource",
  125,
  127,
  "Returns the current CameraDirectorSource (Follow, SceneNode, Intro, or FirstPerson).",
  ["camera", "director", "accessor"],
  "simple",
  true
);
addFn(
  cd,
  "CameraDirector_SetIntroPosition",
  129,
  131,
  "Writes the intro producer eye position (used by intro camera scripts).",
  ["camera", "director", "accessor"],
  "simple",
  true
);
addFn(
  cd,
  "CameraDirector_SetIntroTarget",
  133,
  135,
  "Writes the intro producer look-at target.",
  ["camera", "director", "accessor"],
  "simple",
  true
);
addFn(
  cd,
  "CameraDirector_SetIntroUp",
  137,
  139,
  "Writes the intro producer up vector.",
  ["camera", "director", "accessor"],
  "simple",
  true
);
addFn(
  cd,
  "CameraDirector_GetIntroPose",
  141,
  143,
  "Returns the current intro CameraPose written by script camera setters.",
  ["camera", "director", "accessor"],
  "simple",
  true
);
addFn(
  cd,
  "CameraDirector_SetBlendDuration",
  145,
  147,
  "Sets the ease duration used when switching director sources.",
  ["camera", "director", "blending"],
  "simple",
  true
);
addFn(
  cd,
  "CameraDirector_Update",
  149,
  182,
  "Each frame samples the active source, eases toward it, and writes the result to the global Camera unless debug camera owns the view.",
  ["camera", "director", "entry-point"],
  "moderate",
  true
);

const cp = "src/app/camera/camera_pose.cpp";
addFn(
  cp,
  "SlerpDir",
  53,
  81,
  "Spherically interpolates unit directions, falling back to lerp or an orthonormal axis when inputs are parallel.",
  ["math", "camera", "utility"],
  "moderate",
  false
);
addFn(
  cp,
  "ExtractLook",
  83,
  95,
  "Splits a CameraPose into a look distance and normalized forward for blend math.",
  ["camera", "math", "utility"],
  "simple",
  false
);
addFn(
  cp,
  "CameraPose_FromGlobal",
  99,
  109,
  "Snapshots the process-wide Camera into a CameraPose.",
  ["camera", "serialization", "utility"],
  "simple",
  true
);
addFn(
  cp,
  "CameraPose_ApplyToGlobal",
  111,
  118,
  "Writes a CameraPose onto the process-wide Camera (position, target, up, FOV, clip).",
  ["camera", "serialization", "utility"],
  "simple",
  true
);
addFn(
  cp,
  "CameraPose_Lerp",
  120,
  138,
  "Blends two poses by lerping eye along look distance and slerping view direction.",
  ["camera", "math", "blending"],
  "moderate",
  true
);
addFn(
  cp,
  "CameraPose_FromComponent",
  140,
  160,
  "Builds a CameraPose from a CameraComponent and Transform using world-space eye/forward/up.",
  ["camera", "ecs", "serialization"],
  "moderate",
  true
);
addFn(
  cp,
  "CameraPose_WriteToNode",
  162,
  195,
  "Writes a CameraPose back onto a Transform as an orthonormal local basis plus CameraComponent FOV/clip.",
  ["camera", "ecs", "serialization"],
  "moderate",
  true
);

const dc = "src/app/camera/debug_camera.cpp";
addFn(
  dc,
  "InitFromCameraOnce",
  55,
  89,
  "Seeds orbit yaw, pitch, distance, and focal point from the current global Camera the first time orbit starts.",
  ["camera", "debug", "initialization"],
  "moderate",
  false
);
addFn(
  dc,
  "UpdateCameraFromOrbit",
  92,
  115,
  "Recomputes Camera position/target/up from orbit yaw, pitch, distance, and focal point.",
  ["camera", "debug", "math"],
  "moderate",
  false
);
addFn(
  dc,
  "ApplyFlyViewToCamera",
  132,
  143,
  "Applies fly-mode position plus yaw/pitch forward onto the global Camera.",
  ["camera", "debug", "utility"],
  "simple",
  false
);
addFn(
  dc,
  "UpdateFlyMode",
  145,
  198,
  "RMB look plus WASD/Space/Ctrl fly movement with Shift boost, writing the result to the global Camera.",
  ["camera", "debug", "event-handler"],
  "moderate",
  false
);
addFn(
  dc,
  "DebugCamera_IsEnabled",
  201,
  201,
  "Returns whether the debug camera currently owns the global Camera.",
  ["camera", "debug", "accessor"],
  "simple",
  true
);
addFn(
  dc,
  "DebugCamera_SetControlMode",
  203,
  206,
  "Switches between Orbit (Maya Alt+mouse) and Fly (RMB+WASD) control.",
  ["camera", "debug", "accessor"],
  "simple",
  true
);
addFn(
  dc,
  "DebugCamera_GetControlMode",
  208,
  210,
  "Returns the active DebugCameraControlMode.",
  ["camera", "debug", "accessor"],
  "simple",
  true
);
addFn(
  dc,
  "DebugCamera_SetFlyView",
  212,
  219,
  "Seeds fly-mode world position and yaw/pitch in degrees, then applies it immediately.",
  ["camera", "debug", "utility"],
  "simple",
  true
);
addFn(
  dc,
  "DebugCamera_SetEnabled",
  221,
  256,
  "Enables or disables debug camera, initializing orbit or fly state from the current view.",
  ["camera", "debug", "event-handler"],
  "moderate",
  true
);
addFn(dc, "DebugCamera_SetMoveSpeed", 258, 258, "Sets base fly move speed in units per second.", ["camera", "debug", "accessor"], "simple", true);
addFn(dc, "DebugCamera_SetBoostMultiplier", 259, 259, "Sets the Shift fly-speed multiplier.", ["camera", "debug", "accessor"], "simple", true);
addFn(dc, "DebugCamera_SetMouseSensitivity", 260, 260, "Sets fly look sensitivity in degrees per pixel.", ["camera", "debug", "accessor"], "simple", true);
addFn(dc, "DebugCamera_SetOrbitSensitivity", 261, 261, "Sets orbit tumble sensitivity.", ["camera", "debug", "accessor"], "simple", true);
addFn(dc, "DebugCamera_SetPanSensitivity", 262, 262, "Sets orbit pan sensitivity.", ["camera", "debug", "accessor"], "simple", true);
addFn(dc, "DebugCamera_SetDollySensitivity", 263, 263, "Sets orbit dolly/zoom sensitivity.", ["camera", "debug", "accessor"], "simple", true);
addFn(
  dc,
  "DebugCamera_SetFocalPoint",
  265,
  268,
  "Sets the orbit focal point the camera tumbles around.",
  ["camera", "debug", "accessor"],
  "simple",
  true
);
addFn(
  dc,
  "DebugCamera_SyncOrbitFromCurrentView",
  270,
  298,
  "Rebuilds orbit yaw/pitch/distance from the current Camera position and target.",
  ["camera", "debug", "synchronization"],
  "moderate",
  true
);
addFn(
  dc,
  "DebugCamera_Update",
  300,
  426,
  "Per-frame debug camera: F1 toggle, ImGui capture guards, Maya Alt orbit/pan/dolly, wheel dolly, and RMB fly.",
  ["camera", "debug", "event-handler", "entry-point"],
  "complex",
  true
);

const fp = "src/app/camera/first_person_camera.cpp";
addFn(
  fp,
  "FirstPersonCamera_Initialize",
  68,
  77,
  "Resets first-person attach pointers, pitch limits, eye height, and cached pose.",
  ["camera", "gameplay", "initialization"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_Update",
  79,
  142,
  "Reads mouse and right-stick look, recenters the cursor, and writes an eye-height pose onto the hidden FPS camera node.",
  ["camera", "gameplay", "event-handler"],
  "moderate",
  true
);
addFn(
  fp,
  "FirstPersonCamera_SetTarget",
  144,
  148,
  "Attaches live pointers to player world position and yaw, or detaches when both are null.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_SetEyeHeight",
  150,
  152,
  "Sets eye height above the attached target position.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_SetMouseSensitivity",
  154,
  156,
  "Sets mouse look sensitivity for first-person yaw/pitch.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_SetControllerSensitivity",
  158,
  160,
  "Sets gamepad right-stick look sensitivity.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_SetPitchLimits",
  162,
  165,
  "Clamps first-person pitch range in degrees.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_SetFov",
  167,
  169,
  "Sets first-person vertical FOV in degrees.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_ShowCursor",
  171,
  174,
  "Forwards cursor visibility to the shared third-person cursor helper.",
  ["camera", "input", "utility"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_IsCursorVisible",
  176,
  178,
  "Reports cursor visibility via the shared third-person cursor state.",
  ["camera", "input", "utility"],
  "simple",
  true
);
addFn(
  fp,
  "FirstPersonCamera_TryGetPose",
  180,
  185,
  "Copies the last computed first-person CameraPose for the director if one exists.",
  ["camera", "sampling", "director"],
  "simple",
  true
);

const tp = "src/app/camera/third_person_camera.cpp";
addFn(
  tp,
  "DesiredFaceAnglesToTarget",
  133,
  147,
  "Computes yaw/pitch that face the magnet target from the player, clamped to pitch limits.",
  ["camera", "math", "gameplay"],
  "simple",
  false
);
addFn(
  tp,
  "FollowSharpnessForMode",
  184,
  203,
  "Returns exponential follow sharpness for Normal/Push/Pull/Orbit, easing with speed factor.",
  ["camera", "gameplay", "utility"],
  "moderate",
  false
);
addFn(
  tp,
  "OnMagnetModeChanged",
  205,
  224,
  "Handles magnet mode transitions by snapping or easing distance, FOV, and facing.",
  ["camera", "gameplay", "event-handler"],
  "moderate",
  false
);
addFn(
  tp,
  "ThirdPersonCamera_Initialize",
  228,
  250,
  "Resets follow attach pointers, default distance/height/FOV, magnet mode, trauma, and cached pose.",
  ["camera", "gameplay", "initialization"],
  "moderate",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_Update",
  252,
  390,
  "Updates follow orbit from mouse/stick, magnet facing, smoothed boom, trauma shake, and writes the follow-cam node.",
  ["camera", "gameplay", "event-handler", "entry-point"],
  "complex",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_SetTarget",
  392,
  397,
  "Attaches live pointers to player position and yaw, or detaches the follow camera.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(tp, "ThirdPersonCamera_SetDistance", 399, 401, "Sets default follow boom distance.", ["camera", "gameplay", "accessor"], "simple", true);
addFn(tp, "ThirdPersonCamera_SetHeight", 403, 405, "Sets camera height offset above the look-at point.", ["camera", "gameplay", "accessor"], "simple", true);
addFn(tp, "ThirdPersonCamera_SetLookAtHeight", 407, 409, "Sets the look-at height offset on the target.", ["camera", "gameplay", "accessor"], "simple", true);
addFn(tp, "ThirdPersonCamera_SetMouseSensitivity", 411, 413, "Sets mouse orbit sensitivity.", ["camera", "gameplay", "accessor"], "simple", true);
addFn(tp, "ThirdPersonCamera_SetControllerSensitivity", 415, 417, "Sets gamepad orbit sensitivity.", ["camera", "gameplay", "accessor"], "simple", true);
addFn(tp, "ThirdPersonCamera_SetPitchLimits", 419, 422, "Sets follow-camera pitch clamp range.", ["camera", "gameplay", "accessor"], "simple", true);
addFn(tp, "ThirdPersonCamera_GetPitch", 424, 426, "Returns the current follow-camera pitch in degrees.", ["camera", "gameplay", "accessor"], "simple", true);
addFn(tp, "ThirdPersonCamera_IsCursorVisible", 428, 430, "Returns whether the hardware cursor is shown.", ["camera", "input", "accessor"], "simple", true);
addFn(
  tp,
  "ThirdPersonCamera_ShowCursor",
  432,
  440,
  "Shows or hides the hardware cursor with a ref-counted ShowCursor call.",
  ["camera", "input", "utility"],
  "simple",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_SetMagnetMode",
  442,
  444,
  "Sets Normal, Push, Pull, or Orbit magnet behavior.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_SetMagnetTarget",
  446,
  449,
  "Sets the world position the magnet modes face or orbit.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_TryGetFollowPose",
  451,
  456,
  "Copies the last follow CameraPose for the director if one exists.",
  ["camera", "sampling", "director"],
  "simple",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_GetFollowCamNode",
  458,
  460,
  "Returns the hidden follow-camera GameObject used for listener and scene sync.",
  ["camera", "ecs", "accessor"],
  "simple",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_AddTrauma",
  462,
  464,
  "Adds clamped hit trauma that decays into camera shake.",
  ["camera", "gameplay", "utility"],
  "simple",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_GetTrauma",
  466,
  468,
  "Returns the current trauma amount in [0,1].",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);
addFn(
  tp,
  "ThirdPersonCamera_SetSpeedFactor",
  470,
  472,
  "Sets a [0,1] speed factor that the follow camera eases into FOV boost.",
  ["camera", "gameplay", "accessor"],
  "simple",
  true
);

const calls = [
  ["function:" + pb + ":TryOpenOrUseAsset", "function:engine/ecs/prefab_serializer.h:LoadFromFile"],
  ["function:" + pb + ":OpenDeleteModal", "function:engine/editor/project_asset_ops.h:FindSceneReferences"],
  ["function:" + pb + ":DrawFolderContextMenu", "function:engine/editor/project_asset_ops.h:IsScriptsPath"],
  ["function:" + pb + ":DrawFolderContextMenu", "function:engine/editor/project_asset_ops.h:ShowInExplorer"],
  ["function:" + pb + ":DrawAssetContextMenu", "function:engine/editor/project_asset_ops.h:ShowInExplorer"],
  ["function:" + pb + ":DrawBrowserModals", "function:engine/editor/project_asset_ops.h:RenameAsset"],
  ["function:" + pb + ":DrawBrowserModals", "function:engine/editor/project_asset_ops.h:DeleteAssetToRecycleBin"],
  ["function:" + pb + ":DrawBrowserModals", "function:engine/editor/project_asset_ops.h:CreateFolder"],
  ["function:" + pb + ":DrawBrowserModals", "function:engine/editor/project_asset_ops.h:CreateMinimalScene"],
  ["function:" + pb + ":DrawBrowserModals", "function:engine/editor/project_asset_ops.h:IsCurrentSceneOpen"],
  ["function:" + pb + ":DrawBrowserModals", "function:engine/editor/project_asset_ops.h:FolderContainsCurrentScene"],
  ["function:" + pb + ":DrawBrowserModals", "function:engine/editor/project_asset_index.h:Refresh"],
  ["function:" + pb + ":ProjectBrowserPanel", "function:engine/editor/project_asset_index.h:Refresh"],
  ["function:" + pb + ":ProjectBrowserPanel", "function:engine/editor/project_asset_index.h:GetFolderContents"],
  ["function:" + pb + ":ProjectBrowserPanel", "function:engine/scene/scene_loader.cpp:SceneLoader_CopySceneFileName"],
  ["function:" + tg + ":ExtractWorldTRS", "function:engine/ecs/transform.h:GetWorldMatrix"],
  ["function:" + tg + ":ApplyMatrixToTransform", "function:engine/ecs/transform.h:SetFromLocalMatrix"],
  ["function:" + tg + ":Draw", "function:engine/ecs/transform.h:GetWorldMatrix"],
  ["function:" + tg + ":Draw", "function:engine/ecs/transform.h:SnapshotStartState"],
  ["function:" + tg + ":Draw", "function:engine/ecs/transform.h:Serialize"],
  ["function:" + tg + ":HandleHotkeys", "function:engine/input/mouse.cpp:Mouse_IsRightDown"],
  ["function:engine/scene/scene_camera.cpp:SceneCameraSyncFromActiveNode", "function:engine/ecs/camera_component.h:ApplyToGlobalCamera"],
  ["function:" + cd + ":TrySampleSceneNode", "function:" + cp + ":CameraPose_FromComponent"],
  ["function:" + cd + ":TrySampleActiveSource", "function:" + tp + ":ThirdPersonCamera_TryGetFollowPose"],
  ["function:" + cd + ":TrySampleActiveSource", "function:" + fp + ":FirstPersonCamera_TryGetPose"],
  ["function:" + cd + ":CameraDirector_Update", "function:" + dc + ":DebugCamera_IsEnabled"],
  ["function:" + cd + ":CameraDirector_Update", "function:" + cp + ":CameraPose_FromGlobal"],
  ["function:" + cd + ":CameraDirector_Update", "function:" + cp + ":CameraPose_Lerp"],
  ["function:" + cd + ":CameraDirector_Update", "function:" + cp + ":CameraPose_ApplyToGlobal"],
  ["function:" + cp + ":CameraPose_FromGlobal", "function:" + cam + ":GetCamera"],
  ["function:" + cp + ":CameraPose_ApplyToGlobal", "function:" + cam + ":GetCamera"],
  ["function:" + cp + ":CameraPose_ApplyToGlobal", "function:" + cam + ":SetPosition"],
  ["function:" + cp + ":CameraPose_ApplyToGlobal", "function:" + cam + ":SetTarget"],
  ["function:" + cp + ":CameraPose_ApplyToGlobal", "function:" + cam + ":SetUp"],
  ["function:" + cp + ":CameraPose_ApplyToGlobal", "function:" + cam + ":SetFov"],
  ["function:" + cp + ":CameraPose_ApplyToGlobal", "function:" + cam + ":SetClip"],
  ["function:" + cp + ":CameraPose_FromComponent", "function:engine/ecs/camera_component.h:ComputeEyeWS"],
  ["function:" + cp + ":CameraPose_FromComponent", "function:engine/ecs/camera_component.h:ComputeForwardWS"],
  ["function:" + cp + ":CameraPose_FromComponent", "function:engine/ecs/camera_component.h:ComputeViewUpWS"],
  ["function:" + cp + ":CameraPose_WriteToNode", "function:engine/ecs/transform.h:SetFromLocalMatrix"],
  ["function:" + dc + ":InitFromCameraOnce", "function:" + cam + ":GetCamera"],
  ["function:" + dc + ":UpdateCameraFromOrbit", "function:" + cam + ":GetCamera"],
  ["function:" + dc + ":UpdateCameraFromOrbit", "function:" + cam + ":SetPosition"],
  ["function:" + dc + ":UpdateCameraFromOrbit", "function:" + cam + ":SetTarget"],
  ["function:" + dc + ":UpdateCameraFromOrbit", "function:" + cam + ":SetUp"],
  ["function:" + dc + ":ApplyFlyViewToCamera", "function:" + cam + ":GetCamera"],
  ["function:" + dc + ":ApplyFlyViewToCamera", "function:" + cam + ":SetPosition"],
  ["function:" + dc + ":ApplyFlyViewToCamera", "function:" + cam + ":SetTarget"],
  ["function:" + dc + ":ApplyFlyViewToCamera", "function:" + cam + ":SetUp"],
  ["function:" + dc + ":UpdateFlyMode", "function:engine/input/mouse.cpp:Mouse_GetDelta"],
  ["function:" + dc + ":UpdateFlyMode", "function:engine/input/mouse.cpp:Mouse_IsRightDown"],
  ["function:" + dc + ":DebugCamera_SyncOrbitFromCurrentView", "function:" + cam + ":GetCamera"],
  ["function:" + dc + ":DebugCamera_Update", "function:" + cam + ":GetCamera"],
  ["function:" + dc + ":DebugCamera_Update", "function:engine/input/mouse.cpp:Mouse_GetDelta"],
  ["function:" + dc + ":DebugCamera_Update", "function:engine/input/mouse.cpp:Mouse_GetWheelDelta"],
  ["function:" + dc + ":DebugCamera_Update", "function:engine/input/mouse.cpp:Mouse_IsLeftDown"],
  ["function:" + dc + ":DebugCamera_Update", "function:engine/input/mouse.cpp:Mouse_IsMiddleDown"],
  ["function:" + dc + ":DebugCamera_Update", "function:engine/input/mouse.cpp:Mouse_IsRightDown"],
  ["function:" + fp + ":FirstPersonCamera_Update", "function:engine/input/mouse.cpp:Mouse_GetClientPosition"],
  ["function:" + fp + ":FirstPersonCamera_Update", "function:engine/input/mouse.cpp:Mouse_GetDelta"],
  ["function:" + fp + ":FirstPersonCamera_ShowCursor", "function:" + tp + ":ThirdPersonCamera_ShowCursor"],
  ["function:" + fp + ":FirstPersonCamera_IsCursorVisible", "function:" + tp + ":ThirdPersonCamera_IsCursorVisible"],
  ["function:" + tp + ":ThirdPersonCamera_Update", "function:engine/input/mouse.cpp:Mouse_GetClientPosition"],
  ["function:" + tp + ":ThirdPersonCamera_Update", "function:engine/input/mouse.cpp:Mouse_GetDelta"],
  ["function:" + fp + ":FirstPersonCamera_Update", "function:" + cp + ":CameraPose_WriteToNode"],
  ["function:" + tp + ":ThirdPersonCamera_Update", "function:" + cp + ":CameraPose_WriteToNode"],
];

for (const [s, t] of calls) edges.push(callsE(s, t));

const ids = new Set();
for (const node of nodes) {
  if (ids.has(node.id)) throw new Error("dup id " + node.id);
  ids.add(node.id);
  if (!node.summary) throw new Error("empty summary " + node.id);
  if (!node.tags || node.tags.length < 3) throw new Error("tags " + node.id);
}

const neighbor = JSON.parse(
  fs.readFileSync(
    "C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-neighbors-8.json",
    "utf8"
  )
);
const neighborFiles = new Set();
const neighborSymbols = new Set();
for (const [src, list] of Object.entries(neighbor)) {
  neighborFiles.add(src);
  for (const item of list) {
    neighborFiles.add(item.path);
    for (const sym of item.symbols || []) {
      neighborSymbols.add(item.path + ":" + sym);
    }
  }
}
const importFiles = new Set();
for (const [src, list] of Object.entries(imports)) {
  importFiles.add(src);
  for (const p of list) importFiles.add(p);
}

function targetOk(target, partIds) {
  if (partIds.has(target)) return true;
  if (target.startsWith("file:")) {
    const p = target.slice(5);
    return neighborFiles.has(p) || importFiles.has(p);
  }
  const m = target.match(/^(function|class):(.+):([^:]+)$/);
  if (m) {
    const p = m[2];
    const sym = m[3];
    if (neighborSymbols.has(p + ":" + sym)) return true;
    if (ids.has(target)) return true;
    return false;
  }
  return false;
}

const files = [...new Set(nodes.filter((x) => x.filePath).map((x) => x.filePath))].sort();
const nodeCount = nodes.length;
const edgeCount = edges.length;
const parts = Math.max(1, Math.ceil(Math.max(nodeCount / 60, edgeCount / 120)));
const chunk = Math.ceil(files.length / parts);

console.log("files", files.length);
console.log("nodes", nodeCount, "edges", edgeCount, "imports", importCount);
console.log("parts", parts, "chunk", chunk);

const outDir = "C:/Users/terry/source/repos/ivy-engine/.ua/intermediate";
const written = [];
if (parts === 1) {
  fs.writeFileSync(path.join(outDir, "batch-8.json"), JSON.stringify({ nodes, edges }, null, 2));
  written.push({ file: "batch-8.json", nodes: nodeCount, edges: edgeCount });
} else {
  for (let k = 0; k < parts; k++) {
    const partFiles = files.slice(k * chunk, (k + 1) * chunk);
    const partSet = new Set(partFiles);
    const pNodes = nodes.filter((nd) => partSet.has(nd.filePath));
    const pIds = new Set(pNodes.map((nd) => nd.id));
    const pEdges = edges.filter((ed) => pIds.has(ed.source));
    const bad = [];
    for (const ed of pEdges) {
      if (!targetOk(ed.target, pIds)) bad.push(ed);
    }
    if (bad.length) {
      console.error("VALIDATION FAIL part", k + 1);
      for (const ed of bad) console.error(ed.source, "->", ed.target, ed.type);
      throw new Error("validation failed part " + (k + 1));
    }
    const fname = "batch-8-part-" + (k + 1) + ".json";
    fs.writeFileSync(path.join(outDir, fname), JSON.stringify({ nodes: pNodes, edges: pEdges }, null, 2));
    written.push({ file: fname, nodes: pNodes.length, edges: pEdges.length, files: partFiles });
    console.log(fname, "nodes", pNodes.length, "edges", pEdges.length);
  }
}

fs.writeFileSync(
  "C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-emit-8-summary.json",
  JSON.stringify({ written, nodeCount, edgeCount, importCount, filesSkipped: [] }, null, 2)
);
