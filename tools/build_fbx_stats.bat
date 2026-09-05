@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d "%~dp0\.."
if not exist tools\bin mkdir tools\bin
cl /nologo /EHsc /std:c++17 /I libraries\fbxsdk\include tools\fbx_stats.cpp /Fotools\bin\ /Fetools\bin\fbx_stats.exe /link /LIBPATH:libraries\fbxsdk\lib\x64\debug libfbxsdk.lib
if errorlevel 1 exit /b 1
copy /Y libraries\fbxsdk\dll\Debug\libfbxsdk.dll tools\bin\
echo BUILD_OK
