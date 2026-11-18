#!/usr/bin/env python3
"""Generate testbed/rom/scene/scene_light_select_fly.json.

Verification scene for scene light selection (render.lightSelectionEnabled).
Free-fly debug camera, no scripts. Twelve coloured point lights stand in a row
along X (one pillar under each) plus one dim directional light, so there are
13 enabled lights for 4 GPU slots (1 directional + 3 point lights).

Fly along the row: with selection ON the 3 point lights nearest to the camera
light the ground and the lit patches follow you. With it OFF the first lights in
scene order (the left end of the row) stay lit wherever you are.

Run:  python3 tools/gen_light_select_scene.py
"""
import colorsys
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "testbed/rom/scene/scene_light_select_fly.json"

COUNT = 12
SPACING = 14.0
LIGHT_Y = 3.0
RANGE = 12.0


def transform(x, y, z, rot=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)):
    return {"SceneTransform": {
        "posX": x, "posY": y, "posZ": z,
        "rotX": rot[0], "rotY": rot[1], "rotZ": rot[2],
        "scaleX": scale[0], "scaleY": scale[1], "scaleZ": scale[2]}}


def node(name, comps, tag=0):
    return {"name": name, "tag": tag, "children": [], "components": comps}


def build():
    nodes = [
        node("Directional Light (dim)", {
            **transform(0.0, 10.0, 0.0, rot=(50.0, -35.0, 0.0)),
            "DirectionalLight": {"enabled": True, "castShadows": False,
                                  "color": [0.5, 0.55, 0.7], "intensity": 0.25,
                                  "specColor": [1.0, 1.0, 1.0], "specPower": 32.0}}),
        node("Ground", {
            **transform(0.0, -0.5, 0.0, scale=(400.0, 1.0, 60.0)),
            "visual": {"model": "rom/model/object/Box_Sand.fbx", "shader": "standard"}}, tag=2),
    ]
    x0 = -(COUNT - 1) * SPACING / 2.0
    for i in range(COUNT):
        x = x0 + i * SPACING
        r, g, b = colorsys.hsv_to_rgb(i / COUNT, 0.85, 1.0)
        nodes.append(node(f"Pillar {i + 1:02d}", {
            **transform(x, 1.0, 4.0, scale=(0.6, 2.0, 0.6)),
            "visual": {"model": "rom/model/object/Box_Brown.fbx", "shader": "standard"}}))
        nodes.append(node(f"Point Light {i + 1:02d}", {
            **transform(x, LIGHT_Y, 0.0),
            "PointLight": {"enabled": True, "color": [r, g, b], "intensity": 3.0,
                            "specColor": [1.0, 1.0, 1.0], "specPower": 32.0,
                            "range": RANGE, "attenuation": [1.0, 0.0, 1.0]}}))
    return {
        "version": 4,
        "settings": {
            "render": {"clearColor": [0.05, 0.06, 0.1, 1.0], "defaultShader": "standard",
                       "fogEnabled": False, "shadowQuality": "low", "skyboxEnabled": False,
                       "lightSelectionEnabled": True},
            "debugCamera": {"mode": "fly", "enabledOnPlay": True, "yawDeg": 0.0,
                            "pitchDeg": -20.0, "position": {"x": x0, "y": 6.0, "z": -14.0}},
        },
        "nodes": nodes,
    }


if __name__ == "__main__":
    OUT.write_text(json.dumps(build(), indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}")
