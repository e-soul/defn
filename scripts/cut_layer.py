#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Key a generated layer off its chroma backdrop and give it an alpha channel.

Every background stratum above the combat floor has to be transparent where the sky shows through, and neither
image model can produce that: `gen_art.py` returns JPEG, which has no alpha channel at all. So the layers are
generated over a flat chroma field in a hue the biome palette never uses, and the field is removed here.

    python scripts/cut_layer.py mid.jpg --key magenta -o mid.png
    python scripts/cut_layer.py mid.jpg --report --preview build/art/keyed

This is the step most likely to fail on style grounds rather than technical ones, which is why it is worth
being explicit about why it should work here. Chroma keying fights two things: soft edges and colour that
resembles the key. The DEFN house style removes both. Fills are broad and flat, boundaries are hard cel edges
carrying a near-black inked contour, and the palette for any one biome is deliberately narrow, so a key hue
can be chosen well outside it. What is left for the keyer is the inked contour itself, which is where a naive
implementation fails: a dark line against a saturated backdrop picks up a fringe of the key, and clipping that
fringe eats the line the whole style depends on.

So the matte is built in chroma rather than in RGB. Converting to YCbCr and measuring distance in the (Cb, Cr)
plane makes the decision on hue and saturation while ignoring luminance, and an inked contour is dark but
almost neutral -- far from any saturated key regardless of how dark it is. The distance is then normalised by
the key's own chroma length, so the thresholds mean "how much of this pixel is backdrop" rather than an
absolute number that has to be retuned per key colour.

The fringe is then *solved* rather than approximated: see `despill`. That combination matters more here than
it would elsewhere, because the API returns 4:2:0 JPEG -- chroma is stored at half resolution -- so the
backdrop bleeds a pixel or two into every contour before the file ever reaches this script. Luma is full
resolution and the matte is not, which is the one place this pipeline is fighting the file format rather than
the model.

The result is autocropped to its alpha bounding box and the offset recorded in a `.json` sidecar, so a stamp
that gets trimmed still knows where in the authored frame it belongs.

`--report` prints what the matte did -- how much of the frame survived, how many pixels landed in the soft
band -- and `--preview` writes the cut composited over a checkerboard, which is the only reliable way to see a
halo before it is composited over sky in the game and noticed six weeks later.
"""

import argparse
import json
import pathlib
import sys

import numpy as np
from PIL import Image

# Named keys, all far from any DEFN biome palette in the (Cb, Cr) plane. Magenta is the default because the
# palettes that matter most here -- rust, gunmetal, slate, amber, sand -- all sit on the warm-neutral side of
# chroma space, and nothing in the shipped set goes near it.
KEYS = {
    "magenta": (255, 0, 255),
    "green": (0, 255, 0),
    "cyan": (0, 255, 255),
    "blue": (0, 0, 255),
}

# Where a pixel stops being backdrop and starts being artwork, as a *fraction of the key's own chroma
# magnitude* rather than an absolute distance. The distinction is the whole ballgame. Blend a pixel f of the
# way toward a neutral artwork colour and its distance from the key is (1 - f) times the key's chroma length,
# so on magenta -- chroma length about 212 -- a half-and-half fringe pixel sits 106 units away. An absolute
# threshold of 64 called that fully opaque, which left a 1px magenta fringe the blend solve then declined to
# correct because it believed alpha was already 1. Measured on the first keyed container band, that fringe ran
# +168/255 of magenta against the artwork's own baseline. Normalised, the same pixel gets alpha 0.5 and is
# recovered exactly.
SOFT_LOW = 0.12
SOFT_HIGH = 0.88

# Below this alpha the blend solve is dividing by a number small enough that matte noise dominates the result,
# so the correction is eased out. Chosen well under the soft band's typical alpha so it never touches a pixel
# that is actually visible.
ALPHA_FLOOR = 0.15

# One pixel of shell is what 4:2:0 chroma subsampling contaminates, so one pixel is what comes off by default.
DEFAULT_ERODE = 1


def to_chroma(rgb):
    """(Cb, Cr) about the neutral point. Luminance is dropped on purpose: a dark inked line is not a dark key."""
    red, green, blue = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    luma = 0.299 * red + 0.587 * green + 0.114 * blue
    return np.stack([blue - luma, red - luma], axis=-1), luma


def detect_key(rgb):
    """Median of the frame's outer border. The backdrop is what surrounds the artwork, whatever hue was asked for."""
    border = np.concatenate(
        [
            rgb[:8].reshape(-1, 3),
            rgb[-8:].reshape(-1, 3),
            rgb[:, :8].reshape(-1, 3),
            rgb[:, -8:].reshape(-1, 3),
        ]
    )
    return tuple(float(v) for v in np.median(border, axis=0))


def matte(rgb, key, low, high):
    """Alpha from chroma distance to the key, normalised by the key's own chroma length."""
    chroma, _ = to_chroma(rgb)
    key_chroma, _ = to_chroma(np.asarray(key, dtype=np.float32).reshape(1, 1, 3))
    key_vector = key_chroma[0, 0]
    key_length = float(np.linalg.norm(key_vector))
    distance = np.linalg.norm(chroma - key_vector, axis=-1) / max(key_length, 1e-6)
    alpha = np.clip((distance - low) / max(high - low, 1e-6), 0.0, 1.0)
    # Smoothstep rather than a linear ramp: it flattens the response at both ends, so a nearly-pure backdrop
    # pixel goes fully transparent and a nearly-solid one goes fully opaque instead of hovering at 0.02.
    alpha = alpha * alpha * (3.0 - 2.0 * alpha)
    return alpha, distance, key_vector


def despill(rgb, alpha, key):
    """Recover the artwork's own colour on the fringe by solving the blend the backdrop made, rather than
    approximating it away.

    An anti-aliased contour drawn over the backdrop arrives as `observed = art * a + key * (1 - a)`. The matte
    has already estimated `a` and the key is known, so `art` is not something to be guessed at with a channel
    clamp -- it can simply be solved for. Interior pixels have `a == 1` and come through untouched, which is
    the property a heuristic despill cannot offer: it desaturates legitimate colour that happens to lean
    toward the key hue, and on DEFN's inked contours it left a +66/255 magenta halo where this leaves none.

    Very low alpha is the one place the division is unstable -- at `a = 0.02` any error in the matte is
    multiplied fifty-fold -- so the correction is faded out below ALPHA_FLOOR. Those pixels are almost
    invisible once composited, so what they carry barely matters.
    """
    key_array = np.asarray(key, dtype=np.float32).reshape(1, 1, 3)
    a = alpha[..., None]
    safe = np.maximum(a, ALPHA_FLOOR)
    recovered = (rgb - key_array * (1.0 - a)) / safe
    # Below the floor, ease back toward the observed pixel instead of amplifying matte noise into confetti.
    blend = np.clip(a / ALPHA_FLOOR, 0.0, 1.0)
    return np.clip(recovered * blend + rgb * (1.0 - blend), 0.0, 255.0)


def erode_alpha(alpha, radius):
    """Pull the matte in by `radius` pixels, dropping the outermost shell of the silhouette.

    The API returns 4:2:0 JPEG, so chroma is stored at half resolution and the backdrop bleeds a pixel or two
    into every contour before the file is ever opened. That contamination cannot be solved away: the chroma
    that would say what those pixels really are was discarded by the encoder. On the hardened container band it
    left a plum cast along the top outline measuring +39/255 against the artwork's own baseline.

    Dropping the shell reduces the cast but does not remove it, and the returns fall off fast: measured on that
    band, +39.4 at radius 0, +25.3 at 1, +19.7 at 2. The contamination reaches further than one pixel because a
    4:2:0 encoder spreads chroma error across a whole subsampled block, so eroding until it is gone would eat
    the contour before it succeeded. Radius 1 is the default as the point where most of the cast is gone and
    almost none of the line is.

    Removing the rest needs the information the encoder threw away to be reconstructed rather than trimmed: a
    joint upsample of the half-resolution chroma guided by the full-resolution luma, which JPEG does preserve.
    That is a known technique and is the right next step if the residue proves visible in engine over a warm
    sky. It is not implemented here, and this knob is the cheap approximation in the meantime.
    """
    if radius <= 0:
        return alpha
    eroded = alpha.copy()
    for _ in range(radius):
        shifted = np.stack([
            np.pad(eroded, ((1, 0), (0, 0)), constant_values=0)[:-1],
            np.pad(eroded, ((0, 1), (0, 0)), constant_values=0)[1:],
            np.pad(eroded, ((0, 0), (1, 0)), constant_values=0)[:, :-1],
            np.pad(eroded, ((0, 0), (0, 1)), constant_values=0)[:, 1:],
        ])
        eroded = np.minimum(eroded, shifted.min(axis=0))
    return eroded


def checkerboard(shape, size=32):
    height, width = shape
    ys, xs = np.mgrid[0:height, 0:width]
    board = (((ys // size) + (xs // size)) % 2).astype(np.float32)
    return 96.0 + board * 64.0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="the generated layer, drawn over a chroma backdrop")
    parser.add_argument("--key", default=None, help=f"key colour: one of {', '.join(KEYS)}, 'R,G,B', or omitted to detect from the border")
    parser.add_argument("--low", type=float, default=SOFT_LOW, help=f"chroma distance below which a pixel is backdrop (default {SOFT_LOW})")
    parser.add_argument("--high", type=float, default=SOFT_HIGH, help=f"chroma distance above which a pixel is artwork (default {SOFT_HIGH})")
    parser.add_argument("--erode", type=int, default=DEFAULT_ERODE, help=f"pixels to pull the matte in, removing the 4:2:0 chroma fringe (default {DEFAULT_ERODE}, 0 disables)")
    parser.add_argument("--no-despill", action="store_true", help="skip the fringe solve, to see what it is doing")
    parser.add_argument("--no-crop", action="store_true", help="keep the full authored frame instead of trimming to the alpha bounds")
    parser.add_argument("-o", "--out", help="output path (default: alongside the source, _cut.png)")
    parser.add_argument("--preview", metavar="DIR", help="also write the cut over a checkerboard, for halo inspection")
    parser.add_argument("--report", action="store_true", help="print matte statistics")
    args = parser.parse_args(argv)

    source = pathlib.Path(args.source)
    rgb = np.asarray(Image.open(source).convert("RGB")).astype(np.float32)

    if args.key is None:
        key = detect_key(rgb)
    elif args.key in KEYS:
        key = KEYS[args.key]
    else:
        try:
            key = tuple(float(v) for v in args.key.split(","))
            if len(key) != 3:
                raise ValueError
        except ValueError:
            print(f"--key must name one of {', '.join(KEYS)} or be 'R,G,B'", file=sys.stderr)
            return 2

    alpha, distance, _ = matte(rgb, key, args.low, args.high)
    alpha = erode_alpha(alpha, args.erode)
    colour = rgb if args.no_despill else despill(rgb, alpha, key)

    if args.report:
        total = alpha.size
        print(f"{source}: {rgb.shape[1]}x{rgb.shape[0]}, key ({key[0]:.0f}, {key[1]:.0f}, {key[2]:.0f})")
        print(f"    transparent {np.count_nonzero(alpha <= 0.01) / total:6.1%}")
        print(f"    soft edge   {np.count_nonzero((alpha > 0.01) & (alpha < 0.99)) / total:6.1%}")
        print(f"    opaque      {np.count_nonzero(alpha >= 0.99) / total:6.1%}")
        # The failure this catches is not an edge artefact but a *palette* mistake: artwork drawn in a hue too
        # near the key comes out semi-transparent in the middle of a solid object, and it is invisible here
        # while being obvious in game. A run of container variants measured 0.8-0.9% ambiguous; the one that
        # had drawn several containers in mauve measured 3.5%, which is the whole signal.
        # Measured on the raw normalised distance, not on alpha: the smoothstep in `matte` deliberately pushes
        # mid-range values toward 0 and 1, so reading it off alpha reports 0.5% where the distance says 3.5%.
        ambiguous = np.count_nonzero((distance > args.low) & (distance < args.high)) / total
        verdict = "" if ambiguous < 0.015 else "   <- check for artwork drawn too near the key colour"
        print(f"    ambiguous   {ambiguous:6.2%}{verdict}")

    rgba = np.concatenate([colour, (alpha * 255.0)[..., None]], axis=-1)
    rgba = np.clip(rgba, 0, 255).astype(np.uint8)

    offset = [0, 0]
    if not args.no_crop:
        rows = np.nonzero(alpha.max(axis=1) > 0.01)[0]
        cols = np.nonzero(alpha.max(axis=0) > 0.01)[0]
        if rows.size and cols.size:
            offset = [int(cols[0]), int(rows[0])]
            rgba = rgba[rows[0] : rows[-1] + 1, cols[0] : cols[-1] + 1]
        else:
            print(f"{source}: the matte is empty -- every pixel read as backdrop", file=sys.stderr)
            return 1

    destination = pathlib.Path(args.out) if args.out else source.with_name(source.stem + "_cut.png")
    destination.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba).save(destination)
    sidecar = {
        "source": str(source),
        "key": [round(v) for v in key],
        "low": args.low,
        "high": args.high,
        "despill": not args.no_despill,
        "authored_size": [rgb.shape[1], rgb.shape[0]],
        "offset": offset,
        "size": [rgba.shape[1], rgba.shape[0]],
    }
    destination.with_suffix(".json").write_text(json.dumps(sidecar, indent=2) + "\n", encoding="utf-8")
    print(f"    wrote {destination} ({rgba.shape[1]}x{rgba.shape[0]} at offset {offset})")

    if args.preview:
        preview_dir = pathlib.Path(args.preview)
        preview_dir.mkdir(parents=True, exist_ok=True)
        board = checkerboard(rgba.shape[:2])[..., None]
        a = rgba[..., 3:4].astype(np.float32) / 255.0
        composite = rgba[..., :3].astype(np.float32) * a + board * (1.0 - a)
        path = preview_dir / (destination.stem + "_check.png")
        Image.fromarray(np.clip(composite, 0, 255).astype(np.uint8)).save(path)
        print(f"    preview {path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
