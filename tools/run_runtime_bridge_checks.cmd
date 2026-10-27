@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if "%~1"=="" exit /b 2
if not exist "%~1" mkdir "%~1"
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_protocol_check.cpp engine\testing\runtime_test_bridge.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_protocol_check.exe" /link advapi32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_protocol_check.exe"
if errorlevel 1 exit /b 1
set IVY_BRIDGE_HARNESS=%CD%\%~1\runtime_bridge_protocol_check.exe
py -3 tools\runtime_test_client_check.py -v
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /MT /DGAME_RUNTIME /I. tools\runtime_bridge_release_check.cpp engine\testing\runtime_test_bridge.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_release_check.exe"
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_release_check.exe"
exit /b %errorlevel%
