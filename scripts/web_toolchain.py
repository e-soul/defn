# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PIN_FILE = Path(__file__).resolve().with_name("web-toolchain.json")
DEFAULT_SDK_DIR = REPO_ROOT / "build" / "emsdk"


@dataclass(frozen=True)
class WebToolchain:
    emscripten: str
    emsdk_revision: str
    godot: str


def load_toolchain(path: Path = PIN_FILE) -> WebToolchain:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) != {"emscripten", "emsdk_revision", "godot"}:
        raise ValueError(f"{path}: expected emscripten, emsdk_revision and godot keys")
    if not all(isinstance(value, str) for value in data.values()):
        raise ValueError(f"{path}: toolchain versions and revision must be strings")
    pin = WebToolchain(**data)
    for name in ("emscripten", "godot"):
        if not re.fullmatch(r"\d+\.\d+\.\d+", getattr(pin, name)):
            raise ValueError(f"{path}: {name} must be an exact major.minor.patch version")
    if not re.fullmatch(r"[0-9a-f]{40}", pin.emsdk_revision):
        raise ValueError(f"{path}: emsdk_revision must be a full Git commit SHA")
    return pin


def verify_emscripten(pin: WebToolchain, env: dict[str, str] | None = None) -> str:
    compiler = shutil.which("emcc", path=(os.environ if env is None else env).get("PATH", ""))
    if not compiler:
        raise RuntimeError(
            "Emscripten is missing. Run python scripts/build_web.py --setup "
            "from the repository root, then use that wrapper to build."
        )
    result = subprocess.run(
        [compiler, "--version"], env=env, check=True, capture_output=True, text=True,
    )
    match = re.search(r"^emcc\b[^\r\n]*? (\d+\.\d+\.\d+)\b", result.stdout, re.M)
    if not match or match[1] != pin.emscripten:
        actual = match[1] if match else result.stdout.strip()
        raise RuntimeError(
            f"Web builds require Emscripten {pin.emscripten}; found {actual!r} at {compiler}. "
            "Use python scripts/build_web.py or activate the pinned SDK. "
            "Upgrade web-toolchain.json and the Godot templates together."
        )
    return compiler


def sdk_environment(sdk_dir: Path) -> dict[str, str]:
    config = sdk_dir / ".emscripten"
    compiler = sdk_dir / "upstream" / "emscripten" / ("emcc.bat" if os.name == "nt" else "emcc")
    if not config.is_file() or not compiler.is_file():
        raise RuntimeError(
            f"SDK is not installed and activated at {sdk_dir}. Run python scripts/build_web.py --setup."
        )
    env = os.environ.copy()
    for override in ("EM_LLVM_ROOT", "EM_BINARYEN_ROOT", "EM_NODE_JS", "EMSDK_NODE", "EMSDK_PY"):
        env.pop(override, None)
    env.update({
        "EMSDK": str(sdk_dir),
        "EM_CONFIG": str(config),
        "EM_CACHE": str(sdk_dir / "upstream" / "emscripten" / "cache"),
        "EMSDK_PYTHON": sys.executable,
        "PATH": os.pathsep.join([
            str(sdk_dir), str(sdk_dir / "upstream" / "emscripten"), env.get("PATH", ""),
        ]),
    })
    return env


def verify_sdk_revision(sdk_dir: Path, pin: WebToolchain) -> None:
    if not (sdk_dir / "emsdk.py").is_file():
        raise RuntimeError(
            f"No emsdk checkout at {sdk_dir}. Run python scripts/build_web.py --setup."
        )
    result = subprocess.run(
        ["git", "-C", str(sdk_dir), "rev-parse", "HEAD"],
        check=True, capture_output=True, text=True,
    )
    if result.stdout.strip() != pin.emsdk_revision:
        raise RuntimeError(
            f"{sdk_dir} is not at the pinned emsdk revision {pin.emsdk_revision}. "
            "Use --emsdk-dir with a new local directory and run --setup there; "
            "the wrapper will not reset an existing SDK checkout."
        )
