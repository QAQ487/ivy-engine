/**===========================================================================

 File   :  [model_util.h]

 Author : Terry Yeung
 Date   : 2025/10/18
 -----------------------------------------------------------------------------


 ===========================================================================**/
#ifndef MODEL_UTIL_H
#define MODEL_UTIL_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

namespace fbxsdk {
	class FbxManager;
	class FbxIOSettings;
	class FbxScene;
	class FbxNode;
	class FbxMesh;
	class FbxTexture;
	class FbxFileTexture;
	class FbxSurfaceMaterial;
	class FbxAMatrix;
}

struct PNTKey {
	int p = 0; int t = 0; int n = 0;
	bool operator==(const PNTKey& o) const noexcept { return p == o.p && t == o.t && n == o.n; }
};
struct PNTKeyHasher {
	size_t operator()(const PNTKey& k) const noexcept {
		return (size_t) k.p * 73856093u ^ (size_t) k.t * 19349663u ^ (size_t) k.n * 83492791u;
	}
};

// ������/�p�X���[�e�B���e�B
void Trim(std::string& s);
std::vector<std::string> Split(const std::string& s, char sep);
std::string WideToUTF8(const std::wstring& ws);
std::wstring Utf8ToWide(const char* s);
std::wstring DirnameW(const std::wstring& path);
std::wstring JoinPathW(const std::wstring& dir, const std::wstring& file);
bool IsAbsolutePathW(const std::wstring& p);

// �e�N�X�`���֘A
bool LoadTextureWIC_SRV(const wchar_t* texPath, ID3D11ShaderResourceView** outSRV);
ID3D11ShaderResourceView* CreateSolidWhiteSRV();

// Releases all SRVs held by the internal texture cache. Call once during
// shutdown, before the D3D11 device is destroyed.
void ModelUtil_ReleaseTextureCache();

// �C�ӐF/�s���N/�� 1x1 �e�N�X�`���쐬
ID3D11ShaderResourceView* CreateSolidColorSRV(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
ID3D11ShaderResourceView* CreateSolidPinkSRV();

// MTL: map_Kd �̗L���� outHadMapKd �ŕԂ�
bool TryLoadMTLTexture_SRV(
	const std::wstring& mtlPath,
	const std::wstring& baseDir,
	ID3D11ShaderResourceView** outSRV,
	DirectX::XMFLOAT2* outUVScale,
	DirectX::XMFLOAT2* outUVOffset,
	bool* outClamp,
	bool* outHadMapKd = nullptr);

// FBX: Diffuse �e�N�X�`���T��
struct DiffuseTexInfo {
	std::wstring path;
	std::string uvSetName;
	DirectX::XMFLOAT2 uvScale {1.0f, 1.0f};
	DirectX::XMFLOAT2 uvOffset {0.0f, 0.0f};
};
bool FindFirstDiffuseFileTexture(fbxsdk::FbxNode* node, DiffuseTexInfo& out);

// �⏕
DirectX::XMFLOAT3 TransformNormal_IT(const fbxsdk::FbxAMatrix& invTrans, const DirectX::XMFLOAT3& nIn);
bool HasNegativeDeterminant(const fbxsdk::FbxAMatrix& m);

// FBX SDK ���L
struct FbxShared {
	fbxsdk::FbxManager* mgr;
	fbxsdk::FbxIOSettings* ios;
	FbxShared();
	~FbxShared();
};
FbxShared& GetFbxShared();

// �X�L�j���O
void EnumerateSkinnedMeshes(fbxsdk::FbxNode* root, std::vector<std::pair<fbxsdk::FbxMesh*, fbxsdk::FbxNode*>>& out);
void ComputeSkinnedCP_ForMesh(fbxsdk::FbxMesh* mesh, fbxsdk::FbxNode* meshNode, double timeSec,
	std::vector<DirectX::XMFLOAT3>& outPos, std::vector<DirectX::XMFLOAT3>& outNrm,
	fbxsdk::FbxScene* baseScene = nullptr, fbxsdk::FbxScene* animScene = nullptr);

#endif // !MODEL_UTIL_H