@echo off
setlocal
cd /d "%~dp0.."
rem Removal-light policy (no D3D) is also runnable on Linux:
rem   ./tools/run_content_removal_policy_check.sh
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist tools\phase6-test-build mkdir tools\phase6-test-build
cl /nologo /EHsc /std:c++17 /utf-8 /DNOMINMAX /I. tests\content_removal_policy_test.cpp /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\content_removal_policy_test.exe
if errorlevel 1 exit /b 1
tools\phase6-test-build\content_removal_policy_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /DNOMINMAX /I. tools\content_monitor_check.cpp /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\content_monitor_check.exe
if errorlevel 1 exit /b 1
tools\phase6-test-build\content_monitor_check.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /DNOMINMAX /std:c++17 /MTd /I. /Isrc\app /Isrc\app\camera /Ilibraries\directxtex tools\texture_reload_check.cpp engine\rendering\sprite\texture.cpp engine\ui\ui_font.cpp /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\texture_reload_check.exe /link /LIBPATH:libraries\directxtex DirectXTex_Debug.lib d3d11.lib dxgi.lib windowscodecs.lib ole32.lib user32.lib
if errorlevel 1 exit /b 1
tools\phase6-test-build\texture_reload_check.exe
exit /b %errorlevel%
