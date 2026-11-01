# GUI build produces a playable package

File → Build Current Project does not only compile `IvyEngineRuntime.vcxproj`. It always MSBuilds the runtime host and the open content project's GameScripts, then assembles a playable package (host renamed to `<GameName>.exe`, `libfbxsdk.dll`, `project.json`, `rom/`, `GameScripts.dll`) into a chosen folder.

Compiling the host alone would ignore the open game. Compiling scripts alone is not a runtime-host build. Matching `deploy.bat` exactly would omit `project.json`, which the host already reads for audio buses. After copying the project `rom/`, both the GUI and `deploy.bat` overlay the `.cso` files built with this `IvyEngineRuntime` and the engine branding, then add engine defaults from `resources/` that the content names and does not already contain. `rom/skybox/MySkybox.dds` is added only when content names it. `deploy.bat` still copies `IvyEngineRuntime.exe` by that name so content projects can package without the editor.
