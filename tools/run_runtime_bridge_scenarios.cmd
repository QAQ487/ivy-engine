@echo off
setlocal
cd /d "%~dp0.."
if "%~1"=="" (
    echo usage: tools\run_runtime_bridge_scenarios.cmd ^<out-dir^> [exe]
    echo   without exe: validate-only all testbed\scenarios\*.json
    echo   with exe: live-run each scenario into ^<out-dir^>\<name^>
    exit /b 2
)
if not exist "%~1" mkdir "%~1"

for %%F in (testbed\scenarios\*.json) do (
    echo ==> validate %%~nxF
    py -3 tools\runtime_bridge_scenario_runner.py --scenario "%%F" --validate-only
    if errorlevel 1 exit /b 1
)

if "%~2"=="" goto :pass

for %%F in (testbed\scenarios\*.json) do (
    echo ==> run %%~nxF
    py -3 tools\runtime_bridge_scenario_runner.py --exe "%~2" --scenario "%%F" --out-dir "%~1\%%~nF"
    if errorlevel 1 exit /b 1
)

:pass
echo PASS runtime bridge scenarios
exit /b 0
