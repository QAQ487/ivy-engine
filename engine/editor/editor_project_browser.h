// editor_project_browser.h
// Editor project/asset browser panel.

#pragma once

#include <memory>
#include <vector>

class GameObject;

namespace MapEditor {

GameObject* ProjectBrowserPanel(std::vector<std::unique_ptr<GameObject>>& nodes);

} // namespace MapEditor
