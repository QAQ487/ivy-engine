/**===========================================================================
 File   :  [string_utils.h]
 Description: Shared string conversion utilities.
              Previously ToWString / ToWideString was duplicated in
              map_manager.cpp, prefab_proxy_loader.cpp, and visual_component.h.
              Include this header instead of copy-pasting.
 ===========================================================================**/
#pragma once
#include <string>
#include <Windows.h>

// Convert a UTF-8 / ANSI narrow string to wstring for Win32 / DirectX APIs.
inline std::wstring ToWString(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (needed <= 0) return {};
    std::wstring w(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), needed);
    // MultiByteToWideChar appends a null terminator; remove it from the std::wstring.
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
