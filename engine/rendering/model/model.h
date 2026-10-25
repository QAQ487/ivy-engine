/**===========================================================================

 File   :  [model.h]

 Author : Terry Yeung
 Date   : 2025/10/21
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef MODEL_H
#define MODEL_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fbxsdk.h>
#include <map>
#include "engine/rendering/d3d11/shader.h"

namespace fbxsdk {
    class FbxScene;
    class FbxMesh;
    class FbxNode;
    class FbxTime;
}

struct AnimationClip {
    // This should store the keyframes/bone transforms 
    // extracted from your animation-only FBX files
    std::string name;
    // ... animation data members ...
};

class Model3D {
public:
    Model3D();
    ~Model3D();

    // Owns raw D3D/FBX resource pointers released in Release()/~Model3D();
    // a compiler-generated copy/move would shallow-copy them and cause
    // double-release or use-after-free. Use CreateAnimInstanceFrom() instead.
    Model3D(const Model3D&) = delete;
    Model3D& operator=(const Model3D&) = delete;
    Model3D(Model3D&&) = delete;
    Model3D& operator=(Model3D&&) = delete;

    struct VertexPNTC {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT4 Diffuse;
        DirectX::XMFLOAT2 TexCoord;
        DirectX::XMUINT4 BlendIndices = { 0, 0, 0, 0 };
        DirectX::XMFLOAT4 BlendWeights = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    bool LoadOBJ(const wchar_t* objPath, const wchar_t* fallbackTex = nullptr);
    bool LoadFBX(const wchar_t* fbxPath, const wchar_t* fallbackTex = nullptr);
    bool LoadFBXQuiet(const wchar_t* fbxPath);

    fbxsdk::FbxNode* FindBone(const char* name) const;
    fbxsdk::FbxNode* FindBone(const wchar_t* name) const;
    void ScaleBoneAndChildren(fbxsdk::FbxNode* bone, const fbxsdk::FbxDouble3& scale);
    // Animated bone matrix in model space at the current clip time.
    bool TryGetBoneLocalMatrix(const char* boneName, DirectX::XMMATRIX& out) const;

    void Release();
	bool Released() const { return m_pVB == nullptr && m_pIB == nullptr; }

	DirectX::XMFLOAT3 GetPosition() const { return m_position; }
	DirectX::XMFLOAT3 GetRotateEulerDeg() const { return m_rotateDeg; }
	DirectX::XMFLOAT3 GetScale() const { return m_scale; }
    // Getter for vertices (for collision mesh extraction)
    const std::vector<Model3D::VertexPNTC>& GetVerticesCPU() const { return m_verticesCPU; }


    void SetPosition(const DirectX::XMFLOAT3& p);
    void SetRotateEulerDeg(const DirectX::XMFLOAT3& rDeg);
    void SetScale(const DirectX::XMFLOAT3& s);

    // Per-draw emission override ? set to {0,0,0,0} for no glow, or a colour
    // to add a constant emissive term (shines regardless of lighting).
    void SetEmission(const DirectX::XMFLOAT4& emission) { m_emissionOverride = emission; }
    DirectX::XMFLOAT4 GetEmission() const { return m_emissionOverride; }

    // Non-owning per-draw albedo override (SurfaceFX detail map, etc.).
    void SetTextureOverride(ID3D11ShaderResourceView* srv) { m_pTextureOverride = srv; }
    ID3D11ShaderResourceView* GetTextureOverride() const { return m_pTextureOverride; }

    // Per-draw MaterialComponent params (non-owning normal/ramp SRV).
    struct MaterialOverride {
        bool enabled = false;
        DirectX::XMFLOAT3 color = {1.0f, 1.0f, 1.0f};
        float roughness = 0.5f;
        float metallic = 0.0f;
        float normalStrength = 1.0f;
        ID3D11ShaderResourceView* normalSRV = nullptr;
        // Toon
        float shadeSteps = 3.0f;
        float shadeSmoothness = 0.05f;
        float specularThreshold = 0.85f;
        float outlineWidth = 0.003f;
        DirectX::XMFLOAT4 outlineColor = {0.05f, 0.05f, 0.08f, 1.0f};
        DirectX::XMFLOAT3 shadowTint = {0.6f, 0.65f, 0.85f};
        float rimStrength = 0.25f;
        ID3D11ShaderResourceView* rampSRV = nullptr;
    };
    void SetMaterialOverride(const MaterialOverride& o) { m_materialOverride = o; }
    void ClearMaterialOverride() { m_materialOverride = MaterialOverride{}; }

    void SetShaderVariant(Shader3DVariant variant) { m_shaderVariant = variant; }
    Shader3DVariant GetShaderVariant() const { return m_shaderVariant; }

    void Draw();

    void UpdateAnimation(double dtSec = 1.0 / 60.0);
    // Main thread only: Map dirty CPU VBs + publish GPU palette front buffer.
    void UploadSkinnedVerticesIfDirty();
    void SetAnimationSpeed(double speed) { m_animSpeed = speed; }
    void SetLoop(bool loop) { m_loop = loop; }

    void AddAnimation(const std::string& animName, const char* fileName);
    void EnsureAnimation(const std::string& animName, const char* fileName);
    bool HasAnimation(const std::string& animName) const;
    void MergeAnimScenesFrom(const Model3D& shared);
    void PlayAnimation(const std::string& animName);
    const std::string& GetActiveClip() const { return m_activeClip; }
    double GetAnimDurationSec() const { return m_animDurationSec; }

    // Per-draw playback instance sharing mesh/clip data from a cached asset.
    static std::unique_ptr<Model3D> CreateAnimInstanceFrom(const Model3D& shared);

private:
    bool LoadFBXInternal(const wchar_t* fbxPath, bool reportErrors);
    bool m_isAnimInstance = false;
    bool m_sharesCachedVB = false;
    bool m_gpuSkinEligible = false;
    bool m_gpuBindConflict = false;
    ID3D11Buffer* m_pVB = nullptr;
    ID3D11Buffer* m_pIB = nullptr;
    UINT          m_indexCount = 0;
    ID3D11ShaderResourceView* m_pSRV = nullptr;

    DirectX::XMFLOAT3 m_position {0, 0, 0};
    DirectX::XMFLOAT3 m_rotateDeg {0, 0, 0};
    DirectX::XMFLOAT3 m_scale {1, 1, 1};

    DirectX::XMFLOAT4 m_diffuse {1.0f, 1.0f, 1.0f, 1.0f};

    // Emission colour added to every draw call.  {0,0,0,0} = no glow (default).
    DirectX::XMFLOAT4 m_emissionOverride {0.0f, 0.0f, 0.0f, 0.0f};

    // Non-owning; cleared by caller after Draw.
    ID3D11ShaderResourceView* m_pTextureOverride = nullptr;

    MaterialOverride m_materialOverride{};

    Shader3DVariant m_shaderVariant = Shader3DVariant::Standard;

    // OBJ �p: ���e�N�X�`����ǂݍ��߂����i��1x1�t�H�[���o�b�N�́u�Ȃ��v�����j
    bool m_hasTexture = false;

    // --------- FBX ------------
    fbxsdk::FbxScene* m_baseScene = nullptr;
    std::map<std::string, fbxsdk::FbxScene*> m_animScenes;
    fbxsdk::FbxScene* m_currentAnimScene = nullptr;
    fbxsdk::FbxMesh* m_mesh = nullptr;
    fbxsdk::FbxNode* m_meshNode = nullptr;

    std::vector<int> m_cpIndexOfVertex;

   
    std::vector<VertexPNTC> m_verticesCPU;
    bool m_vbDynamic = false;

    struct Subset {
        UINT startIndex = 0;
        UINT indexCount = 0;
        ID3D11ShaderResourceView* pSRV = nullptr;
        DirectX::XMFLOAT4 diffuse = {1, 1, 1, 1};
    };
    std::vector<Subset> m_subsets;

    bool   m_hasSkin = false;
    double m_animTimeSec = 0.0;
    double m_animStartSec = 0.0;
    double m_animDurationSec = 0.0;
    double m_animSpeed = 1.0;
    bool   m_loop = true;

    // Per-skinned-mesh geometric offset (matches FBX load-time vertex bake).
    struct SkinMeshGeo {
        int cpBase = 0;
        int cpCount = 0;
        fbxsdk::FbxAMatrix geo;
        fbxsdk::FbxAMatrix geoInvTrans;
        bool hasGeoOffset = false;
    };
    std::vector<SkinMeshGeo> m_skinMeshGeo;
    std::vector<int> m_geoIndexPerCP;
    std::unordered_map<fbxsdk::FbxMesh*, int> m_meshCpOffset;

    struct SkinClusterBake {
        int boneIndex = -1;
        fbxsdk::FbxAMatrix bindOffset;
        const int* indices = nullptr;
        int indexCount = 0;
        const double* weights = nullptr;
    };
    struct SkinnedMeshBake {
        fbxsdk::FbxMesh* mesh = nullptr;
        fbxsdk::FbxNode* meshNode = nullptr;
        int cpOffset = 0;
        int cpCount = 0;
        std::vector<SkinClusterBake> clusters;
    };
    struct SkinBoneBake {
        fbxsdk::FbxNode* baseBone = nullptr;
        fbxsdk::FbxNode* animBone = nullptr;
    };
    struct RigidMeshBake {
        fbxsdk::FbxNode* meshNode = nullptr;
        fbxsdk::FbxNode* animMeshNode = nullptr;
        fbxsdk::FbxAMatrix bindGlobal;
        size_t vertStart = 0;
        size_t vertCount = 0;
        std::vector<DirectX::XMFLOAT3> restPos;
        std::vector<DirectX::XMFLOAT3> restNormal;
    };

    // Reused each UpdateAnimation to avoid per-frame heap churn.
    std::vector<std::pair<fbxsdk::FbxMesh*, fbxsdk::FbxNode*>> m_skinnedMeshCache;
    std::vector<SkinnedMeshBake> m_skinBake;
    std::vector<SkinBoneBake> m_skinBones;
    std::vector<fbxsdk::FbxAMatrix> m_boneXformScratch;
    std::vector<DirectX::XMFLOAT3> m_skinPosBuf;
    std::vector<double> m_skinWeightScratch;
    std::vector<int> m_vertexGeoIndex;
    std::vector<RigidMeshBake> m_rigidBake;

    std::map<std::string, AnimationClip> m_animationLibrary;
    std::string m_activeClip;

    void  InitAnimationInfo();
    void  BakeSkinningData();
    void  BakeRigidMeshData();
    void  EnsureClipBaked(const std::string& animName);
    bool  SampleBakedClip();
    void  SkinFromScratch();
    void  BakeVertexBoneWeights();
    void  RecreateVertexBufferFromCPU();
    void  FillBindPoseScratch();
    void  UpdateGpuSkinPalette();
    void  PublishSkinPalette();
    bool  CanGpuSkin() const;
    void  updateDynamicVBFromCP(const std::vector<DirectX::XMFLOAT3>& cpPos);
    void  uploadVertexBufferFromCPU();
    void  markVertexUploadDirty();

    struct BakedAnimPose {
        std::vector<fbxsdk::FbxAMatrix> boneGlobals;
        std::vector<fbxsdk::FbxAMatrix> meshGlobals;
    };
    struct BakedAnimClip {
        double startSec = 0.0;
        double durationSec = 0.0;
        double sampleDt = 1.0 / 60.0;
        std::vector<BakedAnimPose> samples;
        bool ready = false;
    };
    std::map<std::string, BakedAnimClip> m_bakedClips;
    std::vector<fbxsdk::FbxAMatrix> m_meshXformScratch;
    std::vector<fbxsdk::FbxAMatrix> m_boneBindOffset;
    // Double-buffered bone palette: workers write back; Draw reads front
    // after VisualSystem_UploadSkinning publishes (post WaitAll).
    std::vector<DirectX::XMFLOAT4X4> m_gpuPaletteWrite;
    std::vector<DirectX::XMFLOAT4X4> m_gpuPaletteFront;
    bool m_gpuPaletteDirty = false;
    bool m_vbUploadDirty = false;

    const SkinMeshGeo* FindGeoForCP(int cpi) const;
    static DirectX::XMFLOAT3 ApplyGeoPosition(const fbxsdk::FbxAMatrix& geo, const DirectX::XMFLOAT3& p);
};

#endif // MODEL_H