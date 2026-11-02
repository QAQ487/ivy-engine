// Minimal DirectXMath stand-in for Linux offline bridge protocol checks.
#pragma once

#include <cmath>

namespace DirectX {

struct XMFLOAT2 { float x = 0.0f, y = 0.0f; };
struct XMFLOAT3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct XMFLOAT4 { float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f; };

struct XMVECTOR { float f[4]; };
struct XMMATRIX { XMVECTOR r[4]; };

inline float XMConvertToDegrees(float r) { return r * 57.2957795f; }
inline float XMConvertToRadians(float d) { return d * 0.0174532925f; }
inline XMVECTOR XMVectorSet(float x, float y, float z, float w) { return XMVECTOR{{x, y, z, w}}; }
inline XMVECTOR XMQuaternionNormalize(XMVECTOR q) {
    const float len = std::sqrt(q.f[0]*q.f[0]+q.f[1]*q.f[1]+q.f[2]*q.f[2]+q.f[3]*q.f[3]);
    if (len > 0.0f) { q.f[0]/=len; q.f[1]/=len; q.f[2]/=len; q.f[3]/=len; }
    return q;
}
inline XMMATRIX XMMatrixRotationQuaternion(XMVECTOR) {
    XMMATRIX m{};
    m.r[0] = XMVectorSet(1,0,0,0);
    m.r[1] = XMVectorSet(0,1,0,0);
    m.r[2] = XMVectorSet(0,0,1,0);
    m.r[3] = XMVectorSet(0,0,0,1);
    return m;
}

} // namespace DirectX
