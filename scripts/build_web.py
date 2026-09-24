# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from build import (
    PLATFORM_CONFIGS,
    ensure_godot_export_templates,
    format_command,
    resolve_godot_executable,
    templates_install_dir,
)
from web_toolchain import (
    DEFAULT_SDK_DIR,
    REPO_ROOT,
    WebToolchain,
    load_toolchain,
    sdk_environment,
    verify_emscripten,
    verify_sdk_revision,
)

PROJECT_DIR = REPO_ROOT / "defn"
EXPORT_PRESET = "defn_web_release"


def run(command: list[str], *, cwd: Path = REPO_ROOT, env=None) -> None:
    print(f"Running: {format_command(command)}", flush=True)
    subprocess.run(command, cwd=cwd, env=env, check=True)


def setup_sdk(sdk_dir: Path, pin: WebToolchain) -> None:
    if not sdk_dir.exists():
        sdk_dir.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "init", str(sdk_dir)])
        run(["git", "-C", str(sdk_dir), "remote", "add", "origin",
             "https://github.com/emscripten-core/emsdk.git"])
        run(["git", "-C", str(sdk_dir), "fetch", "--depth", "1", "origin", pin.emsdk_revision])
        run(["git", "-C", str(sdk_dir), "checkout", "--detach", "FETCH_HEAD"])
    verify_sdk_revision(sdk_dir, pin)
    manager = [sys.executable, str(sdk_dir / "emsdk.py")]
    run([*manager, "install", pin.emscripten])
    run([*manager, "activate", pin.emscripten])
    verify_emscripten(pin, sdk_environment(sdk_dir))
    print(f"Local Emscripten {pin.emscripten} ready at {sdk_dir}. No global activation.")


def verify_godot(executable: str, pin: WebToolchain) -> None:
    result = subprocess.run(
        [executable, "--version"], check=True, capture_output=True, text=True,
    )
    if not re.match(re.escape(pin.godot) + r"\.stable(?:\.|$)", result.stdout.strip()):
        raise RuntimeError(
            f"Web exports require Godot {pin.godot}.stable; "
            f"{executable} reports {result.stdout.strip()!r}."
        )


def run_godot(command: list[str]) -> None:
    print(f"Running: {format_command(command)}", flush=True)
    result = subprocess.run(command, capture_output=True, text=True, errors="replace")
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    result.check_returncode()
    # Godot can return zero despite resource-import errors.
    if re.search(r"^\s*(?:SCRIPT )?ERROR:", result.stdout + "\n" + result.stderr, re.M):
        raise RuntimeError("Godot reported errors; the Web export has not been published.")


def web_library(mode: str) -> str:
    return f"libdefn_core.web.template_{mode}.wasm32.nothreads.wasm"


def verify_export(directory: Path, mode: str) -> None:
    for name in ("index.html", "index.js", "index.wasm", "index.side.wasm",
                 "index.pck", web_library(mode)):
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"Missing or empty Web export artifact: {path}")
    html = (directory / "index.html").read_text(encoding="utf-8")
    if "$GODOT_" in html or web_library(mode) not in html:
        raise RuntimeError("Exported HTML has unresolved placeholders or lacks the Web extension.")


def export_game(godot: str, output_dir: Path, mode: str) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    # Validate a fresh export, never files left by an earlier successful run.
    with tempfile.TemporaryDirectory(prefix=".web-export-", dir=output_dir) as temporary:
        staging = Path(temporary)
        run_godot([
            godot, "--headless", "--path", str(PROJECT_DIR),
            f"--export-{mode}", EXPORT_PRESET, str(staging / "index.html"),
        ])
        verify_export(staging, mode)
        destination = output_dir / mode
        shutil.copytree(staging, destination, dirs_exist_ok=True)
    print(f"Web {mode} export: {destination / 'index.html'}")


def parse_args(argv=None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build Web exports with the checked-in toolchain.")
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument("--setup", action="store_true", help="Install/activate only the pinned local SDK.")
    actions.add_argument("--build-only", action="store_true", help="Compile WASM without Godot or exports.")
    parser.add_argument("--target", choices=("debug", "release", "both"), default="release")
    parser.add_argument("--emsdk-dir", type=Path, default=DEFAULT_SDK_DIR)
    parser.add_argument("--godot-exe", help="Godot editor path; otherwise use GODOT_BIN or download the pinned editor.")
    parser.add_argument("--output-dir", type=Path, default=REPO_ROOT / "build" / "web")
    return parser.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(argv)
    pin = load_toolchain()
    sdk_dir = args.emsdk_dir.expanduser().resolve()
    if args.setup:
        setup_sdk(sdk_dir, pin)
        return 0

    verify_sdk_revision(sdk_dir, pin)
    env = sdk_environment(sdk_dir)
    compiler = verify_emscripten(pin, env)
    print(f"Using Emscripten {pin.emscripten}: {compiler}", flush=True)
    modes = ("debug", "release") if args.target == "both" else (args.target,)
    godot = None
    if not args.build_only:
        if sys.platform not in ("win32", "linux"):
            raise RuntimeError("Automatic export supports Windows/Linux hosts. Use --build-only for WASM compilation.")
        host = "windows" if sys.platform == "win32" else "linux"
        config = PLATFORM_CONFIGS[host]
        cache_dir = REPO_ROOT / "build"
        godot = resolve_godot_executable(args.godot_exe, pin.godot, cache_dir, config)
        verify_godot(godot, pin)
        ensure_godot_export_templates(pin.godot, cache_dir, host)
        for mode in modes:
            template = templates_install_dir(pin.godot, host) / f"web_dlink_nothreads_{mode}.zip"
            if not template.is_file():
                raise RuntimeError(f"Missing {template}. Reinstall the matching Godot export templates.")
        # The native editor must load our classes before importing scenes and exporting.
        run([sys.executable, "-m", "SCons", f"platform={host}", "target=template_debug"], cwd=PROJECT_DIR)

    for mode in modes:
        run([sys.executable, "-m", "SCons", "platform=web", "threads=no",
             f"target=template_{mode}"], cwd=PROJECT_DIR, env=env)
        library = PROJECT_DIR / "bin" / web_library(mode)
        if not library.is_file():
            raise RuntimeError(f"Web build did not produce {library}")

    if godot:
        run([sys.executable, str(REPO_ROOT / "scripts" / "check_export_presets.py")])
        run_godot([godot, "--headless", "--path", str(PROJECT_DIR), "--import"])
        for mode in modes:
            export_game(godot, args.output_dir.expanduser().resolve(), mode)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Web build failed: {error}", file=sys.stderr)
        raise SystemExit(1)
