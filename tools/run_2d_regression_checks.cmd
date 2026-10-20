@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "tools\phase6-test-build" mkdir "tools\phase6-test-build"
cl /nologo /EHsc /std:c++17 /utf-8 /Gy /I. /Isrc\app\camera tools\sprite_animation_check.cpp  /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\sprite_animation_check.exe /link /OPT:REF
if errorlevel 1 exit /b 1
tools\phase6-test-build\sprite_animation_check.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /Gy /I. /Isrc\app\camera tools\sprite_pick_check.cpp  /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\sprite_pick_check.exe /link /OPT:REF
if errorlevel 1 exit /b 1
tools\phase6-test-build\sprite_pick_check.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /Gy /I. /Isrc\app\camera tools\ui_pivot_check.cpp  /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\ui_pivot_check.exe /link /OPT:REF
if errorlevel 1 exit /b 1
tools\phase6-test-build\ui_pivot_check.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /Gy /I. /Isrc\app\camera tools\scene_execution_policy_check.cpp  /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\scene_execution_policy_check.exe /link /OPT:REF
if errorlevel 1 exit /b 1
tools\phase6-test-build\scene_execution_policy_check.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /Gy /I. /Isrc\app\camera tools\camera_2d_check.cpp src\app\camera\camera.cpp /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\camera_2d_check.exe /link /OPT:REF
if errorlevel 1 exit /b 1
tools\phase6-test-build\camera_2d_check.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /Gy /I. /Isrc\app\camera tools\sprite_animator_check.cpp engine\ecs\sprite_animator_component.cpp /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\sprite_animator_check.exe /link /OPT:REF
if errorlevel 1 exit /b 1
tools\phase6-test-build\sprite_animator_check.exe
if errorlevel 1 exit /b 1
call tools\run_physics2d_check.cmd
if errorlevel 1 exit /b 1
call tools\run_collider_components_check.cmd
if errorlevel 1 exit /b 1
echo PASS: All 2D regression checks
