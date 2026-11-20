// terrain_collision.h
// Terrain collision queries (ADR 0011): raycast and contacts against the triangulated
// heightmap surface. Pure logic in terrain-local space (no D3D / physics types), so it
// is unit-testable on Linux; TerrainCollider (engine/physics/collider/collider.cpp)
// converts world <-> local and feeds the contacts to the regular contact solver.
//
// Every query only touches the cells under the shape (or along the ray), never the
// whole map, and uses the same triangle split as terrain::SampleHeight and the drawn
// mesh, so a body rests exactly on the visible surface.

#pragma once

#include "engine/terrain/terrain_heightmap.h"

#include <cstdint>

namespace terrain {

// Lowest / highest sample of the map (both 0 for an invalid map). Scans every sample:
// compute once per map (TerrainComponent caches it), not per query.
void SampleRange(const Heightmap& map, std::uint16_t& outLow, std::uint16_t& outHigh);

// Nearest hit of a ray with the surface. `dir` must be normalised; hits with t in
// [0, maxDistance] count. The normal is the hit triangle's normal, turned to face the
// ray. False for an invalid map, a zero direction or no hit.
bool Raycast(const Layout& layout, const Heightmap& map, const Float3& origin, const Float3& dir,
             float maxDistance, float& outT, Float3& outNormal);
// Same, with the map's SampleRange passed in (no full-map scan per ray).
bool Raycast(const Layout& layout, const Heightmap& map, std::uint16_t lowSample, std::uint16_t highSample,
             const Float3& origin, const Float3& dir, float maxDistance, float& outT, Float3& outNormal);

struct SurfaceContact {
    Float3 point;           // contact point on the surface (or the body point below it)
    Float3 normal{0, 1, 0}; // unit, from the surface towards the body
    float depth = 0.0f;     // penetration along normal (> 0 on contact)
};

// Deepest of the points that lies below the surface: depth is its distance to the plane
// of the triangle above it, along that triangle's normal. Points outside the terrain's
// XZ area are ignored. Used for boxes / cylinders (corner and rim points).
bool PointsContact(const Layout& layout, const Heightmap& map, const Float3* points, int count,
                   SurfaceContact& out);

// Sphere against the triangles under its footprint (closest point per triangle). A
// centre below the surface is pushed out along the surface normal at the centre.
bool SphereContact(const Layout& layout, const Heightmap& map, const Float3& center, float radius,
                   SurfaceContact& out);

// Capsule (segment a-b plus radius) as spheres along the segment, at most one radius
// apart; the deepest sphere contact wins.
bool CapsuleContact(const Layout& layout, const Heightmap& map, const Float3& a, const Float3& b,
                    float radius, SurfaceContact& out);

} // namespace terrain
