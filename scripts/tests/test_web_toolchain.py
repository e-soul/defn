# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

import json
import io
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build_web
import web_toolchain


class WebToolchainTests(unittest.TestCase):
    def setUp(self):
        self.pin = web_toolchain.load_toolchain()

    def test_pin_is_exact_and_matches_native_godot_version(self):
        from build import DEFAULT_GODOT_VERSION
        self.assertEqual(self.pin.emscripten, "4.0.20")
        self.assertEqual(self.pin.godot, DEFAULT_GODOT_VERSION)

    def test_rejects_floating_versions_and_sdk_revisions(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "pin.json"
            for key in ("emscripten", "godot", "emsdk_revision"):
                data = vars(self.pin).copy()
                data[key] = "latest"
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.subTest(key=key), self.assertRaises(ValueError):
                    web_toolchain.load_toolchain(path)

    @patch("web_toolchain.shutil.which", return_value=None)
    def test_missing_compiler_gives_setup_guidance(self, _which):
        with self.assertRaisesRegex(RuntimeError, "--setup"):
            web_toolchain.verify_emscripten(self.pin)

    @patch("web_toolchain.shutil.which", return_value="emcc")
    @patch("web_toolchain.subprocess.run")
    def test_checks_actual_compiler_banner_not_environment_claim(self, run, _which):
        for version in ("6.0.9", "4.0.200", "unrecognizable"):
            run.return_value.stdout = f"emcc (Emscripten gcc/clang-like replacement) {version}\n"
            with self.subTest(version=version), self.assertRaisesRegex(RuntimeError, "require Emscripten"):
                web_toolchain.verify_emscripten(self.pin)
        run.return_value.stdout = "emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 4.0.20 (hash)\n"
        self.assertEqual(web_toolchain.verify_emscripten(self.pin), "emcc")

    def test_local_environment_overrides_ambient_sdk_without_mutating_it(self):
        with tempfile.TemporaryDirectory(prefix="sdk with spaces ") as temporary:
            sdk = Path(temporary)
            (sdk / ".emscripten").touch()
            compiler = sdk / "upstream" / "emscripten" / ("emcc.bat" if os.name == "nt" else "emcc")
            compiler.parent.mkdir(parents=True)
            compiler.touch()
            with patch.dict(os.environ, {"EM_CONFIG": "wrong", "EM_CACHE": "wrong", "PATH": "ambient"}):
                env = web_toolchain.sdk_environment(sdk)
                self.assertEqual(env["EM_CONFIG"], str(sdk / ".emscripten"))
                self.assertEqual(env["EM_CACHE"], str(sdk / "upstream" / "emscripten" / "cache"))
                self.assertEqual(env["PATH"].split(os.pathsep)[:2],
                                 [str(sdk), str(sdk / "upstream" / "emscripten")])
                self.assertEqual(os.environ["EM_CONFIG"], "wrong")
                self.assertEqual(os.environ["PATH"], "ambient")

    def test_unactivated_sdk_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(RuntimeError, "--setup"):
                web_toolchain.sdk_environment(Path(temporary))

    def test_incomplete_sdk_cannot_fall_back_to_an_ambient_compiler(self):
        with tempfile.TemporaryDirectory() as temporary:
            sdk = Path(temporary)
            (sdk / ".emscripten").touch()
            with self.assertRaisesRegex(RuntimeError, "--setup"):
                web_toolchain.sdk_environment(sdk)

    def test_invalid_pin_shape_is_reported_explicitly(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "pin.json"
            for data in ([], {}, {**vars(self.pin), "godot": 4.7}):
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.subTest(data=data), self.assertRaises(ValueError):
                    web_toolchain.load_toolchain(path)

    @patch("web_toolchain.subprocess.run")
    def test_foreign_sdk_checkout_is_not_reset(self, run):
        run.return_value.stdout = "0" * 40
        with tempfile.TemporaryDirectory() as temporary:
            sdk = Path(temporary)
            (sdk / "emsdk.py").touch()
            with self.assertRaisesRegex(RuntimeError, "will not reset"):
                web_toolchain.verify_sdk_revision(sdk, self.pin)
        self.assertEqual(run.call_count, 1)

    @patch("build_web.subprocess.run")
    def test_godot_version_must_match_stable_pin(self, run):
        for version in ("4.8.stable.official", "4.7.2.dev.official", "4.7.20.stable.official"):
            run.return_value.stdout = version
            with self.subTest(version=version), self.assertRaisesRegex(RuntimeError, "require Godot"):
                build_web.verify_godot("godot", self.pin)
        run.return_value.stdout = self.pin.godot + ".stable.official.hash\n"
        build_web.verify_godot("godot", self.pin)

    @patch("build_web.subprocess.run")
    def test_godot_errors_fail_even_with_zero_exit_code(self, run):
        run.return_value = subprocess.CompletedProcess([], 0, "Import complete\n", "ERROR: Missing class\n")
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, "reported errors"):
                build_web.run_godot(["godot", "--import"])

    def test_export_requires_all_wasm_parts_and_resolved_shell(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for mode in ("debug", "release"):
                library = build_web.web_library(mode)
                for name in ("index.html", "index.js", "index.wasm", "index.side.wasm", "index.pck", library):
                    (root / name).write_text("artifact", encoding="utf-8")
                (root / "index.html").write_text(library, encoding="utf-8")
                build_web.verify_export(root, mode)
                (root / "index.side.wasm").unlink()
                with self.assertRaisesRegex(RuntimeError, "index.side.wasm"):
                    build_web.verify_export(root, mode)
                (root / "index.side.wasm").write_text("wasm", encoding="utf-8")
                (root / "index.html").write_text("$GODOT_CONFIG", encoding="utf-8")
                with self.assertRaisesRegex(RuntimeError, "unresolved"):
                    build_web.verify_export(root, mode)

    @patch("build_web.run_godot")
    def test_stale_export_cannot_mask_missing_fresh_artifacts(self, run):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "release").mkdir()
            sentinel = root / "release" / "index.html"
            sentinel.write_text("previous successful export", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "Missing or empty"):
                build_web.export_game("godot", root, "release")
            self.assertEqual(sentinel.read_text(), "previous successful export")
            self.assertEqual(list(root.iterdir()), [root / "release"])

    @patch("build_web.verify_emscripten")
    @patch("build_web.sdk_environment")
    @patch("build_web.verify_sdk_revision")
    @patch("build_web.run")
    def test_setup_uses_pinned_revision_and_no_global_activation(self, run, revision, environment, compiler):
        with tempfile.TemporaryDirectory() as temporary:
            sdk = Path(temporary) / "new-sdk"
            with redirect_stdout(io.StringIO()):
                build_web.setup_sdk(sdk, self.pin)
            commands = [call.args[0] for call in run.call_args_list]
            self.assertIn(["git", "-C", str(sdk), "fetch", "--depth", "1", "origin", self.pin.emsdk_revision], commands)
            self.assertEqual(commands[-1][-2:], ["activate", self.pin.emscripten])
            self.assertNotIn("--permanent", sum(commands, []))
            revision.assert_called_once_with(sdk, self.pin)
            compiler.assert_called_once_with(self.pin, environment.return_value)

    def test_direct_scons_rejects_a_newer_compiler_before_building(self):
        with tempfile.TemporaryDirectory(prefix="fake compiler ") as temporary:
            compiler = Path(temporary) / ("emcc.bat" if os.name == "nt" else "emcc")
            prefix = "@echo off\n" if os.name == "nt" else "#!/bin/sh\n"
            compiler.write_text(prefix + "echo emcc Emscripten 6.0.9\n", encoding="utf-8")
            compiler.chmod(0o755)
            env = os.environ.copy()
            env["PATH"] = temporary + os.pathsep + env.get("PATH", "")
            result = subprocess.run(
                [sys.executable, "-m", "SCons", "platform=web", "-n", "-j1"],
                cwd=build_web.PROJECT_DIR, env=env, capture_output=True, text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("require Emscripten 4.0.20", result.stdout)
            self.assertIn("6.0.9", result.stdout)
            self.assertNotIn("Building targets", result.stdout)

    @patch("build_web.verify_sdk_revision")
    @patch("build_web.sdk_environment", return_value={"PATH": "pinned"})
    @patch("build_web.verify_emscripten", return_value="pinned/emcc")
    @patch("build_web.run")
    @patch("build_web.resolve_godot_executable")
    def test_build_only_passes_pinned_environment_and_never_invokes_godot(self, godot, run, compiler, environment, revision):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            (project / "bin").mkdir()
            for mode in ("debug", "release"):
                (project / "bin" / build_web.web_library(mode)).touch()
            with patch.object(build_web, "PROJECT_DIR", project), redirect_stdout(io.StringIO()):
                self.assertEqual(build_web.main(["--build-only", "--target", "both", "--jobs", "2"]), 0)
            godot.assert_not_called()
            self.assertEqual(run.call_count, 2)
            for call, mode in zip(run.call_args_list, ("debug", "release")):
                self.assertIn("platform=web", call.args[0])
                self.assertIn(f"target=template_{mode}", call.args[0])
                self.assertIn("-j2", call.args[0])
                self.assertEqual(call.kwargs["env"], environment.return_value)


if __name__ == "__main__":
    unittest.main()
