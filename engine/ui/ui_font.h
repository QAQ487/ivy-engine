// ui_font.h
// TrueType font atlas loader (stb_truetype) for UI text.

#pragma once

#include <string>

struct UIGlyph {
	int srcX = 0;
	int srcY = 0;
	int srcW = 0;
	int srcH = 0;
	float xoff = 0.0f;
	float yoff = 0.0f;
	float xadvance = 0.0f;
};

struct UIFont {
	int textureId = -1;
	int atlasWidth = 0;
	int atlasHeight = 0;
	float bakeSize = 64.0f;
	float ascent = 0.0f;
	float descent = 0.0f;
	float lineHeight = 0.0f;
	UIGlyph glyphs[95]{};
};

const UIFont* UIFont_Get(const wchar_t* ttfPath);
void UIFont_ClearCache();
bool UIFont_IsTrueTypePath(const wchar_t* path);
bool UIFont_IsTrueTypePath(const std::string& path);

bool UIFont_Reload(const wchar_t* path);
