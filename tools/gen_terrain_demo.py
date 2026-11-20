#!/usr/bin/env python3
"""Generate the heightmap terrain demo (ADR 0011, terrain step 2).

Everything here is original procedural content (no third-party assets):

  testbed/rom/terrain/terrain_demo.r16            257 x 257 heightmap (raw 16-bit LE)
  testbed/rom/texture/terrain/ground_demo.png     128 x 128 tileable ground texture
  testbed/rom/scene/scene_terrain_demo.json       Terrain component, CSM sun, props, player

The terrain is 512 x 512 m, heights -10 .. 50 m, centred on the origin, drawn in 8 x 8
chunks of 32 cells (LOD levels 0..4). Pines and boulders from tools/gen_lod_test_assets.py
stand on the surface: their Y comes from sample_height(), which follows the engine's
triangle split (terrain::SampleHeight), so they sit exactly on what is drawn.

Terrain step 3 (collision): Play puts SamplePlayerScript's sphere player on the hills
with the ThirdPerson camera (compiled into the Debug editor / runtime host), and drops a
sphere, box, capsule and cylinder (engine primitives, colliders shown) onto the slope in
front of it, so they land and roll on the heightfield collider. F1 toggles the fly camera.

Run from the repo root:

    python3 tools/gen_terrain_demo.py           # write the files
    python3 tools/gen_terrain_demo.py --check   # fail if the files differ from the generator
"""
import json
import math
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
R16_PATH = ROOT / "testbed/rom/terrain/terrain_demo.r16"
TEXTURE_PATH = ROOT / "testbed/rom/texture/terrain/ground_demo.png"
SCENE_PATH = ROOT / "testbed/rom/scene/scene_terrain_demo.json"
R16_ROM = "rom/terrain/terrain_demo.r16"
TEXTURE_ROM = "rom/texture/terrain/ground_demo.png"

RESOLUTION = 257
SIZE = 512.0
HEIGHT_SCALE = 60.0
HEIGHT_OFFSET = -10.0
CHUNK_QUADS = 32
ORIGIN = (-SIZE * 0.5, 0.0, -SIZE * 0.5)  # GameObject position: terrain centred on (0, 0)
TAG_GROUND = 2
TAG_PLAYER = 5
PLAYER_XZ = (0.0, 6.0)


def height_function(x, z):
    """Terrain-local metres -> height in metres (-10 .. 50). Hills, a ridge, a basin."""
    u, v = x / SIZE, z / SIZE
    h = 8.0
    h += 9.0 * math.sin(u * 2 * math.pi * 1.5 + 0.4) * math.cos(v * 2 * math.pi * 1.2 - 0.7)
    h += 4.0 * math.sin(u * 2 * math.pi * 4.0 + v * 2 * math.pi * 3.0)
    h += 1.5 * math.cos(u * 2 * math.pi * 9.0 - v * 2 * math.pi * 7.0)
    # A ridge along the far (+Z) side.
    h += 30.0 * math.exp(-((v - 0.82) / 0.09) ** 2) * (0.6 + 0.4 * math.sin(u * 2 * math.pi * 2.0))
    # A basin left of the centre, below the camera's first view.
    d = math.hypot(u - 0.36, v - 0.42)
    h -= 14.0 * math.exp(-(d / 0.11) ** 2)
    return h


def build_samples():
    cell = SIZE / (RESOLUTION - 1)
    samples = []
    for zi in range(RESOLUTION):
        for xi in range(RESOLUTION):
            h = height_function(xi * cell, zi * cell)
            t = (h - HEIGHT_OFFSET) / HEIGHT_SCALE * 65535.0
            samples.append(int(round(min(max(t, 0.0), 65535.0))))
    return samples


def encode_r16(samples):
    return struct.pack("<%dH" % len(samples), *samples)


def sample_height(samples, x, z):
    """Port of terrain::SampleHeight (triangle split along (x,z)-(x+1,z+1))."""
    cells = RESOLUTION - 1
    gx = x / SIZE * cells
    gz = z / SIZE * cells
    cx = min(max(int(math.floor(gx)), 0), cells - 1)
    cz = min(max(int(math.floor(gz)), 0), cells - 1)
    fx = min(max(gx - cx, 0.0), 1.0)
    fz = min(max(gz - cz, 0.0), 1.0)

    def h(ix, iz):
        return HEIGHT_OFFSET + samples[iz * RESOLUTION + ix] / 65535.0 * HEIGHT_SCALE

    a, b = h(cx, cz), h(cx + 1, cz)
    c, d = h(cx, cz + 1), h(cx + 1, cz + 1)
    if fz >= fx:
        return a + fz * (c - a) + fx * (d - c)
    return a + fx * (b - a) + fz * (d - b)


def png_bytes(width, height, rgb_rows):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(row) for row in rgb_rows)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def ground_texture():
    """128 x 128 grass/soil mix. Integer periods across the tile, so it repeats seamlessly."""
    n = 128
    rows = []
    for y in range(n):
        row = []
        for x in range(n):
            u, v = 2 * math.pi * x / n, 2 * math.pi * y / n
            k = (0.5 * math.sin(3 * u + 1.0) * math.cos(2 * v)
                 + 0.3 * math.sin(7 * u - 5 * v + 0.5)
                 + 0.2 * math.cos(13 * u + 11 * v))
            fine = math.sin(29 * u + 0.7) * math.sin(31 * v + 1.3)
            t = 0.5 + 0.5 * k  # 0 = soil, 1 = grass
            r = 0.40 + (0.22 - 0.40) * t + 0.03 * fine
            g = 0.33 + (0.42 - 0.33) * t + 0.03 * fine
            b = 0.22 + (0.16 - 0.22) * t + 0.02 * fine
            row += [int(round(min(max(c, 0.0), 1.0) * 255)) for c in (r, g, b)]
        rows.append(row)
    return png_bytes(n, n, rows)


def transform(x, y, z, rot_y=0.0, scale=1.0):
    return {"posX": round(x, 4), "posY": round(y, 4), "posZ": round(z, 4),
            "rotX": 0.0, "rotY": rot_y, "rotZ": 0.0,
            "scaleX": scale, "scaleY": scale, "scaleZ": scale}


def node(name, components, tag=0):
    return {"name": name, "tag": tag, "children": [], "components": components}


def build_scene(samples):
    nodes = [
        node("Directional Light", {
            "SceneTransform": transform(0.0, 40.0, 0.0, -35.0) | {"rotX": 32.0},
            "DirectionalLight": {"enabled": True, "castShadows": True, "color": [1.0, 0.95, 0.86],
                                 "intensity": 1.2, "specColor": [1.0, 1.0, 1.0], "specPower": 32.0},
        }),
        node("Terrain", {
            "SceneTransform": transform(*ORIGIN),
            "Terrain": {
                "heightmap": R16_ROM, "sizeX": SIZE, "sizeZ": SIZE,
                "heightScale": HEIGHT_SCALE, "heightOffset": HEIGHT_OFFSET,
                "chunkQuads": CHUNK_QUADS, "lodBias": 1.0,
                "texture": TEXTURE_ROM, "textureTileSize": 6.0,
                "color": {"r": 1.0, "g": 1.0, "b": 1.0},
                "roughness": 0.95, "metallic": 0.0,
                "castShadows": True, "enabled": True,
            },
        }, tag=TAG_GROUND),
    ]

    pine_lods = [{"model": "rom/model/lod/PineTree_LOD%d.fbx" % i, "maxDistance": d}
                 for i, d in ((1, 40.0), (2, 90.0), (3, 160.0))]
    boulder_lods = [{"model": "rom/model/lod/Boulder_LOD%d.fbx" % i, "maxDistance": d}
                    for i, d in ((1, 30.0), (2, 70.0), (3, 140.0))]

    # Deterministic scatter around the first view (world coordinates, terrain centred).
    for i in range(16):
        angle = i * 2.399963  # golden angle
        radius = 14.0 + 7.5 * math.sqrt(i + 1)
        wx, wz = radius * math.cos(angle), 20.0 + radius * math.sin(angle)
        wy = sample_height(samples, wx - ORIGIN[0], wz - ORIGIN[2])
        nodes.append(node("Pine %02d" % i, {
            "SceneTransform": transform(wx, wy, wz, (i * 47) % 360, 1.0 + 0.15 * (i % 3)),
            "visual": {"model": "rom/model/lod/PineTree_LOD0.fbx", "shader": "standard",
                       "lods": pine_lods, "cullDistance": 400.0},
        }))
    for i in range(6):
        angle = 0.7 + i * 1.047
        wx, wz = 9.0 * math.cos(angle) - 6.0, 16.0 + 9.0 * math.sin(angle)
        wy = sample_height(samples, wx - ORIGIN[0], wz - ORIGIN[2])
        nodes.append(node("Boulder %02d" % i, {
            "SceneTransform": transform(wx, wy, wz, (i * 71) % 360, 1.6 + 0.4 * (i % 2)),
            "visual": {"model": "rom/model/lod/Boulder_LOD0.fbx", "shader": "standard",
                       "lods": boulder_lods, "cullDistance": 300.0},
        }))

    nodes += physics_nodes(samples)

    cam_y = sample_height(samples, 0.0 - ORIGIN[0], -30.0 - ORIGIN[2]) + 18.0
    return {
        "version": 4,
        "settings": {
            "renderPath": "3d",
            "physicsMode": "3d",
            "render": {
                "clearColor": [0.69, 0.77, 0.87, 1.0],
                "defaultShader": "standard",
                "skyboxEnabled": True,
                "skyboxCubemap": "rom/skybox/MySkybox.dds",
                "shadowQuality": "medium",
                "shadowDistance": 160.0,
                "fogEnabled": False,
            },
            "camera": {"mode": "thirdPerson"},
            "debugCamera": {
                "mode": "fly", "enabledOnPlay": False, "yawDeg": 0.0, "pitchDeg": -14.0,
                "position": {"x": 0.0, "y": round(cam_y, 3), "z": -30.0},
            },
        },
        "nodes": nodes,
    }


def legacy_collider(collider_type, radius=0.5, height=1.0, half=0.5):
    """Generic "Collider" JSON (still loaded; the editor rewrites it shape-specific)."""
    return {"colliderCenter": {"x": 0.0, "y": 0.0, "z": 0.0}, "colliderType": collider_type,
            "halfExtents": {"x": half, "y": half, "z": half}, "height": height, "isTrigger": False,
            "meshSizePadding": 0.0, "radius": radius, "showDebugCollider": True}


def physics_nodes(samples):
    """Player plus falling primitives (engine primitives are 1 m: sphere / cube / cylinder
    r 0.5, cylinder height 1, capsule r 0.5 with a 1 m segment)."""
    px, pz = PLAYER_XZ
    py = sample_height(samples, px - ORIGIN[0], pz - ORIGIN[2]) + 1.0
    player = {
        "name": "Player", "tag": TAG_PLAYER,
        "components": {
            "Collider": legacy_collider(2) | {"showDebugCollider": False},
            "SceneTransform": transform(px, py, pz),
            "Script": {"scriptClass": "SamplePlayerScript"},
        },
        "children": [node("Player_Model", {
            "SceneTransform": {"posX": 0.0, "posY": -0.5, "posZ": 0.0, "rotX": 0.0, "rotY": 0.0,
                               "rotZ": 0.0, "scaleX": 0.01, "scaleY": 0.01, "scaleZ": 0.01},
            "visual": {"active_clip": "idle", "animate": True,
                       "animations": {"idle": "rom/model/player/Idle.fbx",
                                      "jump": "rom/model/player/Jump01.fbx",
                                      "run": "rom/model/player/Running.fbx"},
                       "model": "rom/model/player/Y Bot_white.fbx", "shader": "standard"},
        }, tag=TAG_PLAYER)],
    }
    bodies = [
        # name, model, collider type, radius, height, colour, x offset
        ("Drop Sphere", "Sphere.fbx", 2, 0.5, 1.0, [0.9, 0.3, 0.2], -4.5),
        ("Drop Box", "Cube.fbx", 1, 0.5, 1.0, [0.25, 0.55, 0.95], -1.5),
        ("Drop Capsule", "Capsule.fbx", 3, 0.5, 1.0, [0.95, 0.8, 0.2], 1.5),
        ("Drop Cylinder", "Cylinder.fbx", 5, 0.5, 1.0, [0.35, 0.85, 0.4], 4.5),
    ]
    out = [player]
    for i, (name, model, ctype, radius, height, color, dx) in enumerate(bodies):
        x, z = px + dx, pz + 9.0
        y = sample_height(samples, x - ORIGIN[0], z - ORIGIN[2]) + 6.0
        t = transform(x, y, z) | {"rotX": 20.0 * (i % 2), "rotZ": 15.0 * ((i + 1) % 2)}
        out.append(node(name, {
            "SceneTransform": t,
            "visual": {"model": "rom/model/primitives/" + model, "shader": "standard"},
            "Material": {"color": color, "metallic": 0.0, "roughness": 0.5},
            "Collider": legacy_collider(ctype, radius, height),
            "Rigidbody": {"useGravity": True, "isKinematic": False, "mass": 1.0, "drag": 0.05,
                          "angularDrag": 0.2, "restitution": 0.1, "friction": 0.6},
        }))
    return out


def outputs():
    samples = build_samples()
    scene = json.dumps(build_scene(samples), indent=2) + "\n"
    return {
        R16_PATH: encode_r16(samples),
        TEXTURE_PATH: ground_texture(),
        SCENE_PATH: scene.encode("utf-8"),
    }


def main(argv):
    check = "--check" in argv
    stale = []
    for path, data in outputs().items():
        if check:
            if not path.is_file() or path.read_bytes() != data:
                stale.append(path.relative_to(ROOT).as_posix())
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print("wrote", path.relative_to(ROOT).as_posix(), len(data), "bytes")
    if stale:
        print("FAIL: out of date (rerun python3 tools/gen_terrain_demo.py):", ", ".join(stale),
              file=sys.stderr)
        return 1
    if check:
        print("terrain demo files match the generator")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
