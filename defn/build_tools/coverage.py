"""LLVM instrumentation and reports, imported only for coverage requests."""

from pathlib import Path
import os
import shutil
import subprocess

from .configuration import remove_flags
from .quality import add_check
from .runners import execute_runner, isolated_environment, write_success
from .sources import NATIVE_SUPPORT, extension_sources


def configure(env):
    if env["platform"] == "windows":
        env.Replace(CC="clang-cl", CXX="clang-cl")
        if env.get("lto", "none") != "none":
            env["CCFLAGS"] = remove_flags(env.get("CCFLAGS", []), {"/GL"})
            env["LINKFLAGS"] = remove_flags(env.get("LINKFLAGS", []), {"/LTCG"})
            env.Replace(LINK="lld-link")
            env.AppendUnique(CCFLAGS=["-flto=thin" if env["lto"] == "thin" else "-flto"])
        clang = shutil.which("clang-cl")
        if not clang:
            raise ValueError("Coverage requires clang-cl on PATH")
        arch = {"x86_64": "x86_64", "x86_32": "i386", "arm64": "aarch64"}[env["arch"]]
        runtimes = sorted((Path(clang).resolve().parent.parent / "lib" / "clang").glob(
            f"*/lib/windows/clang_rt.profile-{arch}.lib"))
        if not runtimes:
            raise ValueError(f"Unable to locate clang_rt.profile-{arch}.lib")
        env.AppendUnique(LIBS=[str(runtimes[-1])])
    else:
        env.Replace(CC="clang", CXX="clang++", LINK="clang++")
        for key in ("CCFLAGS", "CXXFLAGS"):
            env[key] = remove_flags(env.get(key, []), {"-fno-gnu-unique"})
        env.AppendUnique(LINKFLAGS=["-fprofile-instr-generate"])
    env.AppendUnique(CCFLAGS=["-fprofile-instr-generate", "-fcoverage-mapping"])
    for tool in ("llvm-profdata", "llvm-cov"):
        if not shutil.which(tool):
            raise ValueError(f"Coverage requires {tool} on PATH")
    return env


def prepare_raw(suite):
    raw = Path("build") / "coverage" / suite / "raw"
    if raw.exists():
        shutil.rmtree(raw)
    raw.mkdir(parents=True)
    return raw


def report(label, raw_dirs, objects, sources):
    directory = Path("build") / "coverage" / label
    directory.mkdir(parents=True, exist_ok=True)
    html = directory / "html"
    if html.exists():
        shutil.rmtree(html)
    profile = directory / "coverage.profdata"
    summary = directory / "summary.txt"
    profile.unlink(missing_ok=True)
    summary.unlink(missing_ok=True)
    raw = [str(path) for directory in raw_dirs for path in sorted(directory.glob("*.profraw"))]
    if not raw:
        print(f"{label} coverage did not produce LLVM profile data")
        return 1
    result = subprocess.run(
        [shutil.which("llvm-profdata"), "merge", "-sparse", *raw, "-o", str(profile)], check=False)
    if result.returncode:
        return result.returncode
    object_args = [str(Path(objects[0]).resolve())]
    object_args.extend("--object=" + str(Path(obj).resolve()) for obj in objects[1:])
    source_args = [str(Path(src).resolve()) for src in sources]
    command = [shutil.which("llvm-cov"), "report", *object_args, f"-instr-profile={profile}", *source_args]
    result = subprocess.run(command, check=False, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    summary.write_text(result.stdout, encoding="utf-8")
    print(result.stdout)
    if result.returncode:
        return result.returncode
    result = subprocess.run([
        shutil.which("llvm-cov"), "show", *object_args, f"-instr-profile={profile}",
        "-format=html", f"-output-dir={html}", *source_args,
    ], check=False)
    if result.returncode == 0:
        print(f"{label} coverage: {summary}, {html / 'index.html'}")
    return result.returncode


def run_native(target, source, env):
    raw = prepare_raw("native")
    binary = source[0].abspath
    run_env = os.environ.copy()
    run_env["LLVM_PROFILE_FILE"] = str((raw / "defn_unit_tests-%p.profraw").resolve())
    result = subprocess.run([binary], check=False, env=run_env).returncode
    if result == 0:
        result = report("native", [raw], [binary], NATIVE_SUPPORT)
    if result == 0:
        write_success(target)
    return result


def run_hosted(target, source, env):
    raw = prepare_raw("hosted")
    run_env = isolated_environment("coverage")
    run_env["LLVM_PROFILE_FILE"] = str((raw / "defn_hosted_tests-%p.profraw").resolve())
    result = execute_runner("hosted_test", env, run_env)
    if result == 0:
        result = report("hosted", [raw], [env["DEFN_EXTENSION_LIBRARY_PATH"]],
                        extension_sources(env["target"] == "template_debug"))
    if result == 0:
        write_success(target)
    return result


def run_merged(target, source, env):
    # Include both binaries: native-only rules must not disappear from the merged report.
    result = report("merged", [Path("build/coverage/native/raw"), Path("build/coverage/hosted/raw")],
                    [env["DEFN_EXTENSION_LIBRARY_PATH"], env["DEFN_NATIVE_COVERAGE_PATH"]],
                    sorted(set(NATIVE_SUPPORT + extension_sources(env["target"] == "template_debug"))))
    if result == 0:
        write_success(target)
    return result


def add_native(env, program):
    return add_check(env, "coverage_native", program, run_native, "build/coverage/native/coverage.stamp")


def add_hosted(env, preparation):
    return add_check(env, "coverage_hosted", [preparation, "tests/godot_hosted_runner.gd"],
                     run_hosted, "build/coverage/hosted/coverage.stamp")


def add_merged(env, native, hosted):
    return add_check(env, "coverage", [native, hosted], run_merged, "build/coverage/merged/coverage.stamp")
