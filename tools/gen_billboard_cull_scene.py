#!/usr/bin/env python3
"""Generate testbed/rom/scene/scene_billboard_cull_fly.json.

Verification scene for billboard culling (render.billboardCullingEnabled).
Free-fly debug camera, no scripts. 12 x 12 camera-facing billboards on a 10 m
grid around the origin (144 in total, colour by column), plus a ring of
ground-aligned markers. Look along one axis and about half the grid is behind
you or beside you: the Rendering page's billboard counter shows how many were
culled, and toggling "Billboard / Particle Culling (3D)" must not change the
picture.

Particles and Effekseer effects cannot be authored in scene JSON, so they are
not covered here (see docs/TODO.md for how to check them in a content project).

Run:  python3 tools/gen_billboard_cull_scene.py
"""
import colorsys
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "testbed/rom/scene/scene_billboard_cull_fly.json"

GRID = 12
SPACING = 10.0
SIZE = 3.0
TEXTURE = "rom/model/lod/needles.png"


def transform(x, y, z):
    return {"SceneTransform": {
        "posX": x, "posY": y, "posZ": z, "rotX": 0.0, "rotY": 0.0, "rotZ": 0.0,
        "scaleX": 1.0, "scaleY": 1.0, "scaleZ": 1.0}}


def billboard(name, x, y, z, color, ground=False, size=SIZE):
    return {"name": name, "tag": 0, "children": [], "components": {
        **transform(x, y, z),
        "Billboard": {"texture": TEXTURE, "sizeX": size, "sizeY": size,
                       "color": {"r": color[0], "g": color[1], "b": color[2], "a": 1.0},
                       "rotation": 0.0, "groundAligned": ground, "enabled": True}}}


def build():
    nodes = [
        {"name": "Directional Light", "tag": 0, "children": [], "components": {
            "SceneTransform": {"posX": 0.0, "posY": 10.0, "posZ": 0.0,
                                "rotX": 50.0, "rotY": -35.0, "rotZ": 0.0,
                                "scaleX": 1.0, "scaleY": 1.0, "scaleZ": 1.0},
            "DirectionalLight": {"enabled": True, "castShadows": False,
                                  "color": [1.0, 0.97, 0.92], "intensity": 1.0,
                                  "specColor": [1.0, 1.0, 1.0], "specPower": 32.0}}},
        {"name": "Ground", "tag": 2, "children": [], "components": {
            "SceneTransform": {"posX": 0.0, "posY": -0.5, "posZ": 0.0,
                                "rotX": 0.0, "rotY": 0.0, "rotZ": 0.0,
                                "scaleX": 300.0, "scaleY": 1.0, "scaleZ": 300.0},
            "visual": {"model": "rom/model/object/Box_Green.fbx", "shader": "standard"}}},
    ]
    origin = -(GRID - 1) * SPACING / 2.0
    for ix in range(GRID):
        r, g, b = colorsys.hsv_to_rgb(ix / GRID, 0.7, 1.0)
        for iz in range(GRID):
            nodes.append(billboard(f"Billboard {ix:02d}-{iz:02d}",
                                   origin + ix * SPACING, 2.0, origin + iz * SPACING, (r, g, b)))
    for i in range(16):
        angle = i * (360.0 / 16)
        import math
        x = 25.0 * math.cos(math.radians(angle))
        z = 25.0 * math.sin(math.radians(angle))
        nodes.append(billboard(f"Ground Marker {i:02d}", x, 0.05, z, (1.0, 1.0, 1.0),
                               ground=True, size=4.0))
    return {
        "version": 4,
        "settings": {
            "render": {"clearColor": [0.69, 0.77, 0.87, 1.0], "defaultShader": "standard",
                       "fogEnabled": False, "shadowQuality": "low", "skyboxEnabled": False,
                       "billboardCullingEnabled": True},
            "debugCamera": {"mode": "fly", "enabledOnPlay": True, "yawDeg": 0.0,
                            "pitchDeg": -5.0, "position": {"x": 0.0, "y": 3.0, "z": 0.0}},
        },
        "nodes": nodes,
    }


if __name__ == "__main__":
    OUT.write_text(json.dumps(build(), indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}")
