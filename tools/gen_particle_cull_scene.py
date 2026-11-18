#!/usr/bin/env python3
"""Generate testbed/rom/scene/scene_particle_cull_fly.json.

Verification scene for particle culling (render.billboardCullingEnabled covers
billboards and individual particles). Free-fly debug camera. Particles cannot be
authored in scene JSON, so every emitter node carries a Script component for
SampleParticleEmitterScript (samples/scripts/), which is compiled into the
Debug editor and Debug runtime host only. Release builds show an empty scene.

Layout: 40 emitters on three rings around the camera (8 at 15 m, 16 at 30 m,
16 at 55 m), colour by angle. Look at one emitter and most particles are outside
the frustum: the Rendering page's particle counter shows how many were culled.

Run:  python3 tools/gen_particle_cull_scene.py
"""
import colorsys
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "testbed/rom/scene/scene_particle_cull_fly.json"

RINGS = ((15.0, 8), (30.0, 16), (55.0, 16))


def emitter(name, x, z, color):
    return {"name": name, "tag": 0, "children": [], "components": {
        "SceneTransform": {"posX": x, "posY": 1.5, "posZ": z,
                            "rotX": 0.0, "rotY": 0.0, "rotZ": 0.0,
                            "scaleX": 1.0, "scaleY": 1.0, "scaleZ": 1.0},
        "Script": {"scriptClass": "SampleParticleEmitterScript",
                    "properties": {"emissionRate": 40.0, "lifetime": 5.0,
                                    "startSpeed": 1.5, "startSize": 0.4,
                                    "colorR": round(color[0], 3),
                                    "colorG": round(color[1], 3),
                                    "colorB": round(color[2], 3),
                                    "maxParticles": 256}}}}


def build():
    nodes = [
        {"name": "Directional Light", "tag": 0, "children": [], "components": {
            "SceneTransform": {"posX": 0.0, "posY": 10.0, "posZ": 0.0,
                                "rotX": 50.0, "rotY": -35.0, "rotZ": 0.0,
                                "scaleX": 1.0, "scaleY": 1.0, "scaleZ": 1.0},
            "DirectionalLight": {"enabled": True, "castShadows": False,
                                  "color": [1.0, 0.97, 0.92], "intensity": 0.6,
                                  "specColor": [1.0, 1.0, 1.0], "specPower": 32.0}}},
        {"name": "Ground", "tag": 2, "children": [], "components": {
            "SceneTransform": {"posX": 0.0, "posY": -0.5, "posZ": 0.0,
                                "rotX": 0.0, "rotY": 0.0, "rotZ": 0.0,
                                "scaleX": 300.0, "scaleY": 1.0, "scaleZ": 300.0},
            "visual": {"model": "rom/model/object/Box_Sand.fbx", "shader": "standard"}}},
    ]
    for ring, (radius, count) in enumerate(RINGS):
        for i in range(count):
            angle = i * (360.0 / count)
            x = radius * math.sin(math.radians(angle))
            z = radius * math.cos(math.radians(angle))
            color = colorsys.hsv_to_rgb(angle / 360.0, 0.8, 1.0)
            nodes.append(emitter(f"Emitter R{ring + 1} {i:02d}", x, z, color))
    return {
        "version": 4,
        "settings": {
            "render": {"clearColor": [0.04, 0.05, 0.09, 1.0], "defaultShader": "standard",
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
