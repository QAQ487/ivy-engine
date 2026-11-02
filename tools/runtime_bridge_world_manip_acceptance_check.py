"""Unit checks for world-manip acceptance fixture (no live runtime)."""
import json
import pathlib
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent
FIXTURE = REPO / "testbed" / "rom" / "scene" / "scene_runtime_bridge_world_manip.json"
SCENE_ARG = "rom/scene/scene_runtime_bridge_world_manip.json"
OBJECT_NAME = "DynamicBox"


class WorldManipAcceptanceChecks(unittest.TestCase):
    def test_fixture_exists_and_names_dynamic_box(self):
        self.assertTrue(FIXTURE.is_file())
        data = json.loads(FIXTURE.read_text(encoding="utf-8"))
        names = [n.get("name") for n in data.get("nodes", [])]
        self.assertIn(OBJECT_NAME, names)
        self.assertIn("Ground", names)

    def test_stage_fixture_scene_assets_for_world_manip(self):
        from runtime_bridge_mouse_acceptance import collect_fixture_rom_paths, stage_fixture_scene_assets
        with tempfile.TemporaryDirectory() as tmp:
            package = pathlib.Path(tmp) / "pkg"
            package.mkdir()
            (package / "rom").mkdir()
            stage_fixture_scene_assets(package, FIXTURE)
            for rom_path in collect_fixture_rom_paths(json.loads(FIXTURE.read_text(encoding="utf-8"))):
                self.assertTrue(
                    (package / pathlib.Path(rom_path)).is_file()
                    or (rom_path.startswith("rom/") and (package / "resources" / rom_path[4:]).is_file()),
                    msg="missing staged asset %s" % rom_path,
                )

    def test_scene_arg_shape(self):
        self.assertTrue(SCENE_ARG.startswith("rom/scene/"))
        self.assertTrue(SCENE_ARG.endswith(".json"))
        self.assertNotIn("..", SCENE_ARG)


if __name__ == "__main__":
    unittest.main()
