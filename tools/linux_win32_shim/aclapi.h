// Stub aclapi.h for Linux bridge protocol checks (pipe DACL tests are Windows-only).
#pragma once
#include "windows.h"

#define SE_KERNEL_OBJECT 6
#define DACL_SECURITY_INFORMATION 0x00000004
#define ACCESS_ALLOWED_ACE_TYPE 0

struct ACE_HEADER {
    BYTE AceType;
    BYTE AceFlags;
    WORD AceSize;
};
struct ACCESS_ALLOWED_ACE {
    ACE_HEADER Header;
    DWORD Mask;
    DWORD SidStart;
};
struct ACL {
    BYTE AclRevision;
    BYTE Sbz1;
    WORD AclSize;
    WORD AceCount;
    WORD Sbz2;
};
using PACL = ACL*;

inline DWORD GetSecurityInfo(HANDLE, int, DWORD, void*, void*, PACL*, void*, PSECURITY_DESCRIPTOR*) {
    return 1;
}
inline BOOL GetAce(PACL, DWORD, void**) { return FALSE; }
inline BOOL EqualSid(PSID, PSID) { return FALSE; }
inline BOOL equalSid(PSID a, PSID b) { return EqualSid(a, b); }
