@echo off
setlocal

set ROOT=%~dp0..
if "%ENGINE_ROOT%"=="" set ENGINE_ROOT=C:\path\to\MagneticField_Prototype
set CONFIG=Debug
if not "%1"=="" set CONFIG=%1

set OUT=%ROOT%\deploy
set ENGINE_OUT=%ENGINE_ROOT%\x64\%CONFIG%

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\game\scripts\bin" mkdir "%OUT%\game\scripts\bin"
if not exist "%OUT%\rom" mkdir "%OUT%\rom"

copy /Y "%ENGINE_OUT%\Game_Runtime.exe" "%OUT%\"
copy /Y "%ENGINE_OUT%\libfbxsdk.dll" "%OUT%\"
copy /Y "%ROOT%\game\scripts\bin\GameScripts.dll" "%OUT%\game\scripts\bin\"
if exist "%ROOT%\game\scripts\bin\GameScripts.pdb" copy /Y "%ROOT%\game\scripts\bin\GameScripts.pdb" "%OUT%\game\scripts\bin\"
xcopy /Y /E /I "%ROOT%\rom\*" "%OUT%\rom\"
if exist "%ENGINE_OUT%\rom\branding" (
  if not exist "%OUT%\rom\branding" mkdir "%OUT%\rom\branding"
  xcopy /Y /E /I "%ENGINE_OUT%\rom\branding\*" "%OUT%\rom\branding\"
)

echo Deploy complete: %OUT%
endlocal
