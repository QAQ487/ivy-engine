#include "camera.h"
#include <cassert>
#include <cmath>

int main() {
    Camera camera;
    DirectX::XMFLOAT4X4 matrix;
    DirectX::XMStoreFloat4x4(&matrix, camera.GetProjMatrix(1600,900));
    assert(matrix._34 == 1.0f); // Existing scenes remain perspective.
    camera.orthographic = true;
    camera.orthographicHeight = 10;
    DirectX::XMStoreFloat4x4(&matrix,camera.GetProjMatrix(1600,800));
    assert(std::fabs(matrix._11 - 0.1f) < 1e-6f);
    assert(std::fabs(matrix._22 - 0.2f) < 1e-6f);
    assert(matrix._34 == 0 && matrix._44 == 1);
    DirectX::XMStoreFloat4x4(&matrix,camera.GetProjMatrix(800,1600));
    assert(std::fabs(matrix._11 - 0.4f) < 1e-6f);
    camera.orthographicHeight = 0;
    DirectX::XMStoreFloat4x4(&matrix,camera.GetProjMatrix(0,0));
    assert(std::isfinite(matrix._11) && std::isfinite(matrix._22));
}
