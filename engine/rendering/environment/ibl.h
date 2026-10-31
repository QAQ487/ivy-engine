// ibl.h
// Rendering: ibl declarations.

#pragma once

#include "engine/rendering/d3d11/shader.h"

bool Ibl_Initialize();
void Ibl_Finalize();
void Ibl_ReloadFromSettings();
void Ibl_Bind(Shader3DVariant variant);
bool Ibl_IsReady();
float Ibl_Intensity();
