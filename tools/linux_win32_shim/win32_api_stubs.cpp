// Win32 API stubs for Linux offline runtime-bridge protocol checks.
#include "windows.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <time.h>
#include <unistd.h>
#include <unordered_map>

namespace {
std::mutex g_envMu;
std::unordered_map<std::string, std::string> g_env;
DWORD g_lastError = 0;

ULONGLONG MonoMs() {
    using namespace std::chrono;
    return static_cast<ULONGLONG>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
}

ULONGLONG GetTickCount64() { return MonoMs(); }

DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size) {
    if (!name) return 0;
    std::lock_guard lock(g_envMu);
    if (const char* v = std::getenv(name)) {
        const DWORD n = static_cast<DWORD>(std::strlen(v));
        if (!buffer || size == 0) return n;
        if (n + 1 > size) return n + 1;
        std::memcpy(buffer, v, n + 1);
        return n;
    }
    auto it = g_env.find(name);
    if (it == g_env.end()) return 0;
    const DWORD n = static_cast<DWORD>(it->second.size());
    if (!buffer || size == 0) return n;
    if (n + 1 > size) return n + 1;
    std::memcpy(buffer, it->second.c_str(), n + 1);
    return n;
}

BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value) {
    if (!name) return FALSE;
    std::lock_guard lock(g_envMu);
    if (!value) g_env.erase(name);
    else g_env[name] = value;
    return TRUE;
}

DWORD GetCurrentProcessId() { return static_cast<DWORD>(::getpid()); }
HANDLE GetCurrentProcess() { return reinterpret_cast<HANDLE>(intptr_t{1}); }
BOOL ProcessIdToSessionId(DWORD, DWORD* session) {
    if (session) *session = 1;
    return TRUE;
}
BOOL OpenProcessToken(HANDLE, DWORD, HANDLE* token) {
    if (token) *token = reinterpret_cast<HANDLE>(intptr_t{2});
    return TRUE;
}
BOOL GetTokenInformation(HANDLE, int, void* info, DWORD size, DWORD* needed) {
    if (needed) *needed = sizeof(TOKEN_USER) + sizeof(SID);
    if (!info || size < sizeof(TOKEN_USER) + sizeof(SID)) return FALSE;
    auto* user = reinterpret_cast<TOKEN_USER*>(info);
    auto* sid = reinterpret_cast<SID*>(reinterpret_cast<unsigned char*>(info) + sizeof(TOKEN_USER));
    user->User.Sid = sid;
    user->User.Attributes = 0;
    return TRUE;
}
BOOL CloseHandle(HANDLE) { return TRUE; }
void Sleep(DWORD ms) {
    timespec ts{};
    ts.tv_sec = static_cast<time_t>(ms / 1000);
    ts.tv_nsec = static_cast<long>((ms % 1000) * 1000000L);
    nanosleep(&ts, nullptr);
}
void OutputDebugStringA(LPCSTR) {}

int WideCharToMultiByte(UINT codePage, DWORD, LPCWSTR wide, int wideCount,
    LPSTR out, int outBytes, LPCSTR, BOOL*)
{
    if (codePage != CP_UTF8 || !wide)
        return 0;
    // wideCount < 0 → null-terminated; otherwise character count (no forced NUL).
    size_t srcLen = 0;
    if (wideCount < 0) {
        while (wide[srcLen] != 0)
            ++srcLen;
    } else {
        srcLen = static_cast<size_t>(wideCount);
    }

    // Measure UTF-8 bytes (excluding a trailing NUL we add for -1 queries).
    size_t utf8Len = 0;
    for (size_t i = 0; i < srcLen; ++i) {
        const unsigned int cp = static_cast<unsigned int>(wide[i]);
        if (cp <= 0x7Fu) utf8Len += 1;
        else if (cp <= 0x7FFu) utf8Len += 2;
        else if (cp <= 0xFFFFu) utf8Len += 3;
        else utf8Len += 4;
    }
    const bool wantNul = (wideCount < 0);
    const size_t need = utf8Len + (wantNul ? 1 : 0);
    if (!out || outBytes == 0)
        return static_cast<int>(need);
    if (static_cast<size_t>(outBytes) < need)
        return 0;

    size_t o = 0;
    for (size_t i = 0; i < srcLen; ++i) {
        unsigned int cp = static_cast<unsigned int>(wide[i]);
        if (cp <= 0x7Fu) {
            out[o++] = static_cast<char>(cp);
        } else if (cp <= 0x7FFu) {
            out[o++] = static_cast<char>(0xC0u | (cp >> 6));
            out[o++] = static_cast<char>(0x80u | (cp & 0x3Fu));
        } else if (cp <= 0xFFFFu) {
            out[o++] = static_cast<char>(0xE0u | (cp >> 12));
            out[o++] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            out[o++] = static_cast<char>(0x80u | (cp & 0x3Fu));
        } else {
            out[o++] = static_cast<char>(0xF0u | (cp >> 18));
            out[o++] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
            out[o++] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            out[o++] = static_cast<char>(0x80u | (cp & 0x3Fu));
        }
    }
    if (wantNul)
        out[o] = '\0';
    return static_cast<int>(need);
}

HANDLE CreateEventW(void*, BOOL, BOOL, LPCWSTR) {
    return reinterpret_cast<HANDLE>(intptr_t{3});
}
BOOL SetEvent(HANDLE) { return TRUE; }
DWORD WaitForSingleObject(HANDLE, DWORD) { return WAIT_TIMEOUT; }
DWORD WaitForMultipleObjects(DWORD, const HANDLE*, BOOL, DWORD) { return WAIT_TIMEOUT; }

HANDLE CreateNamedPipeA(LPCSTR, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, SECURITY_ATTRIBUTES*) {
    g_lastError = 1;
    return INVALID_HANDLE_VALUE;
}
BOOL ConnectNamedPipe(HANDLE, OVERLAPPED*) { g_lastError = 1; return FALSE; }
BOOL DisconnectNamedPipe(HANDLE) { return TRUE; }
BOOL ReadFile(HANDLE, void*, DWORD, DWORD*, OVERLAPPED*) { g_lastError = 1; return FALSE; }
BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, OVERLAPPED*) { g_lastError = 1; return FALSE; }
BOOL GetOverlappedResult(HANDLE, OVERLAPPED*, DWORD*, BOOL) { return FALSE; }
BOOL CancelIoEx(HANDLE, OVERLAPPED*) { return TRUE; }
DWORD GetLastError() { return g_lastError; }
void SetLastError(DWORD err) { g_lastError = err; }

HANDLE CreateFileA(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) {
    g_lastError = 2;
    return INVALID_HANDLE_VALUE;
}

BOOL ConvertSidToStringSidA(PSID, LPSTR* stringSid) {
    if (!stringSid) return FALSE;
    *stringSid = static_cast<LPSTR>(std::malloc(8));
    if (!*stringSid) return FALSE;
    std::memcpy(*stringSid, "S-1-0-0", 8);
    return TRUE;
}
BOOL ConvertStringSecurityDescriptorToSecurityDescriptorA(LPCSTR, DWORD, PSECURITY_DESCRIPTOR* sd, DWORD*) {
    if (!sd) return FALSE;
    *sd = std::malloc(1);
    return *sd != nullptr;
}
void* LocalFree(void* p) { std::free(p); return nullptr; }

LPWSTR GetCommandLineW() {
    static wchar_t empty[] = L"";
    return empty;
}

LPWSTR* CommandLineToArgvW(LPCWSTR, int* argc) {
    if (argc) *argc = 0;
    return nullptr;
}
