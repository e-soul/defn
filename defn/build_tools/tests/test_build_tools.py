from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import unittest
import uuid
from unittest.mock import patch

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT))

from build_tools import sources
from build_tools.configuration import (
    disable_compilation_database_emitters, native_environment, stable_hash,
    validate_extension_options, variant_identity,
)
from build_tools.quality import normalize_command, run_tidy
from build_tools.runners import RUNNERS, boolean, runner_arguments, validate_options
from build_tools.selection import select
from build_tools.targets import link_options, stage_action, stage_file


class BuildConfigurationTests(unittest.TestCase):
    def test_variant_paths_expand_compiler_indirection(self):
        from SCons.Script import Environment
        env = Environment(tools=[], ENV=os.environ.copy(), CC="mock-cl", CXX="$CC")
        for platform, arch in (("windows", "x86_64"), ("linux", "x86_64"), ("web", "wasm32")):
            with self.subTest(platform=platform):
                env.Replace(platform=platform, arch=arch, target="template_debug")
                root = variant_identity(env, "hosted")
                self.assertEqual(root.parts[:3], ("build", f"{platform}-{arch}", "template_debug"))
                self.assertEqual(len(root.parts), 5)
                self.assertEqual(root.name, "hosted")
                self.assertNotIn("$", str(root))
                self.assertTrue(root.parent.name.startswith("mock-cl-"))

    def test_native_rejects_foreign_architecture(self):
        with patch("build_tools.configuration.platform.machine", return_value="AMD64"):
            with self.assertRaisesRegex(ValueError, "host-compatible"):
                native_environment({"arch": "arm64"})

    @unittest.skipUnless(os.name == "nt", "MSVC runtime selection")
    def test_native_debug_runtime_respects_static_selection(self):
        static = native_environment({"debug_crt": "yes", "use_static_cpp": "yes"})
        dynamic = native_environment({"debug_crt": "yes", "use_static_cpp": "no"})
        self.assertIn("/MTd", static["CCFLAGS"])
        self.assertIn("/MDd", dynamic["CCFLAGS"])
        self.assertNotEqual(variant_identity(static, "native-tests"),
                            variant_identity(dynamic, "native-tests"))

    @unittest.skipUnless(os.name == "nt", "Windows LLVM linker selection")
    def test_native_llvm_lto_uses_compatible_linker(self):
        env = native_environment({"use_llvm": "yes", "lto": "thin"})
        self.assertEqual(env["LINK"], "lld-link")
        self.assertIn("-flto=thin", env["CCFLAGS"])
        self.assertNotIn("-flto=thin", env.get("LINKFLAGS", []))

    def test_optional_database_keeps_compiler_and_web_dependency_emitters(self):
        from types import SimpleNamespace
        from SCons.Builder import ListEmitter
        calls = []

        def compiler(target, source, env):
            calls.append("compiler")
            return target, source

        def web_pin(target, source, env):
            calls.append("web")
            return target, source

        def emit_compilation_db_entry(target, source, env):
            calls.append("database")
            return target, source

        env = {"BUILDERS": {
            name: SimpleNamespace(emitter={".cpp": ListEmitter([
                ListEmitter([compiler, web_pin]), emit_compilation_db_entry,
            ])}) for name in ("StaticObject", "SharedObject")
        }}
        disable_compilation_database_emitters(env)
        for builder in env["BUILDERS"].values():
            builder.emitter[".cpp"]([], [], env)
        self.assertEqual(calls, ["compiler", "web", "compiler", "web"])

    def test_lazy_selection(self):
        for names in (["test"], ["unit_tests"], ["coverage_native"], ["packaging"]):
            selection = select(names)
            self.assertFalse(selection.extension, names)
            self.assertFalse(selection.hosted_coverage, names)
        ordinary = select([])
        self.assertTrue(ordinary.extension)
        self.assertFalse(ordinary.native)
        self.assertFalse(ordinary.native_coverage)
        self.assertFalse(ordinary.hosted)
        self.assertEqual(select(["test_all"]).runners, {"hosted_test", "conformance"})
        self.assertTrue(select(["coverage"]).native_coverage)
        self.assertTrue(select(["coverage"]).hosted_coverage)
        self.assertFalse(select(["unit_tests", "compiledb"]).extension)
        self.assertFalse(select(["unit_tests"], hosted=True).extension)
        with self.assertRaises(ValueError):
            select(["typo"])
        with self.assertRaises(ValueError):
            select(["unit_tests"], tidy=True)

    def test_runner_validation_and_translation(self):
        validate_options({"seeds": "5", "spacing": "12.5"}, {"matrix"})
        self.assertEqual(runner_arguments("matrix", {"spacing": "12.5"}), ["--friendly-spacing", "12.5"])
        self.assertEqual(runner_arguments("endless", {"dmg": "1,2", "supply": "8"}),
                         ["--hostile-damage-growth", "1,2", "--supply-cap", "8"])
        self.assertEqual(RUNNERS["conformance"].engine_args, ("--fixed-fps", "60"))
        for arguments, selected in [
            ({"seeds": "0"}, {"sim"}), ({"seeds": "x"}, {"sim"}),
            ({"spec": "a"}, {"sim"}), ({"seeds": "2"}, set()),
            ({"bisect": "maybe"}, {"sim"}), ({"dmg": "nan"}, {"endless"}),
            ({"interval": "1,"}, {"endless"}),
            ({"out": "shared"}, {"matrix", "balance"}),
        ]:
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                validate_options(arguments, selected)
        with self.assertRaises(ValueError):
            boolean("typo")

    def test_inventory_is_explicit_unique_and_engine_neutral(self):
        groups = [
            sources.CORE, sources.PRESENTERS, sources.ENGINE, sources.DEBUG_RENDERING,
            sources.SIMULATION, sources.HOSTED_RUNNERS, sources.SHARED_TESTS,
            sources.HOSTED_TESTS, sources.NATIVE_TESTS,
        ]
        inventory = sum(groups, [])
        self.assertEqual(len(inventory), len(set(inventory)))
        self.assertTrue(all((PROJECT / src).is_file() for src in inventory))
        self.assertTrue(set(sources.SHARED_TESTS).issubset(sources.NATIVE))
        self.assertFalse(set(sources.SIMULATION + sources.HOSTED_RUNNERS + sources.HOSTED_TESTS)
                         .intersection(sources.extension_sources()))
        self.assertNotIn(sources.DEBUG_RENDERING[0], sources.extension_sources())
        self.assertIn(sources.DEBUG_RENDERING[0], sources.extension_sources(debug=True))
        self.assertFalse(any("godot" in path or "json" in path for path in sources.include_paths(native=True)))

    def test_native_environment_has_no_godot_state(self):
        env = native_environment({})
        self.assertFalse(env.get("LIBS"))
        self.assertFalse(any("godot" in str(path) for path in env["CPPPATH"]))
        self.assertFalse(any("GDEXTENSION" in str(define) for define in env.get("CPPDEFINES", [])))
        self.assertNotIn("DEFN_HOSTED_TESTS_ENABLED", env.get("CPPDEFINES", []))
        original = variant_identity(env, "native-tests")
        changed = env.Clone()
        changed.Append(CPPDEFINES=["OTHER_ABI"])
        self.assertNotEqual(original, variant_identity(changed, "native-tests"))
        self.assertNotEqual(original, variant_identity(env, "native-coverage"))
        self.assertEqual(original, variant_identity(env.Clone(), "native-tests"))

    def test_hash_and_normalization(self):
        self.assertEqual(stable_hash({"b": 2, "a": 1}), stable_hash({"a": 1, "b": 2}))
        self.assertEqual(normalize_command("c++ -fno-gnu-unique -std=c++23 a.cpp"),
                         "c++ -std=c++23 a.cpp")
        for arguments in ({"typo": "yes"}, {"CCFLAGS": "silently-ignored"}):
            with self.assertRaises(ValueError):
                validate_extension_options(arguments)
        validate_extension_options({"platform": "web", "threads": "no", "use_llvm": "yes"})


class WorkspaceTest(unittest.TestCase):
    def setUp(self):
        self.root = PROJECT / "build" / "build_tools_tests" / uuid.uuid4().hex
        self.root.mkdir(parents=True)

    def tearDown(self):
        shutil.rmtree(self.root)

    def write(self, path, content):
        path = self.root / path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def scons(self, *arguments, success=True):
        result = subprocess.run([sys.executable, "-m", "SCons", "-Q", *arguments],
                                cwd=self.root, capture_output=True, text=True)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def fake_tool(self, name, body):
        script = self.write(name + ".py", body)
        if os.name == "nt":
            return self.write(name + ".cmd", f'@"{sys.executable}" "{script}" %*\n')
        launcher = self.write(name, f'#!{sys.executable}\n' + body)
        launcher.chmod(0o755)
        return launcher


class CacheEnvironmentTests(WorkspaceTest):
    def test_native_cache_initializes_missing_parents_and_preserves_existing_data(self):
        cache = self.root / "fresh-checkout" / "build" / ".scons-cache"
        self.assertFalse(cache.parent.exists())
        with patch.dict(os.environ, {"SCONS_CACHE": str(cache)}):
            native_environment({})
            self.assertTrue((cache / "config").is_file())
            marker = cache / "existing-object"
            marker.write_text("cached object")
            native_environment({})
            self.assertEqual(marker.read_text(), "cached object")


class StagingTests(WorkspaceTest):
    def test_windows_symbols_match_binary_and_are_removed_when_disabled(self):
        binary = self.write("variant/library.dll", "binary")
        symbols = self.write("variant/library.pdb", "symbols")
        staged_binary = self.root / "bin/library.dll"
        staged_symbols = self.root / "bin/library.pdb"
        self.assertEqual(link_options({"is_msvc": True, "debug_symbols": True}, binary),
                         {"PDB": str(symbols)})
        stage_action([staged_binary, staged_symbols], [binary, symbols], {})
        self.assertEqual(staged_symbols.read_text(), "symbols")
        stage_action([staged_binary], [binary], {"DEFN_REMOVE_STALE_PDB": True})
        self.assertFalse(staged_symbols.exists())
        self.assertEqual(staged_binary.read_text(), "binary")

    def test_switching_variants_checks_bytes_and_preserves_unchanged_mtime(self):
        first = self.write("first.dll", "first")
        second = self.write("second.dll", "other")
        staged = self.root / "bin" / "library.dll"
        self.assertTrue(stage_file(staged, first))
        before = staged.stat().st_mtime_ns
        self.assertFalse(stage_file(staged, first))
        self.assertEqual(staged.stat().st_mtime_ns, before)
        self.assertTrue(stage_file(staged, second))
        self.assertTrue(stage_file(staged, first))
        self.assertEqual(staged.read_text(), "first")


class TidyGraphTests(WorkspaceTest):
    def test_missing_database_fails_without_falling_back(self):
        stamp = self.write("success.stamp", "stale success")
        with patch("build_tools.quality.subprocess.run") as runner:
            result = run_tidy([stamp], [], {"DEFN_TIDY_DATABASE": str(self.root / "missing")})
        self.assertEqual(result, 1)
        self.assertFalse(stamp.exists())
        runner.assert_not_called()

    def test_persistent_transitive_dependencies_signatures_and_retry(self):
        tool = self.fake_tool("tidy", """
from pathlib import Path
import json, sys
if "--version" in sys.argv:
    print(Path("version").read_text())
    sys.exit(0)
database = Path(sys.argv[sys.argv.index("-p") + 1]) / "compile_commands.json"
entries = json.loads(database.read_text())
assert len(entries) == 1 and entries[0]["file"] == sys.argv[-1]
name = Path(sys.argv[-1]).name
with Path("calls").open("a") as calls:
    calls.write(name + "\\n")
sys.exit(1 if Path("fail").exists() and name == "a.cpp" else 0)
""")
        self.write("version", "test-tidy 1")
        self.write(".clang-tidy", "Checks: '*'\n")
        self.write("a.cpp", '#include "a.h"\n')
        self.write("b.cpp", '#include "b.h"\n')
        self.write("c.cpp", "")
        self.write("include/a.h", '#include "shared.h"\n')
        self.write("include/shared.h", "struct A {};\n")
        self.write("include/b.h", "struct B {};\n")
        self.write("SConstruct", f"""
import sys
from pathlib import Path
sys.path.insert(0, {str(PROJECT)!r})
from build_tools import quality
quality.shutil.which = lambda *a, **kw: {str(tool)!r}
env = Environment(tools=[], ENV=__import__('os').environ.copy(), CPPPATH=['include'])
env['SHCXXCOM'] = 'c++ -Iinclude -c $SOURCE -o $TARGET ' + ARGUMENTS.get('flags', '')
inventory = ['a.cpp', 'b.cpp'] + (['c.cpp'] if ARGUMENTS.get('extra') else [])
quality.add_tidy(env, Path('build'), inventory, [env.File(src + '.o') for src in inventory])
Default('tidy')
""")
        calls = self.root / "calls"
        self.scons()
        self.assertCountEqual(calls.read_text().splitlines(), ["a.cpp", "b.cpp"])
        calls.write_text("")
        self.scons()
        self.assertEqual(calls.read_text(), "")
        self.write("include/shared.h", "struct A { int x; };\n")
        self.scons()
        self.assertEqual(calls.read_text().splitlines(), ["a.cpp"])
        calls.write_text("")
        self.write("b.cpp", '#include "b.h"\nint b;\n')
        self.scons()
        self.assertEqual(calls.read_text().splitlines(), ["b.cpp"])
        calls.write_text("")
        self.scons("extra=yes")
        self.assertEqual(calls.read_text().splitlines(), ["c.cpp"])
        calls.write_text("")
        self.write(".clang-tidy", "Checks: 'modernize-*'\n")
        self.scons()
        self.assertCountEqual(calls.read_text().splitlines(), ["a.cpp", "b.cpp"])
        calls.write_text("")
        self.write("version", "test-tidy 2")
        self.scons()
        self.assertCountEqual(calls.read_text().splitlines(), ["a.cpp", "b.cpp"])
        calls.write_text("")
        self.scons("flags=-DABI=2")
        self.assertCountEqual(calls.read_text().splitlines(), ["a.cpp", "b.cpp"])
        self.write("fail", "")
        self.write("a.cpp", '#include "a.h"\nint changed;\n')
        self.scons("flags=-DABI=2", success=False)
        self.assertFalse((self.root / "build/a/success.stamp").exists())
        calls.write_text("")
        self.scons("flags=-DABI=2", success=False)
        self.assertEqual(calls.read_text().splitlines(), ["a.cpp"])
        (self.root / "fail").unlink()
        self.scons("flags=-DABI=2")
        calls.write_text("")
        self.scons("flags=-DABI=2")
        self.assertEqual(calls.read_text(), "")


class RunnerGraphTests(WorkspaceTest):
    def test_one_import_parallel_order_isolation_and_failure(self):
        tool = self.fake_tool("godot", """
from pathlib import Path
import json, os, sys
kind = 'import' if '--import' in sys.argv else Path(sys.argv[sys.argv.index('--script') + 1]).stem
assert Path('staged.dll').exists()
if kind != 'import':
    assert Path('prepared').exists()
Path(kind + '.json').write_text(json.dumps({'args': sys.argv, 'home': os.environ['APPDATA']}))
if kind == 'import':
    with Path('imports').open('a') as log:
        log.write('import\\n')
    if Path('fail').exists():
        sys.exit(7)
    Path('prepared').write_text('ready')
""")
        self.write("library.dll", "library")
        self.write("tests/godot_hosted_runner.gd", "")
        self.write("tests/godot_conformance_runner.gd", "")
        self.write("SConstruct", f"""
import sys, os
sys.path.insert(0, {str(PROJECT)!r})
from build_tools.runners import add_preparation, add_runners
from build_tools.targets import stage
env = Environment(tools=[], ENV=os.environ.copy(), DEFN_GODOT_BIN={str(tool)!r})
staged = stage(env, 'library.dll', 'staged.dll')
prepared = add_preparation(env, staged)
nodes = add_runners(env, {{'hosted_test', 'conformance'}}, prepared)
Alias('all', list(nodes.values()))
Default('all')
""")
        self.scons("-j4")
        self.assertEqual((self.root / "imports").read_text().splitlines(), ["import"])
        hosted = json.loads((self.root / "godot_hosted_runner.json").read_text())
        conformance = json.loads((self.root / "godot_conformance_runner.json").read_text())
        self.assertNotEqual(hosted["home"], conformance["home"])
        self.assertIn("--fixed-fps", conformance["args"])
        self.scons("-j1")
        self.assertEqual(len((self.root / "imports").read_text().splitlines()), 2)
        self.write("fail", "")
        for name in ("prepared", "godot_hosted_runner.json", "godot_conformance_runner.json"):
            (self.root / name).unlink()
        self.scons("-j4", success=False)
        self.assertFalse((self.root / "godot_hosted_runner.json").exists())
        self.assertFalse((self.root / "godot_conformance_runner.json").exists())


if __name__ == "__main__":
    unittest.main()
