/**===========================================================================

 File   :  [fade.cpp]

 Author : Terry Yeung
 Date   : 2025/9/19
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "fade.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/rendering/sprite/sprite.h"
#include "scene_manager.h"

static FADE fade;
// Whether to switch scene when fade-out completes
static bool s_doSceneTransition = false;
// Next scene (set on switch)
static SceneBase* s_nextScene = nullptr;

void Fade_Initialize() {
	fade.pos = { 0.0f, 0.0f };
	fade.size = { (float)GetBackBufferWidth(), (float)GetBackBufferHeight() };
	fade.alpha = 0.0f;
	fade.mode = MODE_FADE_NONE;
	fade.use = false;
	fade.color = { 0.0f, 0.0f, 0.0f, 1.0f };
	fade.texture_id = TextureLoad(L"rom/texture/fade.png", false);
	s_doSceneTransition = false;
	s_nextScene = nullptr;
}

void Fade_Finalize() {
	TextureSetTexture(fade.texture_id);
}

void Fade_Update() {
	if (!fade.use) return;

	switch (fade.mode) {
	case MODE_FADE_NONE: break;
	case MODE_FADE_OUT:
		fade.alpha += fade.speed;
		if (fade.alpha >= 1.0f) {
			fade.alpha = 1.0f;
			fade.mode = MODE_FADE_IN;

			// Fade-out done -> switch scene
			if (s_doSceneTransition && s_nextScene) {
				SceneManager::Instance().ChangeScene(s_nextScene);
				s_nextScene = nullptr;
			}
		}
		break;
	case MODE_FADE_IN:
		fade.alpha -= fade.speed;
		if (fade.alpha <= 0.0f) {
			fade.alpha = 0.0f;
			fade.mode = MODE_FADE_NONE;
			fade.use = false;
		}
		break;
	}
}

void Fade_Draw() {
	if (!fade.use) return;
	DirectX::XMFLOAT4 draw_color = fade.color;
	draw_color.w = fade.alpha;
	Sprite_Draw(fade.texture_id, fade.pos.x, fade.pos.y, fade.size.x, fade.size.y, 0, 0,
		(int)fade.size.x, (int)fade.size.y, draw_color);
}

// Start fade-out (set next scene externally if needed)
void Fade_SetOut(float r, float g, float b, float spd, bool changeScene) {
	if (fade.mode == MODE_FADE_NONE) {
		fade.speed = 1.0f / spd;
		fade.color = { r, g, b, 0.0f };
		fade.mode = MODE_FADE_OUT;
		fade.alpha = 0.0f;
		fade.use = true;
		s_doSceneTransition = changeScene;
		// Set s_nextScene externally (e.g. Fade_SetNextScene(new GameScene()))
	}
}

// Start fade-in
void Fade_SetIn(float spd) {
	if (fade.mode == MODE_FADE_NONE) {
		fade.speed = 1.0f / spd;
		fade.mode = MODE_FADE_IN;
		fade.alpha = 1.0f;
		fade.use = true;
	}
}

// Set next scene (switched after fade-out)
void Fade_SetNextScene(SceneBase* next) {
	s_nextScene = next;
}

// Query helpers
bool Fade_IsFadeEnd() { return fade.mode == MODE_FADE_NONE; }
bool Fade_IsOutEnd() { return fade.mode == MODE_FADE_IN && fade.alpha >= 1.0f; }
bool Fade_IsInEnd() { return fade.mode == MODE_FADE_NONE && fade.alpha <= 0.0f; }
bool Fade_IsFading() { return fade.use; }