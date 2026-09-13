#pragma once

#include <memory>
#include <string>
#include <vector>

class GameObject;
class UIElement;

// Stable node locator for undo commands. ptr is primary while alive;
// name re-resolves via scene DFS if the pointer went stale.
struct NodeRef {
	GameObject* ptr = nullptr;
	std::string name;
};

struct UiElementRef {
	UIElement* ptr = nullptr;
	std::string name;
	int index = -1;
};

struct EditorCommand {
	virtual ~EditorCommand() = default;
	virtual void Undo() = 0;
	virtual void Redo() = 0;
	virtual std::string Describe() const = 0;
};

class EditorUndoStack {
public:
	static EditorUndoStack& Instance();

	void Push(std::unique_ptr<EditorCommand> cmd);
	bool Undo();
	bool Redo();
	bool CanUndo() const;
	bool CanRedo() const;
	size_t Depth() const;
	const std::vector<std::string>& HistoryLabels() const;
	std::string TopUndoDescribe() const;
	std::string TopRedoDescribe() const;
	void Clear();

	bool IsRecording() const { return m_recording; }

private:
	EditorUndoStack() = default;
	void RebuildLabels();

	static constexpr size_t kMaxDepth = 100;
	std::vector<std::unique_ptr<EditorCommand>> m_undo;
	std::vector<std::unique_ptr<EditorCommand>> m_redo;
	std::vector<std::string> m_labels;
	bool m_recording = true;
};

NodeRef MakeNodeRef(GameObject* node);
GameObject* ResolveNode(const NodeRef& ref);
UiElementRef MakeUiElementRef(UIElement* el);
UIElement* ResolveUiElement(const UiElementRef& ref);

void EditorUndo_Push(std::unique_ptr<EditorCommand> cmd);
bool EditorUndo_Undo();
bool EditorUndo_Redo();
void EditorUndo_Clear();

void EditorUndo_PushTransform(GameObject* node, std::string beforeJson, std::string afterJson);
void EditorUndo_PushComponentEdit(GameObject* node, const std::string& compType,
	std::string beforeJson, std::string afterJson);
void EditorUndo_PushCreate(GameObject* created, GameObject* parent, int insertIndex);
void EditorUndo_PushDelete(GameObject* toDelete);

void EditorUndo_PushRename(GameObject* node, std::string oldName, std::string newName);
void EditorUndo_PushTag(GameObject* node, int oldTag, int newTag);
void EditorUndo_PushReparent(GameObject* node, GameObject* oldParent, int oldIndex,
	GameObject* newParent, int newIndex);
void EditorUndo_PushAddComponent(GameObject* node, const std::string& compType);
void EditorUndo_PushRemoveComponent(GameObject* node, const std::string& compType);

// Inspector component-edit session (capture before/after burst).
void EditorUndo_BeginComponentEdit(GameObject* node, const char* compType);
void EditorUndo_EndComponentEdit(GameObject* node, const char* compType);
void EditorUndo_DiscardComponentEdit();

// UI canvas undo (Phase 2).
void EditorUndo_PushUiCreate(UIElement* created);
void EditorUndo_PushUiDelete(UIElement* toDelete);
void EditorUndo_PushUiEdit(UIElement* el, std::string beforeJson, std::string afterJson);
void EditorUndo_PushUiReorder(UIElement* el, int oldIndex, int newIndex);
void EditorUndo_BeginUiEdit(UIElement* el);
void EditorUndo_EndUiEdit(UIElement* el);
void EditorUndo_DiscardUiEdit();

// Internal delete used by commands (does not push undo).
void EditorUndo_DeleteNodeInternal(GameObject* toDelete);
