# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

import hashlib
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from stage_web_project import DEFAULT_ART_QUALITY, TILE_BORDER, background_tiles, export_resources, stage_web_project


def patterned_image(size):
    image = Image.new("RGBA", size)
    for y in range(size[1]):
        for x in range(size[0]):
            image.putpixel((x, y), ((x * 17) % 256, (y * 29) % 256, (x + y * 7) % 256, (x * 13 + y) % 256))
    return image


def snapshot(directory):
    return {p.relative_to(directory): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in directory.rglob("*") if p.is_file()}


class StagedTextureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.source, self.stage = root / "source", root / "stage"
        self.source.mkdir()
        self.image = patterned_image((13, 9))
        self.resource = "res://assets/backgrounds/test/layer.png"
        original = self.source / self.resource[6:]
        original.parent.mkdir(parents=True)
        self.image.save(original)
        self.sidecar = original.with_suffix(".png.import")
        self.sidecar.write_text('[remap]\nimporter="texture"\n[params]\ncompress/mode=0\n'
                                'compress/lossy_quality=0.7\nmipmaps/generate=false\n'
                                'process/fix_alpha_border=true\nprocess/size_limit=0\n')

    def test_tiles_preserve_every_pixel_and_the_original_dimensions(self):
        original = snapshot(self.source)
        scene, textures = background_tiles(self.source, self.stage, self.resource, 8)
        text = (self.stage / scene[6:]).read_text()
        self.assertIn("metadata/source_size = Vector2(13, 9)", text)
        regions = re.findall(r'position = Vector2\((\d+), (\d+)\).*?region_rect = Rect2\(2, 2, (\d+), (\d+)\)', text, re.S)
        stitched = Image.new("RGBA", self.image.size)
        for path, region in zip(textures, regions, strict=True):
            x, y, width, height = map(int, region)
            with Image.open(self.stage / path[6:]) as tile:
                self.assertLessEqual(max(tile.size), 8)
                stitched.paste(tile.crop((2, 2, 2 + width, 2 + height)), (x, y))
                for ty in range(tile.height):
                    for tx in range(tile.width):
                        source_pixel = self.image.getpixel(((x + tx - TILE_BORDER) % self.image.width,
                                                           min(self.image.height - 1, max(0, y + ty - TILE_BORDER))))
                        self.assertEqual(tile.getpixel((tx, ty)), source_pixel)
        self.assertEqual(stitched.tobytes(), self.image.tobytes())
        self.assertEqual(snapshot(self.source), original)

    def project(self):
        for name in ("bin", "export_templates", "data"):
            (self.source / name).mkdir()
        (self.source / "project.godot").write_text('config_version=5\n')
        (self.source / "defn_core.gdextension").write_text('[configuration]\nentry_symbol="test"\n')
        (self.source / "export_templates/web_shell.html").write_text("shell")
        (self.source / "bin/test.dll").write_bytes(b"library")
        (self.source / "data/level.json").write_text(json.dumps({"background": self.resource}))
        (self.source / "data/unit_data.json").write_text('{"units": {}}')
        (self.source / "export_presets.cfg").write_text(
            '[preset.0]\nname="native"\nexport_files=PackedStringArray("' + self.resource + '")\n\n'
            '[preset.1]\nname="defn_web_release"\nexport_files=PackedStringArray("' + self.resource + '", "res://data/level.json")\n\n'
            '[preset.1.options]\nhtml/custom_html_shell="res://export_templates/web_shell.html"\n'
        )

    def test_staging_rewrites_only_web_resources_and_never_copies_the_native_import_cache(self):
        self.project()
        (self.source / ".godot").mkdir()
        (self.source / ".godot/native.ctex").write_bytes(b"native cache")
        original = snapshot(self.source)
        report = stage_web_project(self.source, self.stage, 8)
        self.assertEqual(snapshot(self.source), original)
        self.assertFalse((self.stage / ".godot").exists())
        self.assertFalse((self.stage / self.resource[6:]).exists())
        self.assertFalse((self.stage / (self.resource[6:] + ".import")).exists())
        scene = report["backgrounds"][self.resource]
        self.assertEqual(json.loads((self.stage / "data/level.json").read_text())["background"], scene)
        presets = (self.stage / "export_presets.cfg").read_text()
        self.assertIn('name="native"\nexport_files=PackedStringArray("' + self.resource + '")', presets)
        _, shipped = export_resources(presets, "defn_web_release")
        self.assertNotIn(self.resource, shipped)
        self.assertIn(scene, shipped)
        self.assertEqual(report["tile_count"], 12)
        for resource in shipped:
            self.assertTrue((self.stage / resource[6:]).is_file(), resource)

    def test_lossless_comparison_keeps_original_bytes_and_import_settings(self):
        self.project()
        stage_web_project(self.source, self.stage, 32, art_quality=None)
        for path in (self.resource[6:], self.resource[6:] + ".import"):
            self.assertEqual((self.source / path).read_bytes(), (self.stage / path).read_bytes())

    def test_compression_is_scoped_to_art_and_keeps_source_pixels_and_other_import_options(self):
        self.project()
        resources = ["assets/campaign/preview.jpg", "assets/ui/button.png", "assets/units/soldier.png"]
        for relative in resources:
            path = self.source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            self.image.convert("RGB").save(path)
            path.with_suffix(path.suffix + ".import").write_bytes(self.sidecar.read_bytes())
        presets = self.source / "export_presets.cfg"
        presets.write_text(presets.read_text().replace(
            '"res://data/level.json")',
            '"res://data/level.json", ' + ", ".join(json.dumps("res://" + p) for p in resources) + ")",
        ))
        original = snapshot(self.source)
        report = stage_web_project(self.source, self.stage, 32)
        self.assertEqual(report["art_quality"], DEFAULT_ART_QUALITY)
        self.assertEqual(snapshot(self.source), original)
        for relative in [self.resource[6:], *resources]:
            self.assertEqual((self.source / relative).read_bytes(), (self.stage / relative).read_bytes())
            settings = (self.stage / (relative + ".import")).read_text()
            if relative.startswith(("assets/backgrounds/", "assets/campaign/")):
                self.assertIn("compress/mode=1", settings)
                self.assertIn("compress/lossy_quality=0.8", settings)
                self.assertEqual(settings.count("compress/lossy_quality="), 1)
                self.assertIn("mipmaps/generate=false", settings)
                self.assertIn("process/fix_alpha_border=true", settings)
                self.assertIn("process/size_limit=0", settings)
            else:
                self.assertEqual((self.source / (relative + ".import")).read_bytes(),
                                 (self.stage / (relative + ".import")).read_bytes())

    def test_generated_tiles_use_the_same_quality_without_resizing(self):
        self.project()
        self.sidecar.write_text(self.sidecar.read_text().replace("compress/lossy_quality=0.7\n", ""))
        stage_web_project(self.source, self.stage, 8, art_quality=0.98)
        imports = list((self.stage / "generated").rglob("*.import"))
        self.assertEqual(len(imports), 12)
        for path in imports:
            self.assertIn("compress/mode=1", path.read_text())
            self.assertIn("compress/lossy_quality=0.98", path.read_text())
        self.assertIn("metadata/source_size = Vector2(13, 9)",
                      next((self.stage / "generated").rglob("*.tscn")).read_text())

    def test_sprite_quality_uses_catalog_frames_and_is_independent_of_art_and_ui(self):
        self.project()
        resources = ["assets/units/Idle__000.png", "assets/units/Idle__001.png", "assets/tower.png",
                     "assets/muzzle/Flash__000.png", "assets/projectile.png", "assets/ui/button.png"]
        for relative in resources:
            path = self.source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            self.image.save(path)
            path.with_suffix(".png.import").write_bytes(self.sidecar.read_bytes())
        catalog = {"units": {"soldier": {
            "animations": {"idle": {"path_template": "res://assets/units/Idle__%03d.png", "frame_count": 2}},
            "muzzle_flash": {"path_template": "res://assets/muzzle/Flash__%03d.png"},
            "projectile": {"path": "res://assets/projectile.png"}},
            "base": {"animations": {"idle": {"path_template": "res://assets/tower.png", "frame_count": 1}}}}}
        (self.source / "data/unit_data.json").write_text(json.dumps(catalog))
        presets = self.source / "export_presets.cfg"
        presets.write_text(presets.read_text().replace(
            '"res://data/level.json")',
            '"res://data/level.json", ' + ", ".join(json.dumps("res://" + p) for p in resources) + ")",
        ))
        original = snapshot(self.source)
        report = stage_web_project(self.source, self.stage, 32, art_quality=None)
        self.assertEqual(snapshot(self.source), original)
        self.assertIsNone(report["art_quality"])
        self.assertEqual(report["sprite_quality"], 0.8)
        for relative in resources[:-1]:
            self.assertEqual((self.source / relative).read_bytes(), (self.stage / relative).read_bytes())
            settings = (self.stage / (relative + ".import")).read_text()
            self.assertIn("compress/mode=1", settings)
            self.assertIn("compress/lossy_quality=0.8", settings)
            self.assertIn("process/fix_alpha_border=true", settings)
        for relative in (self.resource[6:], resources[-1]):
            self.assertEqual((self.source / (relative + ".import")).read_bytes(),
                             (self.stage / (relative + ".import")).read_bytes())
        self.assertFalse((self.stage / "assets/muzzle/Flash__001.png").exists())
        authored = self.stage.with_name("authored")
        stage_web_project(self.source, authored, 32, art_quality=None, sprite_quality=None)
        for relative in resources:
            self.assertEqual((self.source / (relative + ".import")).read_bytes(),
                             (authored / (relative + ".import")).read_bytes())

    def test_authored_nonlossless_modes_are_preserved(self):
        self.project()
        self.sidecar.write_text(self.sidecar.read_text().replace("compress/mode=0", "compress/mode=2"))
        stage_web_project(self.source, self.stage, 32)
        self.assertEqual((self.stage / (self.resource[6:] + ".import")).read_text(), self.sidecar.read_text())

    def test_failure_cannot_modify_the_source_project(self):
        self.project()
        self.sidecar.write_text(self.sidecar.read_text().replace("mipmaps/generate=false", "mipmaps/generate=true"))
        original = snapshot(self.source)
        with self.assertRaisesRegex(ValueError, "non-mipmapped"):
            stage_web_project(self.source, self.stage, 8)
        self.assertEqual(snapshot(self.source), original)

    def test_rejects_source_overlap_nonempty_staging_and_invalid_limits(self):
        self.project()
        for stage in (self.source, self.source / "stage", self.source.parent):
            with self.subTest(stage=stage), self.assertRaises(ValueError):
                stage_web_project(self.source, stage)
        for limit in (0, 4, 4097):
            with self.subTest(limit=limit), self.assertRaises(ValueError):
                stage_web_project(self.source, self.stage, limit)
        for quality in (0, -1, 1.01, float("nan")):
            with self.subTest(quality=quality), self.assertRaisesRegex(ValueError, "quality"):
                stage_web_project(self.source, self.stage, art_quality=quality)
            with self.subTest(sprite_quality=quality), self.assertRaisesRegex(ValueError, "sprite quality"):
                stage_web_project(self.source, self.stage, sprite_quality=quality)
        self.stage.mkdir()
        (self.stage / "unrelated").touch()
        with self.assertRaisesRegex(ValueError, "empty directory"):
            stage_web_project(self.source, self.stage)


if __name__ == "__main__":
    unittest.main()
