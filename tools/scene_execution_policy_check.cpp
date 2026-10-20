#include "engine/scene/scene_execution_policy.h"
#include <cassert>

int main() {
    using json=nlohmann::json;
    auto legacy=SceneExecutionPolicy::FromSettings(json::object());
    assert(!legacy.scene2D && legacy.physics3D);
    auto empty=SceneExecutionPolicy::FromSettings(nullptr);
    assert(!empty.scene2D && empty.physics3D);
    auto flat=SceneExecutionPolicy::FromSettings({{"renderPath","2d"},{"physicsMode","3d"}});
    assert(flat.scene2D && !flat.physics3D);
    auto noPhysics=SceneExecutionPolicy::FromSettings({{"physicsMode","none"}});
    assert(!noPhysics.scene2D && !noPhysics.physics3D);
    auto physics2D=SceneExecutionPolicy::FromSettings({{"renderPath","2d"},{"physicsMode","2d"}});
    assert(physics2D.physics2D && !physics2D.physics3D);
    json physicsStored; physics2D.WriteSettings(physicsStored);
    assert(SceneExecutionPolicy::FromSettings(physicsStored).physics2D);
    assert(!SceneExecutionPolicy::FromSettings({{"physicsMode","2d"}}).physics2D);
    json stored;
    flat.WriteSettings(stored);
    assert(stored["physicsMode"]=="none");
    auto restored=SceneExecutionPolicy::FromSettings(stored);
    assert(restored.scene2D && !restored.physics3D);
    legacy.WriteSettings(stored);
    restored=SceneExecutionPolicy::FromSettings(stored);
    assert(!restored.scene2D && restored.physics3D);
}
