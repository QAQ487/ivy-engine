/**===========================================================================

 File   :  [main.h]

 Author : Hayato Otsuki
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef MAIN_H
#define MAIN_H

#pragma warning(push)
#pragma warning(disable:4005)

#define _CRT_SECURE_NO_WARNINGS // scanf のwarning防止
#include <stdio.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#define DIRECTINPUT_VERSION 0x0800 // 警告対処
#include "dinput.h"
#include "mmsystem.h"

#pragma warning(pop)

#include <DirectXMath.h>
using namespace DirectX;

/**---------------------------------------------------------------------------
  ＜マクロ定義＞
---------------------------------------------------------------------------**/
#define SCREEN_WIDTH	(1920) // ウインドウの幅
#define SCREEN_HEIGHT	(1080) // ウインドウの高さ

#define SCREEN_CENTER_X	(SCREEN_WIDTH / 2)  // 画面中心X座標
#define SCREEN_CENTER_Y	(SCREEN_HEIGHT / 2) // 画面中心Y座標

#define SCREEN_LEFT		(0)                 // 画面左端X座標
#define SCREEN_RIGHT	(SCREEN_WIDTH)      // 画面右端X座標
#define SCREEN_TOP		(0)                 // 画面上端Y座標
#define SCREEN_BOTTOM	(SCREEN_HEIGHT)     // 画面下端Y座標

/**---------------------------------------------------------------------------
  ＜プロトタイプ宣言＞
---------------------------------------------------------------------------**/

#endif // !MAIN_H
