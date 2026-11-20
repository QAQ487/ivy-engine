/*****************************************************
 * File: engine/ecs/terrain_component.cpp
 * Description: TerrainComponent (ADR 0011). See terrain_component.h.
 *****************************************************/
#include "terrain_component.h"
#include "string_utils.h"
#include "engine/core/content_path.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/rendering/sprite/texture.h"
#include "libraries/nlohmann/json.hpp"

#include <d3d11.h>
#include <fstream>
#include <iterator>

TerrainComponent::~TerrainComponent() {
    ReleaseHeightSRV();
}

std::string TerrainComponent::Serialize() const {
    return terrain::SettingsToJson(settings).dump();
}

void TerrainComponent::Deserialize(const std::string& jsonStr) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(jsonStr);
    } catch (...) {
        hal::DebugWriteA("[TerrainComponent] Deserialize: JSON parse error.\n");
        return;
    }
    const std::string oldHeightmap = settings.heightmap;
    const std::string oldTexture = settings.texture;
    settings = terrain::SettingsFromJson(j);
    if (settings.heightmap != oldHeightmap)
        InvalidateHeightmap();
    if (settings.texture != oldTexture)
        InvalidateTexture();
}

void TerrainComponent::InvalidateHeightmap() {
    m_loadAttempted = false;
}

void TerrainComponent::InvalidateTexture() {
    m_textureAttempted = false;
    m_textureId = -1;
}

void TerrainComponent::FitChunkGrid() {
    const int quads = terrain::ResolveChunkQuads(settings.chunkQuads, m_heightmap.resolution);
    if (m_grid.chunkQuads == quads && m_bounds.size() == static_cast<size_t>(m_grid.ChunkCount()))
        return;
    m_grid = terrain::ChunkGrid{};
    m_bounds.clear();
    m_lodMemory.clear();
    if (terrain::MakeChunkGrid(m_heightmap.resolution, quads, m_grid))
        terrain::ComputeChunkBounds(m_heightmap, m_grid, m_bounds);
}

bool TerrainComponent::EnsureLoaded() {
    if (m_loadAttempted && m_loadedPath == settings.heightmap) {
        if (m_heightmap.IsValid())
            FitChunkGrid();
        return m_heightmap.IsValid() && m_grid.chunkQuads > 0;
    }

    m_loadAttempted = true;
    m_loadedPath = settings.heightmap;
    m_loadError.clear();
    terrain::Heightmap loaded;

    if (settings.heightmap.empty()) {
        terrain::CreateFlat(terrain::kPlaceholderResolution, 0, loaded);
    } else {
        std::ifstream file(content_path::ReadableFileCurrent(settings.heightmap), std::ios::binary);
        if (!file) {
            m_loadError = "cannot open " + settings.heightmap;
        } else {
            const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                                  std::istreambuf_iterator<char>());
            std::string error;
            if (!terrain::DecodeR16(bytes.data(), bytes.size(), loaded, &error))
                m_loadError = settings.heightmap + ": " + error;
        }
    }

    if (!m_loadError.empty()) {
        hal::DebugWriteA(("[Terrain] " + m_loadError + "\n").c_str());
        m_heightmap = terrain::Heightmap{};
        m_grid = terrain::ChunkGrid{};
        m_bounds.clear();
        m_lodMemory.clear();
        ReleaseHeightSRV();
        return false;
    }

    m_heightmap = std::move(loaded);
    m_grid = terrain::ChunkGrid{};
    ReleaseHeightSRV(); // renderer uploads the new map on the next draw
    FitChunkGrid();
    return m_grid.chunkQuads > 0;
}

ID3D11ShaderResourceView* TerrainComponent::GetTextureSRV() {
    if (m_textureAttempted && m_texturePathLoaded == settings.texture)
        return m_textureId >= 0 ? TextureGetSRV(m_textureId) : nullptr;
    m_textureAttempted = true;
    m_texturePathLoaded = settings.texture;
    m_textureId = -1;
    if (settings.texture.empty())
        return nullptr;
    const std::wstring wpath = ToWString(settings.texture);
    if (!wpath.empty())
        m_textureId = TextureLoad(wpath.c_str(), true);
    return m_textureId >= 0 ? TextureGetSRV(m_textureId) : nullptr;
}

void TerrainComponent::SetHeightSRV(ID3D11ShaderResourceView* srv) {
    if (srv == m_heightSrv)
        return;
    ReleaseHeightSRV();
    m_heightSrv = srv;
}

void TerrainComponent::ReleaseHeightSRV() {
    if (m_heightSrv) {
        m_heightSrv->Release();
        m_heightSrv = nullptr;
    }
}
