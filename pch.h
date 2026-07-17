// pch.h: stable headers precompiled once per build.
// Avoid adding headers that change frequently.

#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using std::min;
using std::max;

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
using namespace DirectX;

#include <fbxsdk.h>
#include <Effekseer.h>
#include <EffekseerRendererDX11.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "imguizmo/ImGuizmo.h"

#include "main.h"

// engine/core
#include "engine/core/easing.h"
#include "engine/core/system_timer.h"
#include "engine/core/engine_gui.h"

// engine/scripting
#include "engine/scripting/script_runtime_api.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_factory.h"
#include "engine/scripting/script_watcher.h"

// engine/rendering
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/rendering/sprite/sprite.h"
#include "engine/rendering/model/model_util.h"
#include "engine/rendering/effect/effect_system.h"
#include "engine/rendering/lighting/lighting.h"
#include "engine/rendering/lighting/shadow.h"

#endif // PCH_H
