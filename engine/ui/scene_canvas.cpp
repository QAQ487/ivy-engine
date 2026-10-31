#include "scene_canvas.h"

#include "../input/key_logger.h"
#include "../rendering/sprite/texture.h"
#include "engine/rendering/d3d11/renderer.h"
#include "../rendering/sprite/sprite.h"
#include "../ecs/string_utils.h"
#include "../core/engine_gui.h"
#include "../ecs/string_utils.h"
#include <cstring>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <Windows.h>

using json = nlohmann::json;

std::unique_ptr<UICanvas> g_canvas;

UISprite* g_redOverlay = nullptr; // Full-screen red tint shown during player respawn countdown
UIText* g_scoreText = nullptr;    // Score display (top-left corner)
UIText* g_timerText = nullptr;    // Timer display (top-middle)
std::string g_pendingSceneFile;
bool g_pendingScenePlayOnLoad = false;

namespace {
	std::map<std::string, SceneCanvasButtonActionBinder>& ButtonActionRegistry() {
		static std::map<std::string, SceneCanvasButtonActionBinder> registry;
		return registry;
	}

	void EnsureDefaultButtonActions() {
		auto& registry = ButtonActionRegistry();
		if (registry.find("LoadScene") == registry.end()) {
			registry["LoadScene"] = [](UIButton* button) {
				if (!button) return;
				const std::string scene = button->GetActionScene();
				const bool playOnLoad = button->GetActionPlayOnLoad();
				button->SetOnClick([scene, playOnLoad]() {
					if (!scene.empty())
						SceneCanvas_RequestSceneFile(scene, playOnLoad);
				});
			};
		}

		if (registry.find("ExitGame") == registry.end()) {
			registry["ExitGame"] = [](UIButton* button) {
				if (!button) return;
				button->SetOnClick([]() {
					PostQuitMessage(0);
				});
			};
		}
	}

	UIAnchor AnchorFromString(const std::string& value) {
		if (value == "TOP_LEFT") return UIAnchor::TOP_LEFT;
		if (value == "TOP_CENTER") return UIAnchor::TOP_CENTER;
		if (value == "TOP_RIGHT") return UIAnchor::TOP_RIGHT;
		if (value == "CENTER_LEFT") return UIAnchor::CENTER_LEFT;
		if (value == "CENTER_RIGHT") return UIAnchor::CENTER_RIGHT;
		if (value == "BOTTOM_LEFT") return UIAnchor::BOTTOM_LEFT;
		if (value == "BOTTOM_CENTER") return UIAnchor::BOTTOM_CENTER;
		if (value == "BOTTOM_RIGHT") return UIAnchor::BOTTOM_RIGHT;
		return UIAnchor::CENTER;
	}

	std::string AnchorToString(UIAnchor anchor) {
		switch (anchor) {
		case UIAnchor::TOP_LEFT: return "TOP_LEFT";
		case UIAnchor::TOP_CENTER: return "TOP_CENTER";
		case UIAnchor::TOP_RIGHT: return "TOP_RIGHT";
		case UIAnchor::CENTER_LEFT: return "CENTER_LEFT";
		case UIAnchor::CENTER: return "CENTER";
		case UIAnchor::CENTER_RIGHT: return "CENTER_RIGHT";
		case UIAnchor::BOTTOM_LEFT: return "BOTTOM_LEFT";
		case UIAnchor::BOTTOM_CENTER: return "BOTTOM_CENTER";
		case UIAnchor::BOTTOM_RIGHT: return "BOTTOM_RIGHT";
		default: return "CENTER";
		}
	}

	TextAlign TextAlignFromString(const std::string& value) {
		if (value == "CENTER") return TextAlign::CENTER;
		if (value == "RIGHT") return TextAlign::RIGHT;
		return TextAlign::LEFT;
	}

	std::string TextAlignToString(TextAlign align) {
		switch (align) {
		case TextAlign::CENTER: return "CENTER";
		case TextAlign::RIGHT: return "RIGHT";
		case TextAlign::LEFT:
		default: return "LEFT";
		}
	}

	DirectX::XMFLOAT2 ReadFloat2(const json& source, float defaultX = 0.0f, float defaultY = 0.0f) {
		return {
			source.value("x", defaultX),
			source.value("y", defaultY)
		};
	}

	json WriteFloat2(const DirectX::XMFLOAT2& value) {
		return { {"x", value.x}, {"y", value.y} };
	}

	DirectX::XMFLOAT4 ReadColor(const json& source, const DirectX::XMFLOAT4& fallback) {
		if (!source.is_object()) return fallback;
		return {
			source.value("r", fallback.x),
			source.value("g", fallback.y),
			source.value("b", fallback.z),
			source.value("a", fallback.w)
		};
	}

	json WriteColor(const DirectX::XMFLOAT4& value) {
		return { {"r", value.x}, {"g", value.y}, {"b", value.z}, {"a", value.w} };
	}

	std::string NormalizeSceneFile(const std::string& sceneFile) {
		if (sceneFile.empty()) return "";
		const size_t lastSlash = sceneFile.find_last_of("/\\");
		const size_t lastDot = sceneFile.find_last_of('.');
		const bool hasExtension = lastDot != std::string::npos &&
			(lastSlash == std::string::npos || lastDot > lastSlash);

		std::string normalized = sceneFile;
		if (!hasExtension)
			normalized += ".json";

		if (sceneFile.find('/') != std::string::npos || sceneFile.find('\\') != std::string::npos)
			return normalized;
		return "rom/scene/" + normalized;
	}

	void ApplyCommon(UIElement* element, const json& item) {
		if (!element) return;
		element->SetVisible(item.value("visible", true));
        if(item.contains("pivot")) element->SetPivot(ReadFloat2(item["pivot"],0,1));
	}

	void LoadElementScript(UIElement* element, const json& item) {
		if (!element) return;
		const json script = item.value("script", json::object());
		if (script.is_object() && !script.empty())
			element->LoadScriptFromJson(script);
	}

	void AppendElementScript(const UIElement& element, json& item) {
		const json script = element.SaveScriptToJson();
		if (script.is_object() && !script.empty())
			item["script"] = script;
	}

	std::string ReadOnClickScript(const json& onClick) {
		std::string script = onClick.value("script", "");
		if (script.empty())
			script = onClick.value("action", "");
		if (script == "loadScene")
			script = "LoadScene";
		if (script == "exit" || script == "Exit" || script == "quit" || script == "Quit")
			script = "ExitGame";
		return script;
	}

	void BindSceneHudElements() {
		g_redOverlay = nullptr;
		g_scoreText = nullptr;
		g_timerText = nullptr;
		if (!g_canvas) return;

		g_redOverlay = dynamic_cast<UISprite*>(g_canvas->GetElement("RedOverlay"));
		g_scoreText = dynamic_cast<UIText*>(g_canvas->GetElement("ScoreText"));
		g_timerText = dynamic_cast<UIText*>(g_canvas->GetElement("TimerText"));
	}

	void LoadSpriteElement(UICanvas& canvas, const json& item, bool preparing = false) {
		const std::string name = item.value("name", "Sprite");
		const auto pos = ReadFloat2(item.value("position", json::object()));
		const UIAnchor anchor = AnchorFromString(item.value("anchor", "CENTER"));
		const json size = item.value("size", json::object());
		UISprite* sprite = canvas.AddSprite(
			name, pos, anchor,
			size.value("width", 64.0f),
			size.value("height", 64.0f));

		const std::string texture = item.value("texture", "");
		if (!texture.empty()) sprite->SetTexturePath(texture);
		sprite->SetColor(ReadColor(item.value("color", json::object()), sprite->GetColor()));
		sprite->SetRotation(item.value("rotation", 0.0f));
		LoadElementScript(sprite, item);
		ApplyCommon(sprite, item);
	}

	void LoadTextElement(UICanvas& canvas, const json& item, bool preparing = false) {
		const std::string name = item.value("name", "Text");
		const auto pos = ReadFloat2(item.value("position", json::object()));
		const UIAnchor anchor = AnchorFromString(item.value("anchor", "CENTER"));
		UIText* text = canvas.AddText(name, pos, anchor, item.value("text", ""));
		text->SetColor(ReadColor(item.value("color", json::object()), text->GetColor()));
		text->SetFontSize(item.value("fontSize", 1.0f));
		text->SetAlignment(TextAlignFromString(item.value("alignment", "LEFT")));
		text->SetCharacterSpacing(item.value("characterSpacing", 16.0f));

		const json font = item.value("font", json::object());
		const std::string fontTexture = font.value("texture", "");
		if (!fontTexture.empty()) {
			text->SetFontPath(
				fontTexture,
				font.value("charWidth", 32.0f),
				font.value("charHeight", 32.0f),
				font.value("charsPerRow", 16));
		}
		LoadElementScript(text, item);
		ApplyCommon(text, item);
	}

	void LoadButtonElement(UICanvas& canvas, const json& item, bool preparing = false) {
		const std::string name = item.value("name", "Button");
		const auto pos = ReadFloat2(item.value("position", json::object()));
		const UIAnchor anchor = AnchorFromString(item.value("anchor", "CENTER"));
		const json size = item.value("size", json::object());
		UIButton* button = canvas.AddButton(
			name, pos, anchor,
			size.value("width", 200.0f),
			size.value("height", 60.0f));

		button->SetText(item.value("text", ""));
		button->SetTextScale(item.value("textScale", 1.0f));
		button->SetColor(ReadColor(item.value("color", json::object()), button->GetColor()));
		button->SetHoverColor(ReadColor(item.value("hoverColor", json::object()), button->GetHoverColor()));
		button->SetPressedColor(ReadColor(item.value("pressedColor", json::object()), button->GetPressedColor()));
		button->SetTextColor(ReadColor(item.value("textColor", json::object()), button->GetTextColor()));
		button->SetTextHoverColor(ReadColor(item.value("textHoverColor", json::object()), button->GetTextHoverColor()));

		const std::string texture = item.value("texture", "");
		const std::string hoverTexture = item.value("hoverTexture", "");
		const std::string pressedTexture = item.value("pressedTexture", "");
		if (!texture.empty()) button->SetTexturePath(texture);
		if (!hoverTexture.empty()) button->SetHoverTexturePath(hoverTexture);
		if (!pressedTexture.empty()) button->SetPressedTexturePath(pressedTexture);

		const json font = item.value("font", json::object());
		const std::string fontTexture = font.value("texture", "");
		if (!preparing && !fontTexture.empty()) {
			UIButton::SetFontPath(
				fontTexture,
				font.value("charWidth", 32.0f),
				font.value("charHeight", 32.0f),
				font.value("charsPerRow", 16));
		}

		const json onClick = item.value("onClick", json::object());
		const std::string action = ReadOnClickScript(onClick);
		const std::string scene = onClick.value("scene", "");
		const bool playOnLoad = onClick.value("play", false);
		button->SetAction(action, scene, playOnLoad);
		if (!preparing) SceneCanvas_BindButtonAction(button);

		LoadElementScript(button, item);
		ApplyCommon(button, item);
	}

	void LoadBarElement(UICanvas& canvas, const json& item, bool preparing = false) {
		const std::string name = item.value("name", "Bar");
		const auto pos = ReadFloat2(item.value("position", json::object()));
		const UIAnchor anchor = AnchorFromString(item.value("anchor", "TOP_LEFT"));
		UIBar* bar = canvas.AddBar(
			name,
			pos,
			anchor,
			item.value("maxValue", 100.0f),
			item.value("label", ""));
		bar->SetCurrentValue(item.value("currentValue", bar->GetMaxValue()));
		const json size = item.value("size", json::object());
		bar->SetBarSize(size.value("width", bar->GetBarWidth()), size.value("height", bar->GetBarHeight()));
		bar->SetTextColor(ReadColor(item.value("textColor", json::object()), bar->GetTextColor()));
		bar->SetBarColor(ReadColor(item.value("barColor", json::object()), bar->GetBarColor()));
		bar->SetBackgroundColor(ReadColor(item.value("backgroundColor", json::object()), bar->GetBackgroundColor()));
		bar->SetShowText(item.value("showText", bar->GetShowText()));
		bar->SetShowValue(item.value("showValue", bar->GetShowValue()));
		bar->SetCharacterSpacing(item.value("characterSpacing", bar->GetCharacterSpacing()));
		const std::string fillTexture = item.value("fillTexture", "");
		if (!fillTexture.empty())
			bar->SetFillTexturePath(fillTexture);
		const std::string backgroundTexture = item.value("backgroundTexture", "");
		if (!backgroundTexture.empty())
			bar->SetBackgroundTexturePath(backgroundTexture);
		LoadElementScript(bar, item);
		ApplyCommon(bar, item);
	}

	json SerializeCommon(const UIElement& element, const std::string& type) {
		json item;
		item["type"] = type;
		item["name"] = element.GetName();
		item["anchor"] = AnchorToString(element.GetAnchor());
		item["position"] = WriteFloat2(element.GetPosition());
		item["visible"] = element.IsVisible();
        if(element.HasExplicitPivot()) item["pivot"]=WriteFloat2(element.GetPivot());
		return item;
	}

	json SerializeElementJson(const UIElement& element) {
		if (const auto* sprite = dynamic_cast<const UISprite*>(&element)) {
			json item = SerializeCommon(*sprite, "sprite");
			item["size"] = { {"width", sprite->GetWidth()}, {"height", sprite->GetHeight()} };
			item["texture"] = sprite->GetTexturePath();
			item["color"] = WriteColor(sprite->GetColor());
			item["rotation"] = sprite->GetRotation();
			AppendElementScript(*sprite, item);
			return item;
		}
		if (const auto* text = dynamic_cast<const UIText*>(&element)) {
			json item = SerializeCommon(*text, "text");
			item["text"] = text->GetText();
			item["color"] = WriteColor(text->GetColor());
			item["fontSize"] = text->GetFontSize();
			item["alignment"] = TextAlignToString(text->GetAlignment());
			item["characterSpacing"] = text->GetCharacterSpacing();
			if (!text->GetFontPath().empty()) {
				item["font"] = {
					{"texture", text->GetFontPath()},
					{"charWidth", text->GetFontCharWidth()},
					{"charHeight", text->GetFontCharHeight()},
					{"charsPerRow", text->GetFontCharsPerRow()}
				};
			}
			AppendElementScript(*text, item);
			return item;
		}
		if (const auto* button = dynamic_cast<const UIButton*>(&element)) {
			json item = SerializeCommon(*button, "button");
			item["size"] = { {"width", button->GetWidth()}, {"height", button->GetHeight()} };
			item["text"] = button->GetText();
			item["textScale"] = button->GetTextScale();
			item["color"] = WriteColor(button->GetColor());
			item["hoverColor"] = WriteColor(button->GetHoverColor());
			item["pressedColor"] = WriteColor(button->GetPressedColor());
			item["textColor"] = WriteColor(button->GetTextColor());
			item["textHoverColor"] = WriteColor(button->GetTextHoverColor());
			if (!button->GetTexturePath().empty()) item["texture"] = button->GetTexturePath();
			if (!button->GetHoverTexturePath().empty()) item["hoverTexture"] = button->GetHoverTexturePath();
			if (!button->GetPressedTexturePath().empty()) item["pressedTexture"] = button->GetPressedTexturePath();
			if (!button->GetAction().empty()) {
				item["onClick"] = {
					{"script", button->GetAction()},
					{"scene", button->GetActionScene()},
					{"play", button->GetActionPlayOnLoad()}
				};
			}
			if (!UIButton::GetFontPath().empty()) {
				item["font"] = {
					{"texture", UIButton::GetFontPath()},
					{"charWidth", UIButton::GetFontCharWidth()},
					{"charHeight", UIButton::GetFontCharHeight()},
					{"charsPerRow", UIButton::GetFontCharsPerRow()}
				};
			}
			AppendElementScript(*button, item);
			return item;
		}
		if (const auto* bar = dynamic_cast<const UIBar*>(&element)) {
			json item = SerializeCommon(*bar, "bar");
			item["label"] = bar->GetLabel();
			item["currentValue"] = bar->GetCurrentValue();
			item["maxValue"] = bar->GetMaxValue();
			item["size"] = { {"width", bar->GetBarWidth()}, {"height", bar->GetBarHeight()} };
			item["textColor"] = WriteColor(bar->GetTextColor());
			item["barColor"] = WriteColor(bar->GetBarColor());
			item["backgroundColor"] = WriteColor(bar->GetBackgroundColor());
			item["showText"] = bar->GetShowText();
			item["showValue"] = bar->GetShowValue();
			item["characterSpacing"] = bar->GetCharacterSpacing();
			if (!bar->GetFillTexturePath().empty())
				item["fillTexture"] = bar->GetFillTexturePath();
			if (!bar->GetBackgroundTexturePath().empty())
				item["backgroundTexture"] = bar->GetBackgroundTexturePath();
			AppendElementScript(*bar, item);
			return item;
		}
		return SerializeCommon(element, "unknown");
	}
}

void SceneCanvas_Initialize()
{
	EnsureDefaultButtonActions();
	g_canvas = std::make_unique<UICanvas>("scene");
	g_canvas->Initialize();
	UIButton::SetFontPath("rom/font/LemonSlice.ttf", 32.0f, 32.0f, 16);
	BindSceneHudElements();
}

void SceneCanvas_Finalize()
{
	if (g_canvas)
		g_canvas->Finalize();
	g_canvas.reset();
	UIButton::ShutdownSharedFont();
	g_redOverlay = nullptr;
	g_scoreText = nullptr;
	g_timerText = nullptr;
	g_pendingSceneFile.clear();
	g_pendingScenePlayOnLoad = false;
}

void SceneCanvas_Update(double elapsed_time)
{
	if (g_canvas)
		g_canvas->Update(elapsed_time);
}

void SceneCanvas_Draw()
{
	if (g_canvas)
		g_canvas->Draw();
}

UICanvas* SceneCanvas_GetCanvas()
{
	return g_canvas.get();
}

void SceneCanvas_ClearSceneCanvas()
{
	if (!g_canvas)
		g_canvas = std::make_unique<UICanvas>("scene");
	g_canvas->ClearElements();
	BindSceneHudElements();
}

std::unique_ptr<UICanvas> SceneCanvas_Prepare(const nlohmann::json& canvasJson)
{
    auto canvas = std::make_unique<UICanvas>("scene");
    if (!canvasJson.is_object()) throw std::runtime_error("canvas must be an object");
    if (canvasJson.contains("elements")) {
        const auto& elements = canvasJson["elements"];
        if (!elements.is_array()) throw std::runtime_error("canvas.elements must be an array");
        for (const auto& item : elements) {
            if (!item.is_object()) throw std::runtime_error("canvas element must be an object");
            const auto type = item.value("type", std::string{});
            if (type == "sprite") LoadSpriteElement(*canvas, item, true);
            else if (type == "text") LoadTextElement(*canvas, item, true);
            else if (type == "button") {
                // Validate shared font now; apply it only after the scene commits.
                const auto font = item.value("font", json::object());
                font.value("texture", std::string{});
                font.value("charWidth", 32.0f); font.value("charHeight", 32.0f);
                font.value("charsPerRow", 16);
                LoadButtonElement(*canvas, item, true);
            }
            else if (type == "bar") LoadBarElement(*canvas, item, true);
        }
    }
    return canvas;
}

void SceneCanvas_ApplyPrepared(std::unique_ptr<UICanvas> canvas, const nlohmann::json& canvasJson)
{
    if (g_canvas) g_canvas->Finalize();
    g_canvas = std::move(canvas);
    if (canvasJson.contains("elements")) {
        for (const auto& item : canvasJson["elements"]) {
            if (item.value("type", std::string{}) != "button") continue;
            const auto font = item.value("font", json::object());
            const auto path = font.value("texture", std::string{});
            if (!path.empty()) UIButton::SetFontPath(path, font.value("charWidth", 32.0f),
                font.value("charHeight", 32.0f), font.value("charsPerRow", 16));
        }
    }
    for (const auto& element : g_canvas->GetElements())
        if (auto* button = dynamic_cast<UIButton*>(element.get())) SceneCanvas_BindButtonAction(button);
    g_canvas->Initialize();
    BindSceneHudElements();
}

void SceneCanvas_LoadSceneCanvas(const nlohmann::json& canvasJson)
{
    auto canvas = SceneCanvas_Prepare(canvasJson);
    SceneCanvas_ApplyPrepared(std::move(canvas), canvasJson);
}

nlohmann::json SceneCanvas_SaveSceneCanvas()
{
	json canvas;
	canvas["name"] = g_canvas ? g_canvas->GetName() : "scene";
	canvas["elements"] = json::array();

	if (!g_canvas) return canvas;

	for (const auto& element : g_canvas->GetElements()) {
		if (!element) continue;
		canvas["elements"].push_back(SerializeElementJson(*element));
	}

	return canvas;
}

std::string SceneCanvas_SerializeElement(const UIElement& element)
{
	return SerializeElementJson(element).dump();
}

UIElement* SceneCanvas_InsertElementJson(const std::string& elementJson, int index)
{
	if (!g_canvas) {
		g_canvas = std::make_unique<UICanvas>("scene");
		g_canvas->Initialize();
	}

	json item;
	try {
		item = json::parse(elementJson);
	} catch (...) {
		return nullptr;
	}
	if (!item.is_object()) return nullptr;

	const size_t before = g_canvas->GetElementCount();
	const std::string type = item.value("type", "");
	if (type == "sprite")
		LoadSpriteElement(*g_canvas, item);
	else if (type == "text")
		LoadTextElement(*g_canvas, item);
	else if (type == "button")
		LoadButtonElement(*g_canvas, item);
	else if (type == "bar")
		LoadBarElement(*g_canvas, item);
	else
		return nullptr;

	if (g_canvas->GetElementCount() <= before)
		return nullptr;

	UIElement* created = g_canvas->GetElements().back().get();
	if (created)
		created->Initialize();
	g_canvas->MoveElementToIndex(created, index);
	BindSceneHudElements();
	return created;
}

int SceneCanvas_GetElementIndex(const UIElement* element)
{
	if (!g_canvas || !element) return -1;
	return g_canvas->GetElementIndex(element);
}

void SceneCanvas_RemoveElementInternal(UIElement* element)
{
	if (!g_canvas || !element) return;
	g_canvas->RemoveElement(element);
	BindSceneHudElements();
}

void SceneCanvas_BindButtonAction(UIButton* button)
{
	if (!button) return;

	EnsureDefaultButtonActions();
	auto& registry = ButtonActionRegistry();
	auto it = registry.find(button->GetAction());
	if (it != registry.end())
		it->second(button);
	else
		button->SetOnClick(nullptr);
}

void SceneCanvas_RegisterButtonAction(const std::string& scriptName, SceneCanvasButtonActionBinder binder)
{
	if (scriptName.empty()) return;
	ButtonActionRegistry()[scriptName] = std::move(binder);
}

void SceneCanvas_RequestSceneFile(const std::string& sceneFile, bool playOnLoad)
{
	g_pendingSceneFile = NormalizeSceneFile(sceneFile);
	g_pendingScenePlayOnLoad = playOnLoad;
}

std::string SceneCanvas_ConsumeRequestedSceneFile(bool* playOnLoad)
{
	std::string requested = g_pendingSceneFile;
	if (playOnLoad)
		*playOnLoad = g_pendingScenePlayOnLoad;
	g_pendingSceneFile.clear();
	g_pendingScenePlayOnLoad = false;
	return requested;
}

void SceneCanvas_ShowRedOverlay()
{
	if (g_redOverlay) {
		g_redOverlay->SetVisible(true);
	}
}

void SceneCanvas_HideRedOverlay()
{
	if (g_redOverlay) {
		g_redOverlay->SetVisible(false);
	}
}

void SceneCanvas_SetRedOverlayAlpha(float alpha)
{
	if (g_redOverlay) {
		g_redOverlay->SetColor({ 1.0f, 0.0f, 0.0f, alpha });
	}
}

void SceneCanvas_SetTimerSeconds(float seconds)
{
	if (!g_timerText) return;

	const float clamped = (std::max)(0.0f, seconds);
	const int displaySeconds = static_cast<int>(std::ceil(clamped));
	const int minutes = displaySeconds / 60;
	const int secs = displaySeconds % 60;
	char buffer[16];
	snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
	g_timerText->SetText(buffer);
}
