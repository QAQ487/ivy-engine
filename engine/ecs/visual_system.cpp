/**===========================================================================
 File   : visual_system.cpp
 Brief  : Walk scene graph and upload deferred skinning data.
 ===========================================================================**/
#include "visual_system.h"

#include "visual_component.h"
#include "game_object.h"
#include "engine/scene/scene_loader.h"

namespace {

void UploadObjectTree(GameObject* node) {
	if (!node) return;

	if (VisualComponent* vc = node->GetComponent<VisualComponent>()) {
		if (Model3D* model = vc->GetRealModel())
			model->UploadSkinnedVerticesIfDirty();
	}

	for (const auto& child : node->GetChildren())
		UploadObjectTree(child.get());
}

} // namespace

void VisualSystem_UploadSkinning() {
	// Scene-graph roots cover authored players and runtime-spawned children.
	// Upload is idempotent via per-Model3D dirty flags.
	for (auto& node : SceneLoader_GetSceneNodes())
		UploadObjectTree(node.get());
}
