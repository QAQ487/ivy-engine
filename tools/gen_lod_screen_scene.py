#!/usr/bin/env python3
"""Generate testbed/rom/scene/scene_lod_screen_fly.json.

Verification scene for screen-size LOD (visual.lodMode = "screenSize"). Free-fly
debug camera, no scripts. Four columns of the same boulder at scale 1, 2, 4 and 8,
repeated along +Z. Every boulder uses the same screen-height thresholds
(12% / 6% / 3% of the viewport height), so a bigger boulder keeps its detailed mesh
much further away: the LOD0 -> LOD1 switch distance is proportional to the size.
(With distance mode all four columns would switch at the same distance.)

Rendering page -> "lod ... L0 / L1 / L2+" shows how many draws use each level, and the
scene turns on render.lodDebugLabels: every boulder gets a text label with its level,
camera distance and projected screen height ("L1 23m 5.8%"), coloured green / yellow /
orange / red for L0 / L1 / L2 / L3.

Needs the LOD meshes made by tools/gen_lod_test_assets.py.

Run:  python3 tools/gen_lod_screen_scene.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "testbed/rom/scene/scene_lod_screen_fly.json"

SCALES = (1.0, 2.0, 4.0, 8.0)
COLUMN_X = (-36.0, -12.0, 12.0, 36.0)
Z_ROWS = (12.0, 24.0, 48.0, 90.0, 140.0, 200.0, 280.0, 380.0)
SCREEN_HEIGHTS = (0.12, 0.06, 0.03)
LOD_MODELS = ("rom/model/lod/Boulder_LOD1.fbx",
              "rom/model/lod/Boulder_LOD2.fbx",
              "rom/model/lod/Boulder_LOD3.fbx")


def boulder(name, x, z, scale):
    return {"name": name, "tag": 0, "children": [], "components": {
        "SceneTransform": {"posX": x, "posY": 0.0, "posZ": z, "rotX": 0.0, "rotY": 30.0, "rotZ": 0.0,
                            "scaleX": scale, "scaleY": scale, "scaleZ": scale},
        "visual": {
            "model": "rom/model/lod/Boulder_LOD0.fbx", "shader": "standard",
            "lodMode": "screenSize",
            "lods": [{"model": m, "maxDistance": 0.0, "screenHeight": h}
                     for m, h in zip(LOD_MODELS, SCREEN_HEIGHTS)]}}}


def build():
    nodes = [
        {"name": "Directional Light", "tag": 0, "children": [], "components": {
            "SceneTransform": {"posX": 0.0, "posY": 10.0, "posZ": 0.0,
                                "rotX": 50.0, "rotY": -35.0, "rotZ": 0.0,
                                "scaleX": 1.0, "scaleY": 1.0, "scaleZ": 1.0},
            "DirectionalLight": {"enabled": True, "castShadows": True,
                                  "color": [1.0, 0.97, 0.92], "intensity": 1.1,
                                  "specColor": [1.0, 1.0, 1.0], "specPower": 32.0}}},
        {"name": "Ground", "tag": 2, "children": [], "components": {
            "SceneTransform": {"posX": 0.0, "posY": -0.5, "posZ": 200.0,
                                "rotX": 0.0, "rotY": 0.0, "rotZ": 0.0,
                                "scaleX": 200.0, "scaleY": 1.0, "scaleZ": 900.0},
            "visual": {"model": "rom/model/object/Box_Green.fbx", "shader": "standard"}}},
    ]
    for scale, x in zip(SCALES, COLUMN_X):
        for z in Z_ROWS:
            nodes.append(boulder(f"Boulder x{scale:g} z{z:g}", x, z, scale))
    return {
        "version": 4,
        "settings": {
            "render": {"clearColor": [0.69, 0.77, 0.87, 1.0], "defaultShader": "standard",
                       "fogEnabled": False, "shadowQuality": "medium", "skyboxEnabled": False,
                       "lodEnabled": True, "lodBias": 1.0,
                       "lodDebugLabels": True},
            "debugCamera": {"mode": "fly", "enabledOnPlay": True, "yawDeg": 0.0,
                            "pitchDeg": -3.0, "position": {"x": 0.0, "y": 4.0, "z": -6.0}},
        },
        "nodes": nodes,
    }


if __name__ == "__main__":
    OUT.write_text(json.dumps(build(), indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}")
