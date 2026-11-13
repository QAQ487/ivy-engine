# Audio

## Scene components

Registered types: `Audio` → `api:AudioComponent`, `AudioListener` → `api:AudioListenerComponent`.

Script helpers:

- `api:ScriptApi_FindAudioComponent`
- `api:ScriptApi_Audio_Play` / `api:ScriptApi_Audio_Stop`
- `api:ScriptApi_AudioSetMasterVolume` / `api:ScriptApi_AudioGetMasterVolume`

## Engine singleton

`api:AudioManager` (`engine/audio/audio_manager.h`) owns devices and buses (`api:AudioType`: BGM / SFX / Voice / …). Prefer component + `ScriptApi_*` from gameplay scripts unless you need bus-level APIs (`PlayBGM`, `PlaySFX`, `SetMasterVolume`, …).

Listener sync helpers: `api:AudioListenerSync_Tick`, `api:AudioListenerSync_EnsureOnCamera` in `audio_listener_sync.h`.
