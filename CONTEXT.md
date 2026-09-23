# Ivy Engine

The 3D runtime and ImGui editor that content projects load and play.

## Language

**AudioManager**:
The engine singleton that owns the audio device and bus volumes.
_Avoid_: Audio Mixer, old audio manager, sound system

**Bus**:
A volume category a playing sound belongs to: Master, BGM, SFX, or Voice.
_Avoid_: mixer group, channel, track

**Audio**:
A scene component that plays one wav on a GameObject. Each Audio has its own playback and belongs to a bus.
_Avoid_: AudioSource, sound component, SFX component, voice (that is a bus)

**AudioListener**:
A scene component that is the 3D ear. It lives on a camera.
_Avoid_: listener object, SetListener as the authoring model

**Play mode**:
Editor playback of the loaded scene (F3), using the same components as runtime.
_Avoid_: preview, simulate

**Content project**:
A directory that owns one game's assets, scripts, and `project.json`. The editor has at most one open.
_Avoid_: project (unqualified), Game_Runtime, Game_Project (legacy VS names)

**Runtime host**:
The editor-less executable that plays a content project (`IvyEngineRuntime.exe` in engine output; deployed as `<GameName>.exe` via GUI packaging).
_Avoid_: treating the runtime host as a content project, runtime project

**Playable package**:
A folder that contains the runtime host, `project.json`, one content project's rom, and that project's scripts, in the layout the host can launch.
_Avoid_: build folder, export, ship

**UI click sound**:
Project-wide SFX played when a UIButton activates. Path lives in `project.json` as `audio.uiClickSound`. It keeps playing across a scene load so LoadScene buttons can still be heard.
_Avoid_: per-button clip, PlaySFX string-id for menu clicks

### Visuals

**Visual**:
The scene component that binds a mesh path and clips to a GameObject.
_Avoid_: ModelComponent, SkinnedMesh, model (when you mean the component), the Script that picks clips

**Clip**:
A named animation a Visual can play (idle, run, jump, catch, pickup, throw).
_Avoid_: animation (unqualified), skin animation, anim FBX as the component

**Anim instance**:
The per-Visual playback copy: its own clip time and deformed vertices. The mesh asset is shared; the instance is not.
_Avoid_: skinned animation model, cloned model, skinned model (when you mean the instance)

**Skin**:
A Visual's mesh bound to bones so a Clip can deform it. Skin is the bind, not the playback and not the Script.
_Avoid_: skin animation, GPU skin / CPU skin as the component name

**Bind**:
The rest pose of a Skin: the bone and mesh matrices that make `bindOffset`. If Bind is wrong, posed vertices scatter and do not form the mesh.
_Avoid_: T-pose (one authored Bind), deform (playback)

### Camera

**FirstPerson**:
Play camera mode that places the eye on a walkable camera rig (position + eye height, mouse/stick look). Parallel to ThirdPerson; the CameraDirector samples its pose when `CameraMode::FirstPerson` is active.
_Avoid_: hacking ThirdPerson distance to zero, treating SceneCamera as FPS

**ThirdPerson**:
Play camera mode that follows a `TAG_PLAYER` target at a distance (magnet Push/Pull/Orbit). Scripts attach via `ThirdPersonCamera_SetTarget`.
_Avoid_: follow cam, TPS as the only play camera

## Engine freeze

2026-09: engine frozen at `engine-v1.0-lock` for swing-demo development; additive changes require explicit unfreeze. ADR 0006 unfreezes only the first-person camera path (and its ScriptRuntimeApi appends).
