"""Validate the phase-6 sample's scene contract and relocatable asset references."""
import json
import sys
from pathlib import Path
root = Path(sys.argv[1]).resolve()
def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))
def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get("children", []))
for filename in ("scene_topdown_2d.json", "scene_topdown_2d_next.json", "scene_empty_2d.json"):
    scene = read(root / "rom/scene" / filename)
    assert scene["settings"]["renderPath"] == "2d"
    nodes = list(walk(scene["nodes"]))
    cameras = [n for n in nodes if "Camera" in n["components"]]
    assert len(cameras) == 1 and cameras[0]["components"]["Camera"]["orthographic"]
    if "empty" not in filename:
        player = next(n for n in nodes if n["name"] == "Player2D")
        assert cameras[0] in player["children"], "camera must follow player hierarchy"
        assert player["components"]["Rigidbody2D"]["bodyType"] == "dynamic"
        assert scene["settings"]["physicsMode"] == "2d"
        assert len([n for n in nodes if n["name"] == "Collectible"]) == 3
        assert any(e["name"] == "Score" and e["type"] == "text" for e in scene["canvas"]["elements"])
        script = player["components"]["Script"]["properties"]
        assert script["targetCount"] == 3
        for key in ("sceneFile", "nextScene"):
            assert (root / "rom/scene" / script[key]).is_file()
    def refs(value):
        if isinstance(value, dict):
            for item in value.values(): refs(item)
        elif isinstance(value, list):
            for item in value: refs(item)
        elif isinstance(value, str):
            assert not (len(value) > 1 and value[1] == ":"), "absolute developer path"
            if value.startswith("rom/"):
                assert ".." not in Path(value).parts
                assert (root / value).is_file(), value
    refs(scene)
print("PASS: phase-6 scenes, hierarchy, score, exits and relative assets")
