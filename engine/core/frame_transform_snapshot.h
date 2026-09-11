/**===========================================================================
 File   : frame_transform_snapshot.h
 Brief  : Per-frame read-only world pose snapshot for players / disc.
          Captured on the main thread before a (future) parallel Update batch.
 ===========================================================================**/
#pragma once

#include <vector>
#include <DirectXMath.h>

class GameObject;

struct FrameTransformEntry {
	const GameObject* object = nullptr; // identity only - do not mutate
	DirectX::XMFLOAT3 position{};
	DirectX::XMFLOAT3 rotation{};
};

// Rewrites the flat snapshot from objects tagged TAG_PLAYER or TAG_BULLET.
void FrameTransformSnapshot_Capture(const std::vector<GameObject*>& objects);

const FrameTransformEntry* FrameTransformSnapshot_Find(const GameObject* object);

void FrameTransformSnapshot_Clear();
