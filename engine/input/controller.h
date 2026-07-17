/**===========================================================================
 File   :  [controller.h]
 Author : Hayato Otsuki
 Date   : 2025/11/5
-----------------------------------------------------------------------------
 Xbox/PC コントローラー入力（XInput）ヘッダー
 - 初期化/更新
 - 接続状態
 - ボタンの押下/トリガ/リリース
 - アナログ入力（スティック/トリガ）正規化＆デッドゾーン
 - バイブレーション
============================================================================**/
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <windows.h>
#include <DirectXMath.h>

// 監視する最大コントローラー数（XInput は最大4）
constexpr int CONTROLLER_MAX = 4;

// ボタン列挙（XINPUT_GAMEPAD_* に対応）
enum class ControllerButton : WORD {
    DPadUp = 0x0001, // XINPUT_GAMEPAD_DPAD_UP
    DPadDown = 0x0002, // XINPUT_GAMEPAD_DPAD_DOWN
    DPadLeft = 0x0004, // XINPUT_GAMEPAD_DPAD_LEFT
    DPadRight = 0x0008, // XINPUT_GAMEPAD_DPAD_RIGHT
    Start = 0x0010, // XINPUT_GAMEPAD_START
    Back = 0x0020, // XINPUT_GAMEPAD_BACK
    LeftThumb = 0x0040, // XINPUT_GAMEPAD_LEFT_THUMB
    RightThumb = 0x0080, // XINPUT_GAMEPAD_RIGHT_THUMB
	// LT = 0x0040,
	// RT = 0x0080,
	LB = 0x0100,
	RB = 0x0200,
    A = 0x1000, // XINPUT_GAMEPAD_A
    B = 0x2000, // XINPUT_GAMEPAD_B
    X = 0x4000, // XINPUT_GAMEPAD_X
    Y = 0x8000  // XINPUT_GAMEPAD_Y
};

// 初期化/終了
void Controller_Initialize();
void Controller_Finalize();

// 1フレーム更新（毎フレーム呼ぶ）
void Controller_Update();

// 接続状態
bool Controller_IsConnected(int padIndex = 0);

// ボタン状態
bool Controller_IsPressed(ControllerButton button, int padIndex = 0);
bool Controller_IsTrigger(ControllerButton button, int padIndex = 0);  // 今フレームで押された
bool Controller_IsRelease(ControllerButton button, int padIndex = 0);  // 今フレームで離された

// スティック（デッドゾーン適用・正規化済み）
// 戻り値: [-1,1] 範囲。X: 右正、Y: 上正（DirectXMath に合わせて上正）
DirectX::XMFLOAT2 Controller_GetLeftStick(int padIndex = 0);
DirectX::XMFLOAT2 Controller_GetRightStick(int padIndex = 0);

// トリガ（デッドゾーン適用・正規化済み）
// 戻り値: [0,1] 範囲
float Controller_GetLeftTrigger(int padIndex = 0);
float Controller_GetRightTrigger(int padIndex = 0);

// デッドゾーンの設定
void Controller_SetStickDeadzone(short left, short right);
void Controller_SetTriggerDeadzone(BYTE trigger);

// バイブレーション（0.0f〜1.0f）
void Controller_SetVibration(float leftMotor, float rightMotor, int padIndex = 0);

#endif // CONTROLLER_H