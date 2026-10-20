@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "tools\physics2d-test-build\box2d" mkdir "tools\physics2d-test-build\box2d"
cl /nologo /c /TC /std:c17 /O2 /DBOX2D_DISABLE_SIMD /I libraries\box2d\include libraries\box2d\src\*.c /Fo:tools\physics2d-test-build\box2d\
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /O2 /I. /I libraries\box2d\include tools\physics2d_world_check.cpp engine\physics\2d\physics2d_world.cpp tools\physics2d-test-build\box2d\*.obj /Fo:tools\physics2d-test-build\ /Fe:tools\physics2d-test-build\physics2d_world_check.exe
if errorlevel 1 exit /b 1
tools\physics2d-test-build\physics2d_world_check.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /I. tools\scene_execution_policy_check.cpp /Fo:tools\physics2d-test-build\ /Fe:tools\physics2d-test-build\scene_execution_policy_check.exe
if errorlevel 1 exit /b 1
tools\physics2d-test-build\scene_execution_policy_check.exe
exit /b %errorlevel%
