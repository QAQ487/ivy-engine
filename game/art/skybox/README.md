# Skybox source faces

Production source faces for the default cubemap. Runtime loads the assembled DDS at `rom/skybox/MySkybox.dds`.

## Rebuild

1. Install [DirectXTex](https://github.com/microsoft/DirectXTex) tools so `texassemble.exe` is on `PATH`.
2. From this directory, run `run.bat`.

`run.bat` writes `rom/skybox/MySkybox.dds` (six faces: `px`, `nx`, `py`, `ny`, `pz`, `nz`).

Do not commit `texassemble.exe` under `rom/` — it is ignored via `rom/**/*.exe`.
