@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if "%~1"=="" exit /b 2
if not exist "%~1" mkdir "%~1"
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_protocol_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\mouse.cpp engine\input\controller.cpp tools\test_renderer_stubs.cpp engine\input\key_logger.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_protocol_check.exe" /link advapi32.lib user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_protocol_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_keyboard_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\mouse.cpp engine\input\controller.cpp tools\test_renderer_stubs.cpp engine\input\key_logger.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_keyboard_check.exe" /link advapi32.lib user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_keyboard_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_phase3_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\mouse.cpp engine\input\controller.cpp tools\test_renderer_stubs.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_phase3_check.exe" /link advapi32.lib user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_phase3_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_mouse_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\mouse.cpp engine\input\controller.cpp tools\test_renderer_stubs.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_mouse_check.exe" /link advapi32.lib user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_mouse_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /I. tools\runtime_bridge_controller_axes_check.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_controller_axes_check.exe"
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_controller_axes_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /I. tools\runtime_bridge_controller_probe_check.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_controller_probe_check.exe"
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_controller_probe_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_controller_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\mouse.cpp engine\input\controller.cpp tools\test_renderer_stubs.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_controller_check.exe" /link advapi32.lib user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_controller_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_events_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\mouse.cpp engine\input\controller.cpp tools\test_renderer_stubs.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_events_check.exe" /link advapi32.lib user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_events_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_phase6_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\mouse.cpp engine\input\controller.cpp tools\test_renderer_stubs.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_phase6_check.exe" /link advapi32.lib user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_phase6_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /DGAME_RUNTIME /I. tools\runtime_bridge_world_manip_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\mouse.cpp engine\input\controller.cpp tools\test_renderer_stubs.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_world_manip_check.exe" /link advapi32.lib user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_world_manip_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /I. tests\runtime_test_scene_arg_test.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_test_scene_arg_check.exe"
if errorlevel 1 exit /b 1
"%~1\runtime_test_scene_arg_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /I. tools\runtime_bridge_checkpoint_check.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_checkpoint_check.exe"
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_checkpoint_check.exe"
if errorlevel 1 exit /b 1
set IVY_BRIDGE_HARNESS=%CD%\%~1\runtime_bridge_protocol_check.exe
py -3 tools\runtime_test_client_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_mouse_acceptance_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_controller_acceptance_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_controller_axes_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_controller_probe_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_controller_probe_acceptance_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_explore_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_phase4_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_world_manip_acceptance_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_angular_acceptance_check.py -v
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_scenario_check.py -v
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MT /DGAME_RUNTIME /I. tools\runtime_bridge_release_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\controller.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_release_check.exe" /link user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_release_check.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /std:c++20 /utf-8 /DNOMINMAX /MTd /D_DEBUG /I. tools\runtime_bridge_release_check.cpp engine\testing\runtime_test_bridge.cpp engine\input\keyboard.cpp engine\input\controller.cpp /Fo:"%~1\\" /Fe:"%~1\runtime_bridge_editor_check.exe" /link user32.lib
if errorlevel 1 exit /b 1
"%~1\runtime_bridge_editor_check.exe"
if errorlevel 1 exit /b 1
rem Live acceptances: avoid parenthesized blocks so `exit /b` propagates
rem correctly (cmd can fall through to a later exit /b 0 inside `(...)`).
if "%~2"=="" goto :done
py -3 tools\runtime_bridge_mouse_acceptance.py --exe "%~2" --out-dir "%~1\mouse-runtime"
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_controller_acceptance.py --exe "%~2" --out-dir "%~1\controller-runtime"
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_controller_probe_acceptance.py --exe "%~2" --out-dir "%~1\controller-probe-runtime"
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_world_manip_acceptance.py --exe "%~2" --out-dir "%~1\world-manip-runtime"
if errorlevel 1 exit /b 1
py -3 tools\runtime_bridge_angular_acceptance.py --exe "%~2" --out-dir "%~1\angular-runtime"
if errorlevel 1 exit /b 1
call tools\run_runtime_bridge_scenarios.cmd "%~1\scenarios-runtime" "%~2"
if errorlevel 1 exit /b 1
:done
exit /b 0
