# Scene JSON hot-load

The Editor monitors only the currently loaded scene JSON. External changes are
polled every 500 ms and read after at least 300 ms of stability. Consecutive writes
are coalesced; identical content and Editor saves do not trigger another reload.
Polling and application pause during Play. The latest disk version is processed
after stopping Play. Standalone Runtime does not monitor scene files.

With no unsaved edits, a valid version replaces nodes, Canvas and scene settings
through the existing queued load/preload flow. The Editor camera and edit mode
remain; selection and Undo history are cleared. With unsaved edits, the modal
offers **Load disk version (discard Editor changes)** or **Keep Editor changes**.
Keeping edits ignores that disk version; another changed version can prompt again.

Parsing, settings validation and deserialization happen before old-world teardown.
Failed preparation retains the existing scene and turns the Scene JSON light red.
That content is not retried until the disk content changes. Temporary missing or
locked files are retried. This is whole-scene replacement, without gameplay state
preservation or dependency watching (prefabs, textures and other JSON files).

The toolbar's right edge has two lights: **Scene JSON** on the left, **Script DLL**
on the right. Hover for the name and detailed status. Green means watching, amber
means waiting/processing, red means a reload failed, and grey means paused/inactive
(including Play). Successful reloads briefly brighten green for 1.2 seconds.
Script errors reflect actual DLL load results; a successful later batch clears red.

## Validation

- `tools\run_scene_file_monitor_check.cmd`: isolated polling/debounce/content,
  self-save baseline, recreation, path reset and known-field type validation.
- `tools\run_scene_hot_load_check.ps1`: opt-in Debug Editor host check using its
  own temporary scene. Open an existing project once before running. Verifies
  failed preparation, actual file reload/recovery, nodes/Canvas/settings,
  camera/edit mode, selection/Undo, both conflict handlers and modal presence,
  Play deferral, self-save suppression, and fullscreen/viewport Play scene switches
  using Canvas requests and the default load API. Also verifies Scene/Script lights,
  a missing-DLL failure, and recovery via the normal script reload queue. Requires
  a valid script DLL for the recovery check. Restores the original scene on success.
- `tools\run_play_scene_switch_check.ps1`: existing viewport/fullscreen transition
  check; requires TestProject with `scene_topdown_2d.json` as the startup scene.

The host checks inspect application state and rendered modal presence. They do
not replace manual visual acceptance of the toolbar and conflict dialog.
