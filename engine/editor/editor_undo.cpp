// editor_undo.cpp
// Editor undo/redo command stack.

#include "editor_undo.h"

#include "engine/core/engine_gui.h"
#include "engine/core/engine_gui_internal.h"
#include "engine/ecs/audio_component.h"
#include "engine/ecs/audio_listener_component.h"
#include "engine/ecs/billboard_component.h"
#include "engine/ecs/sprite_renderer_component.h"
#include "engine/ecs/sprite_animator_component.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/collider_component_access.h"
#include "engine/ecs/component.h"
#include "engine/ecs/component_factory.h"
#include "engine/ecs/directional_light_component.h"
#include "engine/ecs/effect_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/material_component.h"
#include "engine/ecs/point_light_component.h"
#include "engine/ecs/prefab_serializer.h"
#include "engine/ecs/rigidbody_component.h"
#include "engine/ecs/physics2d_components.h"
#include "engine/ecs/spot_light_component.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/scene/scene_loader.h"
#include "engine/scripting/script_component.h"
#include "engine/ui/scene_canvas.h"
#include "engine/ui/ui_element.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <utility>

namespace {

// Public imgui has IsItemDeactivatedAfterEdit (last item only), not an "any" variant.
static bool WasAnyItemDeactivatedAfterEdit() {
	ImGuiContext& g = *GImGui;
	return g.DeactivatedItemData.IsAlive && g.DeactivatedItemData.HasBeenEditedBefore;
}

bool NodeInSubtree(GameObject* root, GameObject* target) {
	if (!root || !target) return false;
	if (root == target) return true;
	for (const auto& child : root->GetChildren()) {
		if (NodeInSubtree(child.get(), target))
			return true;
	}
	return false;
}

bool NodeInScene(GameObject* target) {
	if (!target) return false;
	for (const auto& root : SceneLoader_GetSceneNodes()) {
		if (NodeInSubtree(root.get(), target))
			return true;
	}
	return false;
}

GameObject* FindNodeByName(GameObject* node, const std::string& name) {
	if (!node) return nullptr;
	if (node->GetName() == name) return node;
	for (const auto& child : node->GetChildren()) {
		if (GameObject* found = FindNodeByName(child.get(), name))
			return found;
	}
	return nullptr;
}

GameObject* FindNodeByNameInRoots(const std::string& name) {
	if (name.empty()) return nullptr;
	for (const auto& root : SceneLoader_GetSceneNodes()) {
		if (GameObject* found = FindNodeByName(root.get(), name))
			return found;
	}
	return nullptr;
}

Component* FindComponentByTypeName(GameObject* node, const std::string& typeName) {
	if (!node) return nullptr;
	for (const auto& kv : node->GetComponents()) {
		Component* c = kv.second.get();
		if (c && typeName == c->GetTypeName())
			return c;
	}
	return nullptr;
}

int GetSiblingIndex(GameObject* node) {
	if (!node) return -1;
	if (GameObject* parent = node->GetParent()) {
		const auto& ch = parent->GetChildren();
		for (int i = 0; i < static_cast<int>(ch.size()); ++i) {
			if (ch[i].get() == node)
				return i;
		}
		return -1;
	}
	auto& roots = SceneLoader_GetSceneNodes();
	for (int i = 0; i < static_cast<int>(roots.size()); ++i) {
		if (roots[i].get() == node)
			return i;
	}
	return -1;
}

void InsertNodeAt(GameObject* parent, int index, std::unique_ptr<GameObject> node) {
	if (!node) return;
	if (parent) {
		auto& ch = parent->GetChildren();
		node->SetParent(parent);
		const int clamped = std::clamp(index, 0, static_cast<int>(ch.size()));
		ch.insert(ch.begin() + clamped, std::move(node));
	} else {
		auto& roots = SceneLoader_GetSceneNodes();
		const int clamped = std::clamp(index, 0, static_cast<int>(roots.size()));
		roots.insert(roots.begin() + clamped, std::move(node));
	}
}

std::unique_ptr<GameObject> DetachNode(GameObject* toDelete) {
	if (!toDelete) return nullptr;
	if (!toDelete->GetParent()) {
		auto& roots = SceneLoader_GetSceneNodes();
		auto it = std::find_if(roots.begin(), roots.end(),
			[toDelete](const std::unique_ptr<GameObject>& n) { return n.get() == toDelete; });
		if (it == roots.end()) return nullptr;
		std::unique_ptr<GameObject> owned = std::move(*it);
		roots.erase(it);
		return owned;
	}
	GameObject* parent = toDelete->GetParent();
	auto& ch = parent->GetChildren();
	auto it = std::find_if(ch.begin(), ch.end(),
		[toDelete](const std::unique_ptr<GameObject>& n) { return n.get() == toDelete; });
	if (it == ch.end()) return nullptr;
	std::unique_ptr<GameObject> owned = std::move(*it);
	ch.erase(it);
	owned->SetParent(nullptr);
	return owned;
}

void ApplyComponentPostDeserialize(GameObject* node, const std::string& compType) {
	if (!node) return;
	if (compType == "SceneTransform") {
		node->SyncLegacyTransformFromComponents();
	} else if (IsColliderComponentType(compType)) {
		if (auto* col = FindColliderComponent(node))
			col->ApplyToOwner();
	} else if (compType == "Audio") {
		if (auto* ac = node->GetComponent<AudioComponent>())
			ac->ApplyLiveSettings();
	} else if (compType == "Effect") {
		if (auto* ec = node->GetComponent<EffectComponent>()) {
			const std::string path = ec->effectPath;
			ec->effectPath.clear();
			ec->SetEffectPath(path);
		}
	} else if (compType == "Billboard") {
		if (auto* bb = node->GetComponent<BillboardComponent>())
			bb->InvalidateTextureCache();
	}
	EngineGUI_MarkSceneDirty();
	SyncSceneRuntime();
}

std::string ParseNodeNameFromJson(const std::string& jsonStr) {
	try {
		auto j = nlohmann::json::parse(jsonStr);
		if (j.is_object() && j.contains("name") && j["name"].is_string())
			return j["name"].get<std::string>();
		if (j.is_object() && j.contains("nodes") && j["nodes"].is_array() && !j["nodes"].empty()) {
			const auto& n = j["nodes"][0];
			if (n.is_object() && n.contains("name") && n["name"].is_string())
				return n["name"].get<std::string>();
		}
	} catch (...) {
	}
	return {};
}

// ── Component-edit session ───────────────────────────────────────────────────

struct ComponentEditSession {
	bool active = false;
	GameObject* node = nullptr;
	std::string compType;
	std::string beforeJson;
};

ComponentEditSession g_compEdit;

// Per-section pending snapshot (captured before widgets run).
GameObject* g_sectionNode = nullptr;
std::string g_sectionType;
std::string g_sectionBeforeJson;
ImGuiID g_sectionActiveIdBefore = 0;

void DiscardSession() {
	g_compEdit = {};
}

std::string SerializeComponent(GameObject* node, const char* compType) {
	Component* c = FindComponentByTypeName(node, compType);
	return c ? c->Serialize() : std::string();
}

class TransformCommand : public EditorCommand {
public:
	TransformCommand(NodeRef node, std::string beforeJson, std::string afterJson)
		: m_node(std::move(node))
		, m_before(std::move(beforeJson))
		, m_after(std::move(afterJson)) {}

	void Undo() override { Apply(m_before); }
	void Redo() override { Apply(m_after); }

	std::string Describe() const override {
		std::string name = m_node.name.empty() ? "Object" : m_node.name;
		return "Transform " + name;
	}

private:
	void Apply(const std::string& json) {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		engine::Transform* t = node->GetComponent<engine::Transform>();
		if (!t) return;
		t->Deserialize(json);
		node->SyncLegacyTransformFromComponents();
		EngineGUI_MarkSceneDirty();
		SyncSceneRuntime();
	}

	NodeRef m_node;
	std::string m_before;
	std::string m_after;
};

class ComponentEditCommand : public EditorCommand {
public:
	ComponentEditCommand(NodeRef node, std::string compType,
		std::string beforeJson, std::string afterJson)
		: m_node(std::move(node))
		, m_compType(std::move(compType))
		, m_before(std::move(beforeJson))
		, m_after(std::move(afterJson)) {}

	void Undo() override { Apply(m_before); }
	void Redo() override { Apply(m_after); }

	std::string Describe() const override {
		std::string name = m_node.name.empty() ? "Object" : m_node.name;
		return "Edit " + m_compType + " (" + name + ")";
	}

private:
	void Apply(const std::string& json) {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		Component* c = FindComponentByTypeName(node, m_compType);
		if (!c) return;
		c->Deserialize(json);
		ApplyComponentPostDeserialize(node, m_compType);
	}

	NodeRef m_node;
	std::string m_compType;
	std::string m_before;
	std::string m_after;
};

class CreateObjectCommand : public EditorCommand {
public:
	CreateObjectCommand(std::string subtreeJson, NodeRef parent, int insertIndex, NodeRef created)
		: m_json(std::move(subtreeJson))
		, m_parent(std::move(parent))
		, m_index(insertIndex)
		, m_created(std::move(created))
		, m_parentMissing(false) {}

	void Undo() override {
		GameObject* node = ResolveNode(m_created);
		if (!node) {
			const std::string name = ParseNodeNameFromJson(m_json);
			if (!name.empty())
				node = FindNodeByNameInRoots(name);
		}
		if (!node) return;
		EditorUndo_DeleteNodeInternal(node);
		m_created.ptr = nullptr;
	}

	void Redo() override {
		InsertFromJson();
	}

	std::string Describe() const override {
		std::string name = m_created.name.empty() ? ParseNodeNameFromJson(m_json) : m_created.name;
		if (name.empty()) name = "Object";
		std::string s = "Create " + name;
		if (m_parentMissing) s += " (parent missing)";
		return s;
	}

private:
	void InsertFromJson() {
		auto node = PrefabSerializer::Deserialize(m_json);
		if (!node) return;
		m_created = MakeNodeRef(node.get());
		GameObject* parent = ResolveNode(m_parent);
		m_parentMissing = (m_parent.ptr != nullptr || !m_parent.name.empty()) && parent == nullptr;
		GameObject* raw = node.get();
		InsertNodeAt(parent, m_index, std::move(node));
		m_created.ptr = raw;
		g_selNode = raw;
		g_selUiElement = nullptr;
		EngineGUI_MarkSceneDirty();
		SyncSceneRuntime();
	}

	std::string m_json;
	NodeRef m_parent;
	int m_index = 0;
	NodeRef m_created;
	bool m_parentMissing = false;
};

class DeleteObjectCommand : public EditorCommand {
public:
	DeleteObjectCommand(std::string subtreeJson, NodeRef parent, int insertIndex, NodeRef deleted)
		: m_json(std::move(subtreeJson))
		, m_parent(std::move(parent))
		, m_index(insertIndex)
		, m_deleted(std::move(deleted))
		, m_parentMissing(false) {}

	void Undo() override {
		InsertFromJson();
	}

	void Redo() override {
		GameObject* node = ResolveNode(m_deleted);
		if (!node) {
			const std::string name = ParseNodeNameFromJson(m_json);
			if (!name.empty())
				node = FindNodeByNameInRoots(name);
		}
		if (!node) return;
		EditorUndo_DeleteNodeInternal(node);
		m_deleted.ptr = nullptr;
	}

	std::string Describe() const override {
		std::string name = m_deleted.name.empty() ? ParseNodeNameFromJson(m_json) : m_deleted.name;
		if (name.empty()) name = "Object";
		std::string s = "Delete " + name;
		if (m_parentMissing) s += " (parent missing)";
		return s;
	}

private:
	void InsertFromJson() {
		auto node = PrefabSerializer::Deserialize(m_json);
		if (!node) return;
		m_deleted = MakeNodeRef(node.get());
		GameObject* parent = ResolveNode(m_parent);
		m_parentMissing = (m_parent.ptr != nullptr || !m_parent.name.empty()) && parent == nullptr;
		GameObject* raw = node.get();
		InsertNodeAt(parent, m_index, std::move(node));
		m_deleted.ptr = raw;
		g_selNode = raw;
		g_selUiElement = nullptr;
		EngineGUI_MarkSceneDirty();
		SyncSceneRuntime();
	}

	std::string m_json;
	NodeRef m_parent;
	int m_index = 0;
	NodeRef m_deleted;
	bool m_parentMissing = false;
};

class RenameCommand : public EditorCommand {
public:
	RenameCommand(NodeRef node, std::string oldName, std::string newName)
		: m_node(std::move(node)), m_old(std::move(oldName)), m_new(std::move(newName)) {}
	void Undo() override { Apply(m_old); }
	void Redo() override { Apply(m_new); }
	std::string Describe() const override { return "Rename " + m_old + " → " + m_new; }
private:
	void Apply(const std::string& name) {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		node->SetName(name);
		m_node.name = name;
		EngineGUI_MarkSceneDirty();
	}
	NodeRef m_node;
	std::string m_old, m_new;
};

class TagCommand : public EditorCommand {
public:
	TagCommand(NodeRef node, int oldTag, int newTag)
		: m_node(std::move(node)), m_old(oldTag), m_new(newTag) {}
	void Undo() override { Apply(m_old); }
	void Redo() override { Apply(m_new); }
	std::string Describe() const override {
		return "Tag " + (m_node.name.empty() ? "Object" : m_node.name);
	}
private:
	void Apply(int tag) {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		node->SetTag(tag);
		EngineGUI_MarkSceneDirty();
		SyncSceneRuntime();
	}
	NodeRef m_node;
	int m_old = 0, m_new = 0;
};

class ReparentCommand : public EditorCommand {
public:
	ReparentCommand(NodeRef node, NodeRef oldParent, int oldIndex, NodeRef newParent, int newIndex)
		: m_node(std::move(node))
		, m_oldParent(std::move(oldParent))
		, m_oldIndex(oldIndex)
		, m_newParent(std::move(newParent))
		, m_newIndex(newIndex) {}
	void Undo() override { Apply(m_oldParent, m_oldIndex); }
	void Redo() override { Apply(m_newParent, m_newIndex); }
	std::string Describe() const override {
		return "Reparent " + (m_node.name.empty() ? "Object" : m_node.name);
	}
private:
	void Apply(const NodeRef& parentRef, int index) {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		auto owned = DetachNode(node);
		if (!owned) return;
		GameObject* parent = ResolveNode(parentRef);
		GameObject* raw = owned.get();
		InsertNodeAt(parent, index, std::move(owned));
		m_node.ptr = raw;
		EngineGUI_MarkSceneDirty();
		SyncSceneRuntime();
	}
	NodeRef m_node, m_oldParent, m_newParent;
	int m_oldIndex = 0, m_newIndex = 0;
};

void RemoveComponentByTypeName(GameObject* node, const std::string& typeName) {
	if (!node) return;
	if (typeName == "SceneTransform") node->RemoveComponent<engine::Transform>();
	else if (typeName == "visual") node->RemoveComponent<VisualComponent>();
	else if (typeName == "Material") node->RemoveComponent<MaterialComponent>();
	else if (typeName == "Rigidbody2D") node->RemoveComponent<Rigidbody2DComponent>();
	else if (typeName == "Collider2D") node->RemoveComponent<Collider2DComponent>();
	else if (typeName == "Rigidbody") node->RemoveComponent<RigidbodyComponent>();
	else if (IsColliderComponentType(typeName)) RemoveColliderComponent(node);
	else if (typeName == "Billboard") node->RemoveComponent<BillboardComponent>();
	else if (typeName == "SpriteRenderer") node->RemoveComponent<SpriteRendererComponent>();
	else if (typeName == "SpriteAnimator") node->RemoveComponent<SpriteAnimatorComponent>();
	else if (typeName == "Effect") node->RemoveComponent<EffectComponent>();
	else if (typeName == "Audio") node->RemoveComponent<AudioComponent>();
	else if (typeName == "AudioListener") node->RemoveComponent<AudioListenerComponent>();
	else if (typeName == "Script") node->RemoveComponent<ScriptComponent>();
	else if (typeName == "Camera") node->RemoveComponent<CameraComponent>();
	else if (typeName == "DirectionalLight") node->RemoveComponent<DirectionalLightComponent>();
	else if (typeName == "PointLight") node->RemoveComponent<PointLightComponent>();
	else if (typeName == "SpotLight") node->RemoveComponent<SpotLightComponent>();
}

void AddComponentFromJson(GameObject* node, const std::string& typeName, const std::string& jsonStr) {
	if (!node) return;
	if (FindComponentByTypeName(node, typeName)) return;
	auto comp = ComponentFactory::Instance().Create(typeName);
	if (!comp) return;
	if (!jsonStr.empty())
		comp->Deserialize(jsonStr);
	node->AddComponentRaw(std::move(comp));
}

class AddComponentCommand : public EditorCommand {
public:
	AddComponentCommand(NodeRef node, std::string compType, std::string afterJson)
		: m_node(std::move(node)), m_type(std::move(compType)), m_after(std::move(afterJson)) {}
	void Undo() override {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		RemoveComponentByTypeName(node, m_type);
		EngineGUI_MarkSceneDirty();
		SyncSceneRuntime();
	}
	void Redo() override {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		AddComponentFromJson(node, m_type, m_after);
		ApplyComponentPostDeserialize(node, m_type);
	}
	std::string Describe() const override { return "Add " + m_type; }
private:
	NodeRef m_node;
	std::string m_type, m_after;
};

class RemoveComponentCommand : public EditorCommand {
public:
	RemoveComponentCommand(NodeRef node, std::string compType, std::string beforeJson)
		: m_node(std::move(node)), m_type(std::move(compType)), m_before(std::move(beforeJson)) {}
	void Undo() override {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		AddComponentFromJson(node, m_type, m_before);
		ApplyComponentPostDeserialize(node, m_type);
	}
	void Redo() override {
		GameObject* node = ResolveNode(m_node);
		if (!node) return;
		RemoveComponentByTypeName(node, m_type);
		EngineGUI_MarkSceneDirty();
		SyncSceneRuntime();
	}
	std::string Describe() const override { return "Remove " + m_type; }
private:
	NodeRef m_node;
	std::string m_type, m_before;
};

void RemoveComponentByTypeName(GameObject* node, const std::string& typeName);
void AddComponentFromJson(GameObject* node, const std::string& typeName, const std::string& json);

class CreateUiCommand : public EditorCommand {
public:
	CreateUiCommand(std::string json, int index, UiElementRef created)
		: m_json(std::move(json)), m_index(index), m_created(std::move(created)) {}
	void Undo() override {
		UIElement* el = ResolveUiElement(m_created);
		if (!el) return;
		if (g_selUiElement == el) g_selUiElement = nullptr;
		SceneCanvas_RemoveElementInternal(el);
		m_created.ptr = nullptr;
		EngineGUI_MarkSceneDirty();
	}
	void Redo() override {
		UIElement* el = SceneCanvas_InsertElementJson(m_json, m_index);
		if (!el) return;
		m_created = MakeUiElementRef(el);
		g_selUiElement = el;
		g_selNode = nullptr;
		EngineGUI_MarkSceneDirty();
	}
	std::string Describe() const override {
		return "Create UI " + (m_created.name.empty() ? "Element" : m_created.name);
	}
private:
	std::string m_json;
	int m_index = 0;
	UiElementRef m_created;
};

class DeleteUiCommand : public EditorCommand {
public:
	DeleteUiCommand(std::string json, int index, UiElementRef deleted)
		: m_json(std::move(json)), m_index(index), m_deleted(std::move(deleted)) {}
	void Undo() override {
		UIElement* el = SceneCanvas_InsertElementJson(m_json, m_index);
		if (!el) return;
		m_deleted = MakeUiElementRef(el);
		g_selUiElement = el;
		g_selNode = nullptr;
		EngineGUI_MarkSceneDirty();
	}
	void Redo() override {
		UIElement* el = ResolveUiElement(m_deleted);
		if (!el) return;
		if (g_selUiElement == el) g_selUiElement = nullptr;
		SceneCanvas_RemoveElementInternal(el);
		m_deleted.ptr = nullptr;
		EngineGUI_MarkSceneDirty();
	}
	std::string Describe() const override {
		return "Delete UI " + (m_deleted.name.empty() ? "Element" : m_deleted.name);
	}
private:
	std::string m_json;
	int m_index = 0;
	UiElementRef m_deleted;
};

class UiEditCommand : public EditorCommand {
public:
	UiEditCommand(UiElementRef el, std::string beforeJson, std::string afterJson)
		: m_el(std::move(el)), m_before(std::move(beforeJson)), m_after(std::move(afterJson)) {}
	void Undo() override { Apply(m_before); }
	void Redo() override { Apply(m_after); }
	std::string Describe() const override {
		return "Edit UI " + (m_el.name.empty() ? "Element" : m_el.name);
	}
private:
	void Apply(const std::string& json) {
		UIElement* el = ResolveUiElement(m_el);
		const int index = el ? SceneCanvas_GetElementIndex(el) : m_el.index;
		if (el) {
			if (g_selUiElement == el) g_selUiElement = nullptr;
			SceneCanvas_RemoveElementInternal(el);
		}
		UIElement* created = SceneCanvas_InsertElementJson(json, index >= 0 ? index : 0);
		if (!created) return;
		m_el = MakeUiElementRef(created);
		g_selUiElement = created;
		g_selNode = nullptr;
		EngineGUI_MarkSceneDirty();
	}
	UiElementRef m_el;
	std::string m_before, m_after;
};

class UiReorderCommand : public EditorCommand {
public:
	UiReorderCommand(UiElementRef el, int oldIndex, int newIndex)
		: m_el(std::move(el)), m_old(oldIndex), m_new(newIndex) {}
	void Undo() override { Apply(m_old); }
	void Redo() override { Apply(m_new); }
	std::string Describe() const override {
		return "Reorder UI " + (m_el.name.empty() ? "Element" : m_el.name);
	}
private:
	void Apply(int index) {
		UIElement* el = ResolveUiElement(m_el);
		UICanvas* canvas = SceneCanvas_GetCanvas();
		if (!el || !canvas) return;
		canvas->MoveElementToIndex(el, index);
		m_el = MakeUiElementRef(el);
		EngineGUI_MarkSceneDirty();
	}
	UiElementRef m_el;
	int m_old = 0, m_new = 0;
};

struct UiEditSession {
	bool active = false;
	UIElement* el = nullptr;
	std::string beforeJson;
};
UiEditSession g_uiEdit;
UIElement* g_uiSectionEl = nullptr;
std::string g_uiSectionBefore;
ImGuiID g_uiSectionActiveIdBefore = 0;

void DiscardUiSession() { g_uiEdit = {}; }

} // namespace

// ── NodeRef helpers ──────────────────────────────────────────────────────────

NodeRef MakeNodeRef(GameObject* node) {
	NodeRef ref;
	if (node) {
		ref.ptr = node;
		ref.name = node->GetName();
	}
	return ref;
}

UiElementRef MakeUiElementRef(UIElement* el) {
	UiElementRef ref;
	if (el) {
		ref.ptr = el;
		ref.name = el->GetName();
		ref.index = SceneCanvas_GetElementIndex(el);
	}
	return ref;
}

GameObject* ResolveNode(const NodeRef& ref) {
	if (ref.ptr && NodeInScene(ref.ptr))
		return ref.ptr;
	if (!ref.name.empty())
		return FindNodeByNameInRoots(ref.name);
	return nullptr;
}

UIElement* ResolveUiElement(const UiElementRef& ref) {
	UICanvas* canvas = SceneCanvas_GetCanvas();
	if (!canvas) return nullptr;
	if (ref.ptr) {
		for (const auto& owned : canvas->GetElements()) {
			if (owned.get() == ref.ptr)
				return ref.ptr;
		}
	}
	if (ref.index >= 0 && ref.index < static_cast<int>(canvas->GetElementCount())) {
		UIElement* at = canvas->GetElements()[ref.index].get();
		if (at && (ref.name.empty() || at->GetName() == ref.name))
			return at;
	}
	if (!ref.name.empty()) {
		for (const auto& owned : canvas->GetElements()) {
			if (owned && owned->GetName() == ref.name)
				return owned.get();
		}
	}
	return nullptr;
}

void EditorUndo_DeleteNodeInternal(GameObject* toDelete) {
	if (!toDelete) return;
	ClearSceneRuntime();
	if (g_selNode == toDelete)
		g_selNode = nullptr;
	(void)DetachNode(toDelete);
	EngineGUI_MarkSceneDirty();
	SyncSceneRuntime();
}

// ── Stack ────────────────────────────────────────────────────────────────────

EditorUndoStack& EditorUndoStack::Instance() {
	static EditorUndoStack s;
	return s;
}

void EditorUndoStack::RebuildLabels() {
	m_labels.clear();
	m_labels.reserve(m_undo.size());
	for (const auto& cmd : m_undo)
		m_labels.push_back(cmd ? cmd->Describe() : std::string());
}

void EditorUndoStack::Push(std::unique_ptr<EditorCommand> cmd) {
	if (!cmd || !m_recording) return;
	if (g_isPlaying) return;
	m_redo.clear();
	m_undo.push_back(std::move(cmd));
	while (m_undo.size() > kMaxDepth)
		m_undo.erase(m_undo.begin());
	RebuildLabels();
}

bool EditorUndoStack::Undo() {
	if (!CanUndo() || g_isPlaying) return false;
	m_recording = false;
	auto cmd = std::move(m_undo.back());
	m_undo.pop_back();
	cmd->Undo();
	m_redo.push_back(std::move(cmd));
	m_recording = true;
	RebuildLabels();
	return true;
}

bool EditorUndoStack::Redo() {
	if (!CanRedo() || g_isPlaying) return false;
	m_recording = false;
	auto cmd = std::move(m_redo.back());
	m_redo.pop_back();
	cmd->Redo();
	m_undo.push_back(std::move(cmd));
	m_recording = true;
	RebuildLabels();
	return true;
}

bool EditorUndoStack::CanUndo() const { return !m_undo.empty(); }
bool EditorUndoStack::CanRedo() const { return !m_redo.empty(); }
size_t EditorUndoStack::Depth() const { return m_undo.size(); }

const std::vector<std::string>& EditorUndoStack::HistoryLabels() const {
	return m_labels;
}

std::string EditorUndoStack::TopUndoDescribe() const {
	if (m_undo.empty() || !m_undo.back()) return {};
	return m_undo.back()->Describe();
}

std::string EditorUndoStack::TopRedoDescribe() const {
	if (m_redo.empty() || !m_redo.back()) return {};
	return m_redo.back()->Describe();
}

void EditorUndoStack::Clear() {
	m_undo.clear();
	m_redo.clear();
	m_labels.clear();
	DiscardSession();
	DiscardUiSession();
}

void EditorUndo_Push(std::unique_ptr<EditorCommand> cmd) {
	EditorUndoStack::Instance().Push(std::move(cmd));
}

bool EditorUndo_Undo() {
	return EditorUndoStack::Instance().Undo();
}

bool EditorUndo_Redo() {
	return EditorUndoStack::Instance().Redo();
}

void EditorUndo_Clear() {
	EditorUndoStack::Instance().Clear();
}

void EditorUndo_PushTransform(GameObject* node, std::string beforeJson, std::string afterJson) {
	if (!node || beforeJson == afterJson) return;
	EditorUndo_Push(std::make_unique<TransformCommand>(
		MakeNodeRef(node), std::move(beforeJson), std::move(afterJson)));
}

void EditorUndo_PushComponentEdit(GameObject* node, const std::string& compType,
	std::string beforeJson, std::string afterJson) {
	if (!node || beforeJson == afterJson) return;
	EditorUndo_Push(std::make_unique<ComponentEditCommand>(
		MakeNodeRef(node), compType, std::move(beforeJson), std::move(afterJson)));
}

void EditorUndo_PushCreate(GameObject* created, GameObject* parent, int insertIndex) {
	if (!created) return;
	if (!EditorUndoStack::Instance().IsRecording() || g_isPlaying) return;
	std::string json = PrefabSerializer::Serialize(*created);
	NodeRef parentRef = MakeNodeRef(parent);
	if (insertIndex < 0)
		insertIndex = GetSiblingIndex(created);
	EditorUndo_Push(std::make_unique<CreateObjectCommand>(
		std::move(json), parentRef, insertIndex, MakeNodeRef(created)));
}

void EditorUndo_PushDelete(GameObject* toDelete) {
	if (!toDelete) return;
	if (!EditorUndoStack::Instance().IsRecording() || g_isPlaying) return;
	std::string json = PrefabSerializer::Serialize(*toDelete);
	GameObject* parent = toDelete->GetParent();
	const int index = GetSiblingIndex(toDelete);
	EditorUndo_Push(std::make_unique<DeleteObjectCommand>(
		std::move(json), MakeNodeRef(parent), index, MakeNodeRef(toDelete)));
}

void EditorUndo_BeginComponentEdit(GameObject* node, const char* compType) {
	if (!node || !compType || g_isPlaying) return;
	if (!EditorUndoStack::Instance().IsRecording()) return;

	g_sectionNode = node;
	g_sectionType = compType;
	g_sectionBeforeJson = SerializeComponent(node, compType);
	g_sectionActiveIdBefore = ImGui::GetActiveID();
}

void EditorUndo_EndComponentEdit(GameObject* node, const char* compType) {
	if (!node || !compType) return;
	if (g_sectionNode != node || g_sectionType != compType)
		return;

	const bool sectionMatchesSession =
		g_compEdit.active &&
		g_compEdit.node == node &&
		g_compEdit.compType == compType;

	// Foreign open session: Amendment 2b — only discard if this section actually
	// took over editing (an item activated here while another session was open).
	if (g_compEdit.active && !sectionMatchesSession) {
		const ImGuiID activeNow = ImGui::GetActiveID();
		const bool activatedHere =
			activeNow != 0 && activeNow != g_sectionActiveIdBefore;
		if (activatedHere || WasAnyItemDeactivatedAfterEdit()) {
			DiscardSession();
			// Fall through to open/commit for this section.
		} else {
			return;
		}
	}

	if (!g_compEdit.active) {
		const ImGuiID activeNow = ImGui::GetActiveID();
		const bool activatedHere =
			activeNow != 0 && activeNow != g_sectionActiveIdBefore;

		if (WasAnyItemDeactivatedAfterEdit()) {
			// Single-frame edit (checkbox, combo, etc.)
			std::string afterJson = SerializeComponent(node, compType);
			if (!g_sectionBeforeJson.empty() && afterJson != g_sectionBeforeJson) {
				EditorUndo_PushComponentEdit(node, compType,
					g_sectionBeforeJson, std::move(afterJson));
			}
			return;
		}

		if (activatedHere) {
			g_compEdit.active = true;
			g_compEdit.node = node;
			g_compEdit.compType = compType;
			g_compEdit.beforeJson = g_sectionBeforeJson;
			return;
		}

		// Silent mutations this frame (asset drop, scroll wheel).
		std::string afterJson = SerializeComponent(node, compType);
		if (!g_sectionBeforeJson.empty() && afterJson != g_sectionBeforeJson) {
			EditorUndo_PushComponentEdit(node, compType,
				g_sectionBeforeJson, std::move(afterJson));
		}
		return;
	}

	// Active session for this section.
	if (!WasAnyItemDeactivatedAfterEdit())
		return;

	std::string afterJson = SerializeComponent(node, compType);
	if (afterJson != g_compEdit.beforeJson) {
		EditorUndo_PushComponentEdit(node, g_compEdit.compType,
			std::move(g_compEdit.beforeJson), std::move(afterJson));
	}
	DiscardSession();
}

void EditorUndo_DiscardComponentEdit() {
	DiscardSession();
	g_sectionNode = nullptr;
	g_sectionType.clear();
	g_sectionBeforeJson.clear();
	g_sectionActiveIdBefore = 0;
}

void EditorUndo_PushRename(GameObject* node, std::string oldName, std::string newName) {
	if (!node || oldName == newName) return;
	EditorUndo_Push(std::make_unique<RenameCommand>(
		MakeNodeRef(node), std::move(oldName), std::move(newName)));
}

void EditorUndo_PushTag(GameObject* node, int oldTag, int newTag) {
	if (!node || oldTag == newTag) return;
	EditorUndo_Push(std::make_unique<TagCommand>(MakeNodeRef(node), oldTag, newTag));
}

void EditorUndo_PushReparent(GameObject* node, GameObject* oldParent, int oldIndex,
	GameObject* newParent, int newIndex)
{
	if (!node) return;
	EditorUndo_Push(std::make_unique<ReparentCommand>(
		MakeNodeRef(node), MakeNodeRef(oldParent), oldIndex,
		MakeNodeRef(newParent), newIndex));
}

void EditorUndo_PushAddComponent(GameObject* node, const std::string& compType) {
	if (!node || compType.empty()) return;
	Component* c = nullptr;
	for (const auto& kv : node->GetComponents()) {
		if (kv.second && compType == kv.second->GetTypeName()) {
			c = kv.second.get();
			break;
		}
	}
	std::string json = c ? c->Serialize() : "{}";
	EditorUndo_Push(std::make_unique<AddComponentCommand>(
		MakeNodeRef(node), compType, std::move(json)));
}

void EditorUndo_PushRemoveComponent(GameObject* node, const std::string& compType) {
	if (!node || compType.empty()) return;
	Component* c = nullptr;
	for (const auto& kv : node->GetComponents()) {
		if (kv.second && compType == kv.second->GetTypeName()) {
			c = kv.second.get();
			break;
		}
	}
	if (!c) return;
	std::string json = c->Serialize();
	EditorUndo_Push(std::make_unique<RemoveComponentCommand>(
		MakeNodeRef(node), compType, std::move(json)));
}

void EditorUndo_PushUiCreate(UIElement* created) {
	if (!created) return;
	if (!EditorUndoStack::Instance().IsRecording() || g_isPlaying) return;
	const int index = SceneCanvas_GetElementIndex(created);
	std::string json = SceneCanvas_SerializeElement(*created);
	EditorUndo_Push(std::make_unique<CreateUiCommand>(
		std::move(json), index, MakeUiElementRef(created)));
}

void EditorUndo_PushUiDelete(UIElement* toDelete) {
	if (!toDelete) return;
	if (!EditorUndoStack::Instance().IsRecording() || g_isPlaying) return;
	const int index = SceneCanvas_GetElementIndex(toDelete);
	std::string json = SceneCanvas_SerializeElement(*toDelete);
	EditorUndo_Push(std::make_unique<DeleteUiCommand>(
		std::move(json), index, MakeUiElementRef(toDelete)));
}

void EditorUndo_PushUiEdit(UIElement* el, std::string beforeJson, std::string afterJson) {
	if (!el || beforeJson == afterJson) return;
	EditorUndo_Push(std::make_unique<UiEditCommand>(
		MakeUiElementRef(el), std::move(beforeJson), std::move(afterJson)));
}

void EditorUndo_PushUiReorder(UIElement* el, int oldIndex, int newIndex) {
	if (!el || oldIndex == newIndex || oldIndex < 0 || newIndex < 0) return;
	if (!EditorUndoStack::Instance().IsRecording() || g_isPlaying) return;
	EditorUndo_Push(std::make_unique<UiReorderCommand>(
		MakeUiElementRef(el), oldIndex, newIndex));
}

void EditorUndo_BeginUiEdit(UIElement* el) {
	if (!el || g_isPlaying) return;
	if (!EditorUndoStack::Instance().IsRecording()) return;
	g_uiSectionEl = el;
	g_uiSectionBefore = SceneCanvas_SerializeElement(*el);
	g_uiSectionActiveIdBefore = ImGui::GetActiveID();
}

void EditorUndo_EndUiEdit(UIElement* el) {
	if (!el || g_uiSectionEl != el) return;

	const bool matches = g_uiEdit.active && g_uiEdit.el == el;
	if (g_uiEdit.active && !matches) {
		const ImGuiID activeNow = ImGui::GetActiveID();
		const bool activatedHere = activeNow != 0 && activeNow != g_uiSectionActiveIdBefore;
		if (activatedHere || WasAnyItemDeactivatedAfterEdit())
			DiscardUiSession();
		else
			return;
	}

	if (!g_uiEdit.active) {
		const ImGuiID activeNow = ImGui::GetActiveID();
		const bool activatedHere = activeNow != 0 && activeNow != g_uiSectionActiveIdBefore;
		if (WasAnyItemDeactivatedAfterEdit()) {
			std::string afterJson = SceneCanvas_SerializeElement(*el);
			if (afterJson != g_uiSectionBefore)
				EditorUndo_PushUiEdit(el, g_uiSectionBefore, std::move(afterJson));
			return;
		}
		if (activatedHere) {
			g_uiEdit.active = true;
			g_uiEdit.el = el;
			g_uiEdit.beforeJson = g_uiSectionBefore;
			return;
		}

		// Silent mutations this frame (asset drop, draw-order is separate).
		std::string afterJson = SceneCanvas_SerializeElement(*el);
		if (afterJson != g_uiSectionBefore)
			EditorUndo_PushUiEdit(el, g_uiSectionBefore, std::move(afterJson));
		return;
	}

	if (!WasAnyItemDeactivatedAfterEdit())
		return;

	std::string afterJson = SceneCanvas_SerializeElement(*el);
	if (afterJson != g_uiEdit.beforeJson)
		EditorUndo_PushUiEdit(el, std::move(g_uiEdit.beforeJson), std::move(afterJson));
	DiscardUiSession();
}

void EditorUndo_DiscardUiEdit() {
	DiscardUiSession();
	g_uiSectionEl = nullptr;
	g_uiSectionBefore.clear();
	g_uiSectionActiveIdBefore = 0;
}
