@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "tools\collider-components-test-build" mkdir "tools\collider-components-test-build"
cl /nologo /EHsc /std:c++17 /utf-8 /GL /O2 /Gw /Gy /Zc:inline /wd4828 /I. /Isrc\app /Isrc\app\camera /Isrc\app\rendering /Isrc\app\scene /Ilibraries /Ilibraries\directxtex /Ilibraries\fbxsdk\include tools\collider_components_check.cpp engine\ecs\collider_component_serialization.cpp engine\ecs\component_factory.cpp /Fo:tools\collider-components-test-build\ /Fe:tools\collider-components-test-build\collider_components_check.exe /link /LTCG /OPT:REF
if errorlevel 1 exit /b 1
tools\collider-components-test-build\collider_components_check.exe
exit /b %errorlevel%
