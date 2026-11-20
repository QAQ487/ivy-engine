/*****************************************************
 * File: engine/ecs/terrain_component.h
 * Description: TerrainComponent - heightmap terrain on a GameObject (ADR 0011).
 *
 * Registered with ComponentFactory as "Terrain". JSON fields: see
 * engine/terrain/terrain_settings.h. The terrain sits at the GameObject's world
 * position; rotation and scale are ignored (size comes from sizeX / sizeZ /
 * heightScale). Drawn by engine/terrain/terrain_renderer.cpp from
 * GameObject::DrawVisual, in the camera pass and the CSM depth passes.
 *****************************************************/
#pragma once
#include "component.h"
#include "engine/terrain/terrain_brush.h"
#include "engine/terrain/terrain_heightmap.h"
#include "engine/terrain/terrain_settings.h"
#include "engine/terrain/terrain_splat.h"

#include <cstdint>
#include <string>
#include <vector>

struct ID3D11ShaderResourceView;

class TerrainComponent : public Component {
public:
    terrain::Settings settings;

    TerrainComponent() = default;
    ~TerrainComponent() override;
    TerrainComponent(const TerrainComponent&) = delete;
    TerrainComponent& operator=(const TerrainComponent&) = delete;

    const char* GetTypeName() const override { return "Terrain"; }

    std::string Serialize() const override;
    // Reloads the heightmap / texture on the next draw when their paths changed.
    void Deserialize(const std::string& jsonStr) override;

    // Loads the heightmap (or the flat placeholder when the path is empty) after a
    // path change and fits the chunk grid to settings.chunkQuads. False while no map
    // could be loaded (see GetLoadError); a failed path is not retried until it changes
    // or InvalidateHeightmap is called.
    bool EnsureLoaded();
    void InvalidateHeightmap();
    void InvalidateTexture();

    bool IsLoaded() const { return m_heightmap.IsValid(); }
    const std::string& GetLoadError() const { return m_loadError; }
    // Lowest / highest sample of the loaded map (cached at load; 0, 0 when not loaded).
    std::uint16_t GetLowSample() const { return m_lowSample; }
    std::uint16_t GetHighSample() const { return m_highSample; }
    const terrain::Heightmap& GetHeightmap() const { return m_heightmap; }
    const terrain::ChunkGrid& GetChunkGrid() const { return m_grid; }
    const std::vector<terrain::ChunkBounds>& GetChunkBounds() const { return m_bounds; }
    // Per-chunk LOD hysteresis memory (renderer only).
    std::vector<int>& LodMemory() { return m_lodMemory; }

    // Tiled albedo (settings.texture); null when empty or unloadable.
    ID3D11ShaderResourceView* GetTextureSRV();

    // Static heightfield collider (TerrainCollider) on the owner when settings.collision
    // is on and the owner has no collider component of its own; loads the heightmap
    // first. Makes the owner's rigidbody kinematic without gravity (terrain never moves).
    // Called by SceneRuntime when the scene registers its colliders (main thread).
    void ApplyColliderToOwner();
    // Deletes the owner's collider if it is a terrain collider this engine allocated.
    static void ClearTerrainCollider(GameObject* owner);

    // --- Editing (editor sculpt tools, main thread) -------------------------------
    // Mutable map for brushes; call NotifyHeightsChanged with the touched rectangle.
    terrain::Heightmap& EditHeightmap() { return m_heightmap; }
    // Updates chunk bounds and the cached height range, queues the rectangle for the
    // GPU copy and marks the map unsaved.
    void NotifyHeightsChanged(const terrain::SampleRect& rect);
    // Replaces the map with a flat one of `resolution` samples at terrain-local
    // `height` (clamped to the height range). Marks it unsaved. Not undoable.
    bool ResetFlat(int resolution, float height);
    // Edits not yet written to settings.heightmap.
    bool HasUnsavedHeights() const { return m_unsavedHeights; }
    // Uses `romPath` as the heightmap file for the map already in memory (no reload).
    void AdoptHeightmapPath(const std::string& romPath);
    // Writes the map to settings.heightmap (a rom/ path, written into the content
    // project) through a temporary file, and clears the unsaved flag. outBytes, when
    // given, receives the written file.
    bool SaveHeightmap(std::string* error, std::vector<std::uint8_t>* outBytes = nullptr);
    // --- Paint layers (step 5) ------------------------------------------------------
    // Loads settings.splatmap, or an all-layer-0 map at the heightmap's resolution when
    // it is empty. False while no splat map could be loaded (GetSplatError).
    bool EnsureSplatLoaded();
    const terrain::SplatMap& GetSplat() const { return m_splat; }
    terrain::SplatMap& EditSplat() { return m_splat; }
    const std::string& GetSplatError() const { return m_splatError; }
    void NotifySplatChanged(const terrain::SampleRect& rect);
    bool HasUnsavedSplat() const { return m_unsavedSplat; }
    void AdoptSplatPath(const std::string& romPath);
    bool SaveSplat(std::string* error, std::vector<std::uint8_t>* outBytes = nullptr);
    void InvalidateSplat() { m_splatAttempted = false; }
    const terrain::SampleRect& PendingSplatRect() const { return m_pendingSplatRect; }
    void ClearPendingSplatRect() { m_pendingSplatRect = terrain::SampleRect{}; }
    ID3D11ShaderResourceView* GetSplatSRV() const { return m_splatSrv; }
    void SetSplatSRV(ID3D11ShaderResourceView* srv); // takes ownership
    // Texture of effective layer `index` (null when empty / unloadable).
    ID3D11ShaderResourceView* GetLayerSRV(int index);

    // Samples changed since the renderer last uploaded them (renderer only).
    const terrain::SampleRect& PendingGpuRect() const { return m_pendingGpuRect; }
    void ClearPendingGpuRect() { m_pendingGpuRect = terrain::SampleRect{}; }

    // Heightmap GPU copy, owned here and created by the renderer (null = rebuild).
    ID3D11ShaderResourceView* GetHeightSRV() const { return m_heightSrv; }
    void SetHeightSRV(ID3D11ShaderResourceView* srv); // takes ownership, releases the old one

private:
    void ReleaseHeightSRV();
    void FitChunkGrid();

    terrain::Heightmap m_heightmap;
    terrain::ChunkGrid m_grid;
    std::vector<terrain::ChunkBounds> m_bounds;
    std::vector<int> m_lodMemory;
    std::string m_loadedPath;  // path the current map (or failure) belongs to
    bool m_loadAttempted = false;
    std::string m_loadError;
    std::uint16_t m_lowSample = 0;
    std::uint16_t m_highSample = 0;
    bool m_unsavedHeights = false;
    terrain::SampleRect m_pendingGpuRect;

    ID3D11ShaderResourceView* m_heightSrv = nullptr;

    terrain::SplatMap m_splat;
    std::string m_splatLoadedPath;
    bool m_splatAttempted = false;
    std::string m_splatError;
    bool m_unsavedSplat = false;
    terrain::SampleRect m_pendingSplatRect;
    ID3D11ShaderResourceView* m_splatSrv = nullptr;
    struct LayerTexture {
        std::string path;
        int id = -1;
        bool attempted = false;
    };
    LayerTexture m_layerTextures[terrain::kMaxLayers];

    std::string m_texturePathLoaded;
    int m_textureId = -1;
    bool m_textureAttempted = false;
};
