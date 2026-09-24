#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

"""Record a scripted shot of the game for the itch.io page or a trailer.

The heavy lifting is in `defn/tools/capture_runner.gd`; this resolves Godot the way the SCons targets do,
picks the right engine flags for the mode, and hands the result to ffmpeg when one is installed.

    python scripts/capture.py --shot level_03_opening --video --stills
    python scripts/capture.py --shot level_03_opening --recon        # print where the fight actually is

Modes:
  --video   Movie Maker mode. The frame delta is pinned to exactly 1/fps and rendering is decoupled from
            real time, so the recording is smooth at the target rate however slowly it renders. Output is
            the project viewport size (1920x1080) regardless of the window or the display's DPI.
  --stills  No recording, fixed delta, frames as fast as the machine manages. Stills come off the window
            backbuffer, which on a HiDPI display is 4K -- supersampled and downscaled here, which is
            sharper than grabbing 1080p directly.
  --recon   Neither; prints a per-second table of unit counts and positions, which is how the timings in
            a shot file get chosen.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PROJECT_DIR = REPO_ROOT / "defn"
SHOTS_DIR = PROJECT_DIR / "tools" / "shots"
DEFAULT_OUT_DIR = REPO_ROOT / "build" / "capture"
GODOT_EXECUTABLE_ENV_VAR = "GODOT_BIN"


def resolve_godot(explicit: str) -> str:
    for candidate in (explicit, os.environ.get(GODOT_EXECUTABLE_ENV_VAR, "")):
        if candidate and Path(candidate).is_file():
            return candidate
    for name in ("godot", "godot4", "Godot"):
        found = shutil.which(name)
        if found:
            return found
    sys.exit(
        f"Unable to locate Godot. Pass --godot <path> or set {GODOT_EXECUTABLE_ENV_VAR} "
        "(devenv_defn.bat sets it on this machine)."
    )


def prefer_console_build(godot: str) -> str:
    """On Windows the plain editor binary detaches from the parent console, so everything the runner
    prints -- including the warning that a deploy never became affordable -- is lost when it is launched
    from here. The _console build next to it writes to stdout like a normal program."""
    path = Path(godot)
    if sys.platform != "win32" or path.stem.endswith("_console"):
        return godot
    console = path.with_name(f"{path.stem}_console{path.suffix}")
    return str(console) if console.is_file() else godot


def resolve_shot(shot: str) -> tuple[str, str]:
    """Returns (res:// path for the runner, shot name)."""
    path = Path(shot)
    if not path.suffix:
        path = SHOTS_DIR / f"{shot}.json"
    if not path.is_absolute():
        path = (REPO_ROOT / path).resolve() if path.exists() else (SHOTS_DIR / path.name)
    if not path.is_file():
        sys.exit(f"No such shot: {path}")
    return f"res://tools/shots/{path.name}", path.stem


def run_godot(godot: str, args: list[str], verbose: bool) -> None:
    command = [godot, "--path", str(PROJECT_DIR), *args]
    if verbose:
        print(" ".join(command))
    result = subprocess.run(command, cwd=REPO_ROOT, check=False)
    if result.returncode != 0:
        sys.exit(f"Godot exited with {result.returncode}")


def downscale_stills(stills_dir: Path, name: str, width: int) -> None:
    try:
        from PIL import Image
    except ImportError:
        print("Pillow is not installed; leaving stills at capture resolution.")
        return
    for source in sorted(stills_dir.glob(f"{name}_*.png")):
        if source.stem.endswith(f"_{width}"):
            continue
        with Image.open(source) as image:
            if image.width <= width:
                continue
            height = round(image.height * width / image.width)
            resized = image.resize((width, height), Image.LANCZOS)
            target = source.with_name(f"{source.stem}_{width}.png")
            resized.save(target)
            print(f"  {target.relative_to(REPO_ROOT)}  ({width}x{height})")


def encode(avi: Path, out_dir: Path, name: str, gif_seconds: tuple[float, float] | None) -> None:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        print(
            "\nffmpeg is not on PATH, so the recording is left as MJPEG AVI.\n"
            "  winget install Gyan.FFmpeg\n"
            f"then: ffmpeg -i {avi} -c:v libx264 -crf 16 -preset slow -pix_fmt yuv420p {out_dir / (name + '.mp4')}"
        )
        return

    mp4 = out_dir / f"{name}.mp4"
    print(f"\nEncoding {mp4.relative_to(REPO_ROOT)} ...")
    subprocess.run(
        # crf 16 is visually lossless enough to survive YouTube's own re-encode, which is the only
        # quality that matters for an upload.
        [ffmpeg, "-y", "-i", str(avi), "-c:v", "libx264", "-crf", "16", "-preset", "slow",
         "-pix_fmt", "yuv420p", "-movflags", "+faststart", "-c:a", "aac", "-b:a", "192k", str(mp4)],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    print(f"  {mp4.relative_to(REPO_ROOT)}  ({mp4.stat().st_size / 1e6:.1f} MB)")

    if gif_seconds is None:
        return
    start, length = gif_seconds
    palette = out_dir / f"{name}_palette.png"
    gif = out_dir / f"{name}.gif"
    # itch.io covers are 630x500; a 630-wide GIF letterboxes into that and is what the page actually
    # animates. Two passes because a generated palette is the difference between a GIF that looks like
    # the game and one that looks like 1998.
    filters = "fps=15,scale=630:-1:flags=lanczos"
    subprocess.run(
        [ffmpeg, "-y", "-ss", str(start), "-t", str(length), "-i", str(avi),
         "-vf", f"{filters},palettegen=stats_mode=diff", str(palette)],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    subprocess.run(
        [ffmpeg, "-y", "-ss", str(start), "-t", str(length), "-i", str(avi), "-i", str(palette),
         "-lavfi", f"{filters}[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=3", str(gif)],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    palette.unlink(missing_ok=True)
    print(f"  {gif.relative_to(REPO_ROOT)}  ({gif.stat().st_size / 1e6:.1f} MB)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--shot", default="level_03_opening", help="shot name in defn/tools/shots, or a path")
    parser.add_argument("--video", action="store_true", help="record with Movie Maker")
    parser.add_argument("--stills", action="store_true", help="save the shot's marked stills")
    parser.add_argument("--recon", action="store_true", help="print the fight timings instead of capturing")
    parser.add_argument("--recon-seconds", type=int, default=45)
    parser.add_argument("--level", default="", help="level id, overriding the shot's; recon needs it "
                                                    "because it runs before a shot file exists")
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    parser.add_argument("--still-width", type=int, default=1920, help="downscaled copy width (0 to skip)")
    parser.add_argument("--gif", nargs=2, type=float, metavar=("START", "SECONDS"),
                        help="also cut an itch.io cover GIF from this span of the recording")
    parser.add_argument("--encode-only", action="store_true",
                        help="re-encode the existing recording without playing the shot again")
    parser.add_argument("--no-cursor", action="store_true", help="record without the drawn pointer")
    parser.add_argument("--godot", default="", help=f"Godot executable (default: ${GODOT_EXECUTABLE_ENV_VAR})")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if not (args.video or args.stills or args.recon or args.encode_only):
        args.video = True

    out_dir = Path(args.out_dir).resolve()
    stills_dir = out_dir / "stills"
    out_dir.mkdir(parents=True, exist_ok=True)

    # Recording is minutes of rendering; trying a different GIF span or a different x264 setting should
    # not pay for it again.
    if args.encode_only:
        _, name = resolve_shot(args.shot)
        avi = out_dir / f"{name}.avi"
        if not avi.is_file():
            sys.exit(f"No recording to encode at {avi}. Run with --video first.")
        encode(avi, out_dir, name, tuple(args.gif) if args.gif else None)
        return

    godot = prefer_console_build(resolve_godot(args.godot))

    level_args = ["--level", args.level] if args.level else []

    if args.recon:
        run_godot(
            godot,
            ["--fixed-fps", str(args.fps), "--script", "tools/capture_runner.gd", "--",
             "--recon", "--recon-seconds", str(args.recon_seconds), *level_args],
            args.verbose,
        )
        return

    shot_res_path, name = resolve_shot(args.shot)
    runner_args = ["--script", "tools/capture_runner.gd", "--", "--shot", shot_res_path, *level_args]
    if args.no_cursor:
        runner_args.append("--no-cursor")

    if args.stills:
        stills_dir.mkdir(parents=True, exist_ok=True)
        print(f"Stills pass: {name}")
        run_godot(godot, ["--fixed-fps", str(args.fps), *runner_args, "--stills-dir", stills_dir.as_posix()], args.verbose)
        if args.still_width:
            downscale_stills(stills_dir, name, args.still_width)

    if args.video:
        avi = out_dir / f"{name}.avi"
        print(f"Recording: {name} -> {avi.relative_to(REPO_ROOT)}")
        # The movie writer resolves its path itself and silently fails on a relative one, so it gets an
        # absolute path and a directory that already exists.
        run_godot(godot, ["--write-movie", str(avi), "--fixed-fps", str(args.fps), *runner_args], args.verbose)
        if not avi.is_file():
            sys.exit("Godot produced no recording.")
        print(f"  {avi.relative_to(REPO_ROOT)}  ({avi.stat().st_size / 1e6:.1f} MB)")
        encode(avi, out_dir, name, tuple(args.gif) if args.gif else None)


if __name__ == "__main__":
    main()
