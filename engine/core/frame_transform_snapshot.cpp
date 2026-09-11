/**===========================================================================
 File   : frame_transform_snapshot.cpp
 Brief  : Flat per-frame transform snapshot for neighbor reads.
 ===========================================================================**/
#include "frame_transform_snapshot.h"
#include "engine/ecs/game_object.h"

namespace {
std::vector<FrameTransformEntry> g_snapshot;
}

void FrameTransformSnapshot_Capture(const std::vector<GameObject*>& objects) {
	g_snapshot.clear();
	g_snapshot.reserve(objects.size());
	for (GameObject* obj : objects) {
		if (!obj || !obj->IsActive())
			continue;
		const Tag tag = obj->GetTag();
		if (tag != Tag::TAG_PLAYER && tag != Tag::TAG_BULLET)
			continue;
		FrameTransformEntry entry;
		entry.object = obj;
		entry.position = obj->GetPosition();
		entry.rotation = obj->GetRotation();
		g_snapshot.push_back(entry);
	}
}

const FrameTransformEntry* FrameTransformSnapshot_Find(const GameObject* object) {
	if (!object)
		return nullptr;
	for (const FrameTransformEntry& e : g_snapshot) {
		if (e.object == object)
			return &e;
	}
	return nullptr;
}

void FrameTransformSnapshot_Clear() {
	g_snapshot.clear();
}
