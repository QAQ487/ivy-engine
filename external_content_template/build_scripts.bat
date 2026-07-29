@echo off
setlocal
set ROOT=%~dp0
set CONFIG=Debug
if not "%1"=="" set CONFIG=%1

set MSBUILD=
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i

if "%MSBUILD%"=="" (
  echo MSBuild not found. Open the project .sln in Visual Studio instead.
  exit /b 1
)

echo Building GameScripts ^| %CONFIG% ^| x64
"%MSBUILD%" "%ROOT%game\scripts\GameScripts\GameScripts.vcxproj" /p:Configuration=%CONFIG% /p:Platform=x64 /v:m
exit /b %ERRORLEVEL%
