#!/usr/bin/env python3
"""Generate testbed/rom/scene/scene_anim_cull_fly.json.

Verification scene for off-screen animation culling (render.animCullingEnabled).
Free-fly debug camera (no scripts). Every character plays the same looping Idle
clip, so all poses match at any instant while clip time is continuous.

Groups (camera starts at the origin looking +Z):
  Front   in view                              -> evaluated every frame
  Back    behind the camera, far from the sun   -> skipped while you look forward
  Sides   far left / right of the view          -> skipped
  Far     in view direction, past cullDistance  -> skipped (distance cull)
                                                   needs Mesh LOD (3D) on: cullDistance is
                                                   ignored while it is off. LOD debug labels
                                                   are on, so each Far bot shows "CULLED <d>m >
                                                   40m" until you are within 40 m, then "L0 ...".
  Shadow  just behind the camera                -> may stay evaluated if a shadow
                                                   cascade still draws it

Run:  python3 tools/gen_anim_cull_scene.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "testbed/rom/scene/scene_anim_cull_fly.json"

BOT = "rom/model/player/Y Bot_white.fbx"
IDLE = "rom/model/player/Idle.fbx"
BOT_SCALE = 0.01


def transform(x, y, z, rot_y=0.0, scale=1.0):
    return {"SceneTransform": {
        "posX": x, "posY": y, "posZ": z, "rotX": 0.0, "rotY": rot_y, "rotZ": 0.0,
        "scaleX": scale, "scaleY": scale, "scaleZ": scale}}


def bot(name, x, z, cull_distance=0.0):
    visual = {"model": BOT, "shader": "standard", "animate": True,
              "active_clip": "idle", "animations": {"idle": IDLE}}
    if cull_distance > 0.0:
        visual["cullDistance"] = cull_distance
    comps = transform(x, 0.0, z, 180.0, BOT_SCALE)
    comps["visual"] = visual
    return {"name": name, "tag": 0, "children": [], "components": comps}


def build():
    nodes = [
        {"name": "Directional Light", "tag": 0, "children": [], "components": {
            **transform(0.0, 10.0, 0.0, 0.0, 1.0),
            "DirectionalLight": {"enabled": True, "castShadows": True,
                                  "color": [1.0, 0.97, 0.92], "intensity": 1.1,
                                  "specColor": [1.0, 1.0, 1.0], "specPower": 32.0}}},
        {"name": "Ground", "tag": 2, "children": [], "components": {
            **transform(0.0, -0.5, 0.0, 0.0, 1.0),
            "visual": {"model": "rom/model/object/Box_Green.fbx", "shader": "standard"}}},
    ]
    nodes[0]["components"]["SceneTransform"].update(rotX=50.0, rotY=-35.0)
    nodes[1]["components"]["SceneTransform"].update(scaleX=400.0, scaleZ=400.0)

    for i, x in enumerate((-8, -4, 0, 4, 8)):
        nodes.append(bot(f"Front {i + 1}", float(x), 8.0))
    for i, x in enumerate((-8, -4, 0, 4, 8)):
        nodes.append(bot(f"Back {i + 1}", float(x), -45.0))
    for i, x in enumerate((-60, -50, 50, 60)):
        nodes.append(bot(f"Side {i + 1}", float(x), 0.0))
    for i, x in enumerate((-6, -2, 2, 6)):
        nodes.append(bot(f"Far {i + 1} (cullDistance 40)", float(x), 70.0, 40.0))
    nodes.append(bot("Shadow Caster (behind camera)", 0.0, -12.0))

    return {
        "version": 4,
        "settings": {
            "render": {"clearColor": [0.69, 0.77, 0.87, 1.0], "defaultShader": "standard",
                       "fogEnabled": False, "shadowQuality": "medium",
                       "skyboxCubemap": "rom/skybox/MySkybox.dds", "skyboxEnabled": True,
                       "lodEnabled": True, "animCullingEnabled": True,
                       "lodDebugLabels": True},
            "debugCamera": {"mode": "fly", "enabledOnPlay": True, "yawDeg": 0.0,
                            "pitchDeg": -5.0, "position": {"x": 0.0, "y": 2.5, "z": 0.0}},
        },
        "nodes": nodes,
    }


if __name__ == "__main__":
    OUT.write_text(json.dumps(build(), indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}")
