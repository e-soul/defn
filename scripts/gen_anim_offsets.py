#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Measure the per-clip anchor corrections in `unit_data.json`.

The sprite packs crop every animation to its own bounding box, so a clip's canvas is padded by however far the
weapon swings in *that* clip. Godot centers each frame on the node, which parks the canvas on the origin rather
than the character: switching from `shoot` to `attack` then jumps the body by the difference in padding. The
marksman's melee canvas is 759x712 against a 622x466 shoot canvas, and the body pops up and to the right.

The fix is one `offset` per clip, in unscaled texture pixels, that pins the *body* instead. This script measures
those offsets by aligning each clip's first frame to the unit's `idle` first frame, and writes them back into
`data/unit_data.json`. `idle` is the reference and is therefore always zero, which keeps every unit standing
exactly where it stands today.

    python scripts/gen_anim_offsets.py --report          # measure and print, change nothing
    python scripts/gen_anim_offsets.py --write           # measure and update unit_data.json

Frame 0 is the anchor because frame 0 is what the eye compares against the last frame of whatever clip preceded
it. Later frames are free to move: a lunge is *supposed* to travel.

The alignment is silhouette overlap (FFT cross-correlation of the alpha channels), so it is an estimate, not a
rigged skeleton -- the poses genuinely differ. Expect a few pixels of slop, and eyeball `--contact-sheet` before
trusting a large correction. `death` is the one to check: its first frame is still upright, but if a unit's death
starts mid-fall there is no standing pose to match and the number is meaningless.

Requires Pillow and NumPy.
"""

import argparse
import json
import pathlib
import re
import sys

try:
    import numpy as np
    from PIL import Image, ImageDraw
except ImportError as exc:  # pragma: no cover - environment problem, not a code path
    sys.exit(f"{exc}. Install with: pip install pillow numpy")

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
PROJECT_ROOT = REPO_ROOT / "defn"
UNIT_DATA = PROJECT_ROOT / "data" / "unit_data.json"

# Every unit's offsets are measured against this clip, which is what makes them comparable and makes `idle` zero.
REFERENCE_CLIP = "idle"


def resolve(path_template, frame):
    """Turn a `res://` frame template into a path on disk."""
    return PROJECT_ROOT / path_template.replace("res://", "").replace("%03d", f"{frame:03d}")


def alpha(path):
    with Image.open(path) as image:
        return np.asarray(image.convert("RGBA").getchannel("A"), dtype=np.float32) / 255.0


def centered(mask, height, width):
    """Place a frame on a shared canvas the way Godot's `centered` sprite does."""
    canvas = np.zeros((height, width), np.float32)
    rows, cols = mask.shape
    top, left = (height - rows) // 2, (width - cols) // 2
    canvas[top : top + rows, left : left + cols] = mask
    return canvas


def align(reference, clip):
    """Pixels the clip must move by to sit on top of the reference, as (x, y).

    Both are centered on a canvas twice the size of the larger frame, so the correlation cannot wrap around and
    report a shift that is really its own negative.
    """
    height = 2 * max(reference.shape[0], clip.shape[0])
    width = 2 * max(reference.shape[1], clip.shape[1])
    a, b = centered(reference, height, width), centered(clip, height, width)
    correlation = np.fft.fftshift(np.fft.irfft2(np.fft.rfft2(a) * np.conj(np.fft.rfft2(b)), s=a.shape))
    row, col = np.unravel_index(np.argmax(correlation), correlation.shape)
    return int(col - width // 2), int(row - height // 2)


def measure(units):
    """Offsets for every unit that has the reference clip, as {unit: {clip: (x, y)}}."""
    offsets = {}
    for name, unit in units.items():
        clips = unit.get("animations") if isinstance(unit, dict) else None
        if not clips or REFERENCE_CLIP not in clips:
            continue
        reference = alpha(resolve(clips[REFERENCE_CLIP]["path_template"], 0))
        measured = {REFERENCE_CLIP: (0, 0)}
        for clip, config in clips.items():
            if clip == REFERENCE_CLIP:
                continue
            frame = resolve(config["path_template"], 0)
            if not frame.exists():
                print(f"warning: {name}/{clip} has no frame at {frame}", file=sys.stderr)
                continue
            measured[clip] = align(reference, alpha(frame))
        offsets[name] = measured
    return offsets


def report(units, offsets):
    for name, measured in offsets.items():
        scale = units[name].get("scale", 1.0)
        print(f"{name} (scale {scale})")
        for clip, (x, y) in measured.items():
            print(f"    {clip:8s} ({x:+5d},{y:+5d}) px -> ({x * scale:+6.1f},{y * scale:+6.1f}) on screen")


# Every array in `unit_data.json` is a two-number vector written on one line. `json.dumps` splits them across three,
# so put them back: this script means to add a key, not to reformat the file around it.
INLINE_PAIR = re.compile(r"\[\n\s*(-?[\d.]+),\n\s*(-?[\d.]+)\n\s*\]")


def write(document, offsets, path):
    """Update the offsets in place, leaving the rest of the file's key order and style untouched."""
    for name, measured in offsets.items():
        clips = document["units"][name]["animations"]
        for clip, (x, y) in measured.items():
            clips[clip]["offset"] = [x, y]
    path.write_text(INLINE_PAIR.sub(r"[\1, \2]", json.dumps(document, indent=2)) + "\n", encoding="utf-8")


def contact_sheet(units, offsets, path, cell=900):
    """Draw every clip's first frame at its corrected anchor, with the origin crosshaired.

    A correction is right when the feet sit on one line and the body does not slide along it.
    """
    rows = list(offsets.items())
    columns = max(len(measured) for _, measured in rows)
    sheet = Image.new("RGBA", (cell * columns, cell * len(rows)), (30, 30, 40, 255))
    pen = ImageDraw.Draw(sheet)
    for row, (name, measured) in enumerate(rows):
        clips = units[name]["animations"]
        for column, (clip, (x, y)) in enumerate(measured.items()):
            x0, y0 = column * cell, row * cell
            with Image.open(resolve(clips[clip]["path_template"], 0)) as image:
                frame = image.convert("RGBA")
                sheet.alpha_composite(frame, (x0 + cell // 2 - frame.width // 2 + x, y0 + cell // 2 - frame.height // 2 + y))
            pen.line([(x0, y0 + cell // 2), (x0 + cell, y0 + cell // 2)], fill=(255, 0, 0, 255), width=3)
            pen.line([(x0 + cell // 2, y0), (x0 + cell // 2, y0 + cell)], fill=(255, 0, 0, 255), width=3)
            pen.rectangle([x0 + 2, y0 + 2, x0 + cell - 2, y0 + cell - 2], outline=(255, 255, 0, 255), width=2)
            pen.text((x0 + 10, y0 + 10), f"{name} {clip} ({x:+d},{y:+d})", fill=(255, 255, 255, 255))
    sheet.convert("RGB").resize((sheet.width // 3, sheet.height // 3)).save(path)
    print(f"wrote {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--write", action="store_true", help="update data/unit_data.json in place")
    parser.add_argument("--report", action="store_true", help="print the measured offsets")
    parser.add_argument("--contact-sheet", metavar="PNG", help="render the corrected first frames for eyeballing")
    args = parser.parse_args()
    if not (args.write or args.report or args.contact_sheet):
        parser.error("nothing to do: pass --report, --write or --contact-sheet")

    document = json.loads(UNIT_DATA.read_text(encoding="utf-8"))
    units = document["units"]
    offsets = measure(units)

    if args.report:
        report(units, offsets)
    if args.contact_sheet:
        contact_sheet(units, offsets, args.contact_sheet)
    if args.write:
        write(document, offsets, UNIT_DATA)
        print(f"wrote {UNIT_DATA}")


if __name__ == "__main__":
    main()
