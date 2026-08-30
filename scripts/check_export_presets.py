#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Check that every asset the shipped game can reach is listed in `export_presets.cfg`.

Both presets set `export_filter="resources"`, which ships *only* the files named in `export_files`. Nothing
walks the references for us: a unit added to `unit_data.json` renders in the editor and in a debug run, and
its sprites are simply absent from the packaged build. The `hound` shipped that way -- 50 frames missing from
both presets while it spawned in all five campaign levels.

    python scripts/check_export_presets.py            # fail with the unlisted paths
    python scripts/check_export_presets.py --stale     # also list entries nothing reaches

Reachability is resolved the way the game resolves it, not by globbing `assets/`:

  * `unit_data.json` is walked structurally, because its paths are `printf` templates rather than filenames.
    An `animations` entry expands `path_template` x `frame_count`; `muzzle_flash.path_template` expands frames
    0..9, a range hardcoded in `AnimationController::setup_muzzle_flash` and not driven by any field in the
    JSON; any nested `path` naming a `res://` file is a literal, which is how the per-unit sfx are written.
  * Everything else is scanned for `res://` literals -- the other data files, the scenes, `project.godot`,
    the bus layout, and the C++ sources, which hardcode the cutscene stings and the promotion kalimba.
  * A literal naming a directory (`res://data/levels`, from `DataPaths`) expands to the files in it, since
    that is what `level_definition` will ask for at runtime.

The exclusions below are the interesting part, and each one is a decision rather than an oversight.
"""

import argparse
import json
import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
PROJECT_ROOT = REPO_ROOT / "defn"
EXPORT_PRESETS = PROJECT_ROOT / "export_presets.cfg"
UNIT_DATA = PROJECT_ROOT / "data" / "unit_data.json"

# `AnimationController::setup_muzzle_flash` loops `for (int i = 0; i <= 9; ++i)` and keeps whichever textures
# load, so a muzzle set is allowed to be shorter than this. Frames that are not on disk are not required.
MUZZLE_FRAMES = 10

# Not shipped, and not an omission:
#
#   data/lab      The tempo lab's synthetic engagements. Reachable only through `level_definition_in`, which
#                 only `defn_sim_runner.cpp` calls, and only when a scenario spec sets `level_directory`. The
#                 game itself never resolves anything but `LEVELS_DIRECTORY`. Shipping these would put balance
#                 fixtures in the player's PCK. `data_paths.h` names the directory, so it must be excluded
#                 explicitly or the C++ scan would demand it.
UNSHIPPED_PREFIXES = ("res://data/lab",)

# Shipped, but named by no `res://` literal anywhere -- Godot loads both by convention, so they would read as
# stale every time `--stale` ran.
SHIPPED_BY_CONVENTION = frozenset(
    {
        "res://default_bus_layout.tres",
        "res://defn_core.gdextension",
    }
)

# Scanned for `res://` literals. `scenarios/` is deliberately absent: those specs configure the headless
# instruments and are not content, which is also why they name `data/lab` above.
SCAN_GLOBS = (
    "project.godot",
    "*.tres",
    "data/**/*.json",
    "scenes/**/*.tscn",
    "src/**/*.cpp",
    "src/**/*.h",
)

RESOURCE_PATTERN = re.compile(r"res://[A-Za-z0-9_/.:%+-]*")
FRAME_PATTERN = re.compile(r"%(\d*)d")


def to_disk(resource):
    """Where a `res://` path lives in the working tree."""
    return PROJECT_ROOT / resource[len("res://") :]


def unshipped(resource):
    return resource.startswith(UNSHIPPED_PREFIXES)


def expand(template, frames):
    """Every frame of a `printf` template, or the template itself when it has no frame field."""
    match = FRAME_PATTERN.search(template)
    if not match:
        return [template]
    width = int(match.group(1) or 0)
    return [template[: match.start()] + str(frame).zfill(width) + template[match.end() :] for frame in range(frames)]


def read_presets():
    """The `export_files` of each preset, as [(name, {resource, ...}), ...]."""
    text = EXPORT_PRESETS.read_text(encoding="utf-8")
    presets = []
    for block in re.finditer(r"\[preset\.\d+\]\n(.*?)(?=\n\[preset\.|\Z)", text, re.S):
        body = block.group(1)
        files = re.search(r"export_files=PackedStringArray\((.*?)\)\n", body, re.S)
        if not files:
            continue
        name = re.search(r'name="([^"]*)"', body)
        presets.append((name.group(1) if name else "?", set(re.findall(r'"([^"]*)"', files.group(1)))))
    return presets


def walk_unit(node, unit, where, reached):
    """Collect the resources a unit reaches, following the shape of `unit_data.json` rather than its text."""
    if isinstance(node, dict):
        template = node.get("path_template")
        if isinstance(template, str):
            # An animation declares its length; a muzzle flash does not, and takes the hardcoded range.
            frames = int(node["frame_count"]) if "frame_count" in node else MUZZLE_FRAMES
            optional = "frame_count" not in node
            for resource in expand(template, frames):
                reached.setdefault(resource, (f"{unit}{where}", optional))
        literal = node.get("path")
        if isinstance(literal, str) and literal.startswith("res://"):
            reached.setdefault(literal, (f"{unit}{where}", False))
        for key, value in node.items():
            if key not in ("path", "path_template"):
                walk_unit(value, unit, f"{where}.{key}", reached)
    elif isinstance(node, list):
        for index, value in enumerate(node):
            walk_unit(value, unit, f"{where}[{index}]", reached)


def reachable():
    """Every resource the shipped game can ask for, as {resource: (source, optional)}."""
    reached = {}

    for unit, config in json.loads(UNIT_DATA.read_text(encoding="utf-8"))["units"].items():
        walk_unit(config, unit, "", reached)

    for pattern in SCAN_GLOBS:
        for path in sorted(PROJECT_ROOT.glob(pattern)):
            source = path.relative_to(PROJECT_ROOT).as_posix()
            if source == "data/unit_data.json" or unshipped(f"res://{source}"):
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for resource in set(RESOURCE_PATTERN.findall(text)):
                # Templates live only in `unit_data.json`, which is walked above with the frame counts that
                # give them meaning. A stray one here would be a filename with a literal `%` in it.
                if "%" not in resource:
                    reached.setdefault(resource, (source, False))

    # A directory reference is a promise to load whatever is in it.
    for resource, origin in list(reached.items()):
        if to_disk(resource).is_dir():
            del reached[resource]
            for child in sorted(to_disk(resource).iterdir()):
                reached.setdefault(f"{resource}/{child.name}", origin)

    return {resource: origin for resource, origin in reached.items() if not unshipped(resource)}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--stale", action="store_true", help="also list export entries nothing reaches")
    args = parser.parse_args()

    reached = reachable()
    presets = read_presets()
    if not presets:
        sys.exit(f"no presets with an export_files list in {EXPORT_PRESETS}")

    failed = False
    for name, listed in presets:
        missing = sorted(
            resource
            for resource, (_, optional) in reached.items()
            if resource not in listed and not (optional and not to_disk(resource).exists())
        )
        dangling = sorted(resource for resource in reached if not to_disk(resource).exists() and not reached[resource][1])

        if missing:
            failed = True
            print(f"{name}: {len(missing)} reachable asset(s) missing from export_files")
            for resource in missing:
                print(f"  {resource}  <- {reached[resource][0]}")
        if dangling:
            failed = True
            print(f"{name}: {len(dangling)} reference(s) to files that are not on disk")
            for resource in dangling:
                print(f"  {resource}  <- {reached[resource][0]}")
        if not missing and not dangling:
            print(f"{name}: {len(reached)} reachable asset(s), all listed")

        if args.stale:
            stale = sorted(listed - set(reached) - SHIPPED_BY_CONVENTION)
            print(f"{name}: {len(stale)} listed entr(ies) nothing reaches")
            for resource in stale:
                print(f"  {resource}")

    if failed:
        print("\nAdd the paths above to export_files in defn/export_presets.cfg, in every preset.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
