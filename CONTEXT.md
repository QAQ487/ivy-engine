# MagneticField Engine

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

**UI click sound**:
Project-wide SFX played when a UIButton activates. Path lives in `project.json` as `audio.uiClickSound`. It keeps playing across a scene load so LoadScene buttons can still be heard.
_Avoid_: per-button clip, PlaySFX string-id for menu clicks
