@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "tools\phase6-test-build" mkdir "tools\phase6-test-build"
cl /nologo /EHsc /std:c++17 /utf-8 /DNOMINMAX /I. tools\scene_file_monitor_check.cpp /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\scene_file_monitor_check.exe
if errorlevel 1 exit /b 1
tools\phase6-test-build\scene_file_monitor_check.exe
exit /b %errorlevel%
