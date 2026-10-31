/**===========================================================================
 File   : texture.h
 Title  : Texture management
 Author : Terry Yeung
 Date   : 2025/6/6
 Brief  :
   ===========================================================================**/
#ifndef TEXTURE_H
#define TEXTURE_H

#include <d3d11.h>
#include <DirectXMath.h> // XMFLOAT2

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Texture_Finalize();

// Load a texture
// Args
// ptr_file_name : texture file name
// b_mipmap : whether to generate mipmaps
// Returns : texture ID (-1 on failure)
int  TextureLoad(const wchar_t* ptr_file_name, bool mipmap = true);
enum class TextureReloadResult { NotLoaded, Reloaded, Failed };
TextureReloadResult TextureReload(const wchar_t* path);
bool TextureValidateFile(const wchar_t* path);
int TextureLoadQuiet(const wchar_t* ptr_file_name, bool mipmap = true);
int  TextureCreateFromRGBA(const wchar_t* name, const unsigned char* rgba, int width, int height);
int  TextureGetWhiteTexture();
void TextureAllRelease();
void TextureSetTexture(int texture_id);

DirectX::XMUINT2 TextureGetSize(int texture_id);
const unsigned int& TextureGetWidth(int texture_id);
const unsigned int& TextureGetHeight(int texture_id);
ID3D11ShaderResourceView* TextureGetSRV(int texture_id);

// Cubemap loader (DDS only). Returns cached SRV or nullptr on failure.
ID3D11ShaderResourceView* CubemapLoad(const wchar_t* fileName);
void CubemapReleaseAll();
TextureReloadResult CubemapReload(const wchar_t* path);

#endif // TEXTURE_H