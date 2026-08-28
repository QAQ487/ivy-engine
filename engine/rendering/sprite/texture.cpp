/**===========================================================================
 File   : texture.cpp
 Title  : �e�N�X�`���Ǘ�
 Author : Terry
 Date   : 2025/6/6
 Brief  :
 ===========================================================================**/
#include "texture.h"
#include <string>
#include <cstdio>
#include <Windows.h>
 // #include "direct3d.h" // SAFE_RELEASE
#include "engine/rendering/d3d11/renderer.h" // �f�o�C�X�ƃR���e�L�X�g�̎擾
#include "DirectXTex.h" // DirectXTex���C�u������g�p
#include "engine/debug_draw/debug_ofstream.h" // hal::dout
using namespace DirectX;

static constexpr int MAX_TEXTURES = 1024; // �ő�e�N�X�`����

struct Texture {
	std::wstring  m_filename;
	unsigned int m_width = 0;
	unsigned int m_height = 0;
	ID3D11ShaderResourceView* m_ptr_Texture = nullptr; // �e�N�X�`���i�Q�C�R���Ƒ��₷�ɂ͂���𑝂₷�j// �P�ɂ��P��
};

static Texture g_textures[MAX_TEXTURES]; // �e�N�X�`���Ǘ��p�z��
static int g_whiteTextureId = -1;

// ���ӁI�������ŊO������ݒ肳����́BRelease�s�v�B
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) {
	// �f�o�C�X�ƃf�o�C�X�R���e�L�X�g�̕ۑ�
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

int TextureLoad(const wchar_t* ptr_file_name, bool mipmap) {
	// ���łɓǂݍ���ł����t�@�C����Ή�������
	for (int i = 0; i < MAX_TEXTURES; i++) {
		if (!g_textures[i].m_ptr_Texture) continue; // ���łɃe�N�X�`�����ǂݍ��܂�Ă���ꍇ�̓X�L�b�v
		if (g_textures[i].m_filename == ptr_file_name) { // ���łɓ����t�@�C�����̃e�N�X�`�����ǂݍ��܂�Ă���ꍇ�́A���̃e�N�X�`����ID��Ԃ�
			return i;
		}
	}

	/* �Ǘ����� */
	for (int i = 0; i < MAX_TEXTURES; i++) {
		// �󂢂Ă���ꏊ��T��
		if (g_textures[i].m_ptr_Texture) continue; // ���łɃe�N�X�`�����ǂݍ��܂�Ă���ꍇ�̓X�L�b�v

		/* �e�N�X�`���ǂݍ��� */
		TexMetadata metadata;
		ScratchImage image;
		HRESULT loadHr = LoadFromWICFile(ptr_file_name, WIC_FLAGS_NONE, &metadata, image);
		if (FAILED(loadHr)) {
			char pathUtf8[MAX_PATH * 3] = {};
			WideCharToMultiByte(CP_UTF8, 0, ptr_file_name, -1, pathUtf8, sizeof(pathUtf8),
				nullptr, nullptr);
			char msg[512];
			snprintf(msg, sizeof(msg), "Failed to load texture:\n%s", pathUtf8);
			MessageBoxA(nullptr, msg, "Texture Error", MB_OK | MB_ICONERROR);
			return -1;
		}
		// �摜�t�@�C���̃T�C�Y��擾
		g_textures[i].m_width = (unsigned int)metadata.width; // sizet = longlong
		g_textures[i].m_height = (unsigned int)metadata.height;

		// �~�b�v�}�b�v��쐬����
		if (mipmap) {
			ScratchImage mipChain;
			GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
			// TEX_FILTER_BOX | TEX_FILTER_FORCE_NOW_WIC
			image = std::move(mipChain); // mipChain��image�Ɉړ�
			metadata = image.GetMetadata(); // ���^�f�[�^��X�V
		}

		// �V�F�[�_�[���\�[�X�p�b�N�r���[�̐���
		HRESULT hr = CreateShaderResourceView(g_pDevice, image.GetImages(), image.GetImageCount(), metadata, &g_textures[i].m_ptr_Texture);
		if (FAILED(hr)) {
			MessageBoxA(nullptr, "Failed to create texture shader resource view.", "Texture Error", MB_OK | MB_ICONERROR);
			break;
		}

		g_textures[i].m_filename = ptr_file_name; // �t�@�C������ۑ�

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
	for (Texture& t : g_textures) {
		SAFE_RELEASE(t.m_ptr_Texture); // �e�N�X�`���̉��
	}
	g_whiteTextureId = -1;
}

void TextureSetTexture(int texture_id) {
	if (texture_id < 0 || texture_id >= MAX_TEXTURES) {
		hal::dout << "textureSetTexture() : �����ȃe�N�X�`��ID�ł�" << std::endl;
		return; // ������ID�̏ꍇ�͉�����Ȃ�
	}
	// �e�N�X�`���ݒ�
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

// �֐���ǉ�
ID3D11ShaderResourceView* TextureGetSRV(int texture_id) {
    if (texture_id < 0 || texture_id >= MAX_TEXTURES) {
        return nullptr;
    }
    return g_textures[texture_id].m_ptr_Texture;
}

ID3D11ShaderResourceView* CubemapLoad(const wchar_t* fileName) {
	if (!fileName || !fileName[0] || !g_pDevice) return nullptr;

	for (int i = 0; i < MAX_CUBEMAPS; ++i) {
		if (!g_cubemaps[i].srv) continue;
		if (g_cubemaps[i].filename == fileName)
			return g_cubemaps[i].srv;
	}

	TexMetadata metadata;
	ScratchImage image;
	const HRESULT loadHr = LoadFromDDSFile(fileName, DDS_FLAGS_NONE, &metadata, image);
	if (FAILED(loadHr) || !metadata.IsCubemap()) {
		hal::dout << "CubemapLoad failed." << std::endl;
		return nullptr;
	}

	ID3D11ShaderResourceView* srv = nullptr;
	const HRESULT hr = CreateShaderResourceView(
		g_pDevice, image.GetImages(), image.GetImageCount(), metadata, &srv);
	if (FAILED(hr)) return nullptr;

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
