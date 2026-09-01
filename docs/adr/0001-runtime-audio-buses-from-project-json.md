# Runtime applies audio buses from project.json

Game_Runtime does not open the editor project, so settings like gravity in `project.json` never reach a shipped build. Audio bus volumes (Master, BGM, SFX, Voice) would have the same gap if they lived only in Project Settings.

We load those four bus levels from `project.json` at runtime boot as well as in the editor, so a mix set in Project Settings → Audio is what players hear. The same `audio` object also carries `uiClickSound` (the UIButton click WAV). We are not using this as a reason to ship every other project setting (gravity stays editor-only until that is a separate decision).
