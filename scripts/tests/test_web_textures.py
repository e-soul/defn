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
from stage_web_project import TILE_BORDER, background_tiles, export_resources, stage_web_project


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
        self.sidecar.write_text('[remap]\nimporter="texture"\n[params]\ncompress/mode=0\nmipmaps/generate=false\nprocess/size_limit=0\n')

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

    def test_small_backgrounds_keep_their_original_bytes_and_import_settings(self):
        self.project()
        stage_web_project(self.source, self.stage, 32)
        for path in (self.resource[6:], self.resource[6:] + ".import"):
            self.assertEqual((self.source / path).read_bytes(), (self.stage / path).read_bytes())

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
        self.stage.mkdir()
        (self.stage / "unrelated").touch()
        with self.assertRaisesRegex(ValueError, "empty directory"):
            stage_web_project(self.source, self.stage)


if __name__ == "__main__":
    unittest.main()
