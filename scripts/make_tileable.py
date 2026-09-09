#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Cut a seamlessly wrapping strip out of an over-wide generated layer.

Asking an image model for a seamless tile and checking it by eye is how three of the five shipped backgrounds
came to have a visible wrap -- see `check_tiling.py` for the measurements. This script removes the model from
that part of the job. Generate the layer *wider* than it needs to be, then find the window inside it that
already wraps, because in a band of repeating subject matter -- container stacks, a treeline, a ridge, a haze
gradient -- such a window almost always exists.

    python scripts/make_tileable.py wide.jpg --width 3840 -o strip.png
    python scripts/make_tileable.py wide.jpg --width 3072 --tolerance 0.15 --report
    python scripts/make_tileable.py keyed.png --width 6000 --tolerance 0.30 --no-flatten -o band.png

The search is one line of arithmetic. Cropping `[x, x + W)` makes column `x + W - 1` the right edge, and when
`Parallax2D` repeats the strip that column is displayed immediately left of column `x`. So the crop wraps
cleanly exactly when column `x` continues naturally from column `x + W - 1` -- which is what column `x + W` in
the *source* already does, since it is the column the artwork itself put there. Score every candidate by how
far column `x` sits from column `x + W`. `--tolerance` widens the search to a range of crop widths around the
target, which usually finds a much better cut for the cost of a slightly different output size -- and among
the candidates that wrap about equally well, the *widest* is taken rather than the cheapest, because width is
period and period is the whole point. See `best_cut` for why the plain minimum is a trap.

Two flags matter for what you are cutting. A layer lit from one side cannot wrap at all, so by default the
low-order left-to-right brightness trend is fitted out first; `--no-flatten` keeps it. Always pass
`--no-flatten` for an alpha layer that has already been through `cut_layer.py`: its transparent regions are
premultiplied to zero, so a per-column brightness fit would be measuring the silhouette rather than the
lighting.

A short crossfade finishes the join. The `--feather` columns immediately past the crop are the artwork's own
continuation of its right edge, so ramping them over the first `--feather` columns of the crop blends the two
sides using real drawn content rather than a mirror or a blur. Keep it short: this is the one part of the
process that invents pixels, and a long ramp visibly smears the inked contours the house style depends on.

The output goes through `check_tiling.py`'s metric before it is written, and a result that still fails is
reported rather than quietly saved. When a source has no good cut anywhere in it -- a band with one unique
landmark in it and nothing repeating -- that is the honest answer, and the fix is a different generation, not
a longer feather.
"""

import argparse
import pathlib
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from check_tiling import DEFAULT_THRESHOLD, columns, lateral_tilt, seam_ratios  # noqa: E402

# Enough to hide the residual mismatch at a good cut, short enough not to smear an inked contour into mush.
# At 3840 wide this is well under half a percent of the strip.
DEFAULT_FEATHER = 16

# How much worse than the cheapest join a wider crop may be and still be preferred. A quarter is enough to
# recover most of the width the plain minimum throws away, and small enough that it never turns a passing
# wrap into a failing one.
WIDTH_SLACK = 1.25

# Rows sampled when scoring a candidate cut. The score is a mean down the whole column, and a few hundred rows
# estimate that mean to far better precision than the difference between two adjacent candidate widths, so
# reading every row of a 2048-tall source buys nothing and costs the search several minutes -- a 15% tolerance
# on an 8256-wide layer is over two thousand candidate widths. The final verdict is still measured on the full
# image by `check_tiling.py`; this stride only chooses where to cut.
SEARCH_ROWS = 384

# Candidate widths evaluated across the tolerance range. A wide tolerance on a wide source produces thousands
# of candidates, and neighbouring widths differ by one column -- far below what distinguishes a good cut from a
# bad one -- so stepping through them costs minutes and buys nothing. The chosen width is still exact; this only
# limits how many are tried.
SEARCH_WIDTHS = 1200


def flatten_tilt(data, degree=1):
    """Remove the low-order left-to-right brightness trend, per channel.

    A strip lit from the side cannot wrap: its two ends differ by the whole gradient, and no choice of crop
    changes that. Feathering does not save it either -- the ramp simply concentrates the mismatch into a short
    band whose end reads as an edge, which is exactly what happened to the first port-terminal ground.

    The correction fits a low-order polynomial to each channel's per-column mean and subtracts it, restoring
    the overall mean so nothing gets darker or brighter overall. Degree 1 is the default because on the two
    port-terminal grounds the quadratic coefficient came out at zero -- degree 2 gave results identical to
    degree 1 to the tenth of a unit -- so the extra freedom bought nothing and could only start chasing
    content. A mean is used rather than a median: the median was tried and was worse on both, taking a +20.5
    tilt to -25.7 where the mean took it to -13.3.

    What it fixes and what it does not is worth being precise about, because the two cases look the same in
    the tilt number and are not the same problem:

      * A genuine smooth illumination ramp is removed almost exactly. The second port-terminal ground went
        from +20.2 to -1.5 and then cut to a 0.82/0.53 wrap, which is the class of the best shipped asset.
      * A large directional shadow -- the first ground carries a hard diagonal light wedge across a third of
        the frame -- is *content*, not a trend, and no low-order fit represents it. That one only reached
        -13.3, and raising the degree until it did would have meant erasing the artwork's own lighting.

    So this is a repair for an asset that is otherwise good, not a way to accept one that is not. The real fix
    is upstream: a strip that has to tile must be lit flatly across its width, because a raking light also
    means the shadow directions at the two ends disagree, and no per-column correction addresses that.
    """
    height, width = data.shape[:2]
    xs = np.linspace(-1.0, 1.0, width, dtype=np.float64)
    out = data.copy()
    for channel in range(data.shape[2]):
        column_level = data[:, :, channel].mean(axis=0).astype(np.float64)
        trend = np.polyval(np.polyfit(xs, column_level, degree), xs)
        out[:, :, channel] += (trend.mean() - trend).astype(np.float32)[None, :]
    return np.clip(out, 0.0, 255.0)


def load(path):
    image = Image.open(path)
    if image.mode not in ("RGB", "RGBA"):
        image = image.convert("RGB")
    return np.asarray(image).astype(np.float32)


def scoring_view(data):
    """What the search should compare: premultiplied colour, plus alpha as its own channel.

    Colour under a transparent pixel is invisible, and on a keyed layer it is whatever the despill happened to
    leave there. Scoring on raw RGB therefore spends the search optimising a difference nobody can see -- on the
    cloud band it reported a raw edge cost of 22/255 for a cut whose real, premultiplied wrap was perfect, and
    a cost of 2.85 for one that was not. This makes the search agree with the verdict `check_tiling.py` gives.
    """
    if data.shape[2] != 4:
        return data
    alpha = data[:, :, 3:4] / 255.0
    return np.concatenate([data[:, :, :3] * alpha, data[:, :, 3:4]], axis=2)


def best_cut(data, target, tolerance, slack=WIDTH_SLACK):
    """The widest crop that still wraps well, scored on how far column x sits from column x + width.

    Taking the globally cheapest (x, width) is the obvious rule and it is the wrong one. A narrower width
    leaves more candidate positions to search, so it wins on cost by sheer number of tries: asked for 6000
    with a 30% tolerance, the plain minimum picked 4204 -- the shortest width allowed -- purely because the
    range 4200-7800 gave it the most chances. Width is period, and period is the entire reason this pipeline
    exists, so buying a slightly better join by making the tile a third shorter is a bad trade.

    So the cheapest cost is found first, and then the *widest* width whose own best cost is within `slack` of
    it is taken. The join stays in the same class and the tile stays as long as the source can make it.
    """
    source_width = data.shape[1]
    low = max(8, int(round(target * (1.0 - tolerance))))
    high = min(source_width - 1, int(round(target * (1.0 + tolerance))))
    if high < low:
        return None

    # Ceiling division, so a band of 757 rows actually samples every second row rather than every row.
    stride = max(1, -(-data.shape[0] // SEARCH_ROWS))
    sampled = scoring_view(data)[::stride]

    width_step = max(1, -(-(high - low + 1) // SEARCH_WIDTHS))

    per_width = []
    for width in range(low, high + 1, width_step):
        span = source_width - width
        if span < 1:
            continue
        # Every candidate x at once: column x against column x + width, averaged down the sampled rows.
        head = sampled[:, :span]
        tail = sampled[:, width : width + span]
        cost = np.abs(head - tail).mean(axis=(0, 2))
        x = int(np.argmin(cost))
        per_width.append((x, width, float(cost[x])))
    if not per_width:
        return None

    cheapest = min(c for _, _, c in per_width)
    affordable = [entry for entry in per_width if entry[2] <= cheapest * slack + 1e-6]
    return max(affordable, key=lambda entry: entry[1])


def crossfade(data, x, width, feather):
    """Crop [x, x+width), ramping the artwork's own continuation of the right edge over the left edge."""
    crop = data[:, x : x + width].copy()
    if feather <= 0:
        return crop
    available = data.shape[1] - (x + width)
    feather = min(feather, available, width // 4)
    if feather <= 0:
        return crop
    tail = data[:, x + width : x + width + feather]
    ramp = np.linspace(1.0, 0.0, feather, dtype=np.float32)[None, :, None]
    crop[:, :feather] = tail * ramp + crop[:, :feather] * (1.0 - ramp)
    return crop


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="the over-wide generated layer")
    parser.add_argument("--width", type=int, required=True, help="target strip width in pixels")
    parser.add_argument("--tolerance", type=float, default=0.0, help="search widths within this fraction of the target (default 0, exact)")
    parser.add_argument("--no-flatten", action="store_true", help="keep the lateral brightness gradient instead of fitting it out")
    parser.add_argument("--flatten-degree", type=int, default=1, help="polynomial degree for the gradient fit (default 1; higher starts chasing content)")
    parser.add_argument("--feather", type=int, default=DEFAULT_FEATHER, help=f"crossfade columns (default {DEFAULT_FEATHER}, 0 disables)")
    parser.add_argument("--threshold", type=float, default=DEFAULT_THRESHOLD, help="fail if the result's seam ratio exceeds this")
    parser.add_argument("-o", "--out", help="output path (default: alongside the source, _tileable.png)")
    parser.add_argument("--report", action="store_true", help="measure and print, write nothing")
    args = parser.parse_args(argv)

    source = pathlib.Path(args.source)
    data = load(source)
    if data.shape[1] <= args.width:
        print(f"{source}: {data.shape[1]}px wide, needs to exceed the {args.width}px target to have anything to search", file=sys.stderr)
        return 1

    if not args.no_flatten:
        before = lateral_tilt(np.transpose(data, (1, 0, 2)))
        data = flatten_tilt(data, args.flatten_degree)
        after = lateral_tilt(np.transpose(data, (1, 0, 2)))
        print(f"{source}: lateral tilt {before:+.1f} -> {after:+.1f} /255")

    cut = best_cut(data, args.width, args.tolerance)
    if cut is None:
        print(f"{source}: no candidate width fits inside the source", file=sys.stderr)
        return 1
    x, width, cost = cut
    print(f"{source}: cut x={x} width={width} (target {args.width}), raw edge cost {cost:.2f}/255")

    crop = crossfade(data, x, width, args.feather)
    out_image = Image.fromarray(np.clip(crop, 0, 255).astype(np.uint8))

    destination = pathlib.Path(args.out) if args.out else source.with_name(source.stem + "_tileable.png")
    if args.report:
        # The metric reads a file, and the point of --report is not to write one.
        scratch = destination.with_suffix(".probe.png")
        out_image.save(scratch)
        local, step, tilt = seam_ratios(columns(scratch))
        scratch.unlink()
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        out_image.save(destination)
        local, step, tilt = seam_ratios(columns(destination))
        print(f"    wrote {destination}")

    ratio = max(local, step)
    verdict = "ok" if ratio <= args.threshold else "SEAM"
    print(f"    local {local:.2f}  step {step:.2f}  tilt {tilt:+.1f}/255  -> {verdict}")
    return 0 if ratio <= args.threshold else 1


if __name__ == "__main__":
    sys.exit(main())
