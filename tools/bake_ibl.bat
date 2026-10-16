@echo off
cd /d "%~dp0\.."
python tools\bake_ibl.py %*
if errorlevel 1 exit /b 1
echo BAKE_OK
