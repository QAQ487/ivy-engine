# Skybox source faces

Production source faces for the default cubemap. The runtime path is `rom/skybox/MySkybox.dds`, which resolves to `resources/skybox/MySkybox.dds` when a project does not override it.

## Rebuild

1. Install [DirectXTex](https://github.com/microsoft/DirectXTex) tools so `texassemble.exe` is on `PATH`.
2. From this directory, run `run.bat`.

`run.bat` writes `resources/skybox/MySkybox.dds` (six faces: `px`, `nx`, `py`, `ny`, `pz`, `nz`). This `source/` folder is authoring input. Packaging and the OutDir mirror skip it.

Do not commit `texassemble.exe` under `resources/` — it is ignored via `resources/**/*.exe`.
