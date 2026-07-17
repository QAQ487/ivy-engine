/**===========================================================================

 File   :  [sprite.h]

 Author : Hayato Otsuki
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef SPRITE_H
#define SPRITE_H

#include <d3d11.h>
#include <DirectXMath.h>

void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Sprite_Finalize(void);

/**---------------------------------------------------------------------------
 ＜スプライト描画処理＞
 * 引数　：ID、描画位置 X・Y、幅、高さ、テクスチャ左上座標 X・Y、切り出し幅・高さ、回転角度（ラジアン）、色
 ---------------------------------------------------------------------------**/
void Sprite_Draw(int texid, float X, float Y, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});
void Sprite_Draw(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});
void Sprite_Draw(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, float angle, DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});

/**---------------------------------------------------------------------------
  Radial fill sprite drawing (for circular progress indicators)
 * Parameters: texture ID, position X/Y, width, height, texture coords,
 *             rotation angle, fill amount (0.0-1.0), start angle in degrees,
 *             clockwise flag, color
 ---------------------------------------------------------------------------**/
void Sprite_DrawRadialFill(int texid, float X, float Y, float width, float height,
    int tx, int ty, int tw, int th, float angle,
    float fillAmount, float startAngle, bool clockwise,
    DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f});

#endif // !SPRITE_H