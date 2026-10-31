// skybox.h
// Rendering: skybox declarations.

#pragma once

#include <DirectXMath.h>

bool Skybox_Initialize();
void Skybox_Finalize();

void Skybox_ReloadFromSettings();
void Skybox_Draw(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);
