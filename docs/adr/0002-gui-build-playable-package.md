# GUI build produces a playable package

File → Build Current Project does not only compile `IvyEngineRuntime.vcxproj`. It always MSBuilds the runtime host and the open content project's GameScripts, then assembles a playable package (host renamed to `<GameName>.exe`, `libfbxsdk.dll`, `project.json`, `rom/`, `GameScripts.dll`) into a chosen folder.

Compiling the host alone would ignore the open game. Compiling scripts alone is not a runtime-host build. Matching `deploy.bat` exactly would omit `project.json`, which the host already reads for audio buses. Content `rom/shader` is also a stale snapshot; after copying `rom/` the GUI overlays the `.cso` files built with this `IvyEngineRuntime` so GPU skin uses the current Bind. The GUI leaves `deploy.bat` as a simpler path (copies `IvyEngineRuntime.exe` by that name) so content projects can still package without the editor.
