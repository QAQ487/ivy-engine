@echo off
setlocal

set ROOT=%~dp0..
if "%ENGINE_ROOT%"=="" set ENGINE_ROOT=C:\path\to\ivy-engine
set CONFIG=Debug
if not "%1"=="" set CONFIG=%1

set OUT=%ROOT%\deploy
set ENGINE_OUT=%ENGINE_ROOT%\x64\%CONFIG%

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\game\scripts\bin" mkdir "%OUT%\game\scripts\bin"
if not exist "%OUT%\rom" mkdir "%OUT%\rom"

copy /Y "%ENGINE_OUT%\IvyEngineRuntime.exe" "%OUT%\"
copy /Y "%ENGINE_OUT%\libfbxsdk.dll" "%OUT%\"
copy /Y "%ROOT%\game\scripts\bin\GameScripts.dll" "%OUT%\game\scripts\bin\"
if exist "%ROOT%\game\scripts\bin\GameScripts.pdb" copy /Y "%ROOT%\game\scripts\bin\GameScripts.pdb" "%OUT%\game\scripts\bin\"
xcopy /Y /E /I "%ROOT%\rom\*" "%OUT%\rom\"
if exist "%ENGINE_OUT%\resources\shader" (
  if not exist "%OUT%\rom\shader" mkdir "%OUT%\rom\shader"
  xcopy /Y /E /I "%ENGINE_OUT%\resources\shader\*" "%OUT%\rom\shader\"
) else if exist "%ENGINE_ROOT%\resources\shader" (
  if not exist "%OUT%\rom\shader" mkdir "%OUT%\rom\shader"
  xcopy /Y /E /I "%ENGINE_ROOT%\resources\shader\*" "%OUT%\rom\shader\"
)
set "PY=python"
where py >nul 2>&1 && set "PY=py -3"
%PY% "%ENGINE_ROOT%\tools\materialize_engine_defaults.py" "%OUT%\rom" "%ENGINE_ROOT%\resources" "%OUT%\rom"
if errorlevel 1 exit /b 1

echo Deploy complete: %OUT%
endlocal
