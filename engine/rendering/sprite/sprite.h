/**===========================================================================

 File   :  [sprite.h]

 Author : Terry Yeung
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
 ���X�v���C�g�`�揈����
 * �����@�FID�A�`��ʒu X�EY�A���A�����A�e�N�X�`��������W X�EY�A�؂�o�����E�����A��]�p�x�i���W�A���j�A�F
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


// World-space XY quad. UV rect is normalized (left, top, right, bottom).
void Sprite_DrawWorld(int texture, float width, float height, DirectX::XMFLOAT2 pivot,
    DirectX::XMFLOAT4 uv, DirectX::XMFLOAT4 color, const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);

#endif // !SPRITE_H
