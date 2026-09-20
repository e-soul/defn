# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

import io
import os
from pathlib import Path
import subprocess
import sys
import unittest
from contextlib import redirect_stdout
from unittest.mock import mock_open, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import ci_cache


class CacheKeyTests(unittest.TestCase):
    def setUp(self):
        self.arguments = {
            "platform": "linux",
            "architecture": "x86_64",
            "host_os": "Linux",
            "host_arch": "X64",
            "configuration": "coverage",
            "scons_version": "4.10.1",
            "toolchain": {"gcc": "14.2", "clang": "19.1"},
            "build_config": {"SConstruct": "build-policy-digest"},
            "godot_cpp_revision": "a" * 40,
            "revision": "b" * 40,
        }

    def test_revisions_save_new_entries_with_only_compatible_fallback(self):
        first = ci_cache.cache_keys(**self.arguments)
        second = ci_cache.cache_keys(**{**self.arguments, "revision": "c" * 40})
        self.assertNotEqual(first["key"], second["key"])
        self.assertEqual(first["restore-prefix"], second["restore-prefix"])
        self.assertEqual(first["key"], first["restore-prefix"] + "b" * 40)
        self.assertTrue(first["restore-prefix"].endswith("-"))
        self.assertEqual(first, ci_cache.cache_keys(**self.arguments))

    def test_every_compatibility_dimension_invalidates_fallback(self):
        original = ci_cache.cache_keys(**self.arguments)
        changes = {
            "platform": "windows",
            "architecture": "arm64",
            "host_os": "Windows",
            "host_arch": "ARM64",
            "configuration": "release",
            "scons_version": "4.11.0",
            "toolchain": {"gcc": "14.2", "clang": "20.1"},
            "build_config": {"SConstruct": "new-build-policy-digest"},
            "godot_cpp_revision": "d" * 40,
        }
        for name, value in changes.items():
            with self.subTest(name=name):
                changed = ci_cache.cache_keys(**{**self.arguments, name: value})
                self.assertNotEqual(original["restore-prefix"], changed["restore-prefix"])
                self.assertFalse(original["key"].startswith(changed["restore-prefix"]))

    def test_dictionary_order_does_not_change_compatibility(self):
        reordered = {**self.arguments, "toolchain": {"clang": "19.1", "gcc": "14.2"}}
        self.assertEqual(ci_cache.cache_keys(**self.arguments), ci_cache.cache_keys(**reordered))

    def test_web_pin_and_shared_build_configuration_invalidate_fallback(self):
        arguments = {
            **self.arguments, "platform": "web", "architecture": "wasm32",
            "configuration": "debug-release",
            "toolchain": {"emscripten": "4.0.20", "emsdk_revision": "a" * 40, "godot": "4.7.2"},
        }
        original = ci_cache.cache_keys(**arguments)
        for name in arguments["toolchain"]:
            with self.subTest(name=name):
                pin = {**arguments["toolchain"], name: "changed"}
                changed = ci_cache.cache_keys(**{**arguments, "toolchain": pin})
                self.assertNotEqual(original["restore-prefix"], changed["restore-prefix"])
        changed = ci_cache.cache_keys(**{**arguments, "build_config": {"build_tools": "new"}})
        self.assertNotEqual(original["restore-prefix"], changed["restore-prefix"])

    def test_missing_revision_or_unknown_configuration_fails_closed(self):
        for field, value in (
            ("revision", ""), ("godot_cpp_revision", "shortsha"),
            ("configuration", "unknown"),
        ):
            with self.subTest(field=field), self.assertRaises(ValueError):
                ci_cache.cache_keys(**{**self.arguments, field: value})

    def test_build_policy_hash_covers_shared_modules_but_not_game_sources(self):
        root = ci_cache.ROOT
        module = root / "defn" / "build_tools" / "example.py"
        with patch.object(Path, "rglob", return_value=[module]), \
             patch.object(Path, "read_bytes", return_value=b"build policy"):
            configuration = ci_cache.build_configuration(root)
        self.assertIn("defn/SConstruct", configuration)
        self.assertIn("defn/build_tools/example.py", configuration)
        self.assertIn("scripts/build_web.py", configuration)
        self.assertFalse(any("src/" in path for path in configuration))


class ToolchainTests(unittest.TestCase):
    @patch("ci_cache.shutil.which", return_value=None)
    def test_missing_tool_fails_closed(self, _which):
        with self.assertRaisesRegex(RuntimeError, "missing compiler"):
            ci_cache.tool_version("clang", {"PATH": ""})

    @patch("ci_cache.shutil.which", return_value="compiler")
    @patch("ci_cache.subprocess.run")
    def test_failed_version_probe_fails_closed(self, run, _which):
        run.return_value = subprocess.CompletedProcess([], 1, "probe failed")
        with self.assertRaisesRegex(RuntimeError, "Cannot fingerprint"):
            ci_cache.tool_version("clang", {})
        run.return_value = subprocess.CompletedProcess([], 0, "")
        with self.assertRaisesRegex(RuntimeError, "Cannot fingerprint"):
            ci_cache.tool_version("clang", {})

    @patch("ci_cache.shutil.which", return_value="cl.exe")
    @patch("ci_cache.subprocess.run")
    def test_msvc_banner_can_return_no_input_error(self, run, _which):
        run.return_value = subprocess.CompletedProcess([], 2, "Microsoft C/C++ 19.44\n")
        self.assertEqual(ci_cache.tool_version("cl", {}, banner=True), "Microsoft C/C++ 19.44")
        self.assertEqual(run.call_args.args[0], ["cl.exe"])

    @patch("ci_cache.tool_version", side_effect=lambda name, env: name + " version")
    def test_linux_coverage_fingerprints_gcc_and_llvm(self, version):
        identity = ci_cache.native_toolchain("linux", "x86_64", "coverage")
        self.assertEqual(set(identity), {"gcc", "g++", "ld", "clang", "clang++",
                                         "llvm-profdata", "llvm-cov"})
        version.reset_mock()
        ci_cache.native_toolchain("linux", "x86_64", "release")
        self.assertEqual([call.args[0] for call in version.call_args_list], ["gcc", "g++", "ld"])

    def test_cli_writes_github_outputs(self):
        arguments = [
            "--platform", "linux", "--architecture", "x86_64",
            "--configuration", "release", "--scons-version", "4.10.1",
            "--host-os", "Linux", "--host-arch", "X64", "--revision", "b" * 40,
        ]
        output = io.StringIO()
        file = mock_open()
        with patch.dict(os.environ, {"GITHUB_OUTPUT": "mock-output"}), \
             patch("ci_cache.subprocess.check_output", return_value="a" * 40 + "\n"), \
             patch("ci_cache.native_toolchain", return_value={"gcc": "14.2"}), \
             patch("ci_cache.build_configuration", return_value={"SConstruct": "digest"}), \
             patch("builtins.open", file), redirect_stdout(output):
            ci_cache.main(arguments)
        file.assert_called_once_with("mock-output", "a", encoding="utf-8")
        written = "".join(call.args[0] for call in file().write.call_args_list)
        self.assertEqual(written, output.getvalue())
        self.assertEqual(len(written.splitlines()), 2)

    def test_web_cli_also_fingerprints_the_native_import_extension(self):
        arguments = [
            "--platform", "web", "--architecture", "wasm32",
            "--configuration", "debug-release", "--scons-version", "4.10.1",
            "--host-os", "Linux", "--host-arch", "X64", "--revision", "b" * 40,
        ]
        with patch.dict(os.environ, {}, clear=True), \
             patch("ci_cache.subprocess.check_output", return_value="a" * 40), \
             patch("ci_cache.native_toolchain", return_value={"gcc": "14.2"}) as native, \
             patch("ci_cache.build_configuration", return_value={"SConstruct": "digest"}), \
             patch("ci_cache.cache_keys", return_value={}) as keys, \
             redirect_stdout(io.StringIO()):
            ci_cache.main(arguments)
        native.assert_called_once_with("linux", "x86_64", "release")
        self.assertEqual(keys.call_args.kwargs["toolchain"]["native"], {"gcc": "14.2"})
        self.assertIn("emscripten", keys.call_args.kwargs["toolchain"]["web_pin"])


if __name__ == "__main__":
    unittest.main()
