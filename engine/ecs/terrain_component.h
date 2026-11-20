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
#include "engine/terrain/terrain_heightmap.h"
#include "engine/terrain/terrain_settings.h"

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
    const terrain::Heightmap& GetHeightmap() const { return m_heightmap; }
    const terrain::ChunkGrid& GetChunkGrid() const { return m_grid; }
    const std::vector<terrain::ChunkBounds>& GetChunkBounds() const { return m_bounds; }
    // Per-chunk LOD hysteresis memory (renderer only).
    std::vector<int>& LodMemory() { return m_lodMemory; }

    // Tiled albedo (settings.texture); null when empty or unloadable.
    ID3D11ShaderResourceView* GetTextureSRV();

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

    ID3D11ShaderResourceView* m_heightSrv = nullptr;

    std::string m_texturePathLoaded;
    int m_textureId = -1;
    bool m_textureAttempted = false;
};
