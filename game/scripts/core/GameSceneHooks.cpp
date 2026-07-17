#include "LockOnUI.h"
#include "OffscreenEnemyIndicator.h"

extern "C" __declspec(dllexport) bool GameScene_Initialize() {
	const bool lockOnOk = LockOnUI::Initialize();
	const bool indicatorOk = OffscreenEnemyIndicator::Initialize();
	return lockOnOk && indicatorOk;
}

extern "C" __declspec(dllexport) void GameScene_Finalize() {
	OffscreenEnemyIndicator::Finalize();
	LockOnUI::Finalize();
}

extern "C" __declspec(dllexport) void GameScene_Update(double elapsed_time) {
	LockOnUI::Update(elapsed_time);
	OffscreenEnemyIndicator::Update(elapsed_time);
}

extern "C" __declspec(dllexport) void GameScene_PostBillboardDraw() {
	LockOnUI::Draw();
}

extern "C" __declspec(dllexport) void GameScene_PostUIDraw() {
	OffscreenEnemyIndicator::Draw();
}

extern "C" __declspec(dllexport) void GameScene_OnStopPlay() {
	LockOnUI::SetTarget(nullptr, LockOnTargetState::None);
	OffscreenEnemyIndicator::Reset();
}
