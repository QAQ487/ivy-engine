#!/usr/bin/env python3
"""Generate the mesh-LOD test assets and the walkable LOD testbed scene.

Everything here is original procedural content (no third-party assets):

  testbed/rom/model/lod/Boulder_LOD{0,1,2}.fbx   displaced icosphere rock
  testbed/rom/model/lod/PineTree_LOD{0,1,2}.fbx  stacked-cone pine, trunk + foliage
  testbed/rom/scene/scene_lod.json               third-person walkable field

The three levels of one object are the same shape (same noise / same tiers, same
origin at the base) and differ only in facet count, so a LOD switch is a quality
step rather than a jump. Meshes are ASCII FBX 7.4 (Y-up, unit scale 1, like the
engine primitives). Run from the repo root:

    python3 tools/gen_lod_test_assets.py

Player: the scene uses SamplePlayerScript (samples/scripts), which is compiled
into the Debug IvyEngineRuntime host only. See docs/TODO.md for how to run it.
"""
import json
import math
import random
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODEL_DIR = ROOT / "testbed/rom/model/lod"
SCENE_PATH = ROOT / "testbed/rom/scene/scene_lod.json"

Vec = tuple


# --- vector helpers ---------------------------------------------------------------
def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def norm(v):
    n = math.sqrt(dot(v, v))
    return (v[0] / n, v[1] / n, v[2] / n) if n > 1e-12 else (0.0, 1.0, 0.0)


def face_normal(pts):
    """Newell normal (works for triangles and planar-ish quads / n-gons)."""
    nx = ny = nz = 0.0
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        nx += (a[1] - b[1]) * (a[2] + b[2])
        ny += (a[2] - b[2]) * (a[0] + b[0])
        nz += (a[0] - b[0]) * (a[1] + b[1])
    return norm((nx, ny, nz))


# --- mesh container -----------------------------------------------------------------
class Mesh:
    def __init__(self):
        self.verts = []
        self.polys = []   # list of (indices, material_index)
        self.parts = []   # list of lists of polygon indices; each part is a closed solid

    def add_part(self, verts, polys, material):
        base = len(self.verts)
        self.verts += verts
        start = len(self.polys)
        for p in polys:
            self.polys.append(([base + i for i in p], material))
        self.parts.append(list(range(start, len(self.polys))))

    def orient_outward(self):
        """Flip each closed part whose signed volume is negative (inward winding)."""
        for part in self.parts:
            vol = 0.0
            for pi in part:
                idx = self.polys[pi][0]
                p0 = self.verts[idx[0]]
                for k in range(1, len(idx) - 1):
                    vol += dot(p0, cross(self.verts[idx[k]], self.verts[idx[k + 1]])) / 6.0
            if vol < 0.0:
                for pi in part:
                    idx, mat = self.polys[pi]
                    self.polys[pi] = (list(reversed(idx)), mat)

    def validate(self, name):
        """Closed, consistently wound manifold per part; positive volume."""
        edges = Counter()
        for idx, _mat in self.polys:
            for i in range(len(idx)):
                edges[(idx[i], idx[(i + 1) % len(idx)])] += 1
        bad = [e for e, c in edges.items() if c != 1 or (e[1], e[0]) not in edges]
        if bad:
            raise SystemExit("%s: %d non-manifold edges" % (name, len(bad)))
        for part in self.parts:
            vol = 0.0
            for pi in part:
                idx = self.polys[pi][0]
                p0 = self.verts[idx[0]]
                for k in range(1, len(idx) - 1):
                    vol += dot(p0, cross(self.verts[idx[k]], self.verts[idx[k + 1]])) / 6.0
            if vol <= 0.0:
                raise SystemExit("%s: part has non-positive volume (inward winding)" % name)

    def triangle_count(self):
        return sum(len(idx) - 2 for idx, _ in self.polys)


# --- value noise (deterministic, direction based so LODs share a silhouette) --------
def _hash01(ix, iy, iz, seed):
    h = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    h &= 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return h / 0xFFFFFFFF


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(x, y, z, seed):
    ix, iy, iz = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = _smooth(x - ix), _smooth(y - iy), _smooth(z - iz)

    def lerp(a, b, t):
        return a + (b - a) * t

    def corner(dx, dy, dz):
        return _hash01(ix + dx, iy + dy, iz + dz, seed)

    x00 = lerp(corner(0, 0, 0), corner(1, 0, 0), fx)
    x10 = lerp(corner(0, 1, 0), corner(1, 1, 0), fx)
    x01 = lerp(corner(0, 0, 1), corner(1, 0, 1), fx)
    x11 = lerp(corner(0, 1, 1), corner(1, 1, 1), fx)
    return lerp(lerp(x00, x10, fy), lerp(x01, x11, fy), fz)


def fbm(x, y, z, seed):
    total, amp, freq, norm_sum = 0.0, 1.0, 1.0, 0.0
    for octave in range(3):
        total += amp * value_noise(x * freq, y * freq, z * freq, seed + octave * 17)
        norm_sum += amp
        amp *= 0.5
        freq *= 2.1
    return total / norm_sum


# --- boulder --------------------------------------------------------------------------
def icosphere(subdiv):
    t = (1.0 + math.sqrt(5.0)) / 2.0
    verts = [norm(v) for v in [
        (-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0),
        (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
        (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]]
    tris = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
            (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
            (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
            (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    for _ in range(subdiv):
        cache = {}

        def midpoint(a, b):
            key = (min(a, b), max(a, b))
            if key not in cache:
                m = norm(tuple((verts[a][k] + verts[b][k]) * 0.5 for k in range(3)))
                verts.append(m)
                cache[key] = len(verts) - 1
            return cache[key]

        nxt = []
        for a, b, c in tris:
            ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
            nxt += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        tris = nxt
    return verts, [list(t) for t in tris]


BOULDER_SEED = 7
BOULDER_BOTTOM = -0.27


def boulder_mesh(subdiv):
    dirs, tris = icosphere(subdiv)
    verts = []
    for d in dirs:
        n = fbm(d[0] * 1.8 + 3.1, d[1] * 1.8 + 1.7, d[2] * 1.8 + 5.3, BOULDER_SEED)
        r = 0.5 * (0.72 + 0.55 * n)
        x, y, z = d[0] * r, d[1] * r * 0.75, d[2] * r
        y = max(y, BOULDER_BOTTOM) - BOULDER_BOTTOM  # flat base sitting on y = 0
        verts.append((x, y, z))
    mesh = Mesh()
    mesh.add_part(verts, tris, 0)
    mesh.orient_outward()
    return mesh


# --- pine tree ------------------------------------------------------------------------
def ring(radius, y, count, phase=0.0):
    return [(radius * math.cos(phase + 2.0 * math.pi * i / count), y,
             radius * math.sin(phase + 2.0 * math.pi * i / count)) for i in range(count)]


def solid_cone(base_y, height, radius, segments, wavy):
    """Closed cone: apex fan + base disk. wavy alternates the rim radius."""
    count = segments * 2 if wavy else segments
    rim = []
    for i in range(count):
        rr = radius * (0.78 if (wavy and i % 2) else 1.0)
        a = 2.0 * math.pi * i / count
        rim.append((rr * math.cos(a), base_y, rr * math.sin(a)))
    verts = rim + [(0.0, base_y + height, 0.0), (0.0, base_y, 0.0)]
    apex, centre = count, count + 1
    polys = [[apex, (i + 1) % count, i] for i in range(count)]
    polys += [[centre, i, (i + 1) % count] for i in range(count)]
    return verts, polys


def solid_cylinder(radius, height, segments):
    bottom = ring(radius, 0.0, segments)
    top = ring(radius, height, segments)
    verts = bottom + top + [(0.0, 0.0, 0.0), (0.0, height, 0.0)]
    cb, ct = 2 * segments, 2 * segments + 1
    polys = [[i, (i + 1) % segments, segments + (i + 1) % segments, segments + i]
             for i in range(segments)]
    polys += [[cb, (i + 1) % segments, i] for i in range(segments)]
    polys += [[ct, segments + i, segments + (i + 1) % segments] for i in range(segments)]
    return verts, polys


# (trunk segments, foliage tiers, foliage segments, wavy rim)
PINE_LEVELS = [(16, 5, 20, True), (8, 4, 10, False), (5, 3, 6, False)]


def pine_mesh(level):
    trunk_segs, tiers, fol_segs, wavy = PINE_LEVELS[level]
    mesh = Mesh()
    v, p = solid_cylinder(0.22, 1.5, trunk_segs)
    mesh.add_part(v, p, 0)
    # The silhouette (overall height and base radius) is the same at every level.
    for i in range(tiers):
        t = i / max(tiers - 1, 1)
        base_y = 1.0 + t * 3.0
        radius = 1.65 - t * 1.05
        height = 2.0 - t * 0.55
        v, p = solid_cone(base_y, height, radius, fol_segs, wavy)
        mesh.add_part(v, p, 1)
    mesh.orient_outward()
    return mesh


# --- FBX writer -------------------------------------------------------------------------
def fmt(values):
    return ",".join(("%.6g" % v) if isinstance(v, float) else str(v) for v in values)


def write_fbx(path, name, mesh, materials, smooth):
    """materials: list of (name, (r, g, b)). smooth: per-vertex normals, else flat."""
    mesh.validate(name)
    vflat = [c for v in mesh.verts for c in v]

    vertex_normals = None
    if smooth:
        acc = [(0.0, 0.0, 0.0)] * len(mesh.verts)
        for idx, _mat in mesh.polys:
            n = face_normal([mesh.verts[i] for i in idx])
            for i in idx:
                acc[i] = (acc[i][0] + n[0], acc[i][1] + n[1], acc[i][2] + n[2])
        vertex_normals = [norm(a) for a in acc]

    pidx, normals, uvs, poly_mats = [], [], [], []
    for idx, mat in mesh.polys:
        fn = None if smooth else face_normal([mesh.verts[i] for i in idx])
        poly_mats.append(mat)
        for k, i in enumerate(idx):
            pidx.append(~i if k == len(idx) - 1 else i)
            n = vertex_normals[i] if smooth else fn
            normals += [n[0], n[1], n[2]]
            x, y, z = mesh.verts[i]
            uvs += [0.5 + math.atan2(z, x) / (2.0 * math.pi), y * 0.5]
    uv_index = list(range(len(pidx)))

    mat_defs = "".join(
        f'\tMaterial: {100100 + m}, "Material::{mname}", "" {{\n'
        f'\t\tVersion: 102\n\t\tShadingModel: "phong"\n\t\tMultiLayer: 0\n'
        f'\t\tProperties70:  {{\n'
        f'\t\t\tP: "DiffuseColor", "Color", "", "A",{rgb[0]},{rgb[1]},{rgb[2]}\n'
        f'\t\t}}\n\t}}\n'
        for m, (mname, rgb) in enumerate(materials))
    mat_conns = "".join(f'\tC: "OO",{100100 + m},100002\n' for m in range(len(materials)))
    mapping = "AllSame" if len(materials) == 1 else "ByPolygon"
    mat_array = [0] if len(materials) == 1 else poly_mats

    text = f"""; FBX 7.4.0 project file
; Generated by tools/gen_lod_test_assets.py (procedural, original content)
FBXHeaderExtension:  {{
\tFBXHeaderVersion: 1003
\tFBXVersion: 7400
\tCreator: "Ivy Engine tools/gen_lod_test_assets.py"
}}
GlobalSettings:  {{
\tVersion: 1000
\tProperties70:  {{
\t\tP: "UpAxis", "int", "Integer", "",1
\t\tP: "UpAxisSign", "int", "Integer", "",1
\t\tP: "FrontAxis", "int", "Integer", "",2
\t\tP: "FrontAxisSign", "int", "Integer", "",1
\t\tP: "CoordAxis", "int", "Integer", "",0
\t\tP: "CoordAxisSign", "int", "Integer", "",1
\t\tP: "OriginalUpAxis", "int", "Integer", "",-1
\t\tP: "OriginalUpAxisSign", "int", "Integer", "",1
\t\tP: "UnitScaleFactor", "double", "Number", "",1
\t\tP: "OriginalUnitScaleFactor", "double", "Number", "",1
\t}}
}}
Documents:  {{
\tCount: 1
\tDocument: 1000000, "Scene", "Scene" {{
\t\tProperties70:  {{
\t\t\tP: "SourceObject", "object", "", ""
\t\t\tP: "ActiveAnimStackName", "KString", "", "", ""
\t\t}}
\t\tRootNode: 0
\t}}
}}
References:  {{
}}
Definitions:  {{
\tVersion: 100
\tCount: {3 + len(materials)}
\tObjectType: "GlobalSettings" {{
\t\tCount: 1
\t}}
\tObjectType: "Model" {{
\t\tCount: 1
\t}}
\tObjectType: "Geometry" {{
\t\tCount: 1
\t}}
\tObjectType: "Material" {{
\t\tCount: {len(materials)}
\t}}
}}
Objects:  {{
\tGeometry: 100001, "Geometry::{name}", "Mesh" {{
\t\tProperties70:  {{
\t\t}}
\t\tGeometryVersion: 124
\t\tVertices: *{len(vflat)} {{
\t\t\ta: {fmt(vflat)}
\t\t}}
\t\tPolygonVertexIndex: *{len(pidx)} {{
\t\t\ta: {fmt(pidx)}
\t\t}}
\t\tLayerElementNormal: 0 {{
\t\t\tVersion: 101
\t\t\tName: ""
\t\t\tMappingInformationType: "ByPolygonVertex"
\t\t\tReferenceInformationType: "Direct"
\t\t\tNormals: *{len(normals)} {{
\t\t\t\ta: {fmt(normals)}
\t\t\t}}
\t\t}}
\t\tLayerElementUV: 0 {{
\t\t\tVersion: 101
\t\t\tName: "UVMap"
\t\t\tMappingInformationType: "ByPolygonVertex"
\t\t\tReferenceInformationType: "IndexToDirect"
\t\t\tUV: *{len(uvs)} {{
\t\t\t\ta: {fmt(uvs)}
\t\t\t}}
\t\t\tUVIndex: *{len(uv_index)} {{
\t\t\t\ta: {fmt(uv_index)}
\t\t\t}}
\t\t}}
\t\tLayerElementMaterial: 0 {{
\t\t\tVersion: 101
\t\t\tName: ""
\t\t\tMappingInformationType: "{mapping}"
\t\t\tReferenceInformationType: "IndexToDirect"
\t\t\tMaterials: *{len(mat_array)} {{
\t\t\t\ta: {fmt(mat_array)}
\t\t\t}}
\t\t}}
\t\tLayer: 0 {{
\t\t\tVersion: 100
\t\t\tLayerElement:  {{
\t\t\t\tType: "LayerElementNormal"
\t\t\t\tTypedIndex: 0
\t\t\t}}
\t\t\tLayerElement:  {{
\t\t\t\tType: "LayerElementMaterial"
\t\t\t\tTypedIndex: 0
\t\t\t}}
\t\t\tLayerElement:  {{
\t\t\t\tType: "LayerElementUV"
\t\t\t\tTypedIndex: 0
\t\t\t}}
\t\t}}
\t}}
\tModel: 100002, "Model::{name}", "Mesh" {{
\t\tVersion: 232
\t\tProperties70:  {{
\t\t\tP: "InheritType", "enum", "", "",1
\t\t\tP: "DefaultAttributeIndex", "int", "Integer", "",0
\t\t}}
\t\tShading: T
\t\tCulling: "CullingOff"
\t}}
{mat_defs}}}
Connections:  {{
\tC: "OO",100002,0
\tC: "OO",100001,100002
{mat_conns}}}
"""
    path.write_text(text, encoding="utf-8", newline="\n")
    print("wrote %-40s %5d verts %5d tris" % (
        str(path.relative_to(ROOT)), len(mesh.verts), mesh.triangle_count()))


def generate_meshes():
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    rock = [("Rock", (0.52, 0.50, 0.48))]
    # (subdivision, smooth normals): LOD0 smooth, lower levels faceted.
    for level, (subdiv, smooth) in enumerate([(3, True), (2, False), (1, False)]):
        write_fbx(MODEL_DIR / ("Boulder_LOD%d.fbx" % level), "Boulder_LOD%d" % level,
                  boulder_mesh(subdiv), rock, smooth)
    pine = [("Bark", (0.36, 0.24, 0.14)), ("Needles", (0.13, 0.42, 0.17))]
    for level in range(3):
        write_fbx(MODEL_DIR / ("PineTree_LOD%d.fbx" % level), "PineTree_LOD%d" % level,
                  pine_mesh(level), pine, False)


# --- scene -------------------------------------------------------------------------------
def transform(x, y, z, scale=1.0, rot_y=0.0):
    return {"posX": round(x, 3), "posY": round(y, 3), "posZ": round(z, 3),
            "rotX": 0.0, "rotY": round(rot_y, 3), "rotZ": 0.0,
            "scaleX": scale, "scaleY": scale, "scaleZ": scale}


def prop(name, kind, x, z, scale, rot_y, near, far, cull):
    base = "rom/model/lod/%s_LOD%%d.fbx" % kind
    visual = {"model": base % 0, "shader": "standard",
              "lods": [{"model": base % 1, "maxDistance": near},
                       {"model": base % 2, "maxDistance": far}]}
    if cull:
        visual["cullDistance"] = cull
    return {"name": name, "tag": 0, "children": [],
            "components": {"SceneTransform": transform(x, 0.0, z, scale, rot_y), "visual": visual}}


def build_scene():
    rng = random.Random(20261118)
    nodes = [
        {"name": "Directional Light", "tag": 0, "children": [], "components": {
            "SceneTransform": {**transform(0.0, 10.0, 0.0), "rotX": 50.0, "rotY": -35.0},
            "DirectionalLight": {"enabled": True, "castShadows": True, "color": [1.0, 0.97, 0.92],
                                 "intensity": 1.1, "specColor": [1.0, 1.0, 1.0], "specPower": 32.0}}},
        {"name": "Ground", "tag": 2, "children": [], "components": {
            "Collider": {"colliderCenter": {"x": 0.0, "y": 0.0, "z": 0.0}, "colliderType": 1,
                         "halfExtents": {"x": 100.0, "y": 0.5, "z": 100.0}, "height": 1.0,
                         "meshSizePadding": 0.0, "radius": 0.5, "showDebugCollider": False},
            "SceneTransform": {**transform(0.0, -0.5, 0.0), "scaleX": 200.0, "scaleY": 1.0, "scaleZ": 200.0},
            "visual": {"model": "rom/model/object/Box_Green.fbx", "shader": "standard"}}},
        {"name": "Player", "tag": 5, "components": {
            "Collider": {"colliderCenter": {"x": 0.0, "y": 0.0, "z": 0.0}, "colliderType": 2,
                         "halfExtents": {"x": 0.5, "y": 0.5, "z": 0.5}, "height": 2.0,
                         "meshSizePadding": 0.0, "radius": 0.5, "showDebugCollider": False},
            "SceneTransform": transform(0.0, 1.0, 0.0),
            "Script": {"scriptClass": "SamplePlayerScript"}},
         "children": [{"name": "Player_Model", "tag": 5, "children": [], "components": {
             "SceneTransform": {**transform(0.0, -0.5, 0.0, 0.01), "rotY": 180.0},
             "visual": {"active_clip": "idle", "animate": True,
                        "animations": {"idle": "rom/model/player/Idle.fbx",
                                       "jump": "rom/model/player/Jump01.fbx",
                                       "run": "rom/model/player/Running.fbx"},
                        "model": "rom/model/player/Y Bot_white.fbx", "shader": "standard"}}}]},
    ]

    # Gallery in front of the spawn: the same object forced to LOD0 / LOD1 / LOD2
    # (thresholds 1e6/1e6 -> level 0, 0/1e6 -> level 1, 0/0 -> level 2) so the
    # three detail levels can be compared side by side without walking anywhere.
    forced = [("LOD0", 1.0e6, 1.0e6), ("LOD1", 0.0, 1.0e6), ("LOD2", 0.0, 0.0)]
    for i, (label, near, far) in enumerate(forced):
        x = (-6.0, 0.0, 6.0)[i]
        nodes.append(prop("Gallery Pine %s (forced)" % label, "PineTree", x, 11.0, 1.0, 0.0, near, far, 0))
        nodes.append(prop("Gallery Boulder %s (forced)" % label, "Boulder", x, 19.0, 2.5, 30.0, near, far, 0))

    # Natural scatter: levels switch with distance as the player walks.
    placed = [(0.0, 0.0), (-6.0, 11.0), (0.0, 11.0), (6.0, 11.0),
              (-6.0, 19.0), (0.0, 19.0), (6.0, 19.0)]

    def free(x, z, gap):
        return all((x - px) ** 2 + (z - pz) ** 2 >= gap * gap for px, pz in placed)

    def scatter(count, kind, gap, r_min, r_max, scale_range, near, far, cull, label):
        made = 0
        while made < count:
            a = rng.uniform(0.0, 2.0 * math.pi)
            r = math.sqrt(rng.uniform(r_min * r_min, r_max * r_max))
            x, z = r * math.cos(a), r * math.sin(a)
            if not free(x, z, gap):
                continue
            placed.append((x, z))
            made += 1
            nodes.append(prop("%s %02d" % (label, made), kind, x, z,
                              round(rng.uniform(*scale_range), 2), rng.uniform(0.0, 360.0),
                              near, far, cull))

    scatter(70, "PineTree", 5.0, 8.0, 92.0, (0.8, 1.5), 25.0, 60.0, 100.0, "Pine")
    scatter(28, "Boulder", 6.0, 8.0, 92.0, (1.5, 4.0), 20.0, 45.0, 80.0, "Boulder")

    return {"version": 4,
            "settings": {"render": {"clearColor": [0.69, 0.77, 0.87, 1.0], "defaultShader": "standard",
                                    "fogEnabled": False, "shadowQuality": "medium",
                                    "skyboxCubemap": "rom/skybox/MySkybox.dds", "skyboxEnabled": True,
                                    "lodEnabled": True, "lodBias": 1.0}},
            "nodes": nodes, "canvas": {}}


def main():
    generate_meshes()
    scene = build_scene()
    SCENE_PATH.write_text(json.dumps(scene, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("wrote %s: %d top-level nodes" % (SCENE_PATH.relative_to(ROOT), len(scene["nodes"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
