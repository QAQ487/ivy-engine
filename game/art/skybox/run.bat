@echo off
setlocal
rem Rebuild rom/skybox/MySkybox.dds from face PNGs in this folder.
rem Requires DirectXTex "texassemble" on PATH (from DirectXTex tools / texconv package).
rem See README.md in this directory.

cd /d "%~dp0"
set "OUT=%~dp0..\..\..\rom\skybox\MySkybox.dds"

where texassemble >nul 2>&1
if errorlevel 1 (
  echo ERROR: texassemble not found on PATH.
  echo Install DirectXTex tools and add texassemble.exe to PATH, then re-run.
  echo https://github.com/microsoft/DirectXTex
  exit /b 1
)

texassemble cube -o "%OUT%" px.png nx.png py.png ny.png pz.png nz.png
if errorlevel 1 exit /b 1
echo Wrote %OUT%
endlocal
