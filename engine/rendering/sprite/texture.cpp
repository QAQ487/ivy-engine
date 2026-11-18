/**===========================================================================
 File   : texture.cpp
 Title  : Texture management
 Author : Terry Yeung
 Date   : 2025/6/6
 Brief  :
 ===========================================================================**/
#include "texture.h"
#include "engine/ui/ui_font.h"
#include "engine/core/content_path.h"
#include "engine/core/texture_cook_policy.h"
#include <string>
#include <filesystem>
#include <cstdio>
#include <Windows.h>
 // #include "direct3d.h" // SAFE_RELEASE
#include "engine/rendering/d3d11/renderer.h" // Device / context accessors
#include "DirectXTex.h" // DirectXTex library
#include "engine/debug_draw/debug_ofstream.h" // hal::dout
using namespace DirectX;

static constexpr int MAX_TEXTURES = 1024; // Max texture slots

struct Texture {
	std::wstring  m_filename;
	bool mipmap = true;
	unsigned int m_width = 0;
	unsigned int m_height = 0;
	ID3D11ShaderResourceView* m_ptr_Texture = nullptr; // Texture SRV (one per slot)
};

static Texture g_textures[MAX_TEXTURES]; // Texture slot array
static int g_whiteTextureId = -1;

// Provided by caller; do not Release.
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) {
	// Store device and device context
	g_pDevice = pDevice;
	g_pContext = pContext;
}

static constexpr int MAX_CUBEMAPS = 64;

struct CubemapEntry {
	std::wstring filename;
	ID3D11ShaderResourceView* srv = nullptr;
};

static CubemapEntry g_cubemaps[MAX_CUBEMAPS];

void Texture_Finalize() {
	TextureAllRelease();
	CubemapReleaseAll();
}

static std::wstring TexturePathKey(const std::filesystem::path& path) {
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(path, ec);
    return (ec ? path : absolute).lexically_normal().wstring();
}
// A packaged game carries the cooked BC7 .dds instead of the source PNG/JPG/BMP
// (playable package texture cook). Prefer the file the content names; when it is
// not there, use the .dds with the same name. Otherwise keep the old behaviour.
static std::wstring ResolveTexturePath(const wchar_t* path) {
    std::wstring primary = content_path::ReadableWideCurrent(path);
    std::error_code ec;
    if (!primary.empty() && std::filesystem::is_regular_file(std::filesystem::path(primary), ec))
        return primary;
    const std::wstring sibling = texture_cook::DdsSiblingPath(std::wstring(path));
    if (!sibling.empty()) {
        const std::wstring dds = content_path::ReadableWideCurrent(sibling.c_str());
        ec.clear();
        if (!dds.empty() && std::filesystem::is_regular_file(std::filesystem::path(dds), ec))
            return dds;
    }
    return primary;
}
static HRESULT ReadTextureImage(const wchar_t* path, TexMetadata& metadata, ScratchImage& image) {
    const auto ext = std::filesystem::path(path).extension().wstring();
    if (_wcsicmp(ext.c_str(), L".dds") == 0) return LoadFromDDSFile(path, DDS_FLAGS_NONE, &metadata, image);
    if (_wcsicmp(ext.c_str(), L".tga") == 0) return LoadFromTGAFile(path, &metadata, image);
    return LoadFromWICFile(path, WIC_FLAGS_NONE, &metadata, image);
}

static int TextureLoadInternal(const wchar_t* ptr_file_name, bool mipmap, bool reportErrors) {
    if (!ptr_file_name || !ptr_file_name[0] || !g_pDevice) return -1;
    const std::wstring resolvedName = ResolveTexturePath(ptr_file_name);
    const wchar_t* openPath = resolvedName.empty() ? ptr_file_name : resolvedName.c_str();
	// Return existing ID if this file is already loaded
	for (int i = 0; i < MAX_TEXTURES; i++) {
		if (!g_textures[i].m_ptr_Texture) continue; // Skip empty slots
		if (_wcsicmp(TexturePathKey(g_textures[i].m_filename).c_str(), TexturePathKey(ptr_file_name).c_str()) == 0) { // Same file already loaded; return its ID
			return i;
		}
	}

	/* Create new */
	for (int i = 0; i < MAX_TEXTURES; i++) {
		// Find a free slot
		if (g_textures[i].m_ptr_Texture) continue; // Skip occupied slots

		/* Load texture */
		TexMetadata metadata;
		ScratchImage image;
		HRESULT loadHr = ReadTextureImage(openPath, metadata, image);
		if (FAILED(loadHr)) {
			char pathUtf8[MAX_PATH * 3] = {};
			WideCharToMultiByte(CP_UTF8, 0, ptr_file_name, -1, pathUtf8, sizeof(pathUtf8),
				nullptr, nullptr);
			char msg[512];
			snprintf(msg, sizeof(msg), "Failed to load texture:\n%s", pathUtf8);
			if (reportErrors) MessageBoxA(nullptr, msg, "Texture Error", MB_OK | MB_ICONERROR);
			return -1;
		}
		// Source file size
		g_textures[i].m_width = (unsigned int)metadata.width; // sizet = longlong
		g_textures[i].m_height = (unsigned int)metadata.height;

		// Generate mipmaps (block-compressed images arrive with their mips baked in)
		if (mipmap && !IsCompressed(metadata.format)) {
			ScratchImage mipChain;
			GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
			// TEX_FILTER_BOX | TEX_FILTER_FORCE_NOW_WIC
			image = std::move(mipChain); // Move mipChain into image
			metadata = image.GetMetadata(); // Refresh metadata
		}

		// A cooked texture always carries mips; a caller that asked for none (UI, sprites)
		// gets mip 0 only, so it samples like the source image did.
		size_t imageCount = image.GetImageCount();
		if (!mipmap && IsCompressed(metadata.format) && metadata.mipLevels > 1 &&
			metadata.arraySize == 1 && metadata.dimension == TEX_DIMENSION_TEXTURE2D && !metadata.IsCubemap()) {
			metadata.mipLevels = 1;
			imageCount = 1;
		}

		// Create shader resource view
		HRESULT hr = CreateShaderResourceView(g_pDevice, image.GetImages(), imageCount, metadata, &g_textures[i].m_ptr_Texture);
		if (FAILED(hr)) {
			if (reportErrors) MessageBoxA(nullptr, "Failed to create texture shader resource view.", "Texture Error", MB_OK | MB_ICONERROR);
			break;
		}

		g_textures[i].mipmap = mipmap;
		g_textures[i].m_filename = ptr_file_name; // Store filename

		return i;
	}
	return -1;
}

int TextureCreateFromRGBA(const wchar_t* name, const unsigned char* rgba, int width, int height) {
	if (!name || !rgba || width <= 0 || height <= 0 || !g_pDevice)
		return -1;

	for (int i = 0; i < MAX_TEXTURES; i++) {
		if (!g_textures[i].m_ptr_Texture) continue;
		if (g_textures[i].m_filename == name) {
            if (g_textures[i].m_width != static_cast<unsigned>(width) || g_textures[i].m_height != static_cast<unsigned>(height)) return -1;
            ID3D11Resource* resource = nullptr;
            g_textures[i].m_ptr_Texture->GetResource(&resource);
            g_pContext->UpdateSubresource(resource, 0, nullptr, rgba, static_cast<UINT>(width * 4), 0);
            resource->Release();
            return i;
        }
	}

	for (int i = 0; i < MAX_TEXTURES; i++) {
		if (g_textures[i].m_ptr_Texture) continue;

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = static_cast<UINT>(width);
		desc.Height = static_cast<UINT>(height);
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = rgba;
		initData.SysMemPitch = static_cast<UINT>(width * 4);

		ID3D11Texture2D* texture = nullptr;
		HRESULT hr = g_pDevice->CreateTexture2D(&desc, &initData, &texture);
		if (FAILED(hr)) return -1;

		hr = g_pDevice->CreateShaderResourceView(texture, nullptr, &g_textures[i].m_ptr_Texture);
		SAFE_RELEASE(texture);
		if (FAILED(hr)) return -1;

		g_textures[i].m_filename = name;
		g_textures[i].m_width = static_cast<unsigned int>(width);
		g_textures[i].m_height = static_cast<unsigned int>(height);
		return i;
	}

	return -1;
}

int TextureGetWhiteTexture() {
	if (g_whiteTextureId >= 0 && g_textures[g_whiteTextureId].m_ptr_Texture)
		return g_whiteTextureId;

	for (int i = 0; i < MAX_TEXTURES; i++) {
		if (g_textures[i].m_ptr_Texture) continue;

		const unsigned int whitePixel = 0xffffffffu;
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = 1;
		desc.Height = 1;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = &whitePixel;
		initData.SysMemPitch = sizeof(whitePixel);

		ID3D11Texture2D* texture = nullptr;
		HRESULT hr = g_pDevice->CreateTexture2D(&desc, &initData, &texture);
		if (FAILED(hr)) return -1;

		hr = g_pDevice->CreateShaderResourceView(texture, nullptr, &g_textures[i].m_ptr_Texture);
		SAFE_RELEASE(texture);
		if (FAILED(hr)) return -1;

		g_textures[i].m_filename = L"__builtin_white__";
		g_textures[i].m_width = 1;
		g_textures[i].m_height = 1;
		g_whiteTextureId = i;
		return g_whiteTextureId;
	}

	return -1;
}

void TextureAllRelease() {
	UIFont_ClearCache();
	for (Texture& t : g_textures) {
		SAFE_RELEASE(t.m_ptr_Texture); // Release texture
	}
	g_whiteTextureId = -1;
}

void TextureSetTexture(int texture_id) {
	if (texture_id < 0 || texture_id >= MAX_TEXTURES) {
		hal::dout << "textureSetTexture() : invalid texture ID" << std::endl;
		return; // Ignore invalid ID
	}
	// Bind texture
	g_pContext->PSSetShaderResources(0, 1, &g_textures[texture_id].m_ptr_Texture);
}

DirectX::XMUINT2 TextureGetSize(int texture_id) {
	if (texture_id < 0 || texture_id >= MAX_TEXTURES) return { 0, 0 };
	return { g_textures[texture_id].m_width, g_textures[texture_id].m_height };
}

const unsigned int& TextureGetWidth(int texture_id) {
	static const unsigned int kZero = 0;
	if (texture_id < 0 || texture_id >= MAX_TEXTURES) return kZero;
	return g_textures[texture_id].m_width;
}

const unsigned int& TextureGetHeight(int texture_id) {
	static const unsigned int kZero = 0;
	if (texture_id < 0 || texture_id >= MAX_TEXTURES) return kZero;
	return g_textures[texture_id].m_height;
}

// Accessor
ID3D11ShaderResourceView* TextureGetSRV(int texture_id) {
    if (texture_id < 0 || texture_id >= MAX_TEXTURES) {
        return nullptr;
    }
    return g_textures[texture_id].m_ptr_Texture;
}

ID3D11ShaderResourceView* CubemapLoad(const wchar_t* fileName) {
	if (!fileName || !fileName[0] || !g_pDevice) return nullptr;
	const std::wstring resolvedName = content_path::ReadableWideCurrent(fileName);
	const wchar_t* openPath = resolvedName.empty() ? fileName : resolvedName.c_str();

	for (int i = 0; i < MAX_CUBEMAPS; ++i) {
		if (!g_cubemaps[i].srv) continue;
		if (_wcsicmp(TexturePathKey(g_cubemaps[i].filename).c_str(), TexturePathKey(fileName).c_str()) == 0)
			return g_cubemaps[i].srv;
	}

	char pathUtf8[MAX_PATH * 3] = {};
	WideCharToMultiByte(CP_UTF8, 0, fileName, -1, pathUtf8, sizeof(pathUtf8), nullptr, nullptr);

	TexMetadata metadata;
	ScratchImage image;
	const HRESULT loadHr = LoadFromDDSFile(openPath, DDS_FLAGS_NONE, &metadata, image);
	if (FAILED(loadHr)) {
		char msg[512];
		snprintf(msg, sizeof(msg), "CubemapLoad failed: %s hr=0x%08lX\n",
			pathUtf8, static_cast<unsigned long>(loadHr));
		hal::dout << msg;
		return nullptr;
	}
	if (!metadata.IsCubemap()) {
		hal::dout << "CubemapLoad failed: " << pathUtf8 << " is not a cubemap." << std::endl;
		return nullptr;
	}

	ID3D11ShaderResourceView* srv = nullptr;
	const HRESULT hr = CreateShaderResourceView(
		g_pDevice, image.GetImages(), image.GetImageCount(), metadata, &srv);
	if (FAILED(hr)) {
		char msg[512];
		snprintf(msg, sizeof(msg), "CubemapLoad SRV failed: %s hr=0x%08lX\n",
			pathUtf8, static_cast<unsigned long>(hr));
		hal::dout << msg;
		return nullptr;
	}

	for (int i = 0; i < MAX_CUBEMAPS; ++i) {
		if (g_cubemaps[i].srv) continue;
		g_cubemaps[i].filename = fileName;
		g_cubemaps[i].srv = srv;
		return srv;
	}

	srv->Release();
	return nullptr;
}

void CubemapReleaseAll() {
	for (CubemapEntry& entry : g_cubemaps) {
		SAFE_RELEASE(entry.srv);
		entry.filename.clear();
	}
}

int TextureLoad(const wchar_t* path, bool mipmap) { return TextureLoadInternal(path,mipmap,true); }
int TextureLoadQuiet(const wchar_t* path, bool mipmap) { return TextureLoadInternal(path,mipmap,false); }


bool TextureIsLoaded(const wchar_t* path) {
    if (!path || !path[0]) return false;
    const auto key = TexturePathKey(path);
    for (const auto& slot : g_textures) {
        if (!slot.m_ptr_Texture) continue;
        if (_wcsicmp(TexturePathKey(slot.m_filename).c_str(), key.c_str()) == 0) return true;
    }
    return false;
}

bool CubemapIsLoaded(const wchar_t* path) {
    if (!path || !path[0]) return false;
    const auto key = TexturePathKey(path);
    for (const auto& slot : g_cubemaps) {
        if (!slot.srv) continue;
        if (_wcsicmp(TexturePathKey(slot.filename).c_str(), key.c_str()) == 0) return true;
    }
    return false;
}

TextureReloadResult TextureReload(const wchar_t* path) {
    if (!path || !g_pDevice) return TextureReloadResult::Failed;
    const std::wstring resolvedName = ResolveTexturePath(path);
    const wchar_t* openPath = resolvedName.empty() ? path : resolvedName.c_str();
    TexMetadata metadata; ScratchImage image;
    if (FAILED(ReadTextureImage(openPath, metadata, image))) return TextureReloadResult::Failed;
    const auto key = TexturePathKey(path);
    for (auto& slot : g_textures) {
        if (!slot.m_ptr_Texture || _wcsicmp(TexturePathKey(slot.m_filename).c_str(), key.c_str()) != 0) continue;
        HRESULT hr = S_OK;
        if (slot.mipmap && metadata.mipLevels == 1 && !IsCompressed(metadata.format)) {
            ScratchImage chain;
            hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), metadata, TEX_FILTER_DEFAULT, 0, chain);
            if (FAILED(hr)) return TextureReloadResult::Failed;
            image = std::move(chain); metadata = image.GetMetadata();
        }
        size_t imageCount = image.GetImageCount();
        if (!slot.mipmap && IsCompressed(metadata.format) && metadata.mipLevels > 1 &&
            metadata.arraySize == 1 && metadata.dimension == TEX_DIMENSION_TEXTURE2D && !metadata.IsCubemap()) {
            metadata.mipLevels = 1; imageCount = 1;
        }
        ID3D11ShaderResourceView* replacement = nullptr;
        if (FAILED(CreateShaderResourceView(g_pDevice, image.GetImages(), imageCount, metadata, &replacement))) return TextureReloadResult::Failed;
        auto* previous = slot.m_ptr_Texture;
        slot.m_ptr_Texture = replacement; slot.m_width = static_cast<unsigned int>(metadata.width); slot.m_height = static_cast<unsigned int>(metadata.height);
        previous->Release();
        return TextureReloadResult::Reloaded;
    }
    return TextureReloadResult::NotLoaded;
}

TextureReloadResult CubemapReload(const wchar_t* path) {
    const std::wstring resolvedName = content_path::ReadableWideCurrent(path);
    const wchar_t* openPath = resolvedName.empty() ? path : resolvedName.c_str();
    for (auto& slot : g_cubemaps) {
        if (!slot.srv || _wcsicmp(TexturePathKey(slot.filename).c_str(), TexturePathKey(path).c_str())) continue;
        TexMetadata metadata; ScratchImage image;
        if (FAILED(LoadFromDDSFile(openPath, DDS_FLAGS_NONE, &metadata, image)) || !metadata.IsCubemap()) return TextureReloadResult::Failed;
        ID3D11ShaderResourceView* replacement = nullptr;
        if (FAILED(CreateShaderResourceView(g_pDevice, image.GetImages(), image.GetImageCount(), metadata, &replacement))) return TextureReloadResult::Failed;
        auto* previous = slot.srv; slot.srv = replacement; previous->Release();
        return TextureReloadResult::Reloaded;
    }
    return TextureReloadResult::NotLoaded;
}

bool TextureValidateFile(const wchar_t* path) {
    if (!path || !path[0]) return false;
    TexMetadata metadata; ScratchImage image;
    return SUCCEEDED(ReadTextureImage(path, metadata, image));
}
