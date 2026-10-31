// fbx_stats.cpp
// CLI tool that prints FBX mesh/material statistics.

#include <fbxsdk.h>
#include <cstdio>
#include <functional>
#include <string>

using namespace fbxsdk;

static void Walk(FbxNode* node, int depth) {
    if (!node) return;
    if (FbxMesh* mesh = node->GetMesh()) {
        const int skins = mesh->GetDeformerCount(FbxDeformer::eSkin);
        int clusters = 0;
        for (int s = 0; s < skins; ++s) {
            if (auto* skin = static_cast<FbxSkin*>(mesh->GetDeformer(s, FbxDeformer::eSkin)))
                clusters += skin->GetClusterCount();
        }
        std::printf("  %*smesh '%s'  polys=%d  cp=%d  skins=%d  clusters=%d\n",
            depth * 2, "", node->GetName(),
            mesh->GetPolygonCount(), mesh->GetControlPointsCount(), skins, clusters);
    }
    for (int i = 0; i < node->GetChildCount(); ++i)
        Walk(node->GetChild(i), depth + 1);
}

static int Dump(const char* path) {
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);
    FbxImporter* imp = FbxImporter::Create(mgr, "");
    if (!imp->Initialize(path, -1, ios)) {
        std::printf("FAIL import init: %s\n  %s\n", path, imp->GetStatus().GetErrorString());
        mgr->Destroy();
        return 1;
    }
    FbxScene* scene = FbxScene::Create(mgr, "s");
    if (!imp->Import(scene)) {
        std::printf("FAIL import: %s\n", path);
        mgr->Destroy();
        return 1;
    }
    imp->Destroy();

    int skinned = 0;
    int totalPolys = 0;
    int totalCp = 0;
    std::function<void(FbxNode*)> count = [&](FbxNode* n) {
        if (!n) return;
        if (FbxMesh* m = n->GetMesh()) {
            totalPolys += m->GetPolygonCount();
            totalCp += m->GetControlPointsCount();
            if (m->GetDeformerCount(FbxDeformer::eSkin) > 0)
                ++skinned;
        }
        for (int i = 0; i < n->GetChildCount(); ++i)
            count(n->GetChild(i));
    };
    count(scene->GetRootNode());

    std::printf("FILE %s\n  skinnedMeshes=%d  totalPolys=%d  totalCP=%d  gpuSkinWouldNeed1Mesh=%s\n",
        path, skinned, totalPolys, totalCp, skinned == 1 ? "YES" : "NO");
    Walk(scene->GetRootNode(), 0);
    mgr->Destroy();
    return 0;
}

static void TestMatrixConvention() {
    FbxAMatrix F;
    F.SetT(FbxVector4(3.0, -4.0, 5.0, 1.0));
    F.SetR(FbxVector4(20.0, -35.0, 50.0, 0.0));
    F.SetS(FbxVector4(1.5, 0.7, 2.2, 1.0));
    const FbxVector4 pin(1.25, -2.5, 3.75, 1.0);
    const FbxVector4 pout = F.MultT(pin);

    double m[4][4];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            m[r][c] = F.Get(r, c);

    // Row-vector * FbxToXM (what XMVector3Transform does with XMMatrixSet(Get(r,c)))
    double row[4] = { pin[0], pin[1], pin[2], pin[3] };
    double rowOut[4] = {};
    for (int k = 0; k < 4; ++k)
        for (int i = 0; i < 4; ++i)
            rowOut[k] += row[i] * m[i][k];

    // Column Fbx * v (MultT)
    double colOut[4] = {};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            colOut[r] += m[r][c] * pin[c];

    std::printf("CONVENTION MultT=(%.6f,%.6f,%.6f) row*Fbx=(%.6f,%.6f,%.6f) col=Fbx*v=(%.6f,%.6f,%.6f)\n",
        pout[0], pout[1], pout[2],
        rowOut[0], rowOut[1], rowOut[2],
        colOut[0], colOut[1], colOut[2]);

    FbxAMatrix A, B;
    A.SetT(FbxVector4(1, 2, 3, 1));
    B.SetR(FbxVector4(10, 20, 30, 0));
    const FbxAMatrix AB = A * B;
    const FbxVector4 chain = A.MultT(B.MultT(pin));
    const FbxVector4 prod = AB.MultT(pin);
    std::printf("COMPOSE A.MultT(B.MultT)= (%.6f,%.6f,%.6f) (A*B).MultT=(%.6f,%.6f,%.6f)\n",
        chain[0], chain[1], chain[2], prod[0], prod[1], prod[2]);
}

int main(int argc, char** argv) {
    TestMatrixConvention();
    if (argc < 2) {
        std::printf("usage: fbx_stats <file.fbx> [...]\n");
        return 2;
    }
    int rc = 0;
    for (int i = 1; i < argc; ++i)
        rc |= Dump(argv[i]);
    return rc;
}
