/*****************************************************
 * File: engine/ecs/terrain_component.cpp
 * Description: TerrainComponent (ADR 0011). See terrain_component.h.
 *****************************************************/
#include "terrain_component.h"
#include "string_utils.h"
#include "game_object.h"
#include "collider_component_access.h"
#include "rigidbody_component.h"
#include "engine/physics/collider/collider.h"
#include "engine/terrain/terrain_collision.h"
#include "engine/core/content_path.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/rendering/sprite/texture.h"
#include "libraries/nlohmann/json.hpp"

#include <d3d11.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
// Writes bytes to a rom/ path in the content project through a temporary file.
bool WriteRomFile(const std::string& romPath, const std::vector<std::uint8_t>& bytes, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    if (romPath.empty())
        return fail("no file path");
    const std::filesystem::path target = content_path::WritableFileCurrent(romPath);
    if (target.empty())
        return fail("not a writable rom/ path: " + romPath);
    std::error_code ec;
    if (target.has_parent_path())
        std::filesystem::create_directories(target.parent_path(), ec);
    std::filesystem::path temp = target;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            return fail("cannot write " + romPath);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out)
            return fail("writing " + romPath + " failed");
    }
    std::filesystem::rename(temp, target, ec); // replaces an existing file
    if (ec) {
        std::filesystem::remove(temp, ec);
        return fail("cannot replace " + romPath);
    }
    return true;
}
} // namespace

TerrainComponent::~TerrainComponent() {
    ReleaseHeightSRV();
    if (m_splatSrv)
        m_splatSrv->Release();
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
    if (settings.splatmap != m_splatLoadedPath)
        InvalidateSplat();
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
        // Flat at terrain-local height 0 (the bottom when 0 is below the height range).
        terrain::CreateFlat(terrain::kPlaceholderResolution,
                            terrain::HeightToSample(terrain::SanitizeLayout(settings.layout), 0.0f), loaded);
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
        m_lowSample = m_highSample = 0;
        m_unsavedHeights = false;
        m_pendingGpuRect = terrain::SampleRect{};
        ReleaseHeightSRV();
        return false;
    }

    m_heightmap = std::move(loaded);
    terrain::SampleRange(m_heightmap, m_lowSample, m_highSample);
    m_grid = terrain::ChunkGrid{};
    m_unsavedHeights = false;
    m_pendingGpuRect = terrain::SampleRect{};
    ReleaseHeightSRV(); // renderer uploads the new map on the next draw
    FitChunkGrid();
    return m_grid.chunkQuads > 0;
}

void TerrainComponent::NotifyHeightsChanged(const terrain::SampleRect& rect) {
    const terrain::SampleRect r = terrain::ClipRect(rect, m_heightmap);
    if (r.Empty())
        return;
    FitChunkGrid();
    terrain::UpdateChunkBounds(m_heightmap, m_grid, r.x0, r.z0, r.x1, r.z1, m_bounds);
    // Exact range from the chunk bounds (they cover every sample).
    if (!m_bounds.empty()) {
        m_lowSample = terrain::kMaxSample;
        m_highSample = 0;
        for (const auto& b : m_bounds) {
            m_lowSample = std::min(m_lowSample, b.minSample);
            m_highSample = std::max(m_highSample, b.maxSample);
        }
    }
    m_pendingGpuRect = terrain::UnionRect(m_pendingGpuRect, r);
    m_unsavedHeights = true;
}

bool TerrainComponent::ResetFlat(int resolution, float height) {
    terrain::Heightmap flat;
    const terrain::Layout layout = terrain::SanitizeLayout(settings.layout);
    if (!terrain::CreateFlat(resolution, terrain::HeightToSample(layout, height), flat))
        return false;
    m_heightmap = std::move(flat);
    m_loadAttempted = true;
    m_loadedPath = settings.heightmap;
    m_loadError.clear();
    terrain::SampleRange(m_heightmap, m_lowSample, m_highSample);
    m_grid = terrain::ChunkGrid{};
    m_pendingGpuRect = terrain::SampleRect{};
    ReleaseHeightSRV(); // new size: the renderer creates a new texture
    FitChunkGrid();
    m_unsavedHeights = true;
    return true;
}

void TerrainComponent::AdoptHeightmapPath(const std::string& romPath) {
    settings.heightmap = romPath;
    m_loadedPath = romPath;
    m_loadAttempted = true;
}

bool TerrainComponent::SaveHeightmap(std::string* error, std::vector<std::uint8_t>* outBytes) {
    if (!m_heightmap.IsValid()) {
        if (error) *error = "no heightmap loaded";
        return false;
    }
    const std::vector<std::uint8_t> bytes = terrain::EncodeR16(m_heightmap);
    if (!WriteRomFile(settings.heightmap, bytes, error))
        return false;
    m_unsavedHeights = false;
    if (outBytes) *outBytes = bytes;
    return true;
}

bool TerrainComponent::EnsureSplatLoaded() {
    if (m_splatAttempted && m_splatLoadedPath == settings.splatmap)
        return m_splat.IsValid();
    m_splatAttempted = true;
    m_splatLoadedPath = settings.splatmap;
    m_splatError.clear();
    m_unsavedSplat = false;
    m_pendingSplatRect = terrain::SampleRect{};
    if (m_splatSrv) {
        m_splatSrv->Release();
        m_splatSrv = nullptr;
    }
    terrain::SplatMap loaded;
    if (settings.splatmap.empty()) {
        const int res = m_heightmap.IsValid() ? m_heightmap.resolution : terrain::kPlaceholderResolution;
        terrain::CreateSplat(res, 0, loaded);
    } else {
        std::ifstream file(content_path::ReadableFileCurrent(settings.splatmap), std::ios::binary);
        if (!file) {
            m_splatError = "cannot open " + settings.splatmap;
        } else {
            const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                                  std::istreambuf_iterator<char>());
            std::string error;
            if (!terrain::DecodeSplat(bytes.data(), bytes.size(), loaded, &error))
                m_splatError = settings.splatmap + ": " + error;
        }
    }
    if (!m_splatError.empty()) {
        hal::DebugWriteA(("[Terrain] " + m_splatError + "\n").c_str());
        m_splat = terrain::SplatMap{};
        return false;
    }
    m_splat = std::move(loaded);
    return true;
}

void TerrainComponent::NotifySplatChanged(const terrain::SampleRect& rect) {
    const terrain::SampleRect r = terrain::ClipSplatRect(rect, m_splat);
    if (r.Empty())
        return;
    m_pendingSplatRect = terrain::UnionRect(m_pendingSplatRect, r);
    m_unsavedSplat = true;
}

void TerrainComponent::AdoptSplatPath(const std::string& romPath) {
    settings.splatmap = romPath;
    m_splatLoadedPath = romPath;
    m_splatAttempted = true;
}

bool TerrainComponent::SaveSplat(std::string* error, std::vector<std::uint8_t>* outBytes) {
    if (!m_splat.IsValid()) {
        if (error) *error = "no splat map loaded";
        return false;
    }
    const std::vector<std::uint8_t> bytes = terrain::EncodeSplat(m_splat);
    if (!WriteRomFile(settings.splatmap, bytes, error))
        return false;
    m_unsavedSplat = false;
    if (outBytes) *outBytes = bytes;
    return true;
}

void TerrainComponent::SetSplatSRV(ID3D11ShaderResourceView* srv) {
    if (srv == m_splatSrv)
        return;
    if (m_splatSrv)
        m_splatSrv->Release();
    m_splatSrv = srv;
}

ID3D11ShaderResourceView* TerrainComponent::GetLayerSRV(int index) {
    if (index < 0 || index >= terrain::kMaxLayers)
        return nullptr;
    const std::vector<terrain::LayerSettings> layers = terrain::EffectiveLayers(settings);
    const std::string path = index < static_cast<int>(layers.size()) ? layers[index].texture : std::string();
    LayerTexture& slot = m_layerTextures[index];
    if (!slot.attempted || slot.path != path) {
        slot.attempted = true;
        slot.path = path;
        slot.id = -1;
        if (!path.empty()) {
            const std::wstring wpath = ToWString(path);
            if (!wpath.empty())
                slot.id = TextureLoad(wpath.c_str(), true);
        }
    }
    return slot.id >= 0 ? TextureGetSRV(slot.id) : nullptr;
}

void TerrainComponent::ClearTerrainCollider(GameObject* node) {
    if (!node || !node->m_collider || node->m_colliderOwnedByHost)
        return;
    if (node->m_collider->GetType() != ColliderType::TERRAIN)
        return;
    delete node->m_collider;
    node->m_collider = nullptr;
}

void TerrainComponent::ApplyColliderToOwner() {
    if (!owner)
        return;
    // A collider component on the same object wins (one 3D collider per GameObject).
    if (!settings.collision || FindColliderComponent(owner)) {
        ClearTerrainCollider(owner);
        return;
    }
    if (owner->m_collider && owner->m_collider->GetType() != ColliderType::TERRAIN) {
        hal::DebugWriteA("[Terrain] object already has a collider; terrain collision skipped\n");
        return;
    }
    EnsureLoaded();
    if (!owner->m_collider) {
        owner->m_collider = new TerrainCollider(owner);
        owner->m_colliderOwnedByHost = false;
    }
    // ResolveContact reads the terrain's rigidbody: it must be static, never pushed.
    Rigidbody& rb = owner->GetRigidbody();
    rb.SetIsKinematic(true);
    rb.SetUseGravity(false);
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
