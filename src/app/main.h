/**===========================================================================

 File   :  [main.h]

 Author : Terry Yeung
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef MAIN_H
#define MAIN_H

#pragma warning(push)
#pragma warning(disable:4005)

#define _CRT_SECURE_NO_WARNINGS // scanf ��warning�h�~
#include <stdio.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#define DIRECTINPUT_VERSION 0x0800 // �x���Ώ�
#include "dinput.h"
#include "mmsystem.h"

#pragma warning(pop)

#include <DirectXMath.h>
using namespace DirectX;

/**---------------------------------------------------------------------------
  ���}�N����`��
---------------------------------------------------------------------------**/
#define SCREEN_WIDTH	(1920) // �E�C���h�E�̕�
#define SCREEN_HEIGHT	(1080) // �E�C���h�E�̍���

#define SCREEN_CENTER_X	(SCREEN_WIDTH / 2)  // ��ʒ��SX���W
#define SCREEN_CENTER_Y	(SCREEN_HEIGHT / 2) // ��ʒ��SY���W

#define SCREEN_LEFT		(0)                 // ��ʍ��[X���W
#define SCREEN_RIGHT	(SCREEN_WIDTH)      // ��ʉE�[X���W
#define SCREEN_TOP		(0)                 // ��ʏ�[Y���W
#define SCREEN_BOTTOM	(SCREEN_HEIGHT)     // ��ʉ��[Y���W

/**---------------------------------------------------------------------------
  ���v���g�^�C�v�錾��
---------------------------------------------------------------------------**/

#endif // !MAIN_H
