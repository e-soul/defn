#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Measure how well a background layer wraps from its right edge back to its left.

Every gameplay background is drawn by `Parallax2D` with `repeat_size` set, so column `W-1` is displayed
immediately left of column `0` forever. Whether that join is visible has been an eyeball judgement until now,
which is a bad way to accept an asset that repeats every screen for the whole match.

    python scripts/check_tiling.py defn/assets/backgrounds/background_beach_tiling.png
    python scripts/check_tiling.py build/art/*.png --preview build/art/seams

The number reported is a *ratio*, not a raw difference, because a raw one says nothing on its own. Adjacent
columns inside a busy jungle differ far more than adjacent columns inside an empty sky, so the seam is judged
against the image's own texture: how much columns typically differ from their neighbour, measured as the
median over every interior column pair. A ratio near or below 1.0 means the wrap is indistinguishable from any
other column boundary in the picture, which is exactly what seamless means.

Measured over the shipped set, with the lateral tilt alongside for diagnosis:

                                          local   step    tilt
    background_desert_outpost_tiling.png   0.00   0.00    -0.6   wraps
    middle_east_ruin_tiling.png            0.00   0.00    +4.2   wraps
    background_jungle_ruin_tiling.png      0.69   0.00    -3.6   wraps
    background_winter_forest_tiling.png    2.32   6.48    +7.0   seam
    background_beach_tiling.png            0.00  14.07    -4.5   seam
    background_feldkirchen_tiling.png      3.19  17.29    -4.0   seam
    middle_east_ruins.png                  5.89   7.01   +10.4   control, never was a tiling asset

    (A 0.00 means the raw difference fell under the floor described below, not that the edges are identical.)

Three of the five shipped gameplay backgrounds do not actually wrap, and all three fail the same way: they are
lit from the side, so the two ends carry a brightness difference nothing can reconcile. The failures are narrow
and local in `local` terms, which is why they survived visual acceptance -- Feldkirchen's mean column
difference across the join is 4.8 out of 255, but rows 855-971 differ by 140-206, and that band is a hard
vertical value step down a building's face with no inked contour to justify it. Averaged over the frame it
disappears; on screen it is a line. `step` is the column that makes it obvious.

Two failure modes it deliberately does not catch, because no edge metric can:

  * A *compositional* seam. Matching pixels either side of the boundary say nothing about a landmark sitting
    50px from the edge announcing itself every screen. `background_jungle_ruin_tiling.png` is the cautionary
    case: it wraps at 0.69, and the ornate stone arch that dominates the level straddles the join, so it
    arrives once per screen forever. That is what the outer-10%-simple rule in
    `templates/background_tiling.txt` is for, and it is judged by eye.
  * A vertical mismatch. Layers repeat horizontally only, so the top and bottom edges are never joined.

`--preview` writes the image rolled by half its width, which puts the wrap seam down the middle of the frame
where a human can actually see it. That is the check to trust when the ratio lands near the threshold.
"""

import argparse
import pathlib
import sys

import numpy as np
from PIL import Image

# Halfway across the measured gap. Everything that wraps lands at 0.63-0.69 and everything that does not lands
# at 2.32 or worse, so there is a factor of three of empty space between the two populations and the exact
# value here matters little. It is set nearer the failures than the successes to leave new work some room.
DEFAULT_THRESHOLD = 1.5

# Above this the lateral gradient is worth naming as the likely cause *when the wrap has already failed*. It is
# not a criterion on its own: `background_jungle_ruin_tiling.png` wraps at 0.69/0.47 carrying a tilt of -3.6,
# and `middle_east_ruin_tiling.png` wraps at 0.63/0.52 carrying +4.2. Content is allowed to be heavier on one
# side; what cannot be tolerated is a gradient the two ends have to reconcile and cannot.
TILT_WORTH_MENTIONING = 4.0

# Interior pairs are subsampled rather than all measured; the median is stable long before the full set and a
# 3840-wide RGBA layer is a lot of columns to difference for a baseline.
BASELINE_SAMPLES = 512

# Raw differences this small, in 0-255 units, are invisible no matter how quiet the image is, so no ratio is
# computed for them. Some floor is necessary: a smooth image has almost no horizontal variation, its baseline
# collapses toward zero, and a fraction-of-a-unit join divides out to a large ratio. An unclouded sky wash
# scored 2.57 on a 0.5/255 join, and the cloud band scored 2.64 on a 1.9/255 join that was confirmed invisible
# by rendering the join and looking at it.
#
# The two floors differ because the two metrics measure differently visible things. `step` is a coherent offset
# held across the whole edge, which the eye finds easily, so it gets the tighter floor -- and the corpus allows
# it: everything that wraps has a raw step of 0.33 or less, everything that does not has 1.22 or more. `local`
# is scattered difference, which hides, so it tolerates more. Raising its floor to 2.0 costs nothing: every
# genuine failure in the corpus either fails on `step` anyway or has a raw local far above it, the retired
# non-tiling control being 21.7.
LOCAL_FLOOR = 2.0
STEP_FLOOR = 0.6


def columns(path):
    """Float columns as (W, H, C), premultiplied so a transparent pixel's colour cannot skew the difference."""
    image = Image.open(path)
    if image.mode not in ("RGB", "RGBA"):
        image = image.convert("RGBA" if "A" in image.mode or image.mode == "P" else "RGB")
    data = np.asarray(image).astype(np.float32)
    if data.shape[2] == 4:
        alpha = data[:, :, 3:4] / 255.0
        # Colour under a transparent pixel is undefined -- the keyer leaves whatever the despill produced --
        # so comparing it would invent a difference the viewer will never see. Alpha itself stays a channel of
        # its own: a seam where one side is solid and the other is cut out is a real seam.
        data = np.concatenate([data[:, :, :3] * alpha, data[:, :, 3:4]], axis=2)
    return np.transpose(data, (1, 0, 2))


def mean_abs(left, right):
    return float(np.abs(left - right).mean())


def lateral_tilt(cols):
    """How much brighter one side of the image is than the other, in 0-255 units.

    A layer lit from the side carries a left-to-right brightness ramp, and a ramp makes a seamless wrap
    arithmetically impossible: the two ends differ by the whole gradient and no crop can find a join that
    matches. Worse, it hides from the other two ratios once a crossfade is applied -- the feather absorbs the
    step into a short visible ramp and the join columns themselves then agree perfectly.

    It is reported rather than enforced. A large tilt does not by itself mean the image fails -- content is
    allowed to be heavier on one side, and two of the shipped backgrounds wrap perfectly while carrying one.
    It earns its place by explaining a failure that has already been measured, and when it does, the answer is
    never a longer feather: it is either `make_tileable.py --flatten` or, better, a prompt that does not ask
    for raking light across a strip that has to tile.
    """
    width = cols.shape[0]
    edge = max(1, width // 20)
    return float(cols[:edge].mean(axis=(0, 1)).mean() - cols[-edge:].mean(axis=(0, 1)).mean())


def seam_ratios(cols):
    """Two ratios, because one number misses half the seams there are.

    `local` is the mean absolute difference across the join over the median interior column difference. It
    catches a structural mismatch -- a wall that does not line up, a horizon at two heights.

    `step` is the same comparison made on the *signed* mean difference. It catches a coherent tonal shift: a
    join where one side is uniformly a little cooler than the other. Texture cancels under a signed mean, so
    the interior baseline goes to near zero and any consistent offset stands out against it, while under an
    absolute mean the same offset disappears into a busy image's noise. That is not hypothetical -- the first
    port-terminal ground strip cut by `make_tileable.py` scored a comfortable 0.83 on `local` while showing an
    obvious vertical tone step down the middle, because a +10/255 shift held across 2048 rows is invisible to
    an absolute mean and glaring to a viewer.

    The verdict is the worse of the two. They fail on different assets and neither subsumes the other.
    """
    width = cols.shape[0]
    if width < 4:
        return None
    step_index = max(1, (width - 1) // BASELINE_SAMPLES)
    interior = np.arange(0, width - 1, step_index)

    deltas = cols[interior + 1] - cols[interior]

    local_seam = mean_abs(cols[-1], cols[0])
    local_base = float(np.median(np.abs(deltas).mean(axis=(1, 2))))

    # Averaged down the column first, so texture cancels and only a whole-edge bias survives.
    step_seam = float(np.abs((cols[0] - cols[-1]).mean(axis=0)).mean())
    step_base = float(np.median(np.abs(deltas.mean(axis=1)).mean(axis=1)))

    def ratio(seam, base, floor):
        # An invisible difference is not a seam, whatever the baseline says about it.
        if seam < floor:
            return 0.0
        # A flat test card has a zero baseline, and any seam above the floor is infinitely worse than nothing.
        if base < 1e-6:
            return float("inf")
        return seam / base

    return ratio(local_seam, local_base, LOCAL_FLOOR), ratio(step_seam, step_base, STEP_FLOOR), lateral_tilt(cols)


def seam_ratio(cols):
    """The worse of the two ratios, for callers that want a single verdict."""
    ratios = seam_ratios(cols)
    return None if ratios is None else max(ratios[0], ratios[1])


def write_preview(path, out_dir):
    """The image rolled by half its width, so the wrap seam lands down the centre where it can be looked at."""
    image = Image.open(path)
    data = np.asarray(image)
    rolled = np.roll(data, data.shape[1] // 2, axis=1)
    out_dir.mkdir(parents=True, exist_ok=True)
    destination = out_dir / (pathlib.Path(path).stem + "_seam.png")
    Image.fromarray(rolled).save(destination)
    return destination


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("images", nargs="+", help="layer images to check")
    parser.add_argument("--threshold", type=float, default=DEFAULT_THRESHOLD, help=f"fail above this ratio (default {DEFAULT_THRESHOLD})")
    parser.add_argument("--preview", metavar="DIR", help="also write each image rolled by half width, seam centred")
    args = parser.parse_args(argv)

    preview_dir = pathlib.Path(args.preview) if args.preview else None
    failures = 0
    for name in args.images:
        path = pathlib.Path(name)
        try:
            ratios = seam_ratios(columns(path))
        except (OSError, ValueError) as exc:
            print(f"{path}: unreadable: {exc}", file=sys.stderr)
            failures += 1
            continue
        if ratios is None:
            print(f"{path}: too narrow to have a wrap")
            continue
        local, step, tilt = ratios
        ratio = max(local, step)
        failed = ratio > args.threshold
        verdict = "SEAM" if failed else "ok"
        # The gradient is only worth raising when there is a failure for it to explain.
        note = "  (lateral gradient)" if failed and abs(tilt) > TILT_WORTH_MENTIONING else ""
        print(f"{path}: local {local:.2f}  step {step:.2f}  tilt {tilt:+.1f}/255  -> {verdict}{note}")
        if failed:
            failures += 1
        if preview_dir is not None:
            print(f"    preview {write_preview(path, preview_dir)}")

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
