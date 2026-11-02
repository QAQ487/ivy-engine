// Minimal Win32 surface for compiling Debug GAME_RUNTIME bridge protocol
// checks on Linux. Not a general Windows compatibility layer.
#pragma once

#include <cstdint>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif

using BOOL = int;
using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned int;
using UINT = unsigned int;
using ULONG = unsigned long;
using ULONGLONG = unsigned long long;
using LONG = int;
using HANDLE = void*;
using HWND = void*;
using HMODULE = void*;
using HINSTANCE = void*;
using LPSTR = char*;
using LPCSTR = const char*;
using LPWSTR = wchar_t*;
using LPCWSTR = const wchar_t*;
using LPVOID = void*;
using PVOID = void*;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LRESULT = intptr_t;
using DWORD_PTR = uintptr_t;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#endif
#ifndef INFINITE
#define INFINITE 0xFFFFFFFFu
#endif

#define WINAPI
#define CALLBACK
#define DECLSPEC_IMPORT
#define WINUSERAPI
#define WINBASEAPI

#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define READ_CONTROL 0x00020000u
#define OPEN_EXISTING 3
#define FILE_FLAG_OVERLAPPED 0x40000000u
#define FILE_FLAG_FIRST_PIPE_INSTANCE 0x00080000u
#define PIPE_ACCESS_DUPLEX 0x00000003u
#define PIPE_TYPE_BYTE 0x00000000u
#define PIPE_READMODE_BYTE 0x00000000u
#define PIPE_WAIT 0x00000000u
#define PIPE_REJECT_REMOTE_CLIENTS 0x00000008u
#define ERROR_IO_PENDING 997
#define ERROR_PIPE_CONNECTED 535
#define ERROR_SUCCESS 0
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define TOKEN_QUERY 0x0008
#define TokenUser 1
#define SDDL_REVISION_1 1

struct OVERLAPPED {
    uintptr_t Internal = 0;
    uintptr_t InternalHigh = 0;
    DWORD Offset = 0;
    DWORD OffsetHigh = 0;
    HANDLE hEvent = nullptr;
};

struct SECURITY_ATTRIBUTES {
    DWORD nLength = 0;
    void* lpSecurityDescriptor = nullptr;
    BOOL bInheritHandle = FALSE;
};

struct SID {
    BYTE Value[68]{};
};
using PSID = SID*;

struct TOKEN_USER {
    struct { PSID Sid; DWORD Attributes; } User;
};

using PSECURITY_DESCRIPTOR = void*;
using LPSECURITY_ATTRIBUTES = SECURITY_ATTRIBUTES*;

inline void ZeroMemory(void* dest, size_t len) { if (dest && len) std::memset(dest, 0, len); }

ULONGLONG GetTickCount64();
DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size);
BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value);
DWORD GetCurrentProcessId();
HANDLE GetCurrentProcess();
BOOL ProcessIdToSessionId(DWORD pid, DWORD* session);
BOOL OpenProcessToken(HANDLE process, DWORD access, HANDLE* token);
BOOL GetTokenInformation(HANDLE token, int infoClass, void* info, DWORD size, DWORD* needed);
BOOL CloseHandle(HANDLE h);
void Sleep(DWORD ms);
void OutputDebugStringA(LPCSTR msg);

HANDLE CreateEventW(void* attrs, BOOL manual, BOOL initial, LPCWSTR name);
BOOL SetEvent(HANDLE h);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD ms);

HANDLE CreateNamedPipeA(LPCSTR name, DWORD openMode, DWORD pipeMode, DWORD maxInstances,
    DWORD outBuf, DWORD inBuf, DWORD defaultTimeout, SECURITY_ATTRIBUTES* security);
BOOL ConnectNamedPipe(HANDLE pipe, OVERLAPPED* ov);
BOOL DisconnectNamedPipe(HANDLE pipe);
BOOL ReadFile(HANDLE h, void* buf, DWORD n, DWORD* read, OVERLAPPED* ov);
BOOL WriteFile(HANDLE h, const void* buf, DWORD n, DWORD* written, OVERLAPPED* ov);
BOOL GetOverlappedResult(HANDLE h, OVERLAPPED* ov, DWORD* transferred, BOOL wait);
BOOL CancelIoEx(HANDLE h, OVERLAPPED* ov);
DWORD GetLastError();
void SetLastError(DWORD err);

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, void* security, DWORD creation,
    DWORD flags, HANDLE templateFile);

BOOL ConvertSidToStringSidA(PSID sid, LPSTR* stringSid);
BOOL ConvertStringSecurityDescriptorToSecurityDescriptorA(LPCSTR sddl, DWORD rev,
    PSECURITY_DESCRIPTOR* sd, DWORD* size);
void* LocalFree(void* p);

struct POINT { LONG x; LONG y; };
struct RECT { LONG left; LONG top; LONG right; LONG bottom; };
