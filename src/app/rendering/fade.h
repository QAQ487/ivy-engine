/**===========================================================================

 File   :  [fade.h]

 Author : Terry Yeung
 Date   : 2025/9/19
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef FADE_H
#define FADE_H

#include <DirectXMath.h>
 // #include "direct3d.h"
#include "engine/rendering/d3d11/renderer.h"
#include "scene_manager.h" // シーン切り替え用

enum FADE_MODE {
	MODE_FADE_NONE = 0,
	MODE_FADE_OUT,
	MODE_FADE_IN,
};

struct FADE {
	DirectX::XMFLOAT2 pos;
	DirectX::XMFLOAT2 size;
	DirectX::XMFLOAT4 color; // r,g,b,a
	float alpha;
	float speed;
	FADE_MODE mode;
	bool use;
	int texture_id;
};

void Fade_Initialize();
void Fade_Finalize();
void Fade_Update();
void Fade_Draw();

// 外部操作
void Fade_SetOut(float r, float g, float b, float spd, bool changeScene = true);
void Fade_SetIn(float spd);
void Fade_SetNextScene(SceneBase* next);

// フェード完了判定
bool Fade_IsFadeEnd(); // trueならフェード中ではない
bool Fade_IsOutEnd();  // trueならフェードアウト完了
bool Fade_IsInEnd();   // trueならフェードイン完了
bool Fade_IsFading();  // trueならフェード中

#endif // !FADE_H
