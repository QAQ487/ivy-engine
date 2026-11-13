// Minimal shellapi surface for Linux Editor-host bridge compile checks.
#pragma once

#include "windows.h"

// Declared for link against win32_api_stubs.cpp on Linux.
LPWSTR* CommandLineToArgvW(LPCWSTR cmdLine, int* argc);
LPWSTR GetCommandLineW();
