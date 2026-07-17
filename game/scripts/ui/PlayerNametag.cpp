/*****************************************************
 * File: game/scripts/ui/PlayerNametag.cpp
 * Brief: Draw YOU / P2–P4 nametags as ASCII atlas billboards
 *****************************************************/
#include "PlayerNametag.h"
#include "PlayerColor.h"
#include "engine/scripting/script_runtime_api.h"

namespace {
	constexpr wchar_t kFontPath[] = L"rom/texture/consolab_ascii_512.png";
	constexpr float kAtlasSize = 512.0f;
	constexpr float kCharPx = 32.0f;
	constexpr int kCharsPerRow = 16;
	constexpr float kHeightOffset = 2.0f;
	constexpr float kGlyphWorld = 0.44f;

	int g_fontTextureId = -1;
	ID3D11ShaderResourceView* g_fontSrv = nullptr;

	bool EnsureFontLoaded() {
		if (g_fontSrv)
			return true;
		g_fontTextureId = ScriptApi_TextureLoad(kFontPath, false);
		if (g_fontTextureId < 0)
			return false;
		g_fontSrv = ScriptApi_TextureGetSRV(g_fontTextureId);
		return g_fontSrv != nullptr;
	}

	void GlyphUv(char c, DirectX::XMFLOAT2& outOffset, DirectX::XMFLOAT2& outScale) {
		const int texIndex = static_cast<unsigned char>(c) - 32;
		const int col = texIndex % kCharsPerRow;
		const int row = texIndex / kCharsPerRow;
		const float uSize = kCharPx / kAtlasSize;
		const float vSize = kCharPx / kAtlasSize;
		outOffset = { static_cast<float>(col) * uSize, static_cast<float>(row) * vSize };
		outScale = { uSize, vSize };
	}
} // namespace

const char* PlayerNametag_LabelForIndex(int playerIndex) {
	switch (playerIndex) {
	case 0: return "YOU";
	case 1: return "P2";
	case 2: return "P3";
	case 3: return "P4";
	default: return "P?";
	}
}

void PlayerNametag_Draw(int playerIndex, const DirectX::XMFLOAT3& worldFeetPos) {
	if (!EnsureFontLoaded())
		return;

	const char* label = PlayerNametag_LabelForIndex(playerIndex);
	if (!label || !label[0])
		return;

	int glyphCount = 0;
	for (const char* p = label; *p; ++p) {
		const unsigned char c = static_cast<unsigned char>(*p);
		if (c >= 32 && c <= 126)
			++glyphCount;
	}
	if (glyphCount <= 0)
		return;

	DirectX::XMFLOAT4X4 view{};
	ScriptApi_GetCameraViewMatrix(&view);
	const DirectX::XMFLOAT3 right = { view._11, view._21, view._31 };

	const DirectX::XMFLOAT4 color = PlayerColor_GetTint(PlayerColor_GetLobbySlot(playerIndex));
	const float totalWidth = static_cast<float>(glyphCount) * kGlyphWorld;
	float cursorX = -0.5f * totalWidth + 0.5f * kGlyphWorld;
	const float y = worldFeetPos.y + kHeightOffset;

	for (const char* p = label; *p; ++p) {
		const unsigned char c = static_cast<unsigned char>(*p);
		if (c < 32 || c > 126)
			continue;

		DirectX::XMFLOAT2 uvOffset;
		DirectX::XMFLOAT2 uvScale;
		GlyphUv(static_cast<char>(c), uvOffset, uvScale);

		const DirectX::XMFLOAT3 pos = {
			worldFeetPos.x + right.x * cursorX,
			y + right.y * cursorX,
			worldFeetPos.z + right.z * cursorX
		};

		ScriptApi_BillboardAddUv(
			pos,
			{ kGlyphWorld, kGlyphWorld },
			color,
			g_fontSrv,
			0.0f,
			false,
			uvOffset,
			uvScale);

		cursorX += kGlyphWorld;
	}
}
