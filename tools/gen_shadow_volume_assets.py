#!/usr/bin/env python3
"""Generate the stencil shadow-volume demo: uneven terrain, closed casters and the scene.

Everything here is original procedural content (no third-party assets):

  testbed/rom/model/shadow_volume/Terrain.fbx   closed heightfield slab (hills, receiver only)
  testbed/rom/model/shadow_volume/Torus.fbx     ring (genus 1: tests a volume with a hole)
  testbed/rom/model/shadow_volume/Obelisk.fbx   tapered pillar with a pyramid cap
  testbed/rom/scene/scene_shadow_volume.json    render.shadowMode = "volume", free-fly camera

The scene also reuses the closed LOD meshes from tools/gen_lod_test_assets.py
(Boulder_LOD1, PineTree_LOD1). Every caster sits on the terrain at the height of the
same height function, so the shadows fall across slopes rather than a flat plane.
One box keeps "shadowVolume": false to show that casting is per Visual.

The torus and the large boulder carry SampleSpinScript (samples/scripts), compiled into
the Debug editor and Debug runtime host, so in Play they turn and their shadows update
every frame (a Release build without a GameScripts DLL leaves them still).

Meshes are ASCII FBX 7.4 written by gen_lod_test_assets.write_fbx, which refuses
non-manifold or inward-wound meshes, so every caster is closed (what the engine's
shadow-volume builder requires). Needs numpy + Pillow (imported by that module).
Run from the repo root:

    python3 tools/gen_shadow_volume_assets.py
"""
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_lod_test_assets as lod  # noqa: E402  (write_fbx, Mesh, validation)

ROOT = lod.ROOT
MODEL_DIR = ROOT / "testbed/rom/model/shadow_volume"
SCENE_PATH = ROOT / "testbed/rom/scene/scene_shadow_volume.json"
MODEL_ROM = "rom/model/shadow_volume"

TERRAIN_SIZE = 60.0     # metres, square, centred on the origin
TERRAIN_CELLS = 48      # quads per side on the top surface
TERRAIN_BASE = -4.0     # flat bottom of the closed slab


def terrain_height(x, z):
    """Rolling hills, roughly -2.4 .. +2.4 m. Shared by the mesh and the prop placement."""
    return (1.4 * math.sin(x * 0.17 + 0.6) * math.cos(z * 0.14 - 0.3)
            + 0.7 * math.sin((x + z) * 0.31)
            + 0.35 * math.cos(z * 0.47 - x * 0.22))


def terrain_mesh():
    n = TERRAIN_CELLS
    step = TERRAIN_SIZE / n
    half = TERRAIN_SIZE * 0.5
    top, bottom = [], []
    for j in range(n + 1):
        for i in range(n + 1):
            x, z = -half + i * step, -half + j * step
            top.append((x, terrain_height(x, z), z))
            bottom.append((x, TERRAIN_BASE, z))
    verts = top + bottom
    nb = (n + 1) * (n + 1)

    def t(i, j):
        return j * (n + 1) + i

    def b(i, j):
        return nb + t(i, j)

    polys = []
    for j in range(n):
        for i in range(n):
            # Two triangles per cell (the top is not planar); bottom quads face down.
            polys.append([t(i, j), t(i, j + 1), t(i + 1, j + 1)])
            polys.append([t(i, j), t(i + 1, j + 1), t(i + 1, j)])
            polys.append([b(i, j), b(i + 1, j), b(i + 1, j + 1), b(i, j + 1)])
    # Skirt around the four sides (winding fixed by orient_outward below).
    edge = ([(i, 0) for i in range(n)] + [(n, j) for j in range(n)]
            + [(i, n) for i in range(n, 0, -1)] + [(0, j) for j in range(n, 0, -1)])
    for k, (i, j) in enumerate(edge):
        i2, j2 = edge[(k + 1) % len(edge)]
        polys.append([t(i2, j2), b(i2, j2), b(i, j), t(i, j)])
    mesh = lod.Mesh()
    mesh.add_part(verts, polys, 0)
    mesh.orient_outward()
    return mesh


def torus_mesh(major=1.0, minor=0.32, seg_major=36, seg_minor=14):
    verts = []
    for i in range(seg_major):
        a = 2.0 * math.pi * i / seg_major
        ca, sa = math.cos(a), math.sin(a)
        for k in range(seg_minor):
            b = 2.0 * math.pi * k / seg_minor
            r = major + minor * math.cos(b)
            verts.append((r * ca, minor * math.sin(b), r * sa))
    polys = []
    for i in range(seg_major):
        for k in range(seg_minor):
            i2, k2 = (i + 1) % seg_major, (k + 1) % seg_minor
            polys.append([i * seg_minor + k, i * seg_minor + k2,
                          i2 * seg_minor + k2, i2 * seg_minor + k])
    mesh = lod.Mesh()
    mesh.add_part(verts, polys, 0)
    mesh.orient_outward()
    return mesh


def obelisk_mesh(base=0.45, top=0.28, height=3.2, cap=0.6):
    verts = [(-base, 0.0, -base), (base, 0.0, -base), (base, 0.0, base), (-base, 0.0, base),
             (-top, height, -top), (top, height, -top), (top, height, top), (-top, height, top),
             (0.0, height + cap, 0.0)]
    polys = [[0, 1, 2, 3]]
    for i in range(4):
        j = (i + 1) % 4
        polys.append([i, 4 + i, 4 + j, j])
        polys.append([4 + i, 8, 4 + j])
    mesh = lod.Mesh()
    mesh.add_part(verts, polys, 0)
    mesh.orient_outward()
    return mesh


def generate_meshes():
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    lod.write_fbx(MODEL_DIR / "Terrain.fbx", "Terrain", terrain_mesh(),
                  [{"name": "Grass", "rgb": (0.46, 0.62, 0.34), "map": "box"}], smooth=True)
    lod.write_fbx(MODEL_DIR / "Torus.fbx", "Torus", torus_mesh(),
                  [{"name": "Red", "rgb": (0.85, 0.22, 0.18), "map": "box"}], smooth=True)
    lod.write_fbx(MODEL_DIR / "Obelisk.fbx", "Obelisk", obelisk_mesh(),
                  [{"name": "Sandstone", "rgb": (0.86, 0.78, 0.6), "map": "box"}], smooth=False)


def transform(x, y, z, rot=(0.0, 0.0, 0.0), scale=1.0):
    return {"posX": round(x, 4), "posY": round(y, 4), "posZ": round(z, 4),
            "rotX": rot[0], "rotY": rot[1], "rotZ": rot[2],
            "scaleX": scale, "scaleY": scale, "scaleZ": scale}


def node(name, components, tag=0):
    return {"name": name, "tag": tag, "children": [], "components": components}


def prop(name, model, x, z, sink=0.05, lift=0.0, rot=(0.0, 0.0, 0.0), scale=1.0, cast=True,
         spin=0.0):
    y = terrain_height(x, z) - sink + lift
    visual = {"model": model, "shader": "standard"}
    if cast:
        visual["shadowVolume"] = True
    components = {"SceneTransform": transform(x, y, z, rot, scale), "visual": visual}
    if spin:
        # SampleSpinScript (samples/scripts) turns the object about world Y in Play.
        components["Script"] = {"scriptClass": "SampleSpinScript",
                                "properties": {"degreesPerSecond": spin, "busyMillis": 0.0}}
    return node(name, components)


def build_scene():
    light = node("Directional Light", {
        "SceneTransform": transform(0.0, 12.0, 0.0, (34.0, -40.0, 0.0)),
        "DirectionalLight": {
            "enabled": True, "castShadows": True, "color": [1.0, 0.96, 0.9],
            "intensity": 1.15, "specColor": [1.0, 1.0, 1.0], "specPower": 32.0,
        },
    })
    terrain = node("Terrain", {
        "SceneTransform": transform(0.0, 0.0, 0.0),
        "visual": {"model": f"{MODEL_ROM}/Terrain.fbx", "shader": "standard"},
    }, tag=2)  # GROUND
    boulder = "rom/model/lod/Boulder_LOD1.fbx"
    pine = "rom/model/lod/PineTree_LOD1.fbx"
    nodes = [
        light,
        terrain,
        # Torus stands upright on the slope and spins like a coin on edge: its shadow
        # keeps the hole and morphs from a ring to a thin bar and back.
        prop("Caster_Torus", f"{MODEL_ROM}/Torus.fbx", 0.0, 4.0, sink=0.1, lift=1.32,
             rot=(90.0, 25.0, 0.0), scale=1.0, spin=35.0),
        prop("Caster_Obelisk_A", f"{MODEL_ROM}/Obelisk.fbx", -4.5, 6.5, rot=(0.0, 20.0, 0.0)),
        prop("Caster_Obelisk_B", f"{MODEL_ROM}/Obelisk.fbx", 4.5, 9.0, rot=(0.0, -15.0, 0.0),
             scale=1.3),
        prop("Caster_Boulder_A", boulder, -2.0, 1.5, rot=(0.0, 40.0, 0.0), scale=2.2),
        prop("Caster_Boulder_B", boulder, 6.5, 3.0, rot=(0.0, 130.0, 0.0), scale=3.0,
             spin=-20.0),
        prop("Caster_Pine_A", pine, -7.5, 11.0, scale=1.2),
        prop("Caster_Pine_B", pine, -1.5, 13.0, rot=(0.0, 50.0, 0.0), scale=1.0),
        prop("Caster_Pine_C", pine, 8.5, 13.5, rot=(0.0, 110.0, 0.0), scale=1.4),
        # Opt-out example: same scene, no shadow volume (receives, does not cast).
        prop("NoVolume_Box", "rom/model/object/Box_Blue.fbx", 2.5, -1.0, sink=0.0, lift=0.5,
             cast=False),
    ]
    settings = {
        "render": {
            "clearColor": [0.69, 0.77, 0.87, 1.0],
            "defaultShader": "standard",
            "fogEnabled": False,
            "shadowQuality": "medium",
            "shadowMode": "volume",
            "shadowVolumeStrength": 0.55,
            "skyboxCubemap": "rom/skybox/MySkybox.dds",
            "skyboxEnabled": True,
        },
        "debugCamera": {
            "mode": "fly",
            "enabledOnPlay": True,
            "yawDeg": 0.0,
            "pitchDeg": -18.0,
            "position": {"x": 0.0, "y": 6.5, "z": -9.0},
        },
    }
    return {"version": 4, "settings": settings, "nodes": nodes, "canvas": {}}


def main():
    generate_meshes()
    scene = build_scene()
    SCENE_PATH.write_text(json.dumps(scene, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("wrote %s: %d top-level nodes" % (SCENE_PATH.relative_to(ROOT), len(scene["nodes"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
