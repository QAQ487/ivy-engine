/**===========================================================================
 File   : line_sprite.h
 Brief  : Camera-facing stretched quad between two world points (rope/trail).
          Reuses 2D sprite shaders + white texture; no new shader files.
 ===========================================================================**/
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

void LineSprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void LineSprite_Finalize();

// main thread only. Call during 3D render pass (e.g. GameScene_PostBillboardDraw).
// Depth-tested (unlike Sprite_Draw). White texture + tint; no per-call allocation.
void LineSprite_Draw(
	const DirectX::XMFLOAT3& a,
	const DirectX::XMFLOAT3& b,
	float thicknessWorld,
	const DirectX::XMFLOAT4& color);
