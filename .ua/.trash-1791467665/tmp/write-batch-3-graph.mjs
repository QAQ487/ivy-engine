import fs from "fs";

const UA = "C:/Users/terry/source/repos/ivy-engine/.ua";
const input = JSON.parse(
  fs.readFileSync(`${UA}/tmp/ua-file-analyzer-input-3.json`, "utf8")
);
const neighborMap = JSON.parse(
  fs.readFileSync(`${UA}/tmp/ua-file-analyzer-neighbor-3.json`, "utf8")
);

function fileNode(filePath, summary, tags, complexity, languageNotes) {
  const name = filePath.split("/").pop();
  const n = {
    id: `file:${filePath}`,
    type: "file",
    name,
    filePath,
    summary,
    tags,
    complexity,
  };
  if (languageNotes) n.languageNotes = languageNotes;
  return n;
}

function fn(filePath, name, lineRange, summary, tags, complexity) {
  return {
    id: `function:${filePath}:${name}`,
    type: "function",
    name,
    filePath,
    lineRange,
    summary,
    tags,
    complexity,
  };
}

function cls(filePath, name, lineRange, summary, tags, complexity, languageNotes) {
  const n = {
    id: `class:${filePath}:${name}`,
    type: "class",
    name,
    filePath,
    lineRange,
    summary,
    tags,
    complexity,
  };
  if (languageNotes) n.languageNotes = languageNotes;
  return n;
}

function edge(source, target, type, weight) {
  return { source, target, type, direction: "forward", weight };
}

const nodes = [];
const edges = [];

const F = {
  controllerCpp: "engine/input/controller.cpp",
  controllerH: "engine/input/controller.h",
  keyboardCpp: "engine/input/keyboard.cpp",
  keyboardH: "engine/input/keyboard.h",
  particleH: "engine/rendering/particle/particle_system.h",
  sceneNodeCpp: "engine/scene/scene_node.cpp",
  sceneNodeH: "engine/scene/scene_node.h",
  scriptH: "engine/scripting/Script.h",
  dllExports: "engine/scripting/script_dll_exports.cpp",
  dllRegister: "engine/scripting/script_dll_register.h",
  fieldsH: "engine/scripting/script_fields.h",
  runtimeH: "engine/scripting/script_runtime_api.h",
  canvasCpp: "engine/ui/ui_canvas.cpp",
  canvasH: "engine/ui/ui_canvas.h",
  fpCpp: "samples/scripts/SampleFirstPersonScript.cpp",
  fpH: "samples/scripts/SampleFirstPersonScript.h",
  playerCpp: "samples/scripts/SamplePlayerScript.cpp",
  playerH: "samples/scripts/SamplePlayerScript.h",
  spinCpp: "samples/scripts/SampleSpinScript.cpp",
  spinH: "samples/scripts/SampleSpinScript.h",
  triggerCpp: "samples/scripts/SampleTriggerLogScript.cpp",
  triggerH: "samples/scripts/SampleTriggerLogScript.h",
};

// ---- file nodes ----
nodes.push(
  fileNode(
    F.controllerCpp,
    "Implements XInput gamepad polling for up to four pads, with button edge detection, deadzone-normalized sticks and triggers, and rumble.",
    ["input", "controller", "xinput", "event-handler"],
    "moderate",
    "Polls XInputGetState each frame and caches previous button bits for trigger/release edges."
  ),
  fileNode(
    F.controllerH,
    "Public gamepad API: ControllerButton enum plus initialize, update, button, analog-stick, trigger, deadzone, and vibration functions.",
    ["input", "controller", "type-definition", "api-handler"],
    "moderate"
  ),
  fileNode(
    F.keyboardCpp,
    "Maintains current and previous Win32 keyboard bitfields and updates them from window messages for held, released, and edge-triggered keys.",
    ["input", "keyboard", "event-handler"],
    "moderate",
    "Keyboard_ProcessMessage handles WM_KEYDOWN/UP, SYSKEY, and WM_ACTIVATEAPP reset."
  ),
  fileNode(
    F.keyboardH,
    "Keyboard key enum, packed Keyboard_State bitfield, and query/reset/message-pump API used by gameplay, editor, and debug camera.",
    ["input", "keyboard", "type-definition"],
    "complex",
    "Large VK-aligned Keyboard_Keys enum plus a 256-bit Keyboard_State struct of 1-bit fields."
  ),
  fileNode(
    F.particleH,
    "Declares GPU-friendly particle emitter config, SoA particle pool, per-emitter lifetime, and the ParticleSystem facade used by scripts and scenes.",
    ["particle", "data-model", "type-definition", "rendering"],
    "moderate"
  ),
  fileNode(
    F.sceneNodeCpp,
    "Empty translation-unit stub kept so existing build rules that compile scene_node.cpp continue to succeed after GameObject moved to ECS.",
    ["scene-graph", "compatibility", "stub"],
    "simple"
  ),
  fileNode(
    F.sceneNodeH,
    "Compatibility header that aliases engine::GameObject to the canonical ECS GameObject defined in game_object.h.",
    ["scene-graph", "type-definition", "compatibility"],
    "simple",
    "using GameObject = ::GameObject inside namespace engine."
  ),
  fileNode(
    F.scriptH,
    "Base class for user gameplay scripts with create/update/destroy, trigger callbacks, optional physics proxy, and JSON serialize/deserialize hooks.",
    ["scripting", "data-model", "lifecycle"],
    "moderate"
  ),
  fileNode(
    F.dllExports,
    "Script-DLL C ABI: local class registry plus exported Create/Destroy/Serialize/Deserialize/Enumerate entry points for ScriptFactory.",
    ["scripting", "entry-point", "dll-export", "factory"],
    "moderate",
    "extern \"C\" __declspec(dllexport) ABI separate from the in-engine ScriptFactory."
  ),
  fileNode(
    F.dllRegister,
    "REGISTER_SCRIPT macro that registers a script either into the DLL-local export table or the engine ScriptFactory, depending on build flags.",
    ["scripting", "factory", "macro", "registration"],
    "simple",
    "SCRIPT_DLL_BUILD / _WINDLL selects DllScript_Register versus ScriptFactory::Register."
  ),
  fileNode(
    F.fieldsH,
    "X-macro helpers that declare script fields once and expand them into members plus Serialize/Deserialize overrides for prefab JSON.",
    ["scripting", "serialization", "macro", "utility"],
    "simple",
    "SCRIPT_SERIALIZE_METHODS expands FIELD_LIST into nlohmann::json round-trip methods."
  ),
  fileNode(
    F.runtimeH,
    "Host function-pointer table and ScriptApi_* wrappers that expose input, cameras, UI, particles, physics, audio, and scene services to gameplay scripts.",
    ["scripting", "api-handler", "type-definition", "host-abi"],
    "complex",
    "ScriptRuntimeApi is a versioned struct of C function pointers filled by the engine host."
  ),
  fileNode(
    F.canvasCpp,
    "Implements UICanvas element ownership, z-order moves, and keyboard/controller button navigation for in-scene UI.",
    ["ui", "component", "input", "navigation"],
    "complex"
  ),
  fileNode(
    F.canvasH,
    "UICanvas class that owns UI elements and buttons and exposes add/remove/reorder plus directional navigation APIs.",
    ["ui", "component", "type-definition"],
    "moderate"
  ),
  fileNode(
    F.fpCpp,
    "Sample first-person mover: WASD/stick input, kinematic rigidbody locomotion, and first-person camera binding.",
    ["sample", "scripting", "input", "camera"],
    "moderate"
  ),
  fileNode(
    F.fpH,
    "SampleFirstPersonScript declaration with serialized moveSpeed/eyeHeight fields and first-person lifecycle overrides.",
    ["sample", "scripting", "data-model"],
    "simple"
  ),
  fileNode(
    F.playerCpp,
    "Sample third-person player: WASD/stick movement facing camera-relative direction and third-person camera follow setup.",
    ["sample", "scripting", "input", "camera"],
    "moderate"
  ),
  fileNode(
    F.playerH,
    "SamplePlayerScript declaration with serialized moveSpeed and third-person player lifecycle overrides.",
    ["sample", "scripting", "data-model"],
    "simple"
  ),
  fileNode(
    F.spinCpp,
    "Sample Y-axis spinner used to demo parallel-safe OnUpdate, optional busy-wait, and transform rotation.",
    ["sample", "scripting", "animation"],
    "simple"
  ),
  fileNode(
    F.spinH,
    "SampleSpinScript declaration with degreesPerSecond/busyMillis fields and a parallel-safe update override.",
    ["sample", "scripting", "data-model"],
    "simple"
  ),
  fileNode(
    F.triggerCpp,
    "Sample trigger-volume script that tints materials on enter/exit, logs debug text, and optionally smoke-tests physics raycasts.",
    ["sample", "scripting", "physics", "event-handler"],
    "moderate"
  ),
  fileNode(
    F.triggerH,
    "SampleTriggerLogScript declaration with raycast fields and trigger enter/exit lifecycle overrides.",
    ["sample", "scripting", "data-model"],
    "simple"
  )
);

function addSymbol(filePath, node, exported = true) {
  nodes.push(node);
  edges.push(edge(`file:${filePath}`, node.id, "contains", 1.0));
  if (exported) {
    edges.push(edge(`file:${filePath}`, node.id, "exports", 0.8));
  }
}

// controller.cpp functions (all exported)
addSymbol(F.controllerCpp, fn(F.controllerCpp, "TestButton", [38, 40], "Returns whether an XInput button bitmask includes the given ControllerButton.", ["input", "controller", "utility"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "NormalizeThumb", [43, 56], "Applies circular deadzone and length-normalizes a thumbstick axis pair into [-1, 1].", ["input", "controller", "analog"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "NormalizeTrigger", [59, 63], "Applies deadzone and scales an analog trigger into the [0, 1] range.", ["input", "controller", "analog"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_Initialize", [66, 76], "Clears per-pad XInput state, previous buttons, stick caches, and default deadzones.", ["input", "controller", "lifecycle"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_Finalize", [78, 80], "No-op teardown hook for the controller module.", ["input", "controller", "lifecycle"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_Update", [82, 113], "Polls up to four XInput pads and stores buttons, normalized sticks, and triggers for the frame.", ["input", "controller", "event-handler"], "moderate"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_IsConnected", [115, 118], "Returns whether the indexed pad was connected on the last poll.", ["input", "controller", "query"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_IsPressed", [120, 123], "Returns whether a controller button is currently held on the given pad.", ["input", "controller", "query"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_IsTrigger", [125, 131], "Returns true on the frame a controller button transitions from up to down.", ["input", "controller", "query"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_IsRelease", [133, 139], "Returns true on the frame a controller button transitions from down to up.", ["input", "controller", "query"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_GetLeftStick", [141, 144], "Returns the deadzone-normalized left stick vector, or zero if the pad is disconnected.", ["input", "controller", "analog"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_GetRightStick", [146, 149], "Returns the deadzone-normalized right stick vector, or zero if the pad is disconnected.", ["input", "controller", "analog"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_GetLeftTrigger", [151, 154], "Returns the deadzone-normalized left trigger value in [0, 1].", ["input", "controller", "analog"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_GetRightTrigger", [156, 159], "Returns the deadzone-normalized right trigger value in [0, 1].", ["input", "controller", "analog"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_SetStickDeadzone", [161, 167], "Sets left and right thumbstick deadzone magnitudes used during analog normalization.", ["input", "controller", "config"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_SetTriggerDeadzone", [169, 172], "Sets the analog trigger deadzone used during trigger normalization.", ["input", "controller", "config"], "simple"));
addSymbol(F.controllerCpp, fn(F.controllerCpp, "Controller_SetVibration", [174, 185], "Sets left and right rumble motor speeds on a connected pad via XInputSetState.", ["input", "controller", "haptics"], "simple"));

// keyboard.cpp
addSymbol(F.keyboardCpp, fn(F.keyboardCpp, "keycopy", [21, 23], "Copies the current keyboard bitfield into the previous-frame snapshot used for edge detection.", ["input", "keyboard", "utility"], "simple"));
addSymbol(F.keyboardCpp, fn(F.keyboardCpp, "Keyboard_Initialize", [42, 44], "Resets keyboard current and previous state at engine startup.", ["input", "keyboard", "lifecycle"], "simple"));
addSymbol(F.keyboardCpp, fn(F.keyboardCpp, "Keyboard_IsKeyDown", [46, 53], "Returns whether a key is down in an explicit Keyboard_State or the live current state.", ["input", "keyboard", "query"], "simple"));
addSymbol(F.keyboardCpp, fn(F.keyboardCpp, "Keyboard_IsKeyDownTrigger", [56, 66], "Returns true when a key is down this frame and was up in the previous snapshot.", ["input", "keyboard", "query"], "simple"));
addSymbol(F.keyboardCpp, fn(F.keyboardCpp, "Keyboard_IsKeyUp", [68, 75], "Returns whether a key is up in an explicit Keyboard_State or the live current state.", ["input", "keyboard", "query"], "simple"));
addSymbol(F.keyboardCpp, fn(F.keyboardCpp, "Keyboard_Reset", [93, 96], "Zeroes current and previous keyboard bitfields, typically on focus change.", ["input", "keyboard", "lifecycle"], "simple"));
addSymbol(F.keyboardCpp, fn(F.keyboardCpp, "Keyboard_ProcessMessage", [99, 146], "Updates keyboard state from Win32 key, syskey, and activate-app window messages.", ["input", "keyboard", "event-handler"], "moderate"));

// particle classes
addSymbol(F.particleH, cls(F.particleH, "ParticleEmitterConfig", [31, 58], "Tunable emitter settings for shape, lifetime, speed, color, gravity, looping, and billboard texturing.", ["particle", "data-model", "config"], "moderate"));
addSymbol(F.particleH, cls(F.particleH, "ParticlePool", [60, 103], "Structure-of-arrays pool that spawns, ticks, and compacts alive particles up to a fixed capacity.", ["particle", "data-model", "pool"], "moderate"));
addSymbol(F.particleH, cls(F.particleH, "ParticleEmitter", [105, 141], "Runtime emitter that owns a particle pool, emission accumulator, texture, and world or local simulation.", ["particle", "emitter", "runtime"], "moderate"));
addSymbol(F.particleH, cls(F.particleH, "ParticleSystem", [143, 172], "Engine facade that creates, ticks, renders, and destroys named particle emitters.", ["particle", "service", "facade"], "moderate"));

// Script base
addSymbol(F.scriptH, cls(F.scriptH, "Script", [36, 87], "Abstract gameplay-script base with lifecycle, trigger, physics-proxy, and JSON property hooks.", ["scripting", "lifecycle", "base-class"], "moderate"));

// DLL exports (parser missed extern C; these are the public ABI)
addSymbol(F.dllExports, fn(F.dllExports, "DllScript_Register", [19, 21], "Registers a script class name and factory into the DLL-local creator map at static init.", ["scripting", "factory", "registration"], "simple"));
addSymbol(F.dllExports, fn(F.dllExports, "CreateScript", [28, 33], "Exported C entry that instantiates a registered script class by name for ScriptFactory.", ["scripting", "dll-export", "factory"], "simple"));
addSymbol(F.dllExports, fn(F.dllExports, "DestroyScript", [36, 38], "Exported C entry that deletes a script instance created by the DLL.", ["scripting", "dll-export", "lifecycle"], "simple"));
addSymbol(F.dllExports, fn(F.dllExports, "SerializeScript", [41, 53], "Exported C entry that dumps a script's JSON properties into a caller-provided buffer.", ["scripting", "dll-export", "serialization"], "simple"));
addSymbol(F.dllExports, fn(F.dllExports, "DeserializeScript", [56, 65], "Exported C entry that restores script properties from JSON, falling back to an empty object on parse failure.", ["scripting", "dll-export", "serialization"], "simple"));
addSymbol(F.dllExports, fn(F.dllExports, "EnumerateScripts", [68, 71], "Exported C entry that callbacks every script class name registered in this DLL.", ["scripting", "dll-export", "discovery"], "simple"));

// runtime API
addSymbol(
  F.runtimeH,
  cls(
    F.runtimeH,
    "ScriptRuntimeApi",
    [27, 217],
    "Versioned host ABI of function pointers covering input, cameras, UI, particles, physics, audio, prefabs, and debug drawing.",
    ["scripting", "host-abi", "api-handler"],
    "complex",
    "Filled once by the engine; scripts call thin ScriptApi_* wrappers that dereference this table."
  )
);

// UICanvas class (C parser missed the header) + cpp methods
addSymbol(
  F.canvasH,
  cls(
    F.canvasH,
    "UICanvas",
    [26, 121],
    "Scene UI container that owns elements, tracks selectable buttons, and routes keyboard/controller navigation.",
    ["ui", "component", "navigation"],
    "moderate"
  )
);

addSymbol(F.canvasCpp, fn(F.canvasCpp, "UICanvas", [13, 18], "Constructs a named canvas with empty element/button lists and navigation disabled.", ["ui", "lifecycle", "constructor"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "Initialize", [20, 31], "Initializes every owned UI element when the canvas enters a scene.", ["ui", "lifecycle"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "Update", [33, 45], "Updates visible elements and, when enabled, runs keyboard/controller button navigation.", ["ui", "lifecycle", "input"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "Draw", [47, 54], "Draws every visible UI element in canvas order.", ["ui", "rendering"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "Finalize", [56, 65], "Finalizes and releases all owned UI elements.", ["ui", "lifecycle"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "AddElement", [126, 140], "Takes ownership of a UI element, initializes it if the canvas is live, and rebuilds the button list.", ["ui", "hierarchy"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "InsertElementAt", [142, 148], "Inserts an owned UI element at a given index in the draw/update list.", ["ui", "hierarchy"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "GetElementIndex", [150, 157], "Returns the index of a UI element pointer in the canvas list, or -1 if absent.", ["ui", "query"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "MoveElementToIndex", [159, 177], "Moves an existing element to a target index, clamping and preserving selection.", ["ui", "hierarchy"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "ClearElements", [179, 187], "Removes every UI element from the canvas and clears button navigation state.", ["ui", "hierarchy"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "RemoveElement", [189, 214], "Removes a UI element by name or pointer, finalizes it, and rebuilds the button list.", ["ui", "hierarchy"], "moderate"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "MoveElement", [238, 260], "Shifts an element by a relative offset in the canvas order and rebuilds buttons.", ["ui", "hierarchy"], "moderate"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "MoveElementForward", [262, 264], "Moves an element one slot toward the front of the canvas order.", ["ui", "hierarchy"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "MoveElementBackward", [266, 268], "Moves an element one slot toward the back of the canvas order.", ["ui", "hierarchy"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "MoveElementToFront", [270, 273], "Moves an element to the front of the canvas order.", ["ui", "hierarchy"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "MoveElementToBack", [275, 278], "Moves an element to the back of the canvas order.", ["ui", "hierarchy"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "SelectButton", [289, 311], "Selects a navigable UIButton by index and updates selected visual state.", ["ui", "navigation"], "moderate"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "SelectNextButton", [313, 322], "Advances selection to the next button in the rebuilt button list.", ["ui", "navigation"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "SelectPreviousButton", [324, 333], "Moves selection to the previous button in the rebuilt button list.", ["ui", "navigation"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "EnableNavigation", [342, 353], "Turns keyboard/controller UI navigation on or off and restores a valid selection.", ["ui", "navigation", "input"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "SetVisible", [355, 361], "Sets visibility on the canvas and propagates it to owned elements.", ["ui", "visibility"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "RebuildButtonList", [363, 374], "Rebuilds the cached UIButton pointer list from owned elements after hierarchy changes.", ["ui", "navigation"], "simple"));
addSymbol(F.canvasCpp, fn(F.canvasCpp, "UpdateNavigation", [376, 445], "Maps keyboard arrows/WASD and controller D-pad/stick into directional UIButton selection.", ["ui", "navigation", "input"], "complex"));

// samples
addSymbol(F.fpH, cls(F.fpH, "SampleFirstPersonScript", [6, 21], "Sample Script that drives an invisible capsule with first-person camera look and WASD/stick move.", ["sample", "scripting", "first-person"], "simple"));
addSymbol(F.fpCpp, fn(F.fpCpp, "GetMoveInput", [15, 39], "Combines WASD keyboard and left-stick controller input into a camera-relative 2D move vector.", ["sample", "input", "locomotion"], "moderate"));
addSymbol(F.fpCpp, fn(F.fpCpp, "OnCreate", [42, 63], "Configures a kinematic rigidbody, binds the first-person camera, and switches the engine camera mode.", ["sample", "lifecycle", "camera"], "moderate"));
addSymbol(F.fpCpp, fn(F.fpCpp, "OnUpdate", [65, 102], "Applies camera-relative locomotion, yaw, and first-person eye-height follow each frame.", ["sample", "locomotion", "camera"], "moderate"));
addSymbol(F.fpCpp, fn(F.fpCpp, "OnDestroy", [104, 108], "Unbinds the first-person camera target and restores the cursor when the script is destroyed.", ["sample", "lifecycle", "camera"], "simple"));

addSymbol(F.playerH, cls(F.playerH, "SamplePlayerScript", [6, 20], "Sample Script that moves a character with WASD/stick input while a third-person camera follows.", ["sample", "scripting", "third-person"], "simple"));
addSymbol(F.playerCpp, fn(F.playerCpp, "GetMoveInput", [15, 39], "Combines WASD keyboard and left-stick controller input into a planar move vector.", ["sample", "input", "locomotion"], "moderate"));
addSymbol(F.playerCpp, fn(F.playerCpp, "OnCreate", [42, 62], "Configures a kinematic rigidbody, initializes third-person follow camera, and sets play camera mode.", ["sample", "lifecycle", "camera"], "moderate"));
addSymbol(F.playerCpp, fn(F.playerCpp, "OnUpdate", [64, 101], "Moves the player in camera-forward space and yaws the body to face the move direction.", ["sample", "locomotion", "camera"], "moderate"));
addSymbol(F.playerCpp, fn(F.playerCpp, "OnDestroy", [103, 107], "Clears the third-person camera target and restores the cursor on teardown.", ["sample", "lifecycle", "camera"], "simple"));

addSymbol(F.spinH, cls(F.spinH, "SampleSpinScript", [6, 17], "Sample Script that rotates its owner around Y and optionally burns CPU to demo parallel-safe updates.", ["sample", "scripting", "animation"], "simple"));
addSymbol(F.spinCpp, fn(F.spinCpp, "OnUpdate", [11, 32], "Increments yaw by degreesPerSecond and optionally busy-waits to stress the job timeline.", ["sample", "animation", "parallel"], "moderate"));
addSymbol(F.spinCpp, fn(F.spinCpp, "Update_IsParallelSafe", [34, 38], "Declares this script safe to run on worker threads during parallel script updates.", ["sample", "parallel", "lifecycle"], "simple"));

addSymbol(F.triggerH, cls(F.triggerH, "SampleTriggerLogScript", [7, 24], "Sample Script that reacts to trigger enter/exit, tints materials, and optionally raycasts for debug HUD text.", ["sample", "scripting", "physics"], "simple"));
addSymbol(F.triggerCpp, fn(F.triggerCpp, "OnCreate", [11, 14], "Caches the owner's material component for later trigger tinting.", ["sample", "lifecycle"], "simple"));
addSymbol(F.triggerCpp, fn(F.triggerCpp, "OnUpdate", [16, 36], "Optionally raycasts forward and writes a short-lived debug pickup message with hit data.", ["sample", "physics", "debug"], "moderate"));
addSymbol(F.triggerCpp, fn(F.triggerCpp, "OnTriggerEnter", [38, 44], "Tints the owner material and logs a debug message when another collider enters the trigger.", ["sample", "physics", "event-handler"], "simple"));
addSymbol(F.triggerCpp, fn(F.triggerCpp, "OnTriggerExit", [46, 52], "Restores the owner material color and logs a debug message when the other collider exits.", ["sample", "physics", "event-handler"], "simple"));

// ---- imports (1:1 with batchImportData) ----
const importCountExpected = Object.values(input.batchImportData).reduce(
  (n, a) => n + a.length,
  0
);
for (const [from, tos] of Object.entries(input.batchImportData)) {
  for (const to of tos) {
    edges.push(edge(`file:${from}`, `file:${to}`, "imports", 0.7));
  }
}

// ---- inherits ----
edges.push(edge(`class:${F.fpH}:SampleFirstPersonScript`, `class:${F.scriptH}:Script`, "inherits", 0.9));
edges.push(edge(`class:${F.playerH}:SamplePlayerScript`, `class:${F.scriptH}:Script`, "inherits", 0.9));
edges.push(edge(`class:${F.spinH}:SampleSpinScript`, `class:${F.scriptH}:Script`, "inherits", 0.9));
edges.push(edge(`class:${F.triggerH}:SampleTriggerLogScript`, `class:${F.scriptH}:Script`, "inherits", 0.9));

// ---- cross-file calls ----
edges.push(edge(`function:${F.canvasCpp}:UpdateNavigation`, `function:${F.controllerCpp}:Controller_IsTrigger`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:GetMoveInput`, `function:${F.controllerCpp}:Controller_GetLeftStick`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnCreate`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_Initialize`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnCreate`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_SetTarget`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnCreate`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_SetEyeHeight`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnCreate`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_SetMouseSensitivity`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnCreate`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_SetPitchLimits`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnCreate`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_ShowCursor`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnUpdate`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_SetEyeHeight`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnUpdate`, `function:engine/scripting/script_runtime_api.cpp:ScriptApi_GetCameraForward`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnDestroy`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_SetTarget`, "calls", 0.8));
edges.push(edge(`function:${F.fpCpp}:OnDestroy`, `function:src/app/camera/first_person_camera.cpp:FirstPersonCamera_ShowCursor`, "calls", 0.8));

edges.push(edge(`function:${F.playerCpp}:GetMoveInput`, `function:${F.controllerCpp}:Controller_GetLeftStick`, "calls", 0.8));
edges.push(edge(`function:${F.playerCpp}:OnCreate`, `function:src/app/camera/third_person_camera.cpp:ThirdPersonCamera_Initialize`, "calls", 0.8));
edges.push(edge(`function:${F.playerCpp}:OnCreate`, `function:src/app/camera/third_person_camera.cpp:ThirdPersonCamera_SetTarget`, "calls", 0.8));
edges.push(edge(`function:${F.playerCpp}:OnCreate`, `function:src/app/camera/third_person_camera.cpp:ThirdPersonCamera_SetMouseSensitivity`, "calls", 0.8));
edges.push(edge(`function:${F.playerCpp}:OnCreate`, `function:src/app/camera/third_person_camera.cpp:ThirdPersonCamera_SetPitchLimits`, "calls", 0.8));
edges.push(edge(`function:${F.playerCpp}:OnCreate`, `function:src/app/camera/third_person_camera.cpp:ThirdPersonCamera_ShowCursor`, "calls", 0.8));
edges.push(edge(`function:${F.playerCpp}:OnUpdate`, `function:engine/scripting/script_runtime_api.cpp:ScriptApi_GetCameraForward`, "calls", 0.8));
edges.push(edge(`function:${F.playerCpp}:OnDestroy`, `function:src/app/camera/third_person_camera.cpp:ThirdPersonCamera_SetTarget`, "calls", 0.8));
edges.push(edge(`function:${F.playerCpp}:OnDestroy`, `function:src/app/camera/third_person_camera.cpp:ThirdPersonCamera_ShowCursor`, "calls", 0.8));

edges.push(edge(`function:${F.triggerCpp}:OnUpdate`, `function:engine/scripting/script_runtime_api.cpp:ScriptApi_Physics_Raycast`, "calls", 0.8));
edges.push(edge(`function:${F.triggerCpp}:OnUpdate`, `function:engine/scripting/script_runtime_api.cpp:ScriptApi_PickupDebugSetMessage`, "calls", 0.8));
edges.push(edge(`function:${F.triggerCpp}:OnTriggerEnter`, `function:engine/scripting/script_runtime_api.cpp:ScriptApi_PickupDebugSetMessage`, "calls", 0.8));
edges.push(edge(`function:${F.triggerCpp}:OnTriggerExit`, `function:engine/scripting/script_runtime_api.cpp:ScriptApi_PickupDebugSetMessage`, "calls", 0.8));

// duplicate ID check
const ids = new Set();
for (const n of nodes) {
  if (ids.has(n.id)) throw new Error(`duplicate node id ${n.id}`);
  ids.add(n.id);
}

const importEdges = edges.filter((e) => e.type === "imports");
if (importEdges.length !== importCountExpected) {
  throw new Error(
    `import edges ${importEdges.length} != expected ${importCountExpected}`
  );
}

const nodeById = new Map(nodes.map((n) => [n.id, n]));
const neighborSymbols = new Set();
const neighborFiles = new Set();
for (const [src, neighbors] of Object.entries(neighborMap)) {
  neighborFiles.add(src);
  for (const nb of neighbors) {
    neighborFiles.add(nb.path);
    for (const s of nb.symbols || []) neighborSymbols.add(`${nb.path}::${s}`);
  }
}
const importFiles = new Set();
for (const [from, tos] of Object.entries(input.batchImportData)) {
  importFiles.add(from);
  for (const to of tos) importFiles.add(to);
}

function targetOk(id, partNodeIds) {
  if (partNodeIds.has(id)) return true;
  if (id.startsWith("file:")) {
    const p = id.slice("file:".length);
    return neighborFiles.has(p) || importFiles.has(p);
  }
  const m = /^(function|class):(.+):([^:]+)$/.exec(id);
  if (m) {
    const path = m[2];
    const symbol = m[3];
    if (neighborSymbols.has(`${path}::${symbol}`)) return true;
    if (nodeById.has(id)) return true; // other part of same batch
    return false;
  }
  return false;
}

// Split: 3 parts so no part exceeds 60 nodes / 120 edges after sequential file chunking
const batchFiles = [...input.batchFiles.map((f) => f.path)].sort();
const nodeCount = nodes.length;
const edgeCount = edges.length;
const parts = Math.max(
  1,
  Math.ceil(Math.max(nodeCount / 60, edgeCount / 120))
);
// Force 3 if 2 would overflow a sequential chunk (ui_canvas + samples).
const partCount = Math.max(parts, 3);
const chunkSize = Math.ceil(batchFiles.length / partCount);

const written = [];
for (let k = 0; k < partCount; k++) {
  const slice = batchFiles.slice(k * chunkSize, (k + 1) * chunkSize);
  if (!slice.length) continue;
  const fileSet = new Set(slice);
  const partNodes = nodes.filter((n) => fileSet.has(n.filePath));
  const partIds = new Set(partNodes.map((n) => n.id));
  const partEdges = edges.filter((e) => partIds.has(e.source));
  const failed = [];
  for (const e of partEdges) {
    if (!targetOk(e.target, partIds)) {
      failed.push(`${e.source} -> ${e.target} (${e.type})`);
    }
  }
  if (failed.length) {
    throw new Error(`part ${k + 1} validation failed:\n${failed.join("\n")}`);
  }
  const outPath = `${UA}/intermediate/batch-3-part-${k + 1}.json`;
  fs.writeFileSync(outPath, JSON.stringify({ nodes: partNodes, edges: partEdges }, null, 2));
  written.push({
    path: outPath,
    files: slice.length,
    nodes: partNodes.length,
    edges: partEdges.length,
  });
}

console.log(JSON.stringify({
  nodeCount,
  edgeCount,
  importEdges: importEdges.length,
  partCount: written.length,
  parts: written,
}, null, 2));
