#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "libraries/imgui/imstb_truetype.h"

#include "ui_font.h"
#include "engine/rendering/sprite/texture.h"

#include <cctype>
#include <cstdio>
#include <cwctype>
#include <map>
#include <string>
#include <vector>
#include <Windows.h>

namespace {
	constexpr int kFirstChar = 32;
	constexpr int kCharCount = 95;
	constexpr float kBakeSize = 64.0f;
	constexpr int kAtlasSize = 1024;

	std::map<std::wstring, UIFont> g_fonts;

	bool EndsWithIgnoreCase(const std::wstring& value, const wchar_t* suffix) {
		const size_t suffixLen = wcslen(suffix);
		if (value.size() < suffixLen)
			return false;
		for (size_t i = 0; i < suffixLen; ++i) {
			const wchar_t a = towlower(value[value.size() - suffixLen + i]);
			const wchar_t b = towlower(suffix[i]);
			if (a != b)
				return false;
		}
		return true;
	}

	bool EndsWithIgnoreCase(const std::string& value, const char* suffix) {
		const size_t suffixLen = strlen(suffix);
		if (value.size() < suffixLen)
			return false;
		for (size_t i = 0; i < suffixLen; ++i) {
			const char a = static_cast<char>(tolower(static_cast<unsigned char>(
				value[value.size() - suffixLen + i])));
			const char b = static_cast<char>(tolower(static_cast<unsigned char>(suffix[i])));
			if (a != b)
				return false;
		}
		return true;
	}

	std::vector<unsigned char> LoadFile(const wchar_t* path) {
		FILE* file = nullptr;
		if (_wfopen_s(&file, path, L"rb") != 0 || !file)
			return {};
		fseek(file, 0, SEEK_END);
		const long size = ftell(file);
		fseek(file, 0, SEEK_SET);
		if (size <= 0) {
			fclose(file);
			return {};
		}
		std::vector<unsigned char> data(static_cast<size_t>(size));
		const size_t read = fread(data.data(), 1, data.size(), file);
		fclose(file);
		if (read != data.size())
			return {};
		return data;
	}

	bool BakeFont(const wchar_t* ttfPath, UIFont& out) {
		const std::vector<unsigned char> ttf = LoadFile(ttfPath);
		if (ttf.empty())
			return false;

		stbtt_fontinfo info{};
		if (!stbtt_InitFont(&info, ttf.data(), stbtt_GetFontOffsetForIndex(ttf.data(), 0)))
			return false;

		int ascent = 0, descent = 0, lineGap = 0;
		stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
		const float pixelScale = stbtt_ScaleForPixelHeight(&info, kBakeSize);

		std::vector<unsigned char> alpha(static_cast<size_t>(kAtlasSize * kAtlasSize), 0);
		stbtt_bakedchar baked[kCharCount]{};
		const int bakeResult = stbtt_BakeFontBitmap(
			ttf.data(), 0, kBakeSize,
			alpha.data(), kAtlasSize, kAtlasSize,
			kFirstChar, kCharCount, baked);
		if (bakeResult <= 0)
			return false;

		std::vector<unsigned char> rgba(static_cast<size_t>(kAtlasSize * kAtlasSize * 4));
		for (int i = 0; i < kAtlasSize * kAtlasSize; ++i) {
			rgba[static_cast<size_t>(i) * 4 + 0] = 255;
			rgba[static_cast<size_t>(i) * 4 + 1] = 255;
			rgba[static_cast<size_t>(i) * 4 + 2] = 255;
			rgba[static_cast<size_t>(i) * 4 + 3] = alpha[static_cast<size_t>(i)];
		}

		std::wstring atlasName = L"__ttf_atlas__:";
		atlasName += ttfPath;
		const int textureId = TextureCreateFromRGBA(atlasName.c_str(), rgba.data(), kAtlasSize, kAtlasSize);
		if (textureId < 0)
			return false;

		out.textureId = textureId;
		out.atlasWidth = kAtlasSize;
		out.atlasHeight = kAtlasSize;
		out.bakeSize = kBakeSize;
		out.ascent = static_cast<float>(ascent) * pixelScale;
		out.descent = static_cast<float>(descent) * pixelScale;
		out.lineHeight = static_cast<float>(ascent - descent + lineGap) * pixelScale;
		for (int i = 0; i < kCharCount; ++i) {
			UIGlyph& glyph = out.glyphs[i];
			glyph.srcX = baked[i].x0;
			glyph.srcY = baked[i].y0;
			glyph.srcW = baked[i].x1 - baked[i].x0;
			glyph.srcH = baked[i].y1 - baked[i].y0;
			glyph.xoff = baked[i].xoff;
			glyph.yoff = baked[i].yoff;
			glyph.xadvance = baked[i].xadvance;
		}
		return true;
	}
}

bool UIFont_IsTrueTypePath(const wchar_t* path) {
	if (!path || !path[0])
		return false;
	const std::wstring value(path);
	return EndsWithIgnoreCase(value, L".ttf") || EndsWithIgnoreCase(value, L".otf");
}

bool UIFont_IsTrueTypePath(const std::string& path) {
	return EndsWithIgnoreCase(path, ".ttf") || EndsWithIgnoreCase(path, ".otf");
}

const UIFont* UIFont_Get(const wchar_t* ttfPath) {
	if (!ttfPath || !ttfPath[0])
		return nullptr;

	const std::wstring key(ttfPath);
	const auto found = g_fonts.find(key);
	if (found != g_fonts.end())
		return &found->second;

	UIFont font;
	if (!BakeFont(ttfPath, font)) {
		char pathUtf8[MAX_PATH * 3] = {};
		WideCharToMultiByte(CP_UTF8, 0, ttfPath, -1, pathUtf8, sizeof(pathUtf8), nullptr, nullptr);
		char msg[512];
		snprintf(msg, sizeof(msg), "Failed to load TTF font:\n%s", pathUtf8);
		MessageBoxA(nullptr, msg, "Font Error", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	auto inserted = g_fonts.emplace(key, font);
	return &inserted.first->second;
}

void UIFont_ClearCache() {
	g_fonts.clear();
}
