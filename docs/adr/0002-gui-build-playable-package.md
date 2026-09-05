# GUI build produces a playable package

File → Build Current Project does not only compile `Game_Runtime.vcxproj`. It always MSBuilds the runtime host and the open content project's GameScripts, then assembles a playable package (host, `libfbxsdk.dll`, `project.json`, `rom/`, `GameScripts.dll`) into a chosen folder.

Compiling the host alone would ignore the open game. Compiling scripts alone is not a runtime-host build. Matching `deploy.bat` exactly would omit `project.json`, which the host already reads for audio buses. The GUI leaves `deploy.bat` unchanged so content projects can still package without the editor.
