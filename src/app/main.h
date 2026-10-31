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

#define _CRT_SECURE_NO_WARNINGS // Silence scanf warnings
#include <stdio.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#define DIRECTINPUT_VERSION 0x0800 // Suppress related warnings
#include "dinput.h"
#include "mmsystem.h"

#pragma warning(pop)

#include <DirectXMath.h>
using namespace DirectX;

/**---------------------------------------------------------------------------
  <Macro definitions>
---------------------------------------------------------------------------**/
#define SCREEN_WIDTH	(1920) // Window width
#define SCREEN_HEIGHT	(1080) // Window height

#define SCREEN_CENTER_X	(SCREEN_WIDTH / 2)  // Screen center X
#define SCREEN_CENTER_Y	(SCREEN_HEIGHT / 2) // Screen center Y

#define SCREEN_LEFT		(0)                 // Screen left X
#define SCREEN_RIGHT	(SCREEN_WIDTH)      // Screen right X
#define SCREEN_TOP		(0)                 // Screen top Y
#define SCREEN_BOTTOM	(SCREEN_HEIGHT)     // Screen bottom Y

/**---------------------------------------------------------------------------
  <Prototypes>
---------------------------------------------------------------------------**/

#endif // !MAIN_H
