#!/usr/bin/env python3
"""Generate low-poly UV-sphere LOD test meshes (ASCII FBX 7.4) for the testbed.

LOD0 is the engine's existing resources/model/primitives/Sphere.fbx (radius 0.5,
32 segments x 16 rings). This writes the lower levels with the same radius, axis
system and unit scale so only the facet count differs:

  testbed/rom/model/lod/Sphere_LOD1.fbx   16 x 8
  testbed/rom/model/lod/Sphere_LOD2.fbx    8 x 4

Winding is matched to Sphere.fbx (outward-facing test against the reference file),
so the engine's FBX import path treats these like the Blender-made primitives.

Usage (from repo root):  python3 tools/gen_lod_test_meshes.py
"""
import math
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REFERENCE = ROOT / "resources/model/primitives/Sphere.fbx"
OUT_DIR = ROOT / "testbed/rom/model/lod"
RADIUS = 0.5
LEVELS = [("Sphere_LOD1", 16, 8), ("Sphere_LOD2", 8, 4)]


# --- minimal binary FBX reader (only what the winding check needs) ----------------
def _read_reference_polygon_orientation(path):
    """Return +1 if the reference's first polygon winds outward by (p1-p0)x(p2-p0)."""
    data = path.read_bytes()

    def rec(pos):
        end, nprops, _plen, nl = struct.unpack_from("<IIIB", data, pos)
        if end == 0:
            return None, pos + 13
        pos += 13
        name = data[pos:pos + nl].decode()
        pos += nl
        props = []
        for _ in range(nprops):
            t = chr(data[pos])
            pos += 1
            if t in "YCFDIL":
                fmt = {"Y": "<h", "C": "<?", "F": "<f", "D": "<d", "I": "<i", "L": "<q"}[t]
                props.append(struct.unpack_from(fmt, data, pos)[0])
                pos += struct.calcsize(fmt)
            elif t in "fdilb":
                n, enc, clen = struct.unpack_from("<III", data, pos)
                pos += 12
                raw = data[pos:pos + clen]
                pos += clen
                if enc:
                    raw = zlib.decompress(raw)
                code = {"f": "f", "d": "d", "i": "i", "l": "q", "b": "?"}[t]
                props.append(struct.unpack("<%d%s" % (n, code), raw))
            elif t in "SR":
                n = struct.unpack_from("<I", data, pos)[0]
                pos += 4 + n
                props.append(None)
        kids = []
        while pos < end:
            kid, pos = rec(pos)
            if kid:
                kids.append(kid)
        return (name, props, kids), end

    pos = 27
    top = []
    while pos < len(data) - 200:
        node, pos = rec(pos)
        if not node:
            break
        top.append(node)
    objects = next(n for n in top if n[0] == "Objects")
    geom = next(n for n in objects[2] if n[0] == "Geometry")
    verts = next(k for k in geom[2] if k[0] == "Vertices")[1][0]
    idx = next(k for k in geom[2] if k[0] == "PolygonVertexIndex")[1][0]
    poly = []
    for i in idx:
        poly.append(i if i >= 0 else ~i)
        if i < 0:
            break
    pts = [verts[3 * i:3 * i + 3] for i in poly]
    p0, p1, p2 = pts[0], pts[1], pts[2]
    a = [p1[k] - p0[k] for k in range(3)]
    b = [p2[k] - p0[k] for k in range(3)]
    n = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
    c = [sum(p[k] for p in pts) / len(pts) for k in range(3)]
    dot = sum(n[k] * c[k] for k in range(3))
    return 1 if dot > 0 else -1


# --- mesh ------------------------------------------------------------------------
def build_sphere(segments, rings, flip):
    verts = [(0.0, RADIUS, 0.0)]
    for r in range(1, rings):
        phi = math.pi * r / rings
        for s in range(segments):
            theta = 2.0 * math.pi * s / segments
            verts.append((RADIUS * math.sin(phi) * math.cos(theta),
                          RADIUS * math.cos(phi),
                          RADIUS * math.sin(phi) * math.sin(theta)))
    verts.append((0.0, -RADIUS, 0.0))
    bottom = len(verts) - 1

    def ring_vert(r, s):
        return 1 + (r - 1) * segments + (s % segments)

    def uv_of(i):
        x, y, z = verts[i]
        return (0.5 + math.atan2(z, x) / (2.0 * math.pi), 0.5 + y / (2.0 * RADIUS))

    polys = []  # list of vertex-index lists, outward by (p1-p0)x(p2-p0) when not flipped
    for s in range(segments):
        polys.append([0, ring_vert(1, s + 1), ring_vert(1, s)])
    for r in range(1, rings - 1):
        for s in range(segments):
            polys.append([ring_vert(r, s), ring_vert(r, s + 1),
                          ring_vert(r + 1, s + 1), ring_vert(r + 1, s)])
    for s in range(segments):
        polys.append([bottom, ring_vert(rings - 1, s), ring_vert(rings - 1, s + 1)])

    def outward(poly):
        p = [verts[i] for i in poly]
        a = [p[1][k] - p[0][k] for k in range(3)]
        b = [p[2][k] - p[0][k] for k in range(3)]
        n = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
        c = [sum(q[k] for q in p) / len(p) for k in range(3)]
        return sum(n[k] * c[k] for k in range(3)) > 0

    # Normalise every polygon to "outward by the right-hand rule", then apply the
    # reference orientation (flip=True reverses all of them).
    fixed = []
    for poly in polys:
        if not outward(poly):
            poly = list(reversed(poly))
        fixed.append(list(reversed(poly)) if flip else poly)
    return verts, fixed, uv_of


def fmt(values):
    return ",".join(("%.9g" % v) if isinstance(v, float) else str(v) for v in values)


def write_fbx(path, name, segments, rings, flip):
    verts, polys, uv_of = build_sphere(segments, rings, flip)

    vflat = [c for v in verts for c in v]
    pidx, normals, uvs = [], [], []
    for poly in polys:
        for k, i in enumerate(poly):
            pidx.append(~i if k == len(poly) - 1 else i)
            x, y, z = verts[i]
            ln = math.sqrt(x * x + y * y + z * z) or 1.0
            normals += [x / ln, y / ln, z / ln]
            uvs += list(uv_of(i))
    uv_index = list(range(len(pidx)))

    text = f"""; FBX 7.4.0 project file
; Generated by tools/gen_lod_test_meshes.py ({segments} segments x {rings} rings)
FBXHeaderExtension:  {{
\tFBXHeaderVersion: 1003
\tFBXVersion: 7400
\tCreator: "Ivy Engine tools/gen_lod_test_meshes.py"
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
\tCount: 4
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
\t\tCount: 1
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
\t\t\tMappingInformationType: "AllSame"
\t\t\tReferenceInformationType: "IndexToDirect"
\t\t\tMaterials: *1 {{
\t\t\t\ta: 0
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
\tMaterial: 100003, "Material::{name}", "" {{
\t\tVersion: 102
\t\tShadingModel: "phong"
\t\tMultiLayer: 0
\t\tProperties70:  {{
\t\t\tP: "DiffuseColor", "Color", "", "A",0.8,0.8,0.8
\t\t}}
\t}}
}}
Connections:  {{
\tC: "OO",100002,0
\tC: "OO",100001,100002
\tC: "OO",100003,100002
}}
"""
    path.write_text(text, encoding="utf-8", newline="\n")
    print("wrote %s: %d verts, %d polygons" % (path.relative_to(ROOT), len(verts), len(polys)))


def main():
    flip = _read_reference_polygon_orientation(REFERENCE) < 0
    print("reference winding:", "reversed" if flip else "right-hand outward")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, segments, rings in LEVELS:
        write_fbx(OUT_DIR / (name + ".fbx"), name, segments, rings, flip)
    return 0


if __name__ == "__main__":
    sys.exit(main())
