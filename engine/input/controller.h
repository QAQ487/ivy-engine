/**===========================================================================
 File   :  [controller.h]
 Author : Terry Yeung
 Date   : 2025/11/5
-----------------------------------------------------------------------------
 Xbox/PC �R���g���[���[���́iXInput�j�w�b�_�[
 - ������/�X�V
 - �ڑ����
 - �{�^���̉���/�g���K/�����[�X
 - �A�i���O���́i�X�e�B�b�N/�g���K�j���K�����f�b�h�]�[��
 - �o�C�u���[�V����
============================================================================**/
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <windows.h>
#include <DirectXMath.h>

// �Ď�����ő�R���g���[���[���iXInput �͍ő�4�j
constexpr int CONTROLLER_MAX = 4;

// �{�^���񋓁iXINPUT_GAMEPAD_* �ɑΉ��j
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

// ������/�I��
void Controller_Initialize();
void Controller_Finalize();

// 1�t���[���X�V�i���t���[���Ăԁj
void Controller_Update();

// �ڑ����
bool Controller_IsConnected(int padIndex = 0);

// �{�^�����
bool Controller_IsPressed(ControllerButton button, int padIndex = 0);
bool Controller_IsTrigger(ControllerButton button, int padIndex = 0);  // ���t���[���ŉ����ꂽ
bool Controller_IsRelease(ControllerButton button, int padIndex = 0);  // ���t���[���ŗ����ꂽ

// �X�e�B�b�N�i�f�b�h�]�[���K�p�E���K���ς݁j
// �߂�l: [-1,1] �͈́BX: �E���AY: �㐳�iDirectXMath �ɍ��킹�ď㐳�j
DirectX::XMFLOAT2 Controller_GetLeftStick(int padIndex = 0);
DirectX::XMFLOAT2 Controller_GetRightStick(int padIndex = 0);

// �g���K�i�f�b�h�]�[���K�p�E���K���ς݁j
// �߂�l: [0,1] �͈�
float Controller_GetLeftTrigger(int padIndex = 0);
float Controller_GetRightTrigger(int padIndex = 0);

// �f�b�h�]�[���̐ݒ�
void Controller_SetStickDeadzone(short left, short right);
void Controller_SetTriggerDeadzone(BYTE trigger);

// �o�C�u���[�V�����i0.0f?1.0f�j
void Controller_SetVibration(float leftMotor, float rightMotor, int padIndex = 0);

#if defined(_DEBUG) && defined(GAME_RUNTIME)
void Controller_TestInjectLeftStick(float lx, float ly);
void Controller_ClearInjection();
bool Controller_IsInjectionActive();
#endif

#endif // CONTROLLER_H