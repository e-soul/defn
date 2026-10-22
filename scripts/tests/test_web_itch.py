# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

import io
import sys
import tempfile
import unittest
import zipfile
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build_web


def write_export(directory):
    directory.mkdir(parents=True, exist_ok=True)
    library = build_web.web_library("release")
    for name in ("index.js", "index.wasm", "index.side.wasm", "index.pck", library,
                 "index.audio.worklet.js", "index.icon.png"):
        (directory / name).write_bytes(b"\x00exported bytes\xff")
    html = build_web.STANDALONE_SHELL + '<script>const config = ' + repr(library) + ';</script>'
    (directory / "index.html").write_text(html, encoding="utf-8")
    return html


class ItchPackageTests(unittest.TestCase):
    def test_archive_has_root_entry_point_and_preserves_pages_and_binary_payload(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "release"
            html = write_export(source)
            archive_path = root / build_web.ITCH_ARCHIVE
            with redirect_stdout(io.StringIO()):
                build_web.package_itch(source, archive_path)
            with zipfile.ZipFile(archive_path) as archive:
                self.assertEqual(set(archive.namelist()), {path.name for path in source.iterdir()})
                self.assertEqual(archive.read("index.html").decode(), html.replace(build_web.STANDALONE_SHELL, build_web.ITCH_SHELL))
                for path in source.iterdir():
                    if path.name != "index.html":
                        self.assertEqual(archive.read(path.name), path.read_bytes())
            self.assertEqual((source / "index.html").read_text(encoding="utf-8"), html)

    def test_incomplete_or_unrecognized_export_does_not_replace_previous_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "release"
            write_export(source)
            archive_path = root / build_web.ITCH_ARCHIVE
            archive_path.write_bytes(b"previous upload")
            (source / "index.side.wasm").unlink()
            with self.assertRaisesRegex(RuntimeError, "Missing or empty"):
                build_web.package_itch(source, archive_path)
            write_export(source)
            (source / "index.html").write_text(build_web.web_library("release"), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "host marker"):
                build_web.package_itch(source, archive_path)
            self.assertEqual(archive_path.read_bytes(), b"previous upload")

    def test_release_archive_uses_fresh_export_instead_of_old_destination_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            destination = root / "release"
            write_export(destination)
            (destination / "hud-test-fixture.html").write_text("not shipped", encoding="utf-8")

            def export(command):
                write_export(Path(command[-1]).parent)

            with patch("build_web.run_godot", side_effect=export), redirect_stdout(io.StringIO()):
                build_web.export_game("godot", root, "release")
            with zipfile.ZipFile(root / build_web.ITCH_ARCHIVE) as archive:
                self.assertNotIn("hud-test-fixture.html", archive.namelist())
                self.assertIn("index.side.wasm", archive.namelist())


if __name__ == "__main__":
    unittest.main()
