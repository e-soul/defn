# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

"""Compatibility keys for the post-job SCons cache (project and godot-cpp).

Only the revision suffix may be dropped during restore. In particular, runner
toolchain updates and build-configuration changes must never fall back to an
older compiler's objects. SCons still decides which compatible objects to reuse.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
CONFIGURATIONS = {
    "windows": ("coverage", "release"),
    "linux": ("coverage", "release"),
    "web": ("debug-release",),
}


def digest(value):
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def build_configuration(root):
    """Hash build policy, not source files: source revisions share a fallback."""
    paths = [
        root / "defn" / "SConstruct",
        root / "scripts" / "build.py",
        root / "scripts" / "build_web.py",
        root / "scripts" / "web_toolchain.py",
        root / "scripts" / "ci_cache.py",
    ]
    paths.extend(sorted((root / "defn" / "build_tools").rglob("*.py")))
    return {
        path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in paths
    }


def tool_version(command, environment, *, banner=False):
    executable = shutil.which(command, path=environment.get("PATH"))
    if not executable:
        raise RuntimeError(f"Cannot fingerprint missing compiler/tool: {command}")
    result = subprocess.run(
        [executable, *([] if banner else ["--version"])],
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    # cl/link print a version banner but return an error without input files.
    if (result.returncode and not banner) or not result.stdout.strip():
        raise RuntimeError(f"Cannot fingerprint {command}: {result.stdout}")
    return result.stdout.strip()


def native_toolchain(platform, architecture, configuration):
    environment = os.environ.copy()
    identity = {}
    if platform == "windows":
        # Use the same SCons MSVC discovery as godot-cpp, not a possibly different
        # cl.exe on the ambient PATH. INCLUDE/LIB encode the selected SDK/CRT.
        from SCons.Environment import Environment

        target_arch = {
            "x86_64": "amd64",
            "x86_32": "x86",
            "arm64": "arm64",
        }[architecture]
        build_env = Environment(tools=["msvc", "mslink"], TARGET_ARCH=target_arch)
        environment.update(build_env["ENV"])
        identity["msvc"] = str(build_env.get("MSVC_VERSION", ""))
        for name in ("INCLUDE", "LIB", "LIBPATH"):
            identity[name] = environment.get(name, "")
        for command in ("cl", "link"):
            identity[command] = tool_version(command, environment, banner=True)
        coverage_tools = ("clang-cl", "llvm-profdata", "llvm-cov")
    else:
        for command in ("gcc", "g++", "ld"):
            identity[command] = tool_version(command, environment)
        coverage_tools = ("clang", "clang++", "llvm-profdata", "llvm-cov")

    if configuration == "coverage":
        for command in coverage_tools:
            identity[command] = tool_version(command, environment)
    return identity


def cache_keys(*, platform, architecture, host_os, host_arch, configuration,
               scons_version, toolchain, build_config, godot_cpp_revision, revision):
    if configuration not in CONFIGURATIONS[platform]:
        raise ValueError(f"Unsupported {platform} cache configuration: {configuration}")
    if not re.fullmatch(r"[0-9a-fA-F]{40,64}", revision):
        raise ValueError("A full project revision is required")
    if not re.fullmatch(r"[0-9a-fA-F]{40,64}", godot_cpp_revision):
        raise ValueError("A full godot-cpp revision is required")
    compatibility = digest({
        "platform": platform,
        "architecture": architecture,
        "host_os": host_os,
        "host_arch": host_arch,
        "configuration": configuration,
        "scons_version": scons_version,
        "toolchain": toolchain,
        "build_config": build_config,
        "godot_cpp_revision": godot_cpp_revision,
    })
    prefix = f"scons-v2-{platform}-{architecture}-{configuration}-{compatibility}-"
    return {"key": prefix + revision, "restore-prefix": prefix}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=CONFIGURATIONS, required=True)
    parser.add_argument("--architecture", required=True)
    parser.add_argument("--configuration", required=True)
    parser.add_argument("--scons-version", required=True)
    parser.add_argument("--host-os", required=True)
    parser.add_argument("--host-arch", required=True)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args(argv)
    if args.configuration not in CONFIGURATIONS[args.platform]:
        parser.error(f"Unsupported {args.platform} configuration: {args.configuration}")

    godot_cpp_revision = subprocess.check_output(
        ["git", "-C", str(ROOT / "godot-cpp"), "rev-parse", "HEAD"], text=True
    ).strip()
    if args.platform == "web":
        # Export also builds a native debug extension so Godot can import scenes.
        # It shares SCONS_CACHE with WASM and must not reuse another host compiler.
        host_arch = {"X64": "x86_64", "ARM64": "arm64", "X86": "x86_32"}[args.host_arch]
        toolchain = {
            "web_pin": json.loads(
                (ROOT / "scripts" / "web-toolchain.json").read_text(encoding="utf-8")
            ),
            "native": native_toolchain(args.host_os.lower(), host_arch, "release"),
        }
    else:
        toolchain = native_toolchain(args.platform, args.architecture, args.configuration)
    keys = cache_keys(
        **vars(args), toolchain=toolchain, build_config=build_configuration(ROOT),
        godot_cpp_revision=godot_cpp_revision,
    )
    for name, value in keys.items():
        print(f"{name}={value}")
    if output := os.environ.get("GITHUB_OUTPUT"):
        with open(output, "a", encoding="utf-8") as stream:
            for name, value in keys.items():
                stream.write(f"{name}={value}\n")


if __name__ == "__main__":
    main()
