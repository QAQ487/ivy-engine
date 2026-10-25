/**===========================================================================

 File   :  [model_util.cpp]

 Author : Terry Yeung
 Date   : 2025/10/18
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "model_util.h"

#include <Windows.h>
#include <wincodec.h>   // WIC
#include <objbase.h>    // CoInitializeEx
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cassert>
#include <cstring>

#include <fbxsdk.h>
#include "engine/rendering/d3d11/renderer.h"
#include <filesystem>

#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;
using namespace fbxsdk;

// テクスチャSRVキャッシュ（パス -> SRV）
static std::unordered_map<std::wstring, ID3D11ShaderResourceView*> g_srvCache;

// Releases the cache's AddRef'd reference on every cached SRV. Must be called
// before the D3D11 device is destroyed (e.g. from the app's shutdown sequence),
// otherwise every model texture ever loaded stays alive for the process lifetime.
bool ModelUtil_IsTextureCached(const wchar_t* path) {
    std::error_code ec;
    auto key = std::filesystem::absolute(path, ec).lexically_normal().wstring();
    if (ec) return false;
    for (const auto& entry : g_srvCache) {
        auto cached = std::filesystem::absolute(entry.first, ec).lexically_normal().wstring();
        if (!ec && _wcsicmp(key.c_str(), cached.c_str()) == 0) return true;
    }
    return false;
}
void ModelUtil_ReleaseTextureCache() {
	for (auto& kv : g_srvCache) {
		if (kv.second) kv.second->Release();
	}
	g_srvCache.clear();
}

// 文字列ユーティリティ
void Trim(std::string& s) {
	auto notspace = [] (int ch) { return !std::isspace(ch); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}
std::vector<std::string> Split(const std::string& s, char sep) {
	std::vector<std::string> out;
	std::string cur;
	std::istringstream iss(s);
	while (std::getline(iss, cur, sep)) out.emplace_back(cur);
	return out;
}
std::string WideToUTF8(const std::wstring& ws) {
	if (ws.empty()) return {};
	int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int) ws.size(), nullptr, 0, nullptr, nullptr);
	std::string out(len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int) ws.size(), &out[0], len, nullptr, nullptr);
	return out;
}
std::wstring Utf8ToWide(const char* s) {
	if (!s || !*s) return L"";
	int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
	if (len <= 0) return L"";
	std::wstring out(static_cast<size_t>(len - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], len);
	return out;
}
std::wstring DirnameW(const std::wstring& path) {
	size_t pos = path.find_last_of(L"/\\");
	if (pos == std::wstring::npos) return L"";
	return path.substr(0, pos);
}
std::wstring JoinPathW(const std::wstring& dir, const std::wstring& file) {
	if (dir.empty()) return file;
	if (file.empty()) return dir;
	wchar_t last = dir.back();
	if (last == L'\\' || last == L'/') return dir + file;
	return dir + L'\\' + file;
}
bool IsAbsolutePathW(const std::wstring& p) {
	if (p.size() >= 2 && p[1] == L':') return true;
	if (!p.empty() && (p[0] == L'\\' || p[0] == L'/')) return true;
	return false;
}

// WIC ファクトリのシングルトン
static IWICImagingFactory* GetWICFactory() {
	static IWICImagingFactory* s_factory = nullptr;
	if (s_factory) return s_factory;

	// CoInitialize（失敗しても続行）
	HRESULT hrCI = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	(void) hrCI;

	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&s_factory));
	if (FAILED(hr)) {
		hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&s_factory));
	}
	return s_factory;
}

// 画像をWICで読み込み、RGBA8でD3D11テクスチャ＋SRVを作る（可能ならGPUでミップ生成）
static bool LoadTextureWIC_Internal(const wchar_t* path, ID3D11ShaderResourceView** outSRV) {
	if (!path || !*path || !outSRV) return false;

	// キャッシュ
	{
		std::wstring key(path);
		auto it = g_srvCache.find(key);
		if (it != g_srvCache.end() && it->second) {
			it->second->AddRef();
			*outSRV = it->second;
			return true;
		}
	}

	IWICImagingFactory* factory = GetWICFactory();
	if (!factory) return false;

	IWICBitmapDecoder* decoder = nullptr;
	HRESULT hr = factory->CreateDecoderFromFilename(
		path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if (FAILED(hr) || !decoder) return false;

	IWICBitmapFrameDecode* frame = nullptr;
	hr = decoder->GetFrame(0, &frame);
	if (FAILED(hr) || !frame) { decoder->Release(); return false; }

	UINT width = 0, height = 0;
	frame->GetSize(&width, &height);
	if (width == 0 || height == 0) { frame->Release(); decoder->Release(); return false; }

	// RGBA8へ変換
	IWICFormatConverter* converter = nullptr;
	hr = factory->CreateFormatConverter(&converter);
	if (FAILED(hr) || !converter) { frame->Release(); decoder->Release(); return false; }

	hr = converter->Initialize(
		frame,
		GUID_WICPixelFormat32bppRGBA,
		WICBitmapDitherTypeNone,
		nullptr, 0.0, WICBitmapPaletteTypeCustom);
	if (FAILED(hr)) {
		converter->Release(); frame->Release(); decoder->Release();
		return false;
	}

	const UINT stride = width * 4;
	const size_t bufSize = static_cast<size_t>(stride) * static_cast<size_t>(height);
	std::vector<uint8_t> pixels(bufSize);
	hr = converter->CopyPixels(nullptr, stride, (UINT) bufSize, pixels.data());
	converter->Release();
	frame->Release();
	decoder->Release();

	if (FAILED(hr)) return false;

	ID3D11Device* dev = GetDevice();
	ID3D11DeviceContext* ctx = GetDeviceContext();
	if (!dev || !ctx) return false;

	// まずはミップ生成可能なテクスチャで試す
	ID3D11Texture2D* tex = nullptr;
	{
		D3D11_TEXTURE2D_DESC td {};
		td.Width = width;
		td.Height = height;
		td.MipLevels = 0; // ?S????
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

		if (SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &tex)) && tex) {
			// レベル0に書き込み
			ctx->UpdateSubresource(tex, 0, nullptr, pixels.data(), stride, stride * height);

			// SRV作成（デフォルトで全ミップ）
			ID3D11ShaderResourceView* srv = nullptr;
			if (SUCCEEDED(dev->CreateShaderResourceView(tex, nullptr, &srv)) && srv) {
				// ミップ生成
				ctx->GenerateMips(srv);

				// キャッシュへ（AddRefして保管）
				srv->AddRef();
				g_srvCache.emplace(std::wstring(path), srv);

				*outSRV = srv;
				tex->Release();
				return true;
			}
			if (srv) srv->Release();
			tex->Release();
			tex = nullptr;
		}
	}

	// フォールバック：ミップ無し・初期データ付き
	{
		D3D11_TEXTURE2D_DESC td {};
		td.Width = width;
		td.Height = height;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA init {};
		init.pSysMem = pixels.data();
		init.SysMemPitch = stride;

		if (FAILED(dev->CreateTexture2D(&td, &init, &tex)) || !tex) {
			return false;
		}

		ID3D11ShaderResourceView* srv = nullptr;
		if (FAILED(dev->CreateShaderResourceView(tex, nullptr, &srv)) || !srv) {
			tex->Release();
			return false;
		}

		// キャッシュへ（AddRefして保管）
		srv->AddRef();
		g_srvCache.emplace(std::wstring(path), srv);

		*outSRV = srv;
		tex->Release();
		return true;
	}
}

bool LoadTextureWIC_SRV(const wchar_t* texPath, ID3D11ShaderResourceView** outSRV) {
	// WIC のみで読み込む（DirectXTex 不使用）
	return LoadTextureWIC_Internal(texPath, outSRV);
}

// テクスチャ: 単色の 1x1 SRV を作成
ID3D11ShaderResourceView* CreateSolidColorSRV(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	ID3D11Device* dev = GetDevice();
	if (!dev) return nullptr;

	const uint8_t pixel[4] = {r, g, b, a};

	D3D11_TEXTURE2D_DESC desc {};
	desc.Width = 1;
	desc.Height = 1;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA init {};
	init.pSysMem = pixel;
	init.SysMemPitch = 4;

	ID3D11Texture2D* tex = nullptr;
	if (FAILED(dev->CreateTexture2D(&desc, &init, &tex)) || !tex)
		return nullptr;

	ID3D11ShaderResourceView* srv = nullptr;
	HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &srv);
	tex->Release();

	return SUCCEEDED(hr) ? srv : nullptr;
}

ID3D11ShaderResourceView* CreateSolidPinkSRV() {
	// ピンク (255, 0, 255, 255)
	return CreateSolidColorSRV(255, 0, 255, 255);
}

// 白
ID3D11ShaderResourceView* CreateSolidWhiteSRV() {
	ID3D11Device* dev = GetDevice();
	if (!dev) return nullptr;

	const uint8_t pixel[4] = {255, 255, 255, 255};

	D3D11_TEXTURE2D_DESC desc {};
	desc.Width = 1;
	desc.Height = 1;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA init {};
	init.pSysMem = pixel;
	init.SysMemPitch = 4;

	ID3D11Texture2D* tex = nullptr;
	if (FAILED(dev->CreateTexture2D(&desc, &init, &tex)) || !tex)
		return nullptr;

	ID3D11ShaderResourceView* srv = nullptr;
	HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &srv);
	tex->Release();

	return SUCCEEDED(hr) ? srv : nullptr;
}

// MTL: map_Kd の有無と UV 指定を取得。テクスチャのロードも試す。
bool TryLoadMTLTexture_SRV(
	const std::wstring& mtlPath,
	const std::wstring& baseDir,
	ID3D11ShaderResourceView** outSRV,
	DirectX::XMFLOAT2* outUVScale,
	DirectX::XMFLOAT2* outUVOffset,
	bool* outClamp,
	bool* outHadMapKd) {
	if (outUVScale)  *outUVScale = DirectX::XMFLOAT2(1.0f, 1.0f);
	if (outUVOffset) *outUVOffset = DirectX::XMFLOAT2(0.0f, 0.0f);
	if (outClamp)    *outClamp = false;
	if (outHadMapKd) *outHadMapKd = false;

	std::ifstream ifs(WideToUTF8(mtlPath));
	if (!ifs) return false;

	std::string line;
	while (std::getline(ifs, line)) {
		Trim(line);
		if (line.empty() || line[0] == '#') continue;
		if (line.rfind("map_Kd", 0) != 0) continue;

		if (outHadMapKd) *outHadMapKd = true;

		std::istringstream ss(line.substr(6));
		std::string tk, filename;
		while (ss >> tk) {
			if (tk == "-o") {
				float ou = 0, ov = 0; ss >> ou >> ov;
				if (outUVOffset) *outUVOffset = DirectX::XMFLOAT2(ou, ov);
			}
			else if (tk == "-s") {
				float su = 1, sv = 1; ss >> su >> sv;
				if (outUVScale) *outUVScale = DirectX::XMFLOAT2(su, sv);
			}
			else if (tk == "-clamp") {
				std::string onoff; ss >> onoff;
				if (outClamp) *outClamp = (onoff == "on" || onoff == "1" || onoff == "true");
			}
			else if (!tk.empty() && tk[0] == '-') {
				// その他のフラグは無視
			}
			else {
				filename = tk;
			}
		}

		if (!filename.empty()) {
			std::wstring wtex(filename.begin(), filename.end());
			std::wstring full = JoinPathW(baseDir, wtex);
			if (outSRV) {
				ID3D11ShaderResourceView* srv = nullptr;
				if (LoadTextureWIC_SRV(full.c_str(), &srv)) {
					*outSRV = srv;
					return true; // 読み込み成功
				}
			}
			return false; // map_Kd はあったが読み込み失敗
		}
	}
	return false; // map_Kd なし
}

// FBX: 最初に見つかった Diffuse/BaseColor の FileTexture を探索
bool FindFirstDiffuseFileTexture(FbxNode* node, DiffuseTexInfo& out) {
	if (!node) return false;

	auto fillFromFileTex = [&] (FbxFileTexture* f) -> bool {
		if (!f) return false;
		const char* rel = f->GetRelativeFileName();
		const char* abs = f->GetFileName();
		if (!((rel && *rel) || (abs && *abs))) return false;

		out.path = (rel && *rel) ? Utf8ToWide(rel) : Utf8ToWide(abs);

		// UV Scale / Offset / UVSet も
		FbxDouble2 sc = f->GetUVScaling();
		FbxDouble2 tr = f->GetUVTranslation();
		out.uvScale = DirectX::XMFLOAT2((float) sc[0], (float) sc[1]);
		out.uvOffset = DirectX::XMFLOAT2((float) tr[0], (float) tr[1]);

		FbxString uvset = f->UVSet.Get();
		const char* uvName = uvset.Buffer();
		if (uvName && *uvName) out.uvSetName = uvName;

#if _DEBUG
		char log[512];
		sprintf_s(log, "[FBX] FileTexture: rel='%s' abs='%s' uvset='%s' scale=(%.3f,%.3f) offs=(%.3f,%.3f)\n",
			rel ? rel : "", abs ? abs : "", uvName ? uvName : "", (double) sc[0], (double) sc[1], (double) tr[0], (double) tr[1]);
		OutputDebugStringA(log);
#endif
		return true;
	};

	const auto tryProp = [&] (FbxSurfaceMaterial* mat, const char* propName) -> bool {
		if (!mat || !propName) return false;
		FbxProperty prop = mat->FindProperty(propName);
		if (!prop.IsValid()) return false;

		// 1) LayeredTexture
		const int layeredCount = prop.GetSrcObjectCount<FbxLayeredTexture>();
		for (int li = 0; li < layeredCount; ++li) {
			if (auto* layered = prop.GetSrcObject<FbxLayeredTexture>(li)) {
				const int texCount = layered->GetSrcObjectCount<FbxTexture>();
				for (int ti = 0; ti < texCount; ++ti) {
					if (auto* tex = layered->GetSrcObject<FbxTexture>(ti)) {
						if (tex->GetClassId().Is(FbxFileTexture::ClassId)) {
							if (fillFromFileTex(static_cast<FbxFileTexture*>(tex))) return true;
						}
					}
				}
			}
		}

		// 2) 直接の FbxFileTexture
		const int tcount = prop.GetSrcObjectCount<FbxTexture>();
		for (int ti = 0; ti < tcount; ++ti) {
			if (auto* tex = prop.GetSrcObject<FbxTexture>(ti)) {
				if (tex->GetClassId().Is(FbxFileTexture::ClassId)) {
					if (fillFromFileTex(static_cast<FbxFileTexture*>(tex))) return true;
				}
			}
		}
		return false;
	};

	// ノードのマテリアルを探索
	const int mcount = node->GetMaterialCount();
	for (int mi = 0; mi < mcount; ++mi) {
		FbxSurfaceMaterial* mat = node->GetMaterial(mi);
		if (!mat) continue;

		// 探索（テクスチャプロパティ名のバリエーションに対応）
		const char* kTexProps[] = {
			FbxSurfaceMaterial::sDiffuse, "Diffuse",
			"BaseColor", "Base Color", "baseColor", "base_color",
			"Albedo", "AlbedoColor", "albedo", "albedo_color",
			"Maya|base_color", "3dsMax|base_color"
		};
		for (const char* name : kTexProps) {
			if (tryProp(mat, name)) return true;
		}

		// フォールバックA: マテリアル直下の FileTexture
		{
			const int texCount = mat->GetSrcObjectCount<FbxFileTexture>();
			for (int i = 0; i < texCount; ++i) {
				if (auto* f = mat->GetSrcObject<FbxFileTexture>(i)) {
					if (fillFromFileTex(f)) return true;
				}
			}
		}
		// フォールバックB: LayeredTexture 経由
		{
			const int lcount = mat->GetSrcObjectCount<FbxLayeredTexture>();
			for (int i = 0; i < lcount; ++i) {
				if (auto* layered = mat->GetSrcObject<FbxLayeredTexture>(i)) {
					const int tcount = layered->GetSrcObjectCount<FbxTexture>();
					for (int ti = 0; ti < tcount; ++ti) {
						if (auto* tex = layered->GetSrcObject<FbxTexture>(ti)) {
							if (tex->GetClassId().Is(FbxFileTexture::ClassId)) {
								if (fillFromFileTex(static_cast<FbxFileTexture*>(tex))) return true;
							}
						}
					}
				}
			}
		}
	}

	// 子ノードへ再帰
	for (int ci = 0; ci < node->GetChildCount(); ++ci) {
		if (FindFirstDiffuseFileTexture(node->GetChild(ci), out)) return true;
	}
	return false;
}

// 変換
DirectX::XMFLOAT3 TransformNormal_IT(const FbxAMatrix& invTrans, const DirectX::XMFLOAT3& nIn) {
	FbxVector4 nn((double) nIn.x, (double) nIn.y, (double) nIn.z, 0.0);
	FbxVector4 out = invTrans.MultT(nn);
	XMVECTOR v = XMVectorSet((float) out[0], (float) out[1], (float) out[2], 0.0f);
	v = XMVector3Normalize(v);
	DirectX::XMFLOAT3 ret; XMStoreFloat3(&ret, v);
	return ret;
}
bool HasNegativeDeterminant(const FbxAMatrix& m) { return m.Determinant() < 0.0; }

// FBX 共有
FbxShared::FbxShared() : mgr(nullptr), ios(nullptr) {
	mgr = FbxManager::Create();
	ios = FbxIOSettings::Create(mgr, IOSROOT);
	mgr->SetIOSettings(ios);
}
FbxShared::~FbxShared() {
	if (ios) ios->Destroy();
	if (mgr) mgr->Destroy();
}
FbxShared& GetFbxShared() { static FbxShared s; return s; }

// スキニング列挙
void EnumerateSkinnedMeshes(FbxNode* root, std::vector<std::pair<FbxMesh*, FbxNode*>>& out) {
	if (!root) return;
	if (auto* m = root->GetMesh()) {
		if (m->GetDeformerCount(FbxDeformer::eSkin) > 0) {
			out.emplace_back(m, root);
		}
	}
	const int c = root->GetChildCount();
	for (int i = 0; i < c; ++i) EnumerateSkinnedMeshes(root->GetChild(i), out);
}

// CPU スキニング（必要に応じて簡略）
void ComputeSkinnedCP_ForMesh(FbxMesh* mesh, FbxNode* meshNode, double timeSec,
	std::vector<DirectX::XMFLOAT3>& outPos, std::vector<DirectX::XMFLOAT3>& outNrm,
	FbxScene* baseScene, FbxScene* animScene) {
	// 既存実装そのまま
	outPos.clear(); outNrm.clear();
	if (!mesh || !meshNode) return;

	const int cpCount = mesh->GetControlPointsCount();
	outPos.resize(cpCount, DirectX::XMFLOAT3(0, 0, 0));
	outNrm.resize(cpCount, DirectX::XMFLOAT3(0, 1, 0));
	std::vector<double> weightSum(cpCount, 0.0);

	const FbxVector4* cp = mesh->GetControlPoints();
	FbxTime t; t.SetSecondDouble(timeSec);

	FbxAMatrix meshGlobal = meshNode->EvaluateGlobalTransform(t);
	FbxAMatrix toLocal = meshGlobal.Inverse();

	const int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
	if (skinCount <= 0) {
		for (int i = 0; i < cpCount; ++i) {
			outPos[i] = DirectX::XMFLOAT3((float) cp[i][0], (float) cp[i][1], (float) cp[i][2]);
			outNrm[i] = DirectX::XMFLOAT3(0, 1, 0);
		}
		return;
	}

	// Cache bone name lookups from animation scene to avoid repeated string comparisons
	std::unordered_map<std::string, FbxNode*> animBoneCache;
	if (animScene) {
		for (int si = 0; si < skinCount; ++si) {
			FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(si, FbxDeformer::eSkin));
			if (!skin) continue;
			const int clusterCount = skin->GetClusterCount();
			for (int ci = 0; ci < clusterCount; ++ci) {
				FbxCluster* cluster = skin->GetCluster(ci);
				if (!cluster) continue;
				FbxNode* baseBone = cluster->GetLink();
				if (!baseBone) continue;
				std::string boneName = baseBone->GetName();
				auto [it, inserted] = animBoneCache.try_emplace(boneName, nullptr);
				if (inserted) {
					it->second = animScene->FindNodeByName(boneName.c_str());
				}
			}
		}
	}

	for (int si = 0; si < skinCount; ++si) {
		FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(si, FbxDeformer::eSkin));
		if (!skin) continue;
		const int clusterCount = skin->GetClusterCount();
		for (int ci = 0; ci < clusterCount; ++ci) {
			FbxCluster* cluster = skin->GetCluster(ci);
			if (!cluster) continue;

			FbxNode* baseBone = cluster->GetLink();
			if (!baseBone) continue;

			// Get bind pose matrices from BASE SCENE (mesh file)
			FbxAMatrix meshBind, baseBoneBind;
			cluster->GetTransformMatrix(meshBind);
			cluster->GetTransformLinkMatrix(baseBoneBind);

			// Get current bone transform from ANIMATION SCENE (animation file)
			FbxAMatrix boneNow;
			FbxNode* animBone = nullptr;
			if (animScene) {
				std::string boneName = baseBone->GetName();
				auto it = animBoneCache.find(boneName);
				if (it != animBoneCache.end()) {
					animBone = it->second;
				}
			}
			if (animBone) {
				boneNow = animBone->EvaluateGlobalTransform(t);
			} else {
				boneNow = baseBone->EvaluateGlobalTransform(t);
			}

			// Calculate final cluster matrix:
			// AnimationGlobalTransform * Inverse(BaseBoneBindMatrix) * BaseMeshBindMatrix
			FbxAMatrix clusterM = boneNow * baseBoneBind.Inverse() * meshBind;

			int indexCount = cluster->GetControlPointIndicesCount();
			const int* indices = cluster->GetControlPointIndices();
			const double* weights = cluster->GetControlPointWeights();
			for (int k = 0; k < indexCount; ++k) {
				int cpi = indices[k];
				if (cpi < 0 || cpi >= cpCount) continue;
				double w = weights[k];
				if (w <= 0.0) continue;

				FbxVector4 p = cp[cpi];
				FbxVector4 pDef = clusterM.MultT(p);
				FbxVector4 pLoc = toLocal.MultT(pDef);

				outPos[cpi].x += (float) (pLoc[0] * w);
				outPos[cpi].y += (float) (pLoc[1] * w);
				outPos[cpi].z += (float) (pLoc[2] * w);

				weightSum[cpi] += w;
			}
		}
	}

	for (int i = 0; i < cpCount; ++i) {
		double remain = 1.0 - weightSum[i];
		if (remain > 0.0) {
			FbxVector4 p = cp[i];
			outPos[i].x += (float) (p[0] * remain);
			outPos[i].y += (float) (p[1] * remain);
			outPos[i].z += (float) (p[2] * remain);
		}
	}
	for (int i = 0; i < cpCount; ++i) {
		XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&outNrm[i]));
		XMStoreFloat3(&outNrm[i], n);
	}
}