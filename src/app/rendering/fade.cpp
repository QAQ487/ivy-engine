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
// �t�F�[�h�A�E�g�������ɃV�[���؂�ւ����邩
static bool s_doSceneTransition = false;
// ���̃V�[���i�؂�ւ����ɃZ�b�g�j
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

			// �t�F�[�h�A�E�g����->�V�[���؂�ւ�
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

// �t�F�[�h�A�E�g�J�n�i�K�v�Ȃ玟�V�[����O������Z�b�g�j
void Fade_SetOut(float r, float g, float b, float spd, bool changeScene) {
	if (fade.mode == MODE_FADE_NONE) {
		fade.speed = 1.0f / spd;
		fade.color = { r, g, b, 0.0f };
		fade.mode = MODE_FADE_OUT;
		fade.alpha = 0.0f;
		fade.use = true;
		s_doSceneTransition = changeScene;
		// s_nextScene�͊O������Z�b�g���邱�Ɓi��FFade_SetNextScene(new GameScene()))
	}
}

// �t�F�[�h�C���J�n
void Fade_SetIn(float spd) {
	if (fade.mode == MODE_FADE_NONE) {
		fade.speed = 1.0f / spd;
		fade.mode = MODE_FADE_IN;
		fade.alpha = 1.0f;
		fade.use = true;
	}
}

// ���V�[���ݒ�i�t�F�[�h�A�E�g��ɐ؂�ւ���j
void Fade_SetNextScene(SceneBase* next) {
	s_nextScene = next;
}

// ����n
bool Fade_IsFadeEnd() { return fade.mode == MODE_FADE_NONE; }
bool Fade_IsOutEnd() { return fade.mode == MODE_FADE_IN && fade.alpha >= 1.0f; }
bool Fade_IsInEnd() { return fade.mode == MODE_FADE_NONE && fade.alpha <= 0.0f; }
bool Fade_IsFading() { return fade.use; }