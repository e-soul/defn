# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from stage_web_project import stage_web_project


def snapshot(directory):
    return {path.relative_to(directory): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in directory.rglob("*") if path.is_file()}


class StagedMusicTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.source, self.stage = root / "source", root / "stage"
        for name in ("assets/music", "assets/sfx", "data", "bin", "export_templates"):
            (self.source / name).mkdir(parents=True)
        (self.source / "project.godot").write_text("config_version=5\n")
        (self.source / "defn_core.gdextension").write_text("[configuration]\n")
        (self.source / "data/unit_data.json").write_text('{"units": {}}')
        self.track = "res://assets/music/theme.mp3"
        self.playlist = "res://data/music_playlist.json"
        self.sfx = "res://assets/sfx/click.mp3"
        self.unused = "res://assets/music/unused.mp3"
        (self.source / self.playlist[6:]).write_text(json.dumps({"tracks": [self.track], "volume_linear": 0.36}))
        for resource in (self.track, self.sfx, self.unused):
            (self.source / resource[6:]).write_bytes(b"original audio")
            (self.source / (resource[6:] + ".import")).write_text(
                '[remap]\nimporter="mp3"\ntype="AudioStreamMP3"\n[params]\nloop=false\nloop_offset=2\n',
            )
        self.selected = [self.playlist, self.track, self.sfx]
        self.write_presets()

    def write_presets(self):
        (self.source / "export_presets.cfg").write_text(
            '[preset.0]\nname="native"\nexport_files=PackedStringArray("' + self.track + '")\n\n'
            '[preset.1]\nname="defn_web_release"\nexport_files=PackedStringArray('
            + ", ".join(json.dumps(resource) for resource in self.selected) + ")\n",
        )

    def stage_project(self, **kwargs):
        with redirect_stdout(StringIO()):
            return stage_web_project(self.source, self.stage, **kwargs)

    @staticmethod
    def encode(command, **kwargs):
        Path(command[-1]).write_bytes(b"web MP3")
        return subprocess.CompletedProcess(command, 0, "", "")

    @patch("stage_web_project.shutil.which", return_value="/tools/ffmpeg")
    @patch("stage_web_project.subprocess.run", side_effect=encode)
    def test_default_encodes_only_playlist_tracks_and_preserves_sources_paths_and_imports(self, run, which):
        original = snapshot(self.source)
        report = self.stage_project()
        self.assertEqual(snapshot(self.source), original)
        self.assertEqual((self.stage / self.track[6:]).read_bytes(), b"web MP3")
        self.assertEqual((self.stage / self.sfx[6:]).read_bytes(), b"original audio")
        self.assertFalse((self.stage / self.unused[6:]).exists())
        for resource in (self.track + ".import", self.sfx + ".import", self.playlist, "res://export_presets.cfg"):
            self.assertEqual((self.stage / resource[6:]).read_bytes(), (self.source / resource[6:]).read_bytes())
        self.assertEqual(report["music_bitrate_kbps"], 128)
        self.assertEqual(report["music"], {self.track: {"source_bytes": 14, "web_bytes": 7}})
        run.assert_called_once()
        command = run.call_args.args[0]
        self.assertEqual(command[0], "/tools/ffmpeg")
        self.assertEqual(command[command.index("-i") + 1], str(self.source / self.track[6:]))
        self.assertEqual(command[command.index("-b:a") + 1], "128k")
        self.assertEqual(command[command.index("-c:a") + 1], "libmp3lame")
        self.assertNotIn("-ar", command)
        self.assertNotIn("-ac", command)

    @patch("stage_web_project.shutil.which", return_value="custom-ffmpeg")
    @patch("stage_web_project.subprocess.run", side_effect=encode)
    def test_bitrate_and_executable_can_be_overridden(self, run, which):
        report = self.stage_project(music_bitrate=160, ffmpeg_exe="/custom/ffmpeg")
        which.assert_called_once_with("/custom/ffmpeg")
        command = run.call_args.args[0]
        self.assertEqual(command[command.index("-b:a") + 1], "160k")
        self.assertEqual(report["music_bitrate_kbps"], 160)

    @patch("stage_web_project.shutil.which")
    @patch("stage_web_project.subprocess.run")
    def test_original_music_needs_no_ffmpeg_and_copies_original_bytes(self, run, which):
        report = self.stage_project(music_bitrate=None)
        which.assert_not_called()
        run.assert_not_called()
        self.assertEqual((self.stage / self.track[6:]).read_bytes(), b"original audio")
        self.assertIsNone(report["music_bitrate_kbps"])

    @patch("stage_web_project.shutil.which")
    def test_projects_without_selected_playlists_need_no_ffmpeg(self, which):
        self.selected.remove(self.playlist)
        self.write_presets()
        self.stage_project()
        which.assert_not_called()
        self.assertEqual((self.stage / self.track[6:]).read_bytes(), b"original audio")

    @patch("stage_web_project.shutil.which", return_value=None)
    def test_missing_ffmpeg_fails_with_guidance_and_preserves_sources(self, which):
        original = snapshot(self.source)
        with self.assertRaisesRegex(RuntimeError, "--original-music"):
            self.stage_project()
        self.assertEqual(snapshot(self.source), original)

    @patch("stage_web_project.shutil.which", return_value="ffmpeg")
    @patch("stage_web_project.subprocess.run")
    def test_failed_or_empty_encode_is_rejected_and_preserves_sources(self, run, which):
        original = snapshot(self.source)
        for status in (1, 0):
            run.return_value = subprocess.CompletedProcess([], status, "", "Unknown encoder libmp3lame")
            self.stage = self.stage.parent / f"failure-{status}"
            with self.subTest(status=status), self.assertRaisesRegex(RuntimeError, "theme.mp3.*libmp3lame"):
                self.stage_project()
        self.assertEqual(snapshot(self.source), original)

    def test_invalid_bitrate_and_unselected_or_non_mp3_tracks_are_rejected(self):
        for bitrate in (0, -1, 129, 321):
            with self.subTest(bitrate=bitrate), self.assertRaisesRegex(ValueError, "bitrate"):
                self.stage_project(music_bitrate=bitrate)
        for track in (self.unused, "res://assets/music/theme.ogg"):
            (self.source / self.playlist[6:]).write_text(json.dumps({"tracks": [track]}))
            with self.subTest(track=track), self.assertRaisesRegex(ValueError, "selected MP3"):
                self.stage_project()


if __name__ == "__main__":
    unittest.main()
