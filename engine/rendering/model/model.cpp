/**===========================================================================

 File   :  [model.cpp]

 Author : Terry Yeung
 Date   : 2025/10/15
 -----------------------------------------------------------------------------

 ===========================================================================**/
	#include "model.h"
#include "model_util.h"
#include "engine/core/content_path.h"
#include "engine/rendering/culling/frustum_cull_math.h"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cctype>
#include <cassert>
#include <cstring>
#include <functional>
#include <cmath>
#include <cstdio>

#include "engine/rendering/d3d11/renderer.h"
#include "camera.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/lighting/shadow.h"
#include "engine/rendering/lighting/shadow_volume.h"
#include "engine/debug_draw/debug_ofstream.h"

using namespace DirectX;
using namespace fbxsdk;

#ifndef MODEL3D_CPU_SKIN_ORACLE
#define MODEL3D_CPU_SKIN_ORACLE 0
#endif

Model3D::Model3D() {}
Model3D::~Model3D() { Release(); }

void Model3D::Release() {
	ReleaseShadowVolumeInstanceData();
	// Drop this model's share of the volume data (instances keep theirs until released).
	m_shadowVolume.reset();
	if (m_isAnimInstance) {
		if (m_pVB && !m_sharesCachedVB) { m_pVB->Release(); }
		m_pVB = nullptr;
		m_sharesCachedVB = false;
		m_verticesCPU.clear();
		m_cpIndexOfVertex.clear();
		m_skinPosBuf.clear();
		m_skinWeightScratch.clear();
		m_boneXformScratch.clear();
		m_skinBake.clear();
		m_skinBones.clear();
		m_rigidBake.clear();
		m_activeClip.clear();
		m_animTimeSec = 0.0;
		m_animStartSec = 0.0;
		m_animDurationSec = 0.0;
		m_currentAnimScene = nullptr;
		m_pIB = nullptr;
		m_pSRV = nullptr;
		m_subsets.clear();
		m_baseScene = nullptr;
		m_animScenes.clear();
		m_bakedClips.clear();
		m_meshXformScratch.clear();
		m_mesh = nullptr;
		m_meshNode = nullptr;
		m_isAnimInstance = false;
		m_hasLocalAabb = false;
		m_localAabbMin = XMFLOAT3(0, 0, 0);
		m_localAabbMax = XMFLOAT3(0, 0, 0);
		return;
	}

	// Release subset SRVs first (check for overlap with main SRV)
	for (auto& s : m_subsets) {
		// Do not release if it is the same pointer as the main SRV (avoid double-free)
		if (s.pSRV && s.pSRV != m_pSRV) {
			s.pSRV->Release();
		}
		s.pSRV = nullptr;
	}
	m_subsets.clear();

	// Release D3D11 resources
	if (m_pVB) { m_pVB->Release(); m_pVB = nullptr; }
	if (m_pIB) { m_pIB->Release(); m_pIB = nullptr; }
	if (m_pSRV) { m_pSRV->Release(); m_pSRV = nullptr; }
	m_indexCount = 0;

	// Clear FBX-related pointers
	m_mesh = nullptr;
	m_meshNode = nullptr;

	if (m_baseScene) {
		//m_baseScene->Destroy();
		m_baseScene = nullptr;
	}

	m_cpIndexOfVertex.clear();
	m_verticesCPU.clear();
	m_vbDynamic = false;

	m_hasSkin = false;
	m_animTimeSec = 0.0;
	m_animStartSec = 0.0;
	m_animDurationSec = 0.0;
	m_animSpeed = 1.0;
	m_loop = true;
	m_skinMeshGeo.clear();
	m_geoIndexPerCP.clear();
	m_meshCpOffset.clear();
	m_skinnedMeshCache.clear();
	m_skinBake.clear();
	m_skinBones.clear();
	m_boneXformScratch.clear();
	m_skinPosBuf.clear();
	m_skinWeightScratch.clear();
	m_vertexGeoIndex.clear();
	m_rigidBake.clear();
	m_bakedClips.clear();
	m_meshXformScratch.clear();
	m_boneBindOffset.clear();
	m_gpuPaletteWrite.clear();
	m_gpuPaletteFront.clear();
	m_gpuPaletteDirty = false;
	m_vbUploadDirty = false;
	m_gpuSkinEligible = false;
	m_gpuBindConflict = false;
	m_sharesCachedVB = false;

	m_diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_hasTexture = false;
	m_hasLocalAabb = false;
	m_localAabbMin = XMFLOAT3(0, 0, 0);
	m_localAabbMax = XMFLOAT3(0, 0, 0);
}

void Model3D::CacheLocalAabbFromPositions(const XMFLOAT3* positions, size_t count, bool inflateForSkin) {
	m_hasLocalAabb = false;
	m_localAabbMin = XMFLOAT3(0, 0, 0);
	m_localAabbMax = XMFLOAT3(0, 0, 0);
	if (!positions || count == 0)
		return;

	frustum_cull::Aabb box = frustum_cull::MakeAabbFromPoints(positions, count);
	if (!box.valid)
		return;

	// Bind-pose AABB underestimates skinned / animated meshes. Inflate
	// conservatively so edge silhouettes do not pop when culled.
	if (inflateForSkin) {
		const float hx = 0.5f * (box.max.x - box.min.x);
		const float hy = 0.5f * (box.max.y - box.min.y);
		const float hz = 0.5f * (box.max.z - box.min.z);
		const float pad = (std::max)(hx, (std::max)(hy, hz)) * 0.5f;
		frustum_cull::InflateAabb(box, pad);
	}

	m_localAabbMin = box.min;
	m_localAabbMax = box.max;
	m_hasLocalAabb = true;
}

/**===========================================================================
   <OBJ loader>
 ===========================================================================**/
bool Model3D::LoadOBJ(const wchar_t* objPath, const wchar_t* fallbackTex) {
	Release();

	const std::wstring resolvedObj = content_path::ReadableWideCurrent(objPath);
	const wchar_t* openObj = resolvedObj.empty() ? objPath : resolvedObj.c_str();
	std::ifstream ifs(WideToUTF8(openObj));
	if (!ifs) {
		MessageBoxA(nullptr, "Failed to load OBJ file.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}
	std::wstring baseDirW = DirnameW(openObj);

	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT3> normals;
	std::vector<XMFLOAT2> texcoords;

	std::vector<VertexPNTC> vertices;
	std::vector<uint32_t> indices;
	std::unordered_map<PNTKey, uint32_t, PNTKeyHasher> vcache;
	vertices.reserve(4096);
	indices.reserve(8192);
	vcache.reserve(4096);

	std::string line;
	std::wstring mtlFileW;

	// UV transform from MTL (applied in batch later)
	XMFLOAT2 uvScale {1.0f, 1.0f};
	XMFLOAT2 uvOffset {0.0f, 0.0f};
	bool uvClamp = false;

	auto addVertex = [&] (const PNTKey& key, const XMFLOAT3& faceN) -> uint32_t {
		auto it = vcache.find(key);
		if (it != vcache.end()) return it->second;

		VertexPNTC v {};
		if (key.p > 0) {
			size_t idx = static_cast<size_t>(key.p - 1);
			if (idx < positions.size()) v.Position = positions[idx];
		}
		if (key.t > 0) {
			size_t idx = static_cast<size_t>(key.t - 1);
			if (idx < texcoords.size()) v.TexCoord = texcoords[idx];
		}
		if (key.n > 0) {
			size_t idx = static_cast<size_t>(key.n - 1);
			if (idx < normals.size()) v.Normal = normals[idx];
		}
		else v.Normal = faceN;

		// Start white (apply MTL Kd to all vertices later)
		v.Diffuse = XMFLOAT4(1, 1, 1, 1);

		uint32_t newIdx = static_cast<uint32_t>(vertices.size());
		vertices.emplace_back(v);
		vcache.emplace(key, newIdx);
		return newIdx;
	};

	while (std::getline(ifs, line)) {
		Trim(line);
		if (line.empty() || line[0] == '#') continue;

		if (line.rfind("v ", 0) == 0) {
			std::istringstream ss(line.substr(2));
			float x, y, z; ss >> x >> y >> z;
			positions.emplace_back(x, y, z);
		}
		else if (line.rfind("vt ", 0) == 0) {
			std::istringstream ss(line.substr(3));
			float u, v; ss >> u >> v;
			// Flip v for DirectX convention
			texcoords.emplace_back(u, 1.0f - v);
		}
		else if (line.rfind("vn ", 0) == 0) {
			std::istringstream ss(line.substr(3));
			float x, y, z; ss >> x >> y >> z;
			normals.emplace_back(x, y, z);
		}
		else if (line.rfind("f ", 0) == 0) {
			std::istringstream ss(line.substr(2));
			std::vector<std::string> vtokens;
			std::string tok;
			while (ss >> tok) vtokens.emplace_back(tok);
			if (vtokens.size() < 3) continue;

			auto parseTriplet = [] (const std::string& s) -> PNTKey {
				PNTKey k {};
				std::vector<std::string> elems = Split(s, '/');
				if (!elems.empty() && !elems[0].empty()) k.p = std::stoi(elems[0]);
				if (elems.size() >= 2 && !elems[1].empty()) k.t = std::stoi(elems[1]);
				if (elems.size() >= 3 && !elems[2].empty()) k.n = std::stoi(elems[2]);
				return k;
			};

			// Parse once and reuse
			std::vector<PNTKey> keys;
			keys.reserve(vtokens.size());
			bool anyNoNormal = false;
			for (size_t i = 0; i < vtokens.size(); ++i) {
				PNTKey k = parseTriplet(vtokens[i]);
				if (k.n <= 0) anyNoNormal = true;
				keys.emplace_back(k);
			}

			// Compute face normals when needed
			XMFLOAT3 faceN {0, 0, 1};
			if (anyNoNormal) {
				std::function<XMFLOAT3(int)> get_pos = [&] (int p) -> XMFLOAT3 {
					int idx = (std::max) (1, p) - 1;
					if (idx >= 0 && static_cast<size_t>(idx) < positions.size())
						return positions[static_cast<size_t>(idx)];
					return XMFLOAT3 {0, 0, 0};
				};
				XMFLOAT3 p0 = get_pos(keys[0].p);
				XMFLOAT3 p1 = get_pos(keys[1].p);
				XMFLOAT3 p2 = get_pos(keys[2].p);
				XMVECTOR a = XMVectorSet(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z, 0);
				XMVECTOR b = XMVectorSet(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z, 0);
				XMVECTOR n = XMVector3Normalize(XMVector3Cross(a, b));
				XMStoreFloat3(&faceN, n);
			}

			// Fan triangulation (supports polygons)
			for (size_t i = 1; i + 1 < keys.size(); ++i) {
				const PNTKey& a = keys[0];
				const PNTKey& b = keys[i];
				const PNTKey& c = keys[i + 1];

				uint32_t i0 = addVertex(a, faceN);
				uint32_t i1 = addVertex(b, faceN);
				uint32_t i2 = addVertex(c, faceN);

				indices.push_back(i0);
				indices.push_back(i1);
				indices.push_back(i2);
			}
		}
		else if (line.rfind("mtllib ", 0) == 0) {
			std::string mtl = line.substr(7);
			Trim(mtl);
			if (!mtl.empty()) {
				std::wstring wmtl(mtl.begin(), mtl.end());
				mtlFileW = JoinPathW(baseDirW, wmtl);
			}
		}
	}

	if (vertices.empty() || indices.empty()) {
		MessageBoxA(nullptr, "OBJ has no drawable geometry.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	// Read texture/UV transform and Kd (diffuse) from MTL
	bool texOK = false;
	if (!mtlFileW.empty()) {
		// Read Kd (omitted detail)
		// Load texture
		texOK = TryLoadMTLTexture_SRV(mtlFileW, baseDirW, &m_pSRV, &uvScale, &uvOffset, &uvClamp);
	}

	// Fallback texture (if needed)
	if (!texOK && fallbackTex && fallbackTex[0]) {
		texOK = LoadTextureWIC_SRV(fallbackTex, &m_pSRV);
	}

	// Set OBJ has-texture flag (white 1x1 counts as none)
	m_hasTexture = texOK;

	// If no texture from any path, assign 1x1 white (draw dummy)
	if (!texOK || !m_pSRV) {
		m_pSRV = CreateSolidWhiteSRV();
	}

	// Apply UV scale/offset/clamp to all vertices
	if (uvScale.x != 1.0f || uvScale.y != 1.0f || uvOffset.x != 0.0f || uvOffset.y != 0.0f || uvClamp) {
		for (size_t i = 0; i < vertices.size(); ++i) {
			vertices[i].TexCoord.x = vertices[i].TexCoord.x * uvScale.x + uvOffset.x;
			vertices[i].TexCoord.y = vertices[i].TexCoord.y * uvScale.y + uvOffset.y;
			if (uvClamp) {
				vertices[i].TexCoord.x = (std::max) (0.0f, (std::min) (1.0f, vertices[i].TexCoord.x));
				vertices[i].TexCoord.y = (std::max) (0.0f, (std::min) (1.0f, vertices[i].TexCoord.y));
			}
		}
	}

	// Also apply MTL Kd (diffuse) to vertex color
	for (auto& v : vertices) {
		v.Diffuse = m_diffuse;
	}

	// Create VB
	{
		D3D11_BUFFER_DESC bd {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(VertexPNTC));
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA init {};
		init.pSysMem = vertices.data();

		if (FAILED(GetDevice()->CreateBuffer(&bd, &init, &m_pVB)) || !m_pVB) {
			MessageBoxA(nullptr, "OBJ: failed to create vertex buffer.", "Error", MB_OK | MB_ICONERROR);
			return false;
		}
	}
	// Create IB
	{
		D3D11_BUFFER_DESC bd {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA init {};
		init.pSysMem = indices.data();

		if (FAILED(GetDevice()->CreateBuffer(&bd, &init, &m_pIB)) || !m_pIB) {
			MessageBoxA(nullptr, "OBJ: failed to create index buffer.", "Error", MB_OK | MB_ICONERROR);
			return false;
		}
		m_indexCount = static_cast<UINT>(indices.size());
	}

	{
		std::vector<XMFLOAT3> positions;
		positions.reserve(vertices.size());
		for (const auto& v : vertices)
			positions.push_back(v.Position);
		CacheLocalAabbFromPositions(positions.data(), positions.size(), false);
	}

	return true;
}

/**===========================================================================
   <FBX loader>
 ===========================================================================**/
bool Model3D::LoadFBX(const wchar_t* fbxPath, const wchar_t*) { return LoadFBXInternal(fbxPath, true); }
bool Model3D::LoadFBXQuiet(const wchar_t* fbxPath) { return LoadFBXInternal(fbxPath, false); }
bool Model3D::LoadFBXInternal(const wchar_t* fbxPath, bool reportErrors) {
	Release();

	FbxShared& shared = GetFbxShared();

	// Import
	FbxImporter* importer = FbxImporter::Create(shared.mgr, "");
	const std::wstring resolvedFbx = content_path::ReadableWideCurrent(fbxPath);
	const wchar_t* openFbx = resolvedFbx.empty() ? fbxPath : resolvedFbx.c_str();
	std::string path_utf8;
	{
		int len = WideCharToMultiByte(CP_UTF8, 0, openFbx, -1, nullptr, 0, nullptr, nullptr);
		path_utf8.resize((size_t) len);
		WideCharToMultiByte(CP_UTF8, 0, openFbx, -1, path_utf8.data(), len, nullptr, nullptr);
	}
	if (!importer->Initialize(path_utf8.c_str(), -1, shared.mgr->GetIOSettings())) {
		importer->Destroy();
		if (reportErrors) MessageBoxA(nullptr, "Failed to open FBX file.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}
	m_baseScene = FbxScene::Create(shared.mgr, "scene");
	if (!importer->Import(m_baseScene)) {
		importer->Destroy();
		if (m_baseScene) { m_baseScene->Destroy(); m_baseScene = nullptr; }
		if (reportErrors) MessageBoxA(nullptr, "Failed to import FBX file.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}
	importer->Destroy();

	m_currentAnimScene = m_baseScene;

	// Axis / units
	{
		FbxAxisSystem axis = m_baseScene->GetGlobalSettings().GetAxisSystem();
		if (!(axis == FbxAxisSystem::DirectX)) {
			FbxAxisSystem::DirectX.DeepConvertScene(m_baseScene);
		}
		FbxSystemUnit unit = m_baseScene->GetGlobalSettings().GetSystemUnit();
		if (!(unit == FbxSystemUnit::cm)) {
			FbxSystemUnit::cm.ConvertScene(m_baseScene);
		}
	}

	// Enumerate skinned meshes
	std::vector<std::pair<FbxMesh*, FbxNode*>> skinnedList;
	EnumerateSkinnedMeshes(m_baseScene->GetRootNode(), skinnedList);
	std::unordered_map<FbxMesh*, int> cpBase;
	int runningBase = 0;
	for (auto& pr : skinnedList) {
		FbxMesh* m = pr.first;
		cpBase[m] = runningBase;
		runningBase += m->GetControlPointsCount();
	}
	m_hasSkin = !skinnedList.empty();
	m_skinnedMeshCache = skinnedList;
	m_meshCpOffset = cpBase;

	// Select mesh to draw
	m_mesh = nullptr; m_meshNode = nullptr;
	if (!skinnedList.empty()) { m_mesh = skinnedList[0].first; m_meshNode = skinnedList[0].second; }
	else {
		std::function<void(FbxNode*)> pick = [&] (FbxNode* n) {
			if (!n || m_mesh) return;
			if (auto* ms = n->GetMesh()) { m_mesh = ms; m_meshNode = n; return; }
			for (int i = 0; i < n->GetChildCount() && !m_mesh; ++i) pick(n->GetChild(i));
		};
		pick(m_baseScene->GetRootNode());
	}
	if (!m_mesh || !m_meshNode) {
		if (reportErrors) MessageBoxA(nullptr, "FBX: mesh not found.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	// Base directory
	std::wstring baseDir = DirnameW(fbxPath);

	// Per-material texture SRV / diffuse color cache
	std::unordered_map<FbxSurfaceMaterial*, ID3D11ShaderResourceView*> matSRV;
	std::unordered_map<FbxSurfaceMaterial*, XMFLOAT4>                 matColor;

	auto getOrCreateBlue = [] () -> ID3D11ShaderResourceView* {
		static ID3D11ShaderResourceView* sBlue = nullptr;
		if (!sBlue) {
			// sBlue = CreateSolidColorSRV(1.0f,1.0f,1.0f,1.0f);
		}
		if (sBlue) sBlue->AddRef();
		return sBlue;
	};

	// Find Diffuse/BaseColor FbxFileTexture on material and create SRV (nullptr if none)
	auto LoadSRVFromMaterial = [&](FbxSurfaceMaterial* mat) -> ID3D11ShaderResourceView* {
		if (!mat) return nullptr;

		// Reuse if already created (AddRef and return)
		if (auto it = matSRV.find(mat); it != matSRV.end()) {
			if (it->second) it->second->AddRef();
			return it->second;
		}

		// Scan properties for FileTexture (Diffuse/BaseColor)
		auto tryPropForFile = [&] (const char* propName) -> FbxFileTexture* {
			FbxProperty prop = mat->FindProperty(propName);
			if (!prop.IsValid()) return nullptr;

			// Via LayeredTexture
			const int layeredCount = prop.GetSrcObjectCount<FbxLayeredTexture>();
			for (int li = 0; li < layeredCount; ++li) {
				if (auto* layered = prop.GetSrcObject<FbxLayeredTexture>(li)) {
					const int texCount = layered->GetSrcObjectCount<FbxTexture>();
					for (int ti = 0; ti < texCount; ++ti) {
						if (auto* tex = layered->GetSrcObject<FbxTexture>(ti)) {
							if (tex->GetClassId().Is(FbxFileTexture::ClassId))
								return static_cast<FbxFileTexture*>(tex);
						}
					}
				}
			}
			// Via LayeredTexture
			const int tcount = prop.GetSrcObjectCount<FbxTexture>();
			for (int ti = 0; ti < tcount; ++ti) {
				if (auto* tex = prop.GetSrcObject<FbxTexture>(ti)) {
					if (tex->GetClassId().Is(FbxFileTexture::ClassId))
						return static_cast<FbxFileTexture*>(tex);
				}
			}
			return nullptr;
		};

		const char* kTexProps[] = {
			FbxSurfaceMaterial::sDiffuse, "Diffuse",
			"BaseColor", "Base Color", "baseColor", "base_color",
			"Albedo", "AlbedoColor", "albedo", "albedo_color",
			"Maya|base_color", "3dsMax|base_color"
		};

		FbxFileTexture* fileTex = nullptr;
		for (auto* name : kTexProps) {
			fileTex = tryPropForFile(name);
			if (fileTex) break;
		}

		ID3D11ShaderResourceView* srv = nullptr;
		if (fileTex) {
			const char* rel = fileTex->GetRelativeFileName();
			const char* abs = fileTex->GetFileName();
			std::wstring path = rel && *rel ? Utf8ToWide(rel) : Utf8ToWide(abs);
			// Resolve candidates (baseDir/filename, etc.)
			size_t pos = path.find_last_of(L"/\\");
			std::wstring justName = (pos == std::wstring::npos) ? path : path.substr(pos + 1);

			std::vector<std::wstring> candidates;
			if (IsAbsolutePathW(path)) {
				candidates.push_back(path);
			}
			else {
				candidates.push_back(JoinPathW(baseDir, path));
				candidates.push_back(JoinPathW(baseDir, justName));
				candidates.push_back(JoinPathW(JoinPathW(baseDir, L"Textures"), justName));
				candidates.push_back(JoinPathW(JoinPathW(baseDir, L"textures"), justName));
				candidates.push_back(JoinPathW(JoinPathW(baseDir, L"Texture"), justName));
				candidates.push_back(JoinPathW(JoinPathW(baseDir, L"texture"), justName));
			}
			for (const auto& c : candidates) {
				if (!srv && LoadTextureWIC_SRV(c.c_str(), &srv)) break;
			}
		}

		// Cache (store even if null)
		matSRV.emplace(mat, srv);
		if (srv) srv->AddRef();
		return srv;
	};

	// Get material diffuse color (white if missing)
	auto LoadDiffuseColorFromMaterial = [&] (FbxSurfaceMaterial* mat) -> XMFLOAT4 {
		if (!mat) return XMFLOAT4(1, 1, 1, 1);
		if (auto it = matColor.find(mat); it != matColor.end()) return it->second;

		const char* kColorProps[] = {
			FbxSurfaceMaterial::sDiffuse,
			"Diffuse",
			"BaseColor", "Base Color", "baseColor", "base_color",
			"Albedo", "AlbedoColor", "albedo", "albedo_color",
			"Maya|base_color", "3dsMax|base_color"
		};
		XMFLOAT4 out(1, 1, 1, 1);
		for (auto* name : kColorProps) {
			FbxProperty prop = mat->FindProperty(name);
			if (!prop.IsValid()) continue;
			if (prop.GetPropertyDataType().GetType() == eFbxDouble3
				|| prop.GetPropertyDataType().GetType() == eFbxDouble4) {
				FbxDouble3 v = prop.Get<FbxDouble3>();
				out = XMFLOAT4((float) v[0], (float) v[1], (float) v[2], 1.0f);
				break;
			}
		}
		matColor.emplace(mat, out);
		return out;
	};

	// Generate vertices (split into subsets)
	m_verticesCPU.clear();
	m_cpIndexOfVertex.clear();
	m_subsets.clear();
	m_skinMeshGeo.clear();
	m_geoIndexPerCP.clear();
	m_skinBake.clear();
	m_skinBones.clear();
	m_boneXformScratch.clear();
	m_skinPosBuf.clear();
	m_skinWeightScratch.clear();
	m_vertexGeoIndex.clear();
	m_rigidBake.clear();

	std::function<void(FbxNode*)> Traverse = [&] (FbxNode* n) {
		if (!n) return;
		FbxMesh* mesh = n->GetMesh();
		if (mesh) {
			const int polyCount = mesh->GetPolygonCount();
			if (polyCount > 0) {
				const bool meshIsSkinned = mesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
				const size_t rigidVertStart = m_verticesCPU.size();
				FbxAMatrix geo;
				geo.SetT(n->GetGeometricTranslation(FbxNode::eSourcePivot));
				geo.SetR(n->GetGeometricRotation(FbxNode::eSourcePivot));
				geo.SetS(n->GetGeometricScaling(FbxNode::eSourcePivot));
				FbxAMatrix finalM = geo;
				FbxAMatrix invTrans = finalM.Inverse().Transpose();
				const bool invertWinding = HasNegativeDeterminant(finalM);

				int cpBaseOffset = 0;
				const auto itSkinBase = cpBase.find(mesh);
				if (itSkinBase != cpBase.end())
					cpBaseOffset = itSkinBase->second;
				if (m_hasSkin && itSkinBase != cpBase.end()) {
					SkinMeshGeo geoInfo {};
					geoInfo.cpBase = cpBaseOffset;
					geoInfo.cpCount = mesh->GetControlPointsCount();
					geoInfo.geo = finalM;
					geoInfo.geoInvTrans = finalM.Inverse().Transpose();
					geoInfo.hasGeoOffset =
						finalM.GetT()[0] != 0.0 || finalM.GetT()[1] != 0.0 || finalM.GetT()[2] != 0.0 ||
						finalM.GetS()[0] != 1.0 || finalM.GetS()[1] != 1.0 || finalM.GetS()[2] != 1.0 ||
						finalM.GetR()[0] != 0.0 || finalM.GetR()[1] != 0.0 || finalM.GetR()[2] != 0.0;
					m_skinMeshGeo.emplace_back(geoInfo);
				}

				// Normals and UV set
				const char* uvNameC = nullptr;
				FbxStringList uvSets;
				mesh->GetUVSetNames(uvSets);
				if (uvSets.GetCount() > 0) uvNameC = uvSets[0];

				FbxLayerElementNormal* nElem = mesh->GetElementNormalCount() > 0 ? mesh->GetElementNormal(0) : nullptr;
				FbxLayerElement::EMappingMode nMap = nElem ? nElem->GetMappingMode() : FbxLayerElement::eNone;
				FbxLayerElement::EReferenceMode nRef = nElem ? nElem->GetReferenceMode() : FbxLayerElement::eDirect;

				// Assign materials (per polygon)
				FbxLayerElementMaterial* mElem = mesh->GetElementMaterialCount() > 0 ? mesh->GetElementMaterial(0) : nullptr;
				auto getPolygonMaterial = [&] (int p) -> FbxSurfaceMaterial* {
					if (!mElem) return (n->GetMaterialCount() > 0 ? n->GetMaterial(0) : nullptr);
					if (mElem->GetMappingMode() == FbxLayerElement::eByPolygon) {
						int mi = mElem->GetIndexArray().GetAt(p);
						if (mi >= 0 && mi < n->GetMaterialCount()) return n->GetMaterial(mi);
					}
					else if (mElem->GetMappingMode() == FbxLayerElement::eAllSame) {
						int mi = mElem->GetIndexArray().GetCount() > 0 ? mElem->GetIndexArray().GetAt(0) : 0;
						if (mi >= 0 && mi < n->GetMaterialCount()) return n->GetMaterial(mi);
					}
					return (n->GetMaterialCount() > 0 ? n->GetMaterial(0) : nullptr);
				};

				// For subset creation
				UINT runningIndex = (UINT)m_verticesCPU.size(); // First index for the upcoming range
				bool subsetOpen = false;
				Subset curSubset {};

				auto openSubset = [&] (ID3D11ShaderResourceView* srv, const XMFLOAT4& col) {
					curSubset = {};
					curSubset.startIndex = (UINT) m_verticesCPU.size(); // 0-based index since IB = 0..n-1
					curSubset.indexCount = 0;
					curSubset.pSRV = srv;
					if (!curSubset.pSRV) {
						curSubset.pSRV = getOrCreateBlue(); // No texture -> blue 1x1
					}
					curSubset.diffuse = col;
					subsetOpen = true;
				};
				auto closeSubset = [&] () {
					if (!subsetOpen) return;
					if (curSubset.indexCount > 0) {
						m_subsets.emplace_back(curSubset);
					}
					else {
						if (curSubset.pSRV) curSubset.pSRV->Release();
					}
					subsetOpen = false;
				};

				for (int p = 0; p < polyCount; ++p) {
					int sz = mesh->GetPolygonSize(p);
					if (sz < 3) continue;

					// This polygon's material -> SRV / Diffuse
					FbxSurfaceMaterial* fbxMat = getPolygonMaterial(p);
					ID3D11ShaderResourceView* srv = LoadSRVFromMaterial(fbxMat); // Already AddRef'd
					XMFLOAT4 mcol = LoadDiffuseColorFromMaterial(fbxMat);

					// Switch subset (start new if SRV differs)
					if (!subsetOpen) {
						openSubset(srv, mcol);
					}
					else {
						bool diffSrv = false;
						if ((srv == nullptr && curSubset.pSRV != nullptr) ||
							(srv != nullptr && curSubset.pSRV == nullptr) ||
							(srv && curSubset.pSRV && srv != curSubset.pSRV)) {
							diffSrv = true;
						}
						if (diffSrv) {
							closeSubset();
							openSubset(srv, mcol);
						}
						else {
							// Same SRV -> keep current subset
							if (srv) srv->Release(); // If not opening a subset, release the extra ref
						}
					}

					XMFLOAT3 faceN(0, 0, 1);
					bool needFaceN = (nElem == nullptr || !(nMap == FbxLayerElement::eByPolygonVertex || nMap == FbxLayerElement::eByControlPoint));

					int pvBase = mesh->GetPolygonVertexIndex(p);
					for (int i = 1; i + 1 < sz; ++i) {
						int cpIdxTri[3] = {
							mesh->GetPolygonVertices()[pvBase + 0],
							mesh->GetPolygonVertices()[pvBase + i],
							mesh->GetPolygonVertices()[pvBase + i + 1]
						};

						VertexPNTC tri[3] {};
						// Position (geometric correction)
						for (int c = 0; c < 3; ++c) {
							FbxVector4 p0 = mesh->GetControlPoints()[cpIdxTri[c]];
							FbxVector4 pl = finalM.MultT(p0);
							tri[c].Position = XMFLOAT3((float) pl[0], (float) pl[1], (float) pl[2]);
						}

						if (needFaceN) {
							XMVECTOR a = XMVectorSet(
								tri[1].Position.x - tri[0].Position.x,
								tri[1].Position.y - tri[0].Position.y,
								tri[1].Position.z - tri[0].Position.z, 0);
							XMVECTOR b = XMVectorSet(
								tri[2].Position.x - tri[0].Position.x,
								tri[2].Position.y - tri[0].Position.y,
								tri[2].Position.z - tri[0].Position.z, 0);
							XMVECTOR nrm = XMVector3Normalize(XMVector3Cross(a, b));
							XMStoreFloat3(&faceN, nrm);
						}

						for (int c = 0; c < 3; ++c) {
							if (nElem && (nMap == FbxLayerElement::eByPolygonVertex || nMap == FbxLayerElement::eByControlPoint)) {
								int idx = 0;
								if (nMap == FbxLayerElement::eByPolygonVertex) {
									int corner = (c == 0) ? 0 : (c == 1 ? i : i + 1);
									int pvIndex = mesh->GetPolygonVertexIndex(p) + corner;
									idx = (nRef == FbxLayerElement::eDirect) ? pvIndex : nElem->GetIndexArray().GetAt(pvIndex);
								}
								else {
									int cpI = cpIdxTri[c];
									idx = (nRef == FbxLayerElement::eDirect) ? cpI : nElem->GetIndexArray().GetAt(cpI);
								}
								FbxVector4 nv = nElem->GetDirectArray().GetAt(idx);
								XMFLOAT3 nIn((float) nv[0], (float) nv[1], (float) nv[2]);
								tri[c].Normal = TransformNormal_IT(invTrans, nIn);
							}
							else {
								tri[c].Normal = faceN;
							}
						}

						for (int c = 0; c < 3; ++c) {
							int corner = (c == 0) ? 0 : (c == 1 ? i : i + 1);
							FbxVector2 uv(0.0, 0.0);
							bool unmapped = false;
							if (uvNameC && mesh->GetElementUVCount() > 0) {
								mesh->GetPolygonVertexUV(p, corner, uvNameC, uv, unmapped);
							}
							float u = (float) uv[0];
							float v = 1.0f - (float) uv[1];
							tri[c].TexCoord = XMFLOAT2(u, v);
						}

						// Change: also apply material diffuse to vertex color
						tri[0].Diffuse = mcol;
						tri[1].Diffuse = mcol;
						tri[2].Diffuse = mcol;

						int order[3] = {0, 1, 2};
						if (invertWinding) { order[1] = 2; order[2] = 1; }

						// Push vertex
						m_verticesCPU.emplace_back(tri[order[0]]);
						m_verticesCPU.emplace_back(tri[order[1]]);
						m_verticesCPU.emplace_back(tri[order[2]]);

						// CP index: skinned meshes only (non-skinned must not alias skin CP buffer)
						if (meshIsSkinned && itSkinBase != cpBase.end()) {
							m_cpIndexOfVertex.emplace_back(cpBaseOffset + cpIdxTri[order[0]]);
							m_cpIndexOfVertex.emplace_back(cpBaseOffset + cpIdxTri[order[1]]);
							m_cpIndexOfVertex.emplace_back(cpBaseOffset + cpIdxTri[order[2]]);
						}
						else {
							m_cpIndexOfVertex.emplace_back(-1);
							m_cpIndexOfVertex.emplace_back(-1);
							m_cpIndexOfVertex.emplace_back(-1);
						}

						// Add to current subset index count (3 verts = 3 indices)
						curSubset.indexCount += 3;
					}
				}

				// Finalize the last subset
				closeSubset();

				if (m_hasSkin && !meshIsSkinned) {
					const size_t vertCount = m_verticesCPU.size() - rigidVertStart;
					if (vertCount > 0) {
						RigidMeshBake rb {};
						rb.meshNode = n;
						rb.vertStart = rigidVertStart;
						rb.vertCount = vertCount;
						rb.restPos.resize(vertCount);
						rb.restNormal.resize(vertCount);
						for (size_t i = 0; i < vertCount; ++i) {
							const VertexPNTC& v = m_verticesCPU[rigidVertStart + i];
							rb.restPos[i] = v.Position;
							rb.restNormal[i] = v.Normal;
						}
						m_rigidBake.push_back(std::move(rb));
					}
				}
			}
		}
		for (int i = 0; i < n->GetChildCount(); ++i) Traverse(n->GetChild(i));
	};
	Traverse(m_baseScene->GetRootNode());

	if (m_verticesCPU.empty() || m_cpIndexOfVertex.size() != m_verticesCPU.size()) {
		if (reportErrors) MessageBoxA(nullptr, "FBX has no valid polygons.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	// Create VB / IB
	{
		m_vbDynamic = m_hasSkin;
		D3D11_BUFFER_DESC bd {};
		bd.Usage = m_vbDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bd.ByteWidth = static_cast<UINT>(m_verticesCPU.size() * sizeof(VertexPNTC));
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = m_vbDynamic ? D3D11_CPU_ACCESS_WRITE : 0;

		D3D11_SUBRESOURCE_DATA init {};
		init.pSysMem = m_verticesCPU.data();
		if (FAILED(GetDevice()->CreateBuffer(&bd, &init, &m_pVB)) || !m_pVB) {
			if (reportErrors) MessageBoxA(nullptr, "FBX: failed to create vertex buffer.", "Error", MB_OK | MB_ICONERROR);
			return false;
		}
	}
	{
		m_indexCount = static_cast<UINT>(m_verticesCPU.size());
		std::vector<uint32_t> idx(m_indexCount);
		for (UINT i = 0; i < m_indexCount; ++i) idx[i] = i;

		D3D11_BUFFER_DESC bd {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = static_cast<UINT>(idx.size() * sizeof(uint32_t));
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA init {};
		init.pSysMem = idx.data();
		if (FAILED(GetDevice()->CreateBuffer(&bd, &init, &m_pIB)) || !m_pIB) {
			if (reportErrors) MessageBoxA(nullptr, "FBX: failed to create index buffer.", "Error", MB_OK | MB_ICONERROR);
			return false;
		}
	}

	if (m_hasSkin && !m_skinMeshGeo.empty()) {
		int totalCP = 0;
		for (const auto& g : m_skinMeshGeo)
			totalCP = (std::max)(totalCP, g.cpBase + g.cpCount);
		m_geoIndexPerCP.assign((size_t)totalCP, -1);
		for (size_t gi = 0; gi < m_skinMeshGeo.size(); ++gi) {
			const auto& g = m_skinMeshGeo[gi];
			for (int c = 0; c < g.cpCount; ++c)
				m_geoIndexPerCP[(size_t)g.cpBase + (size_t)c] = (int)gi;
		}

		m_vertexGeoIndex.resize(m_verticesCPU.size());
		for (size_t vi = 0; vi < m_verticesCPU.size(); ++vi) {
			const int cpi = m_cpIndexOfVertex[vi];
			m_vertexGeoIndex[vi] = (cpi >= 0 && (size_t)cpi < m_geoIndexPerCP.size())
				? m_geoIndexPerCP[(size_t)cpi] : -1;
		}

		m_skinPosBuf.assign((size_t)totalCP, XMFLOAT3(0, 0, 0));
		m_skinWeightScratch.assign((size_t)totalCP, 0.0);
	}

	InitAnimationInfo();

	if (m_hasSkin) {
		m_skinnedMeshCache = skinnedList;
		m_meshCpOffset = cpBase;
		BakeSkinningData();
		BakeRigidMeshData();
		BakeVertexBoneWeights();
		const size_t gpuSlots = m_skinBake.size() * m_skinBones.size();
		m_gpuSkinEligible =
			!m_skinBake.empty()
			&& !m_skinBones.empty()
			&& gpuSlots > 0
			&& gpuSlots <= (size_t)SHADER_MAX_BONES
			&& m_rigidBake.empty()
			&& !m_gpuBindConflict
			&& m_boneBindOffset.size() == gpuSlots;
		{
			char msg[256];
			snprintf(msg, sizeof(msg),
				"[Model3D] gpuSkin meshes=%zu bones=%zu slots=%zu verts=%zu eligible=%d conflict=%d\n",
				m_skinBake.size(), m_skinBones.size(), gpuSlots,
				m_verticesCPU.size(), m_gpuSkinEligible ? 1 : 0, m_gpuBindConflict ? 1 : 0);
			hal::DebugWriteA(msg);
		}
		if (m_gpuSkinEligible) {
			m_vbDynamic = false;
			RecreateVertexBufferFromCPU();
			FillBindPoseScratch();
		}
	}

	// Cleanup matSRV cache
	// SRVs handed to subsets are already AddRef'd, so
	// release the cache-side references
	for (auto& pair : matSRV) {
		if (pair.second) {
			pair.second->Release();
		}
	}
	matSRV.clear();

	{
		std::vector<XMFLOAT3> positions;
		positions.reserve(m_verticesCPU.size());
		for (const auto& v : m_verticesCPU)
			positions.push_back(v.Position);
		CacheLocalAabbFromPositions(positions.data(), positions.size(), m_hasSkin);
	}

	return true;
}

fbxsdk::FbxNode* Model3D::FindBone(const char* name) const {
	if (!m_baseScene || !name || !*name) return nullptr;
	std::function<fbxsdk::FbxNode* (fbxsdk::FbxNode*)> search = [&] (fbxsdk::FbxNode* node) -> fbxsdk::FbxNode* {
		if (!node) return nullptr;
		if (strcmp(node->GetName(), name) == 0) return node;
		for (int i = 0; i < node->GetChildCount(); ++i) {
			if (auto* found = search(node->GetChild(i))) return found;
		}
		return nullptr;
	};
	return search(m_baseScene->GetRootNode());
}

fbxsdk::FbxNode* Model3D::FindBone(const wchar_t* name) const {
	// Use WideToUTF8 if comparing as UTF-8
	std::string nameUtf8 = WideToUTF8(name);
	if (!m_baseScene || !name || !*name) return nullptr;
	std::function<fbxsdk::FbxNode* (fbxsdk::FbxNode*)> search = [&] (fbxsdk::FbxNode* node) -> fbxsdk::FbxNode* {
		if (!node) return nullptr;
		if (strcmp(node->GetName(), nameUtf8.c_str()) == 0) return node;
		for (int i = 0; i < node->GetChildCount(); ++i) {
			if (auto* found = search(node->GetChild(i))) return found;
		}
		return nullptr;
	};
	return search(m_baseScene->GetRootNode());
}

bool Model3D::TryGetBoneLocalMatrix(const char* boneName, DirectX::XMMATRIX& out) const {
	if (!boneName || !*boneName)
		return false;

	if (m_boneXformScratch.size() == m_skinBones.size() && !m_skinBones.empty()) {
		for (size_t i = 0; i < m_skinBones.size(); ++i) {
			const FbxNode* node = m_skinBones[i].animBone ? m_skinBones[i].animBone : m_skinBones[i].baseBone;
			if (!node || strcmp(node->GetName(), boneName) != 0)
				continue;
			const FbxAMatrix& m = m_boneXformScratch[i];
			out = XMMatrixSet(
				(float)m.Get(0, 0), (float)m.Get(0, 1), (float)m.Get(0, 2), (float)m.Get(0, 3),
				(float)m.Get(1, 0), (float)m.Get(1, 1), (float)m.Get(1, 2), (float)m.Get(1, 3),
				(float)m.Get(2, 0), (float)m.Get(2, 1), (float)m.Get(2, 2), (float)m.Get(2, 3),
				(float)m.Get(3, 0), (float)m.Get(3, 1), (float)m.Get(3, 2), (float)m.Get(3, 3));
			return true;
		}
	}

	FbxScene* scene = m_currentAnimScene ? m_currentAnimScene : m_baseScene;
	if (!scene)
		return false;

	FbxNode* bone = scene->FindNodeByName(boneName);
	if (!bone && m_baseScene && scene != m_baseScene)
		bone = m_baseScene->FindNodeByName(boneName);
	if (!bone)
		return false;

	FbxTime t;
	t.SetSecondDouble(m_animTimeSec);
	const FbxAMatrix m = bone->EvaluateGlobalTransform(t);
	out = XMMatrixSet(
		(float)m.Get(0, 0), (float)m.Get(0, 1), (float)m.Get(0, 2), (float)m.Get(0, 3),
		(float)m.Get(1, 0), (float)m.Get(1, 1), (float)m.Get(1, 2), (float)m.Get(1, 3),
		(float)m.Get(2, 0), (float)m.Get(2, 1), (float)m.Get(2, 2), (float)m.Get(2, 3),
		(float)m.Get(3, 0), (float)m.Get(3, 1), (float)m.Get(3, 2), (float)m.Get(3, 3));
	return true;
}

void Model3D::ScaleBoneAndChildren(fbxsdk::FbxNode* bone, const fbxsdk::FbxDouble3& scale) {
	if (!bone) return;
	// To get current scale and multiply, use below
	//FbxDouble3 current = bone->LclScaling.Get();
	//bone->LclScaling.Set(FbxDouble3(current[0]*scale[0], current[1]*scale[1], current[2]*scale[2]));

	// To overwrite instead (e.g. scale 2x)
	bone->LclScaling.Set(scale);


	int childCount = bone->GetChildCount();
	for (int i = 0; i < childCount; ++i) {
		ScaleBoneAndChildren(bone->GetChild(i), scale);
	}
}

void Model3D::SetPosition(const XMFLOAT3& p) { m_position = p; }
void Model3D::SetRotateEulerDeg(const XMFLOAT3& rDeg) { m_rotateDeg = rDeg; }
void Model3D::SetScale(const XMFLOAT3& s) { m_scale = s; }

namespace {

void ApplyMaterialOverride(MATERIAL& mat, const Model3D::MaterialOverride& o) {
	if (!o.enabled)
		return;

	mat.Diffuse.x *= o.color.x;
	mat.Diffuse.y *= o.color.y;
	mat.Diffuse.z *= o.color.z;

	const float rough = (std::max)(0.0f, (std::min)(1.0f, o.roughness));
	const float metal = (std::max)(0.0f, (std::min)(1.0f, o.metallic));
	mat.Shininess = 2.0f + (1.0f - rough) * 254.0f;
	mat.Roughness = rough;
	mat.Metallic = metal;

	const float f0 = 0.04f;
	mat.Specular.x = f0 + (o.color.x - f0) * metal;
	mat.Specular.y = f0 + (o.color.y - f0) * metal;
	mat.Specular.z = f0 + (o.color.z - f0) * metal;

	const bool hasNormal = o.normalSRV != nullptr && o.normalStrength > 0.0f;
	mat.NormalStrength = hasNormal ? o.normalStrength : 0.0f;

	mat.ShadeSteps = o.shadeSteps;
	mat.ShadeSmoothness = o.shadeSmoothness;
	mat.SpecularThreshold = o.specularThreshold;
	mat.OutlineWidth = o.outlineWidth;
	mat.OutlineColor = o.outlineColor;
	mat.ShadowTint = DirectX::XMFLOAT4(o.shadowTint.x, o.shadowTint.y, o.shadowTint.z, 1.0f);
	mat.UseRamp = (o.rampSRV != nullptr) ? 1.0f : 0.0f;
	mat.RimStrength = o.rimStrength;
}

void BindNormalMapSlot(ID3D11ShaderResourceView* srv) {
	ID3D11ShaderResourceView* slot = srv;
	GetDeviceContext()->PSSetShaderResources(5, 1, &slot);
}

void UnbindNormalMapSlot() {
	ID3D11ShaderResourceView* nullSRV = nullptr;
	GetDeviceContext()->PSSetShaderResources(5, 1, &nullSRV);
}

void BindRampSlot(ID3D11ShaderResourceView* srv) {
	ID3D11ShaderResourceView* slot = srv;
	GetDeviceContext()->PSSetShaderResources(6, 1, &slot);
}

void UnbindRampSlot() {
	ID3D11ShaderResourceView* nullSRV = nullptr;
	GetDeviceContext()->PSSetShaderResources(6, 1, &nullSRV);
}

void InitMaterialDefaults(MATERIAL& mat, bool hasTex, const DirectX::XMFLOAT4& emission) {
	mat.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, hasTex ? 1.0f : 0.0f);
	mat.Ambient = DirectX::XMFLOAT4(0.30f, 0.28f, 0.25f, 0.6f);
	mat.Specular = DirectX::XMFLOAT4(0.02f, 0.02f, 0.02f, 0.5f);
	mat.Shininess = 32.0f;
	mat.Emission = emission;
	mat.Roughness = 0.5f;
	mat.Metallic = 0.0f;
	mat.NormalStrength = 0.0f;
	mat.ShadeSteps = 3.0f;
	mat.ShadeSmoothness = 0.05f;
	mat.SpecularThreshold = 0.85f;
	mat.OutlineWidth = 0.003f;
	mat.OutlineColor = DirectX::XMFLOAT4(0.05f, 0.05f, 0.08f, 1.0f);
	mat.ShadowTint = DirectX::XMFLOAT4(0.6f, 0.65f, 0.85f, 1.0f);
	mat.UseRamp = 0.0f;
	mat.RimStrength = 0.25f;
	mat._padToon0 = 0.0f;
	mat._padToon1 = 0.0f;
}

} // namespace

void Model3D::Draw() {
	if (!m_pVB || !m_pIB || m_indexCount == 0) return;

	SetDepthEnable(true);

	if (!Shader_IsExternalVP()) {
		const auto& cam = GetCamera();
		Shader_SetProjectionMatrix(cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
		Shader_SetViewMatrix(cam.GetViewMatrix());
	}

	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(
		DirectX::XMConvertToRadians(m_rotateDeg.x),
		DirectX::XMConvertToRadians(m_rotateDeg.y),
		DirectX::XMConvertToRadians(m_rotateDeg.z));
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	Shader_SetWorldMatrix(S * R * T);

	if (m_shaderVariant == Shader3DVariant::SurfaceFX) {
		float rMax = 1.0f;
		if (!m_verticesCPU.empty()) {
			rMax = 0.0f;
			for (const auto& v : m_verticesCPU) {
				const float r = sqrtf(v.Position.x * v.Position.x + v.Position.z * v.Position.z);
				rMax = (std::max)(rMax, r);
			}
			if (rMax < 1e-4f) rMax = 1.0f;
		}
		Shader_SetSurfaceFXMeshRadius(rMax);
		Shader_CommitSurfaceFX();
	}

	if (CanGpuSkin())
		Shader_SetSkinPalette(m_gpuPaletteFront.data(), (int)m_gpuPaletteFront.size(), true);
	else
		Shader_SetSkinPalette(nullptr, 0, false);

	UINT stride = sizeof(VertexPNTC);
	UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &m_pVB, &stride, &offset);
	GetDeviceContext()->IASetIndexBuffer(m_pIB, DXGI_FORMAT_R32_UINT, 0);
	GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const bool isToon = (m_shaderVariant == Shader3DVariant::Toon);
	const float outlineWidth = m_materialOverride.enabled
		? m_materialOverride.outlineWidth
		: 0.003f;

	// Inverted-hull outline pass (toon only; skip during shadow map generation)
	if (isToon && outlineWidth > 1e-6f && Shadow::GetRenderingLightIndex() < 0) {
		MATERIAL outlineMat{};
		InitMaterialDefaults(outlineMat, false, m_emissionOverride);
		ApplyMaterialOverride(outlineMat, m_materialOverride);
		Shader_SetMaterial(outlineMat);

		SetCullFront(true);
		Shader3D_BeginOutline();
		if (!m_subsets.empty()) {
			for (const auto& s : m_subsets)
				GetDeviceContext()->DrawIndexed(s.indexCount, s.startIndex, 0);
		} else {
			GetDeviceContext()->DrawIndexed(m_indexCount, 0, 0);
		}
		SetCullFront(false);
	}

	Shader3D_Begin(m_shaderVariant);
	if (CanGpuSkin())
		Shader3D_UseGpuSkin();

	const bool bindNormal =
		m_materialOverride.enabled
		&& m_materialOverride.normalSRV
		&& m_materialOverride.normalStrength > 0.0f
		&& m_shaderVariant != Shader3DVariant::SurfaceFX;
	if (bindNormal)
		BindNormalMapSlot(m_materialOverride.normalSRV);
	else
		UnbindNormalMapSlot();

	const bool bindRamp = isToon && m_materialOverride.enabled && m_materialOverride.rampSRV;
	if (bindRamp)
		BindRampSlot(m_materialOverride.rampSRV);
	else
		UnbindRampSlot();

	if (!m_subsets.empty()) {
		for (const auto& s : m_subsets) {
			ID3D11ShaderResourceView* toBind = m_pTextureOverride
				? m_pTextureOverride
				: (s.pSRV ? s.pSRV : m_pSRV);
			GetDeviceContext()->PSSetShaderResources(0, 1, &toBind);

			bool hasTex = m_pTextureOverride || (s.pSRV != nullptr);

			MATERIAL mat{};
			InitMaterialDefaults(mat, hasTex, m_emissionOverride);
			ApplyMaterialOverride(mat, m_materialOverride);
			Shader_SetMaterial(mat);
			GetDeviceContext()->DrawIndexed(s.indexCount, s.startIndex, 0);
		}
	} else {
		ID3D11ShaderResourceView* toBind = m_pTextureOverride ? m_pTextureOverride : m_pSRV;
		GetDeviceContext()->PSSetShaderResources(0, 1, &toBind);

		bool hasTex = m_pTextureOverride || m_hasTexture;

		MATERIAL mat{};
		InitMaterialDefaults(mat, hasTex, m_emissionOverride);
		ApplyMaterialOverride(mat, m_materialOverride);
		Shader_SetMaterial(mat);
		GetDeviceContext()->DrawIndexed(m_indexCount, 0, 0);
	}

	if (bindNormal)
		UnbindNormalMapSlot();
	if (bindRamp)
		UnbindRampSlot();
}

/**===========================================================================
   <Shadow volume>
 ===========================================================================**/
Model3D::ShadowVolumeData::~ShadowVolumeData() {
	if (vb) { vb->Release(); vb = nullptr; }
	if (ib) { ib->Release(); ib = nullptr; }
}

void Model3D::ReleaseShadowVolumeInstanceData() {
	if (m_svDynamicVB) { m_svDynamicVB->Release(); m_svDynamicVB = nullptr; }
	m_svScratch.clear();
}

bool Model3D::ShadowVolumeNeedsCpuRefresh() const {
	// GPU skin deforms the bind-pose corners in the volume VS. CPU skin and rigid clips
	// rewrite m_verticesCPU every update, so the corners are re-extracted per draw.
	if (CanGpuSkin()) return false;
	return m_vbDynamic || m_hasSkin || !m_rigidBake.empty();
}

bool Model3D::EnsureShadowVolume(const char* debugName) {
	if (!m_shadowVolume)
		m_shadowVolume = std::make_shared<ShadowVolumeData>();
	ShadowVolumeData& sv = *m_shadowVolume;
	if (sv.attempted)
		return sv.closed && sv.ib && sv.vb;
	sv.attempted = true;

	const char* name = (debugName && *debugName) ? debugName : "(unnamed mesh)";
	if (m_verticesCPU.empty()) {
		char msg[512];
		std::snprintf(msg, sizeof(msg),
			"[ShadowVolume] %s: no CPU vertices (only FBX meshes can cast shadow volumes)\n", name);
		hal::DebugWriteA(msg);
		return false;
	}

	// FBX index buffers are the identity: vertex i is corner i.
	std::vector<uint32_t> identity(m_verticesCPU.size());
	for (size_t i = 0; i < identity.size(); ++i) identity[i] = static_cast<uint32_t>(i);
	const auto posOf = [this](size_t i) {
		const XMFLOAT3& p = m_verticesCPU[i].Position;
		return shadow_volume::Vec3 { p.x, p.y, p.z };
	};
	sv.topology = shadow_volume::Build(m_verticesCPU.size(), posOf, identity.data(), identity.size());
	const shadow_volume::BuildStats& st = sv.topology.stats;
	if (!sv.topology.Closed()) {
		char msg[512];
		std::snprintf(msg, sizeof(msg),
			"[ShadowVolume] %s: mesh is not closed (open edges %zu, non-manifold edges %zu, "
			"triangles %zu); it casts no shadow volume\n",
			name, st.openEdges, st.nonManifoldEdges, st.triangles);
		hal::DebugWriteA(msg);
		sv.topology = {};
		return false;
	}

	std::vector<shadow_volume::Vec3> normals;
	shadow_volume::FillCornerNormals(sv.topology, posOf, normals);
	std::vector<VertexPNTC> corners(sv.topology.CornerCount());
	for (size_t c = 0; c < corners.size(); ++c) {
		corners[c] = m_verticesCPU[sv.topology.cornerSource[c]];
		corners[c].Normal = XMFLOAT3(normals[c].x, normals[c].y, normals[c].z);
	}

	D3D11_BUFFER_DESC bd {};
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.ByteWidth = static_cast<UINT>(corners.size() * sizeof(VertexPNTC));
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA init {};
	init.pSysMem = corners.data();
	if (FAILED(GetDevice()->CreateBuffer(&bd, &init, &sv.vb)) || !sv.vb) {
		hal::DebugWriteA("[ShadowVolume] failed to create the volume vertex buffer\n");
		return false;
	}

	bd.ByteWidth = static_cast<UINT>(sv.topology.indices.size() * sizeof(uint32_t));
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	init.pSysMem = sv.topology.indices.data();
	if (FAILED(GetDevice()->CreateBuffer(&bd, &init, &sv.ib)) || !sv.ib) {
		hal::DebugWriteA("[ShadowVolume] failed to create the volume index buffer\n");
		sv.vb->Release();
		sv.vb = nullptr;
		return false;
	}
	sv.indexCount = static_cast<UINT>(sv.topology.indices.size());
	sv.closed = true;

	char msg[512];
	std::snprintf(msg, sizeof(msg),
		"[ShadowVolume] %s: %zu triangles, %zu edges -> %u volume indices%s\n",
		name, st.triangles, st.edges, sv.indexCount,
		st.flippedWinding ? " (inward winding flipped)" : "");
	hal::DebugWriteA(msg);
	return true;
}

bool Model3D::DrawShadowVolume(const char* debugName) {
	if (!ShadowVolume::IsPassActive())
		return true;
	if (!EnsureShadowVolume(debugName))
		return false;
	ShadowVolumeData& sv = *m_shadowVolume;

	ID3D11Buffer* vb = sv.vb;
	if (ShadowVolumeNeedsCpuRefresh()) {
		const auto& topo = sv.topology;
		if (topo.cornerSource.empty())
			return false;
		const auto posOf = [this](size_t i) {
			const XMFLOAT3& p = m_verticesCPU[i].Position;
			return shadow_volume::Vec3 { p.x, p.y, p.z };
		};
		for (uint32_t src : topo.cornerSource)
			if (src >= m_verticesCPU.size())
				return false;  // vertex count changed under us; skip this frame
		static thread_local std::vector<shadow_volume::Vec3> normals;
		shadow_volume::FillCornerNormals(topo, posOf, normals);
		m_svScratch.resize(topo.CornerCount());
		for (size_t c = 0; c < m_svScratch.size(); ++c) {
			m_svScratch[c] = m_verticesCPU[topo.cornerSource[c]];
			m_svScratch[c].Normal = XMFLOAT3(normals[c].x, normals[c].y, normals[c].z);
		}
		const UINT bytes = static_cast<UINT>(m_svScratch.size() * sizeof(VertexPNTC));
		if (!m_svDynamicVB) {
			D3D11_BUFFER_DESC bd {};
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.ByteWidth = bytes;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			if (FAILED(GetDevice()->CreateBuffer(&bd, nullptr, &m_svDynamicVB)) || !m_svDynamicVB)
				return true;  // closed mesh; transient failure, just skip this draw
		}
		D3D11_MAPPED_SUBRESOURCE mapped {};
		if (FAILED(GetDeviceContext()->Map(m_svDynamicVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return true;
		std::memcpy(mapped.pData, m_svScratch.data(), bytes);
		GetDeviceContext()->Unmap(m_svDynamicVB, 0);
		vb = m_svDynamicVB;
	}

	if (!Shader_IsExternalVP()) {
		const auto& cam = GetCamera();
		Shader_SetProjectionMatrix(cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
		Shader_SetViewMatrix(cam.GetViewMatrix());
	}
	const XMMATRIX T = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	const XMMATRIX R = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(m_rotateDeg.x),
		XMConvertToRadians(m_rotateDeg.y),
		XMConvertToRadians(m_rotateDeg.z));
	const XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	const XMMATRIX world = S * R * T;
	Shader_SetWorldMatrix(world);

	if (CanGpuSkin())
		Shader_SetSkinPalette(m_gpuPaletteFront.data(), (int)m_gpuPaletteFront.size(), true);
	else
		Shader_SetSkinPalette(nullptr, 0, false);

	const UINT stride = sizeof(VertexPNTC);
	const UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	GetDeviceContext()->IASetIndexBuffer(sv.ib, DXGI_FORMAT_R32_UINT, 0);
	ShadowVolume::DrawVolume(world, sv.indexCount);
	return true;
}

const Model3D::SkinMeshGeo* Model3D::FindGeoForCP(int cpi) const {
	if (cpi < 0 || (size_t)cpi >= m_geoIndexPerCP.size()) return nullptr;
	const int gi = m_geoIndexPerCP[(size_t)cpi];
	if (gi < 0 || (size_t)gi >= m_skinMeshGeo.size()) return nullptr;
	return &m_skinMeshGeo[(size_t)gi];
}

XMFLOAT3 Model3D::ApplyGeoPosition(const FbxAMatrix& geo, const XMFLOAT3& p) {
	const FbxVector4 out = geo.MultT(FbxVector4(p.x, p.y, p.z, 1.0));
	return XMFLOAT3((float)out[0], (float)out[1], (float)out[2]);
}

namespace {

XMMATRIX FbxToXM(const FbxAMatrix& m) {
	return XMMatrixSet(
		(float)m.Get(0, 0), (float)m.Get(0, 1), (float)m.Get(0, 2), (float)m.Get(0, 3),
		(float)m.Get(1, 0), (float)m.Get(1, 1), (float)m.Get(1, 2), (float)m.Get(1, 3),
		(float)m.Get(2, 0), (float)m.Get(2, 1), (float)m.Get(2, 2), (float)m.Get(2, 3),
		(float)m.Get(3, 0), (float)m.Get(3, 1), (float)m.Get(3, 2), (float)m.Get(3, 3));
}

} // namespace

bool Model3D::CanGpuSkin() const {
	const size_t slots = m_skinBake.size() * m_skinBones.size();
	return m_gpuSkinEligible
		&& !m_gpuBindConflict
		&& slots > 0
		&& slots <= (size_t)SHADER_MAX_BONES
		&& m_gpuPaletteFront.size() == slots
		&& m_boneBindOffset.size() == slots
		&& !m_skinBones.empty();
}

void Model3D::RecreateVertexBufferFromCPU() {
	if (m_sharesCachedVB || m_verticesCPU.empty())
		return;
	if (m_pVB) {
		m_pVB->Release();
		m_pVB = nullptr;
	}

	D3D11_BUFFER_DESC bd {};
	bd.Usage = m_vbDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	bd.ByteWidth = static_cast<UINT>(m_verticesCPU.size() * sizeof(VertexPNTC));
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = m_vbDynamic ? D3D11_CPU_ACCESS_WRITE : 0;

	D3D11_SUBRESOURCE_DATA init {};
	init.pSysMem = m_verticesCPU.data();
	GetDevice()->CreateBuffer(&bd, &init, &m_pVB);
}

void Model3D::BakeVertexBoneWeights() {
	for (VertexPNTC& v : m_verticesCPU) {
		v.BlendIndices = { 0, 0, 0, 0 };
		v.BlendWeights = { 0.0f, 0.0f, 0.0f, 0.0f };
	}
	if (m_skinBake.empty() || m_cpIndexOfVertex.size() != m_verticesCPU.size())
		return;

	int totalCP = 0;
	for (const SkinnedMeshBake& bake : m_skinBake)
		totalCP = (std::max)(totalCP, bake.cpOffset + bake.cpCount);
	if (totalCP <= 0)
		return;

	const int nBones = (int)m_skinBones.size();
	struct Infl { int bone = 0; double w = 0.0; };
	std::vector<std::vector<Infl>> perCp((size_t)totalCP);
	for (size_t mi = 0; mi < m_skinBake.size(); ++mi) {
		const SkinnedMeshBake& bake = m_skinBake[mi];
		const int boneBase = (nBones > 0) ? (int)mi * nBones : 0;
		for (const SkinClusterBake& cluster : bake.clusters) {
			if (cluster.boneIndex < 0 || cluster.indexCount <= 0 || !cluster.indices || !cluster.weights)
				continue;
			for (int k = 0; k < cluster.indexCount; ++k) {
				const int local = cluster.indices[k];
				if (local < 0 || local >= bake.cpCount)
					continue;
				const int cpi = bake.cpOffset + local;
				if (cpi < 0 || cpi >= totalCP)
					continue;
				const double w = cluster.weights[k];
				if (w <= 0.0)
					continue;
				perCp[(size_t)cpi].push_back({ boneBase + cluster.boneIndex, w });
			}
		}
	}

	std::vector<XMUINT4> cpIdx((size_t)totalCP, XMUINT4{ 0, 0, 0, 0 });
	std::vector<XMFLOAT4> cpW((size_t)totalCP, XMFLOAT4{ 0, 0, 0, 0 });
	for (int i = 0; i < totalCP; ++i) {
		auto& list = perCp[(size_t)i];
		if (list.empty())
			continue;
		std::sort(list.begin(), list.end(), [](const Infl& a, const Infl& b) { return a.w > b.w; });
		// Merge identical bones after sort (keep first N unique).
		Infl top[4] {};
		int n = 0;
		for (const Infl& in : list) {
			int found = -1;
			for (int t = 0; t < n; ++t) {
				if (top[t].bone == in.bone) { found = t; break; }
			}
			if (found >= 0) {
				top[found].w += in.w;
				continue;
			}
			if (n < 4)
				top[n++] = in;
		}
		float w[4] = {};
		uint32_t idx[4] = {};
		for (int t = 0; t < n; ++t) {
			idx[t] = (uint32_t)top[t].bone;
			w[t] = (float)top[t].w;
		}
		cpIdx[(size_t)i] = XMUINT4{ idx[0], idx[1], idx[2], idx[3] };
		cpW[(size_t)i] = XMFLOAT4{ w[0], w[1], w[2], w[3] };
	}

	for (size_t vi = 0; vi < m_verticesCPU.size(); ++vi) {
		const int cpi = m_cpIndexOfVertex[vi];
		if (cpi < 0 || cpi >= totalCP)
			continue;
		m_verticesCPU[vi].BlendIndices = cpIdx[(size_t)cpi];
		m_verticesCPU[vi].BlendWeights = cpW[(size_t)cpi];
	}
}

void Model3D::FillBindPoseScratch() {
	if (m_skinBones.empty() || !m_baseScene)
		return;

	FbxTime t;
	t.SetSecondDouble(m_animStartSec);
	FbxAnimEvaluator* eval = m_baseScene->GetAnimationEvaluator();
	if (!eval)
		return;

	m_boneXformScratch.resize(m_skinBones.size());
	for (size_t i = 0; i < m_skinBones.size(); ++i) {
		FbxNode* node = m_skinBones[i].baseBone;
		m_boneXformScratch[i] = (node && eval) ? eval->GetNodeGlobalTransform(node, t) : FbxAMatrix();
	}
	m_meshXformScratch.resize(m_skinBake.size());
	for (size_t i = 0; i < m_skinBake.size(); ++i) {
		FbxNode* node = m_skinBake[i].meshNode;
		m_meshXformScratch[i] = (node && eval) ? eval->GetNodeGlobalTransform(node, t) : FbxAMatrix();
	}
	UpdateGpuSkinPalette();
	PublishSkinPalette();
}

void Model3D::UpdateGpuSkinPalette() {
	const size_t nBones = m_skinBones.size();
	const size_t nMeshes = m_skinBake.size();
	const size_t slots = nMeshes * nBones;
	if (!m_gpuSkinEligible || m_gpuBindConflict || slots == 0
		|| m_boneXformScratch.size() != nBones
		|| m_boneBindOffset.size() != slots)
		return;

	m_gpuPaletteWrite.resize(slots);
	for (size_t mi = 0; mi < nMeshes; ++mi) {
		FbxAMatrix geo;
		geo.SetIdentity();
		if (mi < m_skinBake.size() && m_skinBake[mi].mesh) {
			for (const SkinMeshGeo& g : m_skinMeshGeo) {
				if (g.cpBase == m_skinBake[mi].cpOffset) {
					geo = g.geo;
					break;
				}
			}
		}
		const FbxAMatrix geoInv = geo.Inverse();

		FbxAMatrix meshGlobal;
		meshGlobal.SetIdentity();
		if (mi < m_meshXformScratch.size())
			meshGlobal = m_meshXformScratch[mi];
		const FbxAMatrix toLocal = meshGlobal.Inverse();

		for (size_t bi = 0; bi < nBones; ++bi) {
			const size_t slot = mi * nBones + bi;
			const FbxAMatrix palette =
				geo * toLocal * m_boneXformScratch[bi] * m_boneBindOffset[slot] * geoInv;
			// MultT is row-vector * matrix (same as DirectXMath). Shader_SetSkinPalette
			// transposes for HLSL like WVP ? do not transpose here.
			XMStoreFloat4x4(&m_gpuPaletteWrite[slot], FbxToXM(palette));
		}
	}
	m_gpuPaletteDirty = true;
}

void Model3D::PublishSkinPalette() {
	if (!m_gpuPaletteDirty)
		return;
	m_gpuPaletteFront = m_gpuPaletteWrite;
	m_gpuPaletteDirty = false;
}

void Model3D::UploadSkinnedVerticesIfDirty() {
	if (m_vbUploadDirty) {
		uploadVertexBufferFromCPU();
		m_vbUploadDirty = false;
	}
	PublishSkinPalette();
}

void Model3D::markVertexUploadDirty() {
	m_vbUploadDirty = true;
}

namespace {

void GetAnimStackSpan(FbxScene* primary, FbxScene* fallback, FbxAnimStack* stack,
	double& outStartSec, double& outDurationSec)
{
	outStartSec = 0.0;
	outDurationSec = 0.0;
	if (!stack) return;

	FbxTakeInfo* take = primary ? primary->GetTakeInfo(stack->GetName()) : nullptr;
	if (!take && fallback)
		take = fallback->GetTakeInfo(stack->GetName());
	if (take) {
		const FbxTime start = take->mLocalTimeSpan.GetStart();
		const FbxTime stop = take->mLocalTimeSpan.GetStop();
		outStartSec = start.GetSecondDouble();
		outDurationSec = stop.GetSecondDouble() - outStartSec;
		return;
	}

	FbxScene* spanScene = primary ? primary : fallback;
	if (!spanScene) return;
	FbxTimeSpan span;
	spanScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(span);
	outStartSec = span.GetStart().GetSecondDouble();
	outDurationSec = span.GetStop().GetSecondDouble() - outStartSec;
}

FbxNode* FindSampleSkinBone(FbxScene* scene) {
	if (!scene) return nullptr;
	std::vector<std::pair<FbxMesh*, FbxNode*>> skinned;
	EnumerateSkinnedMeshes(scene->GetRootNode(), skinned);
	if (skinned.empty() || !skinned[0].first) return nullptr;
	FbxMesh* mesh = skinned[0].first;
	if (mesh->GetDeformerCount(FbxDeformer::eSkin) <= 0) return nullptr;
	FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(0, FbxDeformer::eSkin));
	if (!skin || skin->GetClusterCount() <= 0) return nullptr;
	return skin->GetCluster(0)->GetLink();
}

double ScoreStackBoneMotion(FbxScene* scene, FbxScene* baseScene, FbxAnimStack* stack, FbxNode* sampleBone) {
	if (!scene || !stack || !sampleBone) return 0.0;

	FbxAnimStack* prevStack = scene->GetCurrentAnimationStack();
	scene->SetCurrentAnimationStack(stack);
	FbxAnimEvaluator* eval = scene->GetAnimationEvaluator();
	if (!eval) {
		if (prevStack) scene->SetCurrentAnimationStack(prevStack);
		return 0.0;
	}

	double startSec = 0.0;
	double durationSec = 0.0;
	GetAnimStackSpan(scene, baseScene, stack, startSec, durationSec);
	if (durationSec <= 1e-6) durationSec = 1.0;

	FbxTime t0;
	t0.SetSecondDouble(startSec);
	FbxTime t1;
	t1.SetSecondDouble(startSec + (durationSec * 0.25));
	if (t1.GetSecondDouble() <= t0.GetSecondDouble())
		t1.SetSecondDouble(t0.GetSecondDouble() + 0.1);

	const FbxAMatrix m0 = eval->GetNodeGlobalTransform(sampleBone, t0);
	const FbxAMatrix m1 = eval->GetNodeGlobalTransform(sampleBone, t1);
	const FbxVector4 dt = m1.GetT() - m0.GetT();
	const FbxVector4 dr = m1.GetR() - m0.GetR();
	const double score =
		dt[0] * dt[0] + dt[1] * dt[1] + dt[2] * dt[2] +
		dr[0] * dr[0] + dr[1] * dr[1] + dr[2] * dr[2];

	if (prevStack) scene->SetCurrentAnimationStack(prevStack);
	return score;
}

FbxAnimStack* PickBestAnimStack(FbxScene* animScene, FbxScene* baseScene,
	double& outStartSec, double& outDurationSec)
{
	outStartSec = 0.0;
	outDurationSec = 0.0;
	if (!animScene) return nullptr;

	const int stackCount = animScene->GetSrcObjectCount<FbxAnimStack>();
	if (stackCount <= 0) return nullptr;

	FbxNode* sampleBone = FindSampleSkinBone(baseScene ? baseScene : animScene);

	FbxAnimStack* bestStack = animScene->GetSrcObject<FbxAnimStack>(0);
	double bestStart = 0.0;
	double bestDuration = 0.0;
	GetAnimStackSpan(animScene, baseScene, bestStack, bestStart, bestDuration);

	double bestMotion = ScoreStackBoneMotion(animScene, baseScene, bestStack, sampleBone);
	for (int i = 1; i < stackCount; ++i) {
		FbxAnimStack* stack = animScene->GetSrcObject<FbxAnimStack>(i);
		double startSec = 0.0;
		double durationSec = 0.0;
		GetAnimStackSpan(animScene, baseScene, stack, startSec, durationSec);
		const double motion = ScoreStackBoneMotion(animScene, baseScene, stack, sampleBone);
		if (motion > bestMotion + 1e-8 ||
			(std::fabs(motion - bestMotion) <= 1e-8 && durationSec > bestDuration)) {
			bestStack = stack;
			bestStart = startSec;
			bestDuration = durationSec;
			bestMotion = motion;
		}
	}

	outStartSec = bestStart;
	outDurationSec = bestDuration;
	return bestStack;
}

} // namespace

void Model3D::BakeSkinningData() {
	m_skinBake.clear();
	m_skinBones.clear();
	m_boneBindOffset.clear();
	m_gpuBindConflict = false;
	if (!m_hasSkin || m_skinnedMeshCache.empty()) return;

	std::unordered_map<FbxNode*, int> boneIndex;
	auto resolveBoneIndex = [&](FbxNode* baseBone) -> int {
		if (!baseBone) return -1;
		auto it = boneIndex.find(baseBone);
		if (it != boneIndex.end()) return it->second;
		const int idx = (int)m_skinBones.size();
		SkinBoneBake entry {};
		entry.baseBone = baseBone;
		if (m_currentAnimScene)
			entry.animBone = m_currentAnimScene->FindNodeByName(baseBone->GetName());
		m_skinBones.push_back(entry);
		boneIndex.emplace(baseBone, idx);
		return idx;
	};

	m_skinBake.reserve(m_skinnedMeshCache.size());
	for (size_t mi = 0; mi < m_skinnedMeshCache.size(); ++mi) {
		FbxMesh* mesh = m_skinnedMeshCache[mi].first;
		FbxNode* meshNode = m_skinnedMeshCache[mi].second;
		if (!mesh || !meshNode) continue;

		SkinnedMeshBake bake {};
		bake.mesh = mesh;
		bake.meshNode = meshNode;
		bake.cpCount = mesh->GetControlPointsCount();
		if (const auto itOff = m_meshCpOffset.find(mesh); itOff != m_meshCpOffset.end())
			bake.cpOffset = itOff->second;

		const int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
		for (int si = 0; si < skinCount; ++si) {
			FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(si, FbxDeformer::eSkin));
			if (!skin) continue;
			const int clusterCount = skin->GetClusterCount();
			for (int ci = 0; ci < clusterCount; ++ci) {
				FbxCluster* cluster = skin->GetCluster(ci);
				if (!cluster) continue;

				FbxNode* baseBone = cluster->GetLink();
				const int boneIdx = resolveBoneIndex(baseBone);
				if (boneIdx < 0) continue;

				FbxAMatrix meshBind, baseBoneBind;
				cluster->GetTransformMatrix(meshBind);
				cluster->GetTransformLinkMatrix(baseBoneBind);

				SkinClusterBake clusterBake {};
				clusterBake.boneIndex = boneIdx;
				clusterBake.bindOffset = baseBoneBind.Inverse() * meshBind;
				clusterBake.indices = cluster->GetControlPointIndices();
				clusterBake.indexCount = cluster->GetControlPointIndicesCount();
				clusterBake.weights = cluster->GetControlPointWeights();
				bake.clusters.push_back(clusterBake);
			}
		}

		m_skinBake.push_back(std::move(bake));
	}

	const size_t nBones = m_skinBones.size();
	const size_t nMeshes = m_skinBake.size();
	const size_t slots = nMeshes * nBones;
	m_boneBindOffset.assign(slots, FbxAMatrix());
	std::vector<char> haveBind(slots, 0);
	for (size_t mi = 0; mi < nMeshes; ++mi) {
		for (const SkinClusterBake& cluster : m_skinBake[mi].clusters) {
			if (cluster.boneIndex < 0 || (size_t)cluster.boneIndex >= nBones)
				continue;
			const size_t slot = mi * nBones + (size_t)cluster.boneIndex;
			if (!haveBind[slot]) {
				m_boneBindOffset[slot] = cluster.bindOffset;
				haveBind[slot] = 1;
				continue;
			}
			const FbxAMatrix& prev = m_boneBindOffset[slot];
			for (int r = 0; r < 4 && !m_gpuBindConflict; ++r) {
				for (int c = 0; c < 4; ++c) {
					if (fabs(prev.Get(r, c) - cluster.bindOffset.Get(r, c)) > 1e-4) {
						m_gpuBindConflict = true;
						break;
					}
				}
			}
		}
	}
}

void Model3D::BakeRigidMeshData() {
	if (m_rigidBake.empty()) return;

	FbxTime bindTime;
	bindTime.SetSecondDouble(m_animStartSec);

	for (RigidMeshBake& rb : m_rigidBake) {
		rb.animMeshNode = nullptr;
		if (!rb.meshNode) continue;

		rb.bindGlobal = rb.meshNode->EvaluateGlobalTransform(bindTime);
		if (m_currentAnimScene)
			rb.animMeshNode = m_currentAnimScene->FindNodeByName(rb.meshNode->GetName());
	}
}

// Get playback length (pick AnimStack by bone-motion score)
void Model3D::InitAnimationInfo() {
	m_animTimeSec = 0.0;
	m_animStartSec = 0.0;
	m_animDurationSec = 0.0;

	if (!m_currentAnimScene) return;

	FbxAnimStack* stack = PickBestAnimStack(m_currentAnimScene, m_baseScene,
		m_animStartSec, m_animDurationSec);
	if (!stack) return;

	m_currentAnimScene->SetCurrentAnimationStack(stack);
	// Anim stacks are scene-owned. Never attach another scene's stack to m_baseScene ?
	// boot preload EnsureClipBaked was doing that and corrupting FBX evaluator state.
	if (m_baseScene && m_baseScene != m_currentAnimScene) {
		if (FbxAnimStack* baseStack = m_baseScene->FindSrcObject<FbxAnimStack>(stack->GetName()))
			m_baseScene->SetCurrentAnimationStack(baseStack);
	}

	if (m_animDurationSec < 0.0) m_animDurationSec = 0.0;
}

namespace {

FbxAMatrix LerpFbxAMatrix(const FbxAMatrix& a, const FbxAMatrix& b, double t) {
	if (t <= 0.0) return a;
	if (t >= 1.0) return b;
	const double s = 1.0 - t;
	const FbxVector4 ta = a.GetT();
	const FbxVector4 tb = b.GetT();
	const FbxVector4 ra = a.GetR();
	const FbxVector4 rb = b.GetR();
	const FbxVector4 sa = a.GetS();
	const FbxVector4 sb = b.GetS();
	return FbxAMatrix(
		FbxVector4(ta[0] * s + tb[0] * t, ta[1] * s + tb[1] * t, ta[2] * s + tb[2] * t, 1.0),
		FbxVector4(ra[0] * s + rb[0] * t, ra[1] * s + rb[1] * t, ra[2] * s + rb[2] * t, 1.0),
		FbxVector4(sa[0] * s + sb[0] * t, sa[1] * s + sb[1] * t, sa[2] * s + sb[2] * t, 1.0));
}

} // namespace

void Model3D::EnsureClipBaked(const std::string& animName) {
	if (!m_hasSkin || m_skinBones.empty() || m_skinBake.empty())
		return;

	BakedAnimClip& clip = m_bakedClips[animName];
	if (clip.ready)
		return;

	FbxScene* scene = nullptr;
	if (animName.empty()) {
		scene = m_baseScene;
	} else {
		auto it = m_animScenes.find(animName);
		if (it != m_animScenes.end())
			scene = it->second;
	}
	if (!scene) {
		clip.ready = true;
		return;
	}

	FbxScene* prevScene = m_currentAnimScene;
	m_currentAnimScene = scene;
	InitAnimationInfo();

	clip.startSec = m_animStartSec;
	clip.durationSec = m_animDurationSec;
	clip.sampleDt = 1.0 / 60.0;

	int nSamples = 1;
	if (clip.durationSec > 0.0 && clip.sampleDt > 0.0)
		nSamples = (std::max)(1, (int)std::lround(clip.durationSec / clip.sampleDt) + 1);

	FbxAnimEvaluator* animEval = scene->GetAnimationEvaluator();
	FbxAnimEvaluator* baseEval = m_baseScene ? m_baseScene->GetAnimationEvaluator() : animEval;
	if (!animEval) {
		m_currentAnimScene = prevScene;
		clip.ready = true;
		return;
	}

	const size_t nBones = m_skinBones.size();
	std::vector<FbxNode*> animBones(nBones, nullptr);
	std::vector<FbxAnimEvaluator*> boneEvals(nBones, animEval);
	for (size_t i = 0; i < nBones; ++i) {
		FbxNode* baseBone = m_skinBones[i].baseBone;
		FbxNode* node = nullptr;
		if (baseBone)
			node = scene->FindNodeByName(baseBone->GetName());
		if (!node)
			node = baseBone;
		animBones[i] = node;
		if (node && m_baseScene && node->GetScene() == m_baseScene && baseEval)
			boneEvals[i] = baseEval;
	}

	std::vector<FbxNode*> meshNodes(m_skinBake.size(), nullptr);
	std::vector<FbxAnimEvaluator*> meshEvals(m_skinBake.size(), animEval);
	for (size_t mi = 0; mi < m_skinBake.size(); ++mi) {
		FbxNode* meshNode = m_skinBake[mi].meshNode;
		FbxNode* node = nullptr;
		if (meshNode)
			node = scene->FindNodeByName(meshNode->GetName());
		if (!node)
			node = meshNode;
		meshNodes[mi] = node;
		if (node && m_baseScene && node->GetScene() == m_baseScene && baseEval)
			meshEvals[mi] = baseEval;
	}

	clip.samples.resize((size_t)nSamples);
	for (int s = 0; s < nSamples; ++s) {
		double tSec = clip.startSec + (double)s * clip.sampleDt;
		const double endSec = clip.startSec + clip.durationSec;
		if (clip.durationSec > 0.0 && tSec > endSec)
			tSec = endSec;

		FbxTime t;
		t.SetSecondDouble(tSec);

		BakedAnimPose& pose = clip.samples[(size_t)s];
		pose.boneGlobals.resize(nBones);
		for (size_t i = 0; i < nBones; ++i) {
			FbxNode* node = animBones[i];
			FbxAnimEvaluator* eval = boneEvals[i];
			pose.boneGlobals[i] = (node && eval) ? eval->GetNodeGlobalTransform(node, t) : FbxAMatrix();
		}
		pose.meshGlobals.resize(meshNodes.size());
		for (size_t mi = 0; mi < meshNodes.size(); ++mi) {
			FbxNode* node = meshNodes[mi];
			FbxAnimEvaluator* eval = meshEvals[mi];
			pose.meshGlobals[mi] = (node && eval) ? eval->GetNodeGlobalTransform(node, t) : FbxAMatrix();
		}
	}

	clip.ready = true;
	m_currentAnimScene = prevScene;
}

bool Model3D::SampleBakedClip() {
	auto it = m_bakedClips.find(m_activeClip);
	if (it == m_bakedClips.end() || !it->second.ready || it->second.samples.empty())
		return false;

	const BakedAnimClip& clip = it->second;
	double t = m_animTimeSec;
	if (t < 0.0)
		t = 0.0;
	if (clip.durationSec > 0.0) {
		if (m_loop) {
			t = std::fmod(t, clip.durationSec);
			if (t < 0.0)
				t += clip.durationSec;
		} else if (t > clip.durationSec) {
			t = clip.durationSec;
		}
	}

	const double u = (clip.sampleDt > 0.0) ? (t / clip.sampleDt) : 0.0;
	const int n = (int)clip.samples.size();
	int i0 = (int)u;
	if (i0 < 0) i0 = 0;
	if (i0 >= n) i0 = n - 1;
	int i1 = i0 + 1;
	if (i1 >= n)
		i1 = m_loop ? 0 : i0;
	const double a = u - (double)((int)u);

	const BakedAnimPose& p0 = clip.samples[(size_t)i0];
	const BakedAnimPose& p1 = clip.samples[(size_t)i1];
	const size_t nb = p0.boneGlobals.size();
	m_boneXformScratch.resize(nb);
	for (size_t i = 0; i < nb; ++i) {
		const FbxAMatrix& b1 = (i < p1.boneGlobals.size()) ? p1.boneGlobals[i] : p0.boneGlobals[i];
		m_boneXformScratch[i] = LerpFbxAMatrix(p0.boneGlobals[i], b1, a);
	}
	const size_t nm = p0.meshGlobals.size();
	m_meshXformScratch.resize(nm);
	for (size_t i = 0; i < nm; ++i) {
		const FbxAMatrix& m1 = (i < p1.meshGlobals.size()) ? p1.meshGlobals[i] : p0.meshGlobals[i];
		m_meshXformScratch[i] = LerpFbxAMatrix(p0.meshGlobals[i], m1, a);
	}
	return true;
}

void Model3D::SkinFromScratch() {
	const bool hasSkinning = m_hasSkin && !m_skinBake.empty() && !m_skinPosBuf.empty();
	if (!hasSkinning)
		return;

	std::fill(m_skinPosBuf.begin(), m_skinPosBuf.end(), XMFLOAT3(0, 0, 0));
	std::fill(m_skinWeightScratch.begin(), m_skinWeightScratch.end(), 0.0);

	for (size_t mi = 0; mi < m_skinBake.size(); ++mi) {
		const SkinnedMeshBake& bake = m_skinBake[mi];
		if (!bake.mesh || bake.cpCount <= 0)
			continue;

		const FbxVector4* cp = bake.mesh->GetControlPoints();
		const FbxAMatrix meshGlobal = (mi < m_meshXformScratch.size())
			? m_meshXformScratch[mi]
			: FbxAMatrix();
		const FbxAMatrix toLocal = meshGlobal.Inverse();
		XMFLOAT3* outPos = m_skinPosBuf.data() + (size_t)bake.cpOffset;
		double* weightSum = m_skinWeightScratch.data() + (size_t)bake.cpOffset;

		for (const SkinClusterBake& cluster : bake.clusters) {
			if (cluster.boneIndex < 0 || cluster.indexCount <= 0 || !cluster.indices || !cluster.weights)
				continue;
			if ((size_t)cluster.boneIndex >= m_boneXformScratch.size())
				continue;

			const FbxAMatrix clusterM = m_boneXformScratch[(size_t)cluster.boneIndex] * cluster.bindOffset;
			for (int k = 0; k < cluster.indexCount; ++k) {
				const int cpi = cluster.indices[k];
				if (cpi < 0 || cpi >= bake.cpCount)
					continue;
				const double w = cluster.weights[k];
				if (w <= 0.0)
					continue;

				const FbxVector4 pLoc = toLocal.MultT(clusterM.MultT(cp[cpi]));
				outPos[cpi].x += (float)(pLoc[0] * w);
				outPos[cpi].y += (float)(pLoc[1] * w);
				outPos[cpi].z += (float)(pLoc[2] * w);
				weightSum[cpi] += w;
			}
		}

		for (int i = 0; i < bake.cpCount; ++i) {
			const double remain = 1.0 - weightSum[i];
			if (remain > 0.0) {
				outPos[i].x += (float)(cp[i][0] * remain);
				outPos[i].y += (float)(cp[i][1] * remain);
				outPos[i].z += (float)(cp[i][2] * remain);
			}
		}
	}

	updateDynamicVBFromCP(m_skinPosBuf);
}

void Model3D::AdvanceAnimationTime(double dtSec) {
	m_animTimeSec += dtSec * m_animSpeed;
	if (m_animDurationSec > 0.0) {
		if (m_loop) {
			while (m_animTimeSec >= m_animDurationSec) m_animTimeSec -= m_animDurationSec;
			while (m_animTimeSec < 0.0) m_animTimeSec += m_animDurationSec;
		} else if (m_animSpeed >= 0.0) {
			if (m_animTimeSec > m_animDurationSec) m_animTimeSec = m_animDurationSec;
			if (m_animTimeSec < 0.0) m_animTimeSec = 0.0;
		} else {
			if (m_animTimeSec < 0.0) m_animTimeSec = 0.0;
			if (m_animTimeSec > m_animDurationSec) m_animTimeSec = m_animDurationSec;
		}
	}
}

// Advance animation (CPU skin + update VB when skinned)
void Model3D::UpdateAnimation(double dtSec) {
	if (!m_baseScene && m_bakedClips.empty()) return;

	const bool hasSkinning = m_hasSkin && !m_skinBake.empty() && !m_skinPosBuf.empty();
	const bool hasRigidAnim = !m_rigidBake.empty();
	if (!hasSkinning && !hasRigidAnim) return;

	AdvanceAnimationTime(dtSec);

	if (hasSkinning && SampleBakedClip()) {
#if MODEL3D_CPU_SKIN_ORACLE
		SkinFromScratch();
#else
		if (m_gpuSkinEligible && !m_gpuBindConflict)
			UpdateGpuSkinPalette();
		else
			SkinFromScratch();
#endif
		return;
	}

	if (!m_baseScene || !m_currentAnimScene) return;

	const double evalTimeSec = m_animStartSec + m_animTimeSec;
	FbxTime t;
	t.SetSecondDouble(evalTimeSec);

	FbxAnimEvaluator* animEval = m_currentAnimScene->GetAnimationEvaluator();
	FbxAnimEvaluator* baseEval = m_baseScene ? m_baseScene->GetAnimationEvaluator() : animEval;

	if (hasSkinning) {
		m_boneXformScratch.resize(m_skinBones.size());
		for (size_t bi = 0; bi < m_skinBones.size(); ++bi) {
			const SkinBoneBake& bone = m_skinBones[bi];
			FbxNode* node = bone.animBone ? bone.animBone : bone.baseBone;
			FbxAnimEvaluator* eval = bone.animBone ? animEval : baseEval;
			m_boneXformScratch[bi] = (node && eval) ? eval->GetNodeGlobalTransform(node, t) : FbxAMatrix();
		}

		m_meshXformScratch.resize(m_skinBake.size());
		for (size_t mi = 0; mi < m_skinBake.size(); ++mi) {
			const SkinnedMeshBake& bake = m_skinBake[mi];
			FbxAnimEvaluator* meshEval = animEval;
			if (m_baseScene && m_currentAnimScene != m_baseScene)
				meshEval = baseEval;
			m_meshXformScratch[mi] = (bake.meshNode && meshEval)
				? meshEval->GetNodeGlobalTransform(bake.meshNode, t)
				: FbxAMatrix();
		}

#if MODEL3D_CPU_SKIN_ORACLE
		SkinFromScratch();
#else
		if (m_gpuSkinEligible && !m_gpuBindConflict)
			UpdateGpuSkinPalette();
		else
			SkinFromScratch();
#endif
	}

	if (hasRigidAnim) {
		for (RigidMeshBake& rb : m_rigidBake) {
			if (!rb.meshNode || rb.vertCount == 0) continue;

			FbxNode* node = rb.animMeshNode ? rb.animMeshNode : rb.meshNode;
			FbxAnimEvaluator* eval = (rb.animMeshNode && m_currentAnimScene != m_baseScene)
				? animEval : baseEval;
			if (!node || !eval) continue;

			const FbxAMatrix currentGlobal = eval->GetNodeGlobalTransform(node, t);
			const FbxAMatrix delta = currentGlobal * rb.bindGlobal.Inverse();
			const FbxAMatrix invTrans = delta.Inverse().Transpose();

			for (size_t i = 0; i < rb.vertCount; ++i) {
				const size_t vi = rb.vertStart + i;
				const FbxVector4 p(rb.restPos[i].x, rb.restPos[i].y, rb.restPos[i].z, 1.0);
				const FbxVector4 out = delta.MultT(p);
				m_verticesCPU[vi].Position = XMFLOAT3((float)out[0], (float)out[1], (float)out[2]);
				m_verticesCPU[vi].Normal = TransformNormal_IT(invTrans, rb.restNormal[i]);
			}
		}
		if (!hasSkinning)
			markVertexUploadDirty();
	}
}


bool Model3D::HasAnimation(const std::string& animName) const {
	return m_animScenes.find(animName) != m_animScenes.end();
}

void Model3D::MergeAnimScenesFrom(const Model3D& shared) {
	for (const auto& [name, scene] : shared.m_animScenes) {
		if (!scene || HasAnimation(name))
			continue;
		m_animScenes.emplace(name, scene);
	}
	for (const auto& [name, clip] : shared.m_bakedClips) {
		if (!clip.ready)
			continue;
		BakedAnimClip& dst = m_bakedClips[name];
		if (!dst.ready)
			dst = clip;
	}
}

void Model3D::EnsureAnimation(const std::string& animName, const char* fileName) {
	if (animName.empty() || !fileName || !fileName[0]) return;
	if (HasAnimation(animName)) {
		EnsureClipBaked(animName);
		return;
	}
	AddAnimation(animName, fileName);
	EnsureClipBaked(animName);
}

void Model3D::AddAnimation(const std::string& animName, const char* fileName)
{
	FbxShared& shared = GetFbxShared();

	// 1. Create Importer
	FbxImporter* importer = FbxImporter::Create(shared.mgr, "");
	const std::filesystem::path resolvedAnim = content_path::ReadableFileCurrent(fileName);
	std::string path_utf8 = resolvedAnim.empty() ? std::string(fileName) : resolvedAnim.string();

	if (!importer->Initialize(path_utf8.c_str(), -1, shared.mgr->GetIOSettings())) {
		std::string msg = "[Model3D] Failed to load animation '";
		msg += animName;
		msg += "' from '";
		msg += path_utf8;
		msg += "'\n";
		hal::DebugWriteA(msg.c_str());
		importer->Destroy();
		return; // Handle Error
	}

	// 2. Create a NEW scene for just this animation
	FbxScene* newScene = FbxScene::Create(shared.mgr, animName.c_str());
	importer->Import(newScene);
	importer->Destroy();

	// 3. Normalize to match Base Scene (DirectX, cm)
	FbxAxisSystem::DirectX.DeepConvertScene(newScene);
	FbxSystemUnit::cm.ConvertScene(newScene);

	// 4. Store it
	m_animScenes[animName] = newScene;
}

void Model3D::PlayAnimation(const std::string& animName) {
	if (animName.empty() || animName == "base") {
		m_activeClip = "";
		m_currentAnimScene = m_baseScene;
		EnsureClipBaked("");
		auto it = m_bakedClips.find("");
		if (it != m_bakedClips.end() && it->second.ready) {
			m_animStartSec = it->second.startSec;
			m_animDurationSec = it->second.durationSec;
			m_animTimeSec = 0.0;
			return;
		}
		InitAnimationInfo();
		BakeSkinningData();
		BakeRigidMeshData();
		return;
	}

	auto it = m_animScenes.find(animName);
	if (it == m_animScenes.end())
		return;

	m_activeClip = animName;
	m_currentAnimScene = it->second;
	m_animTimeSec = 0.0;
	EnsureClipBaked(animName);
	auto baked = m_bakedClips.find(animName);
	if (baked != m_bakedClips.end() && baked->second.ready) {
		m_animStartSec = baked->second.startSec;
		m_animDurationSec = baked->second.durationSec;
		return;
	}

	InitAnimationInfo();
	BakeSkinningData();
	BakeRigidMeshData();
}

// Apply CP array to expanded verts -> Map dynamic VB and write back
void Model3D::updateDynamicVBFromCP(const std::vector<XMFLOAT3>& cpPos) {
	if (!m_pVB || !m_vbDynamic) return;
	if (cpPos.empty() || m_cpIndexOfVertex.size() != m_verticesCPU.size()) return;

	const bool hasVertexGeo = m_vertexGeoIndex.size() == m_verticesCPU.size();
	for (size_t i = 0; i < m_verticesCPU.size(); ++i) {
		const int cpi = m_cpIndexOfVertex[i];
		if (cpi < 0 || (size_t)cpi >= cpPos.size()) continue;

		XMFLOAT3 pos = cpPos[(size_t)cpi];
		if (hasVertexGeo) {
			const int gi = m_vertexGeoIndex[i];
			if (gi >= 0 && (size_t)gi < m_skinMeshGeo.size()) {
				const SkinMeshGeo& geo = m_skinMeshGeo[(size_t)gi];
				if (geo.hasGeoOffset)
					pos = ApplyGeoPosition(geo.geo, pos);
			}
		}
		m_verticesCPU[i].Position = pos;
	}

	markVertexUploadDirty();
}

void Model3D::uploadVertexBufferFromCPU() {
	if (!m_pVB || m_verticesCPU.empty()) return;

	D3D11_MAPPED_SUBRESOURCE msr {};
	if (SUCCEEDED(GetDeviceContext()->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) {
		memcpy(msr.pData, m_verticesCPU.data(), m_verticesCPU.size() * sizeof(VertexPNTC));
		GetDeviceContext()->Unmap(m_pVB, 0);
	}
}

std::unique_ptr<Model3D> Model3D::CreateAnimInstanceFrom(const Model3D& shared) {
	if (shared.Released() || shared.m_isAnimInstance)
		return nullptr;

	auto inst = std::make_unique<Model3D>();
	inst->m_isAnimInstance = true;

	// Shared GPU resources (owned by cache; not released by instances).
	inst->m_pIB = shared.m_pIB;
	inst->m_pSRV = shared.m_pSRV;
	inst->m_subsets = shared.m_subsets;
	inst->m_indexCount = shared.m_indexCount;
	inst->m_hasTexture = shared.m_hasTexture;
	inst->m_diffuse = shared.m_diffuse;
	inst->m_hasLocalAabb = shared.m_hasLocalAabb;
	inst->m_localAabbMin = shared.m_localAabbMin;
	inst->m_localAabbMax = shared.m_localAabbMax;

	// Shared FBX / bake metadata (read-only after load).
	inst->m_baseScene = shared.m_baseScene;
	inst->m_animScenes = shared.m_animScenes;
	inst->m_currentAnimScene = shared.m_baseScene;
	inst->m_mesh = shared.m_mesh;
	inst->m_meshNode = shared.m_meshNode;
	inst->m_hasSkin = shared.m_hasSkin;
	inst->m_cpIndexOfVertex = shared.m_cpIndexOfVertex;
	inst->m_skinMeshGeo = shared.m_skinMeshGeo;
	inst->m_geoIndexPerCP = shared.m_geoIndexPerCP;
	inst->m_meshCpOffset = shared.m_meshCpOffset;
	inst->m_skinnedMeshCache = shared.m_skinnedMeshCache;
	inst->m_vertexGeoIndex = shared.m_vertexGeoIndex;
	inst->m_skinBake = shared.m_skinBake;
	inst->m_skinBones = shared.m_skinBones;
	inst->m_rigidBake = shared.m_rigidBake;
	inst->m_bakedClips = shared.m_bakedClips;
	inst->m_gpuSkinEligible = shared.m_gpuSkinEligible;
	inst->m_gpuBindConflict = shared.m_gpuBindConflict;
	inst->m_boneBindOffset = shared.m_boneBindOffset;
	inst->m_gpuPaletteWrite = shared.m_gpuPaletteWrite;
	inst->m_gpuPaletteFront = shared.m_gpuPaletteFront;
	inst->m_gpuPaletteDirty = false;
	inst->m_vbUploadDirty = false;
	if (!shared.m_shadowVolume)
		shared.m_shadowVolume = std::make_shared<ShadowVolumeData>();
	inst->m_shadowVolume = shared.m_shadowVolume;

	// Per-instance playback. GPU skin shares the cached bind-pose VB.
	inst->m_verticesCPU = shared.m_verticesCPU;
	const bool shareVB = inst->m_gpuSkinEligible && inst->m_rigidBake.empty();
	inst->m_sharesCachedVB = shareVB;
	if (shareVB) {
		inst->m_pVB = shared.m_pVB;
		inst->m_vbDynamic = false;
	} else if (!inst->m_verticesCPU.empty()) {
		const bool needsAnimatedVB = inst->m_hasSkin || !inst->m_rigidBake.empty();
		inst->m_vbDynamic = needsAnimatedVB ? true : shared.m_vbDynamic;
		D3D11_BUFFER_DESC bd {};
		bd.Usage = inst->m_vbDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bd.ByteWidth = static_cast<UINT>(inst->m_verticesCPU.size() * sizeof(VertexPNTC));
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = inst->m_vbDynamic ? D3D11_CPU_ACCESS_WRITE : 0;

		D3D11_SUBRESOURCE_DATA init {};
		init.pSysMem = inst->m_verticesCPU.data();
		if (FAILED(GetDevice()->CreateBuffer(&bd, &init, &inst->m_pVB)) || !inst->m_pVB)
			return nullptr;
	}

	inst->m_skinPosBuf = shared.m_skinPosBuf;
	inst->m_skinWeightScratch = shared.m_skinWeightScratch;
	inst->m_boneXformScratch.resize(shared.m_skinBones.size());

	// When clips are already baked (boot/editor preload), skip InitAnimationInfo ?
	// it would mutate the shared base FbxScene; PlayAnimation sets timing from bake.
	if (inst->m_bakedClips.empty())
		inst->InitAnimationInfo();
	if (inst->m_hasSkin && inst->m_bakedClips.empty())
		inst->BakeSkinningData();
	if (!inst->m_rigidBake.empty() && inst->m_bakedClips.empty())
		inst->BakeRigidMeshData();

	return inst;
}
