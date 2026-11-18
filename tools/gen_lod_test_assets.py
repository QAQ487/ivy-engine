#!/usr/bin/env python3
"""Generate the mesh-LOD test assets and the walkable LOD testbed scene.

Everything here is original procedural content (no third-party assets):

  testbed/rom/model/lod/Boulder_LOD{0..3}.fbx    displaced icosphere rock (textured)
  testbed/rom/model/lod/PineTree_LOD{0..3}.fbx   stacked-cone pine, trunk + foliage (textured)
  testbed/rom/model/lod/{rock_albedo,bark,needles}.png   procedural tileable textures
  testbed/rom/scene/scene_lod.json               third-person walkable field (needs SamplePlayerScript)
  testbed/rom/scene/scene_lod_fly.json           same field, free-fly debug camera, no scripts

The three levels of one object are the same shape (same noise / same tiers, same
origin at the base) and differ only in facet count, so a LOD switch is a quality
step rather than a jump. Meshes are ASCII FBX 7.4 (Y-up, unit scale 1, like the
engine primitives). Textures need numpy + Pillow. Run from the repo root:

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

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
MODEL_DIR = ROOT / "testbed/rom/model/lod"
SCENE_PATH = ROOT / "testbed/rom/scene/scene_lod.json"
FLY_SCENE_PATH = ROOT / "testbed/rom/scene/scene_lod_fly.json"

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



# --- procedural textures (tileable, deterministic) ---------------------------------------
def _periodic_noise(size, cells_y, cells_x, rng):
    """Value noise on a torus so the texture tiles without seams. Returns [0,1]."""
    grid = rng.random((cells_y, cells_x))
    ys = np.linspace(0.0, cells_y, size, endpoint=False)
    xs = np.linspace(0.0, cells_x, size, endpoint=False)
    y0, x0 = np.floor(ys).astype(int), np.floor(xs).astype(int)
    fy, fx = ys - y0, xs - x0
    fy, fx = fy * fy * (3.0 - 2.0 * fy), fx * fx * (3.0 - 2.0 * fx)
    y1, x1 = (y0 + 1) % cells_y, (x0 + 1) % cells_x
    y0, x0 = y0 % cells_y, x0 % cells_x
    g00 = grid[y0[:, None], x0[None, :]]
    g01 = grid[y0[:, None], x1[None, :]]
    g10 = grid[y1[:, None], x0[None, :]]
    g11 = grid[y1[:, None], x1[None, :]]
    fyc, fxr = fy[:, None], fx[None, :]
    return (g00 * (1 - fxr) + g01 * fxr) * (1 - fyc) + (g10 * (1 - fxr) + g11 * fxr) * fyc


def _fbm_tex(size, rng, cells, stretch=(1, 1)):
    total = np.zeros((size, size))
    weight = 0.0
    amp = 1.0
    for c in cells:
        total += amp * _periodic_noise(size, c * stretch[0], c * stretch[1], rng)
        weight += amp
        amp *= 0.55
    return total / weight


def _smoothstep(lo, hi, x):
    t = np.clip((x - lo) / (hi - lo), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _save_png(path, rgb):
    img = Image.fromarray((np.clip(rgb, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), "RGB")
    img.save(path, optimize=True)
    print("wrote %-40s %4dx%-4d %6d bytes" % (str(path.relative_to(ROOT)), img.width, img.height,
                                              path.stat().st_size))


def make_rock_texture(path, size=512):
    rng = np.random.default_rng(101)
    lumps = _fbm_tex(size, rng, [3, 6, 12, 24, 48])
    grain = _fbm_tex(size, rng, [16, 32, 64, 128])
    base = 0.60 + 0.34 * (lumps - 0.5) + 0.16 * (grain - 0.5)
    rgb = base[..., None] * np.array([1.0, 0.97, 0.92])
    # Thin dark cracks where a ridged noise peaks.
    ridge = 1.0 - np.abs(2.0 * _fbm_tex(size, rng, [4, 8, 16, 32]) - 1.0)
    crack = _smoothstep(0.90, 0.985, ridge)
    rgb *= (1.0 - 0.55 * crack)[..., None]
    # Mossy patches.
    moss = _smoothstep(0.60, 0.74, _fbm_tex(size, rng, [3, 6, 12, 24, 48]))
    moss_col = base[..., None] * np.array([0.62, 0.86, 0.42])
    rgb = rgb * (1.0 - 0.60 * moss[..., None]) + moss_col * (0.60 * moss[..., None])
    _save_png(path, rgb)


def make_bark_texture(path, size=256):
    rng = np.random.default_rng(202)
    streaks = _fbm_tex(size, rng, [2, 4, 8], stretch=(1, 8))      # tall vertical streaks
    fine = _fbm_tex(size, rng, [16, 32, 64], stretch=(1, 3))
    groove = _smoothstep(0.30, 0.55, streaks)
    base = 0.32 + 0.38 * groove * (0.55 + 0.45 * fine) - 0.05
    rgb = base[..., None] * np.array([1.0, 0.68, 0.42])
    _save_png(path, rgb)


def make_needles_texture(path, size=256):
    rng = np.random.default_rng(303)
    clump = _fbm_tex(size, rng, [4, 8, 16])
    speck = _fbm_tex(size, rng, [32, 64, 128])
    base = 0.30 + 0.34 * clump + 0.24 * (speck - 0.5)
    rgb = base[..., None] * np.array([0.62, 1.0, 0.55])
    tips = _smoothstep(0.66, 0.80, speck)
    rgb = rgb * (1.0 - 0.35 * tips[..., None]) + np.array([0.55, 0.78, 0.30]) * (0.35 * tips[..., None])
    _save_png(path, rgb)


# --- boulder ----------------------------------------------------------------------------
BOULDER_SEED = 7
BOULDER_BOTTOM = -0.27
# (icosphere subdivisions, smooth normals, high-frequency detail strength). The large lumps
# (silhouette) are identical on every level; only the finer relief fades out with distance.
BOULDER_LEVELS = [(4, True, 1.0), (3, False, 0.7), (2, False, 0.3), (1, False, 0.0)]


def boulder_mesh(subdiv, detail):
    dirs, tris = icosphere(subdiv)
    verts = []
    for d in dirs:
        lumps = fbm(d[0] * 1.8 + 3.1, d[1] * 1.8 + 1.7, d[2] * 1.8 + 5.3, BOULDER_SEED)
        ridge = 1.0 - abs(2.0 * fbm(d[0] * 4.3 + 9.2, d[1] * 4.3 + 2.4, d[2] * 4.3 + 6.8,
                                    BOULDER_SEED + 5) - 1.0)
        chip = fbm(d[0] * 9.5 + 1.3, d[1] * 9.5 + 7.7, d[2] * 9.5 + 3.9, BOULDER_SEED + 11)
        r = 0.5 * (0.68 + 0.52 * lumps + detail * (0.16 * ridge + 0.05 * chip))
        x, y, z = d[0] * r, d[1] * r * 0.75, d[2] * r
        y = max(y, BOULDER_BOTTOM) - BOULDER_BOTTOM  # flat base sitting on y = 0
        verts.append((x, y, z))
    mesh = Mesh()
    mesh.add_part(verts, tris, 0)
    mesh.orient_outward()
    return mesh


# --- pine tree --------------------------------------------------------------------------
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
PINE_LEVELS = [(20, 6, 28, True), (10, 5, 14, False), (6, 4, 8, False), (4, 3, 5, False)]
PINE_HEIGHT = 5.45   # overall height of every level (trunk top 1.0 + tiers)
PINE_CROWN = 3.3     # crown diameter (base tier radius 1.65): what dominates the screen coverage
ROCK_SIZE = 1.1      # typical boulder diameter at scale 1


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


# --- UV mapping -------------------------------------------------------------------------
def poly_uvs(mesh, idx, material):
    """Per-corner UVs for one polygon. 'box' = dominant-axis planar projection (no seams),
    'cyl' = cylindrical around Y, unwrapped per polygon so no triangle spans the seam."""
    pts = [mesh.verts[i] for i in idx]
    su, sv = material.get("uv_scale", (1.0, 1.0))
    if material["map"] == "box":
        n = face_normal(pts)
        axis = max(range(3), key=lambda k: abs(n[k]))
        if axis == 0:
            uv = [(p[2], p[1]) for p in pts]
        elif axis == 1:
            uv = [(p[0], p[2]) for p in pts]
        else:
            uv = [(p[0], p[1]) for p in pts]
        return [(u * su + 0.37 * axis, v * sv + 0.21 * axis) for u, v in uv]
    us = [math.atan2(p[2], p[0]) / (2.0 * math.pi) for p in pts]
    on_axis = [p[0] * p[0] + p[2] * p[2] < 1e-9 for p in pts]
    off = [i for i in range(len(pts)) if not on_axis[i]]
    ref = us[off[0]]
    for i in off:
        us[i] += round(ref - us[i])
    mean = sum(us[i] for i in off) / len(off)
    for i in range(len(pts)):
        if on_axis[i]:
            us[i] = mean
    return [(us[i] * su, pts[i][1] * sv) for i in range(len(pts))]


# --- FBX writer -------------------------------------------------------------------------
def fmt(values):
    return ",".join(("%.5g" % v) if isinstance(v, float) else str(v) for v in values)


def write_fbx(path, name, mesh, materials, smooth):
    """materials: list of dicts {name, rgb, map, uv_scale?, texture?}.
    smooth: per-vertex normals, else flat per-polygon normals."""
    mesh.validate(name)
    vflat = [c for v in mesh.verts for c in v]

    if smooth:
        acc = [(0.0, 0.0, 0.0)] * len(mesh.verts)
        for idx, _mat in mesh.polys:
            n = face_normal([mesh.verts[i] for i in idx])
            for i in idx:
                acc[i] = (acc[i][0] + n[0], acc[i][1] + n[1], acc[i][2] + n[2])
        normal_values = [norm(a) for a in acc]
    else:
        normal_values = [face_normal([mesh.verts[i] for i in idx]) for idx, _mat in mesh.polys]
    normals = [c for n in normal_values for c in n]

    pidx, normal_index, uv_index, poly_mats = [], [], [], []
    uv_table, uv_values = {}, []
    for pi, (idx, mat) in enumerate(mesh.polys):
        poly_mats.append(mat)
        for k, (i, uv) in enumerate(zip(idx, poly_uvs(mesh, idx, materials[mat]))):
            pidx.append(~i if k == len(idx) - 1 else i)
            normal_index.append(i if smooth else pi)
            key = (round(uv[0], 4), round(uv[1], 4))
            if key not in uv_table:
                uv_table[key] = len(uv_values) // 2
                uv_values += [key[0], key[1]]
            uv_index.append(uv_table[key])

    tex_ids = {}
    for m, mt in enumerate(materials):
        if mt.get("texture"):
            tex_ids[m] = 100200 + 2 * m
    mat_defs = ""
    for m, mt in enumerate(materials):
        r, g, b = mt["rgb"]
        mat_defs += (
            f'\tMaterial: {100100 + m}, "Material::{mt["name"]}", "" {{\n'
            f'\t\tVersion: 102\n\t\tShadingModel: "Phong"\n\t\tMultiLayer: 0\n'
            f'\t\tProperties70:  {{\n'
            f'\t\t\tP: "DiffuseColor", "Color", "", "A",{r},{g},{b}\n'
            f'\t\t\tP: "SpecularColor", "Color", "", "A",0.2,0.2,0.2\n'
            f'\t\t\tP: "SpecularFactor", "Number", "", "A",0.1\n'
            f'\t\t\tP: "Shininess", "Number", "", "A",8\n'
            f'\t\t}}\n\t}}\n')
    tex_defs, conns = "", ""
    for m, mt in enumerate(materials):
        conns += f'\tC: "OO",{100100 + m},100002\n'
        if m in tex_ids:
            tid, vid = tex_ids[m], tex_ids[m] + 1
            fname = mt["texture"]
            tex_defs += (
                f'\tVideo: {vid}, "Video::{fname}", "Clip" {{\n'
                f'\t\tType: "Clip"\n\t\tProperties70:  {{\n'
                f'\t\t\tP: "Path", "KString", "Url", "", "{fname}"\n\t\t}}\n'
                f'\t\tUseMipMap: 0\n\t\tFilename: "{fname}"\n\t\tRelativeFilename: "{fname}"\n\t}}\n'
                f'\tTexture: {tid}, "{fname}::Texture", "" {{\n'
                f'\t\tType: "TextureVideoClip"\n\t\tVersion: 202\n'
                f'\t\tTextureName: "{fname}::Texture"\n\t\tMedia: "{fname}::Video"\n'
                f'\t\tFileName: "{fname}"\n\t\tRelativeFilename: "{fname}"\n'
                f'\t\tProperties70:  {{\n\t\t\tP: "UseMaterial", "bool", "", "",1\n\t\t}}\n\t}}\n')
            conns += f'\tC: "OP",{tid},{100100 + m},"DiffuseColor"\n\tC: "OO",{vid},{tid}\n'
    n_tex = len(tex_ids)
    mapping = "AllSame" if len(materials) == 1 else "ByPolygon"
    mat_array = [0] if len(materials) == 1 else poly_mats
    normal_mapping = "ByPolygonVertex"

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
\tCount: {3 + len(materials) + 2 * n_tex}
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
""" + (f"""\tObjectType: "Texture" {{
\t\tCount: {n_tex}
\t}}
\tObjectType: "Video" {{
\t\tCount: {n_tex}
\t}}
""" if n_tex else "") + f"""}}
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
\t\t\tMappingInformationType: "{normal_mapping}"
\t\t\tReferenceInformationType: "IndexToDirect"
\t\t\tNormals: *{len(normals)} {{
\t\t\t\ta: {fmt(normals)}
\t\t\t}}
\t\t\tNormalsIndex: *{len(normal_index)} {{
\t\t\t\ta: {fmt(normal_index)}
\t\t\t}}
\t\t}}
\t\tLayerElementUV: 0 {{
\t\t\tVersion: 101
\t\t\tName: "UVMap"
\t\t\tMappingInformationType: "ByPolygonVertex"
\t\t\tReferenceInformationType: "IndexToDirect"
\t\t\tUV: *{len(uv_values)} {{
\t\t\t\ta: {fmt(uv_values)}
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
{mat_defs}{tex_defs}}}
Connections:  {{
\tC: "OO",100002,0
\tC: "OO",100001,100002
{conns}}}
"""
    path.write_text(text, encoding="utf-8", newline="\n")
    print("wrote %-40s %5d verts %5d tris %7d bytes" % (
        str(path.relative_to(ROOT)), len(mesh.verts), mesh.triangle_count(), path.stat().st_size))


ROCK_MATERIALS = [{"name": "Rock", "rgb": (1.0, 1.0, 1.0), "map": "box", "uv_scale": (1.3, 1.3),
                   "texture": "rock_albedo.png"}]
PINE_MATERIALS = [
    {"name": "Bark", "rgb": (1.0, 1.0, 1.0), "map": "cyl", "uv_scale": (2.0, 0.7),
     "texture": "bark.png"},
    {"name": "Needles", "rgb": (1.0, 1.0, 1.0), "map": "cyl", "uv_scale": (3.0, 0.5),
     "texture": "needles.png"},
]


def generate_meshes():
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    make_rock_texture(MODEL_DIR / "rock_albedo.png")
    make_bark_texture(MODEL_DIR / "bark.png")
    make_needles_texture(MODEL_DIR / "needles.png")
    for level, (subdiv, smooth, detail) in enumerate(BOULDER_LEVELS):
        write_fbx(MODEL_DIR / ("Boulder_LOD%d.fbx" % level), "Boulder_LOD%d" % level,
                  boulder_mesh(subdiv, detail), ROCK_MATERIALS, smooth)
    for level in range(len(PINE_LEVELS)):
        write_fbx(MODEL_DIR / ("PineTree_LOD%d.fbx" % level), "PineTree_LOD%d" % level,
                  pine_mesh(level), PINE_MATERIALS, False)


# --- scene -------------------------------------------------------------------------------
def transform(x, y, z, scale=1.0, rot_y=0.0):
    return {"posX": round(x, 3), "posY": round(y, 3), "posZ": round(z, 3),
            "rotX": 0.0, "rotY": round(rot_y, 3), "rotZ": 0.0,
            "scaleX": scale, "scaleY": scale, "scaleZ": scale}


# Distances are derived from the object's size so detail is held until the object is small
# on screen (vertical FOV 45 deg: coverage = size / (2 * d * tan(22.5 deg)), so
# d = size / (0.8284 * coverage)). LOD0 -> 1 at 12 % of the screen height (~120 px at 1080p),
# 1 -> 2 at 6 %, 2 -> 3 at 3 %; the object is dropped below 0.7 % (capped, the field is
# ~140 m across). Scale every distance with render.lodBias (< 1 keeps detail further out).
LOD_SWITCH_COVERAGE = (0.12, 0.06, 0.03)
LOD_CULL_COVERAGE = 0.007
LOD_CULL_CAP = 160.0
FOV_TAN = 0.8284271247461903   # 2 * tan(22.5 deg)


def lod_distances(size, count):
    return [round(size / (FOV_TAN * c), 1) for c in LOD_SWITCH_COVERAGE[:count]]


def prop(name, kind, x, z, scale, rot_y, size, forced=None):
    """size = world extent the screen coverage is measured on (tree crown, rock diameter).
    forced = explicit per-level switch distances (used by the side-by-side gallery)."""
    levels = len(PINE_LEVELS) if kind == "PineTree" else len(BOULDER_LEVELS)
    base = "rom/model/lod/%s_LOD%%d.fbx" % kind
    distances = forced if forced is not None else lod_distances(size, levels - 1)
    visual = {"model": base % 0, "shader": "standard",
              "lods": [{"model": base % (i + 1), "maxDistance": d} for i, d in enumerate(distances)]}
    if forced is None:
        visual["cullDistance"] = round(min(size / (FOV_TAN * LOD_CULL_COVERAGE), LOD_CULL_CAP), 1)
    return {"name": name, "tag": 0, "children": [],
            "components": {"SceneTransform": transform(x, 0.0, z, scale, rot_y), "visual": visual}}


def build_scene(fly):
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
    ]
    if not fly:
        nodes.append(
        {"name": "Player", "tag": 5, "components": {
            "Collider": {"colliderCenter": {"x": 0.0, "y": 0.0, "z": 0.0}, "colliderType": 2,
                         "halfExtents": {"x": 0.5, "y": 0.5, "z": 0.5}, "height": 2.0,
                         "meshSizePadding": 0.0, "radius": 0.5, "showDebugCollider": False},
            "SceneTransform": transform(0.0, 1.0, 0.0),
            "Script": {"scriptClass": "SamplePlayerScript"}},
         "children": [{"name": "Player_Model", "tag": 5, "children": [], "components": {
             # Y Bot natively faces +Z (toes at +Z in the FBX; the DirectX axis conversion only
             # mirrors X), which is heading 0 for SamplePlayerScript. No extra yaw here: an
             # additional 180 makes the bot face away from its movement direction.
             "SceneTransform": transform(0.0, -0.5, 0.0, 0.01),
             "visual": {"active_clip": "idle", "animate": True,
                        "animations": {"idle": "rom/model/player/Idle.fbx",
                                       "jump": "rom/model/player/Jump01.fbx",
                                       "run": "rom/model/player/Running.fbx"},
                        "model": "rom/model/player/Y Bot_white.fbx", "shader": "standard"}}}]})

    # Gallery in front of the spawn: the same object forced to each LOD level by thresholds
    # (1e6 keeps a level, 0 skips past it) so all detail levels can be compared side by side
    # without walking anywhere. Columns left to right: LOD0, LOD1, LOD2, LOD3.
    columns = (-9.0, -3.0, 3.0, 9.0)
    for level, x in enumerate(columns):
        forced = [0.0] * level + [1.0e6] * (len(columns) - 1 - level)
        nodes.append(prop("Gallery Pine LOD%d (forced)" % level, "PineTree", x, 11.0, 1.0, 0.0,
                          PINE_HEIGHT, forced))
        nodes.append(prop("Gallery Boulder LOD%d (forced)" % level, "Boulder", x, 19.0, 2.5, 30.0,
                          2.5, forced))

    # Natural scatter: levels switch with distance as the player walks.
    placed = [(0.0, 0.0)] + [(x, 11.0) for x in columns] + [(x, 19.0) for x in columns]

    def free(x, z, gap):
        return all((x - px) ** 2 + (z - pz) ** 2 >= gap * gap for px, pz in placed)

    def scatter(count, kind, gap, r_min, r_max, scale_range, unit_size, label):
        made = 0
        while made < count:
            a = rng.uniform(0.0, 2.0 * math.pi)
            r = math.sqrt(rng.uniform(r_min * r_min, r_max * r_max))
            x, z = r * math.cos(a), r * math.sin(a)
            if not free(x, z, gap):
                continue
            placed.append((x, z))
            made += 1
            scale = round(rng.uniform(*scale_range), 2)
            nodes.append(prop("%s %02d" % (label, made), kind, x, z, scale,
                              rng.uniform(0.0, 360.0), unit_size * scale))

    scatter(70, "PineTree", 5.0, 8.0, 92.0, (0.8, 1.5), PINE_CROWN, "Pine")
    scatter(28, "Boulder", 6.0, 8.0, 92.0, (1.5, 4.0), ROCK_SIZE, "Boulder")

    settings = {"render": {"clearColor": [0.69, 0.77, 0.87, 1.0], "defaultShader": "standard",
                           "fogEnabled": False, "shadowQuality": "medium",
                           "skyboxCubemap": "rom/skybox/MySkybox.dds", "skyboxEnabled": True,
                           "lodEnabled": True, "lodBias": 1.0}}
    if fly:
        # No scripts needed: the free-fly debug camera is the viewer (RMB + WASD, Shift boost).
        settings["debugCamera"] = {"mode": "fly", "enabledOnPlay": True, "yawDeg": 0.0,
                                   "pitchDeg": -8.0, "position": {"x": 0.0, "y": 3.0, "z": -8.0}}
    else:
        # Third-person follow camera; the target is set by SamplePlayerScript on Player.
        settings["camera"] = {"mode": "thirdPerson"}
    return {"version": 4, "settings": settings, "nodes": nodes, "canvas": {}}


def main():
    generate_meshes()
    for path, fly in ((SCENE_PATH, False), (FLY_SCENE_PATH, True)):
        scene = build_scene(fly)
        path.write_text(json.dumps(scene, indent=2) + "\n", encoding="utf-8", newline="\n")
        print("wrote %s: %d top-level nodes" % (path.relative_to(ROOT), len(scene["nodes"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
