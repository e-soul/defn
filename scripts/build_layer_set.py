#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Build a biome's shipped background layers from its tileable sources, and print the level data to go with them.

This is the last step of the layer pipeline and the only one that is not per-image. `gen_art.py` produces
candidates, `cut_layer.py` keys the alpha-bearing ones, `make_tileable.py` cuts each keeper into a strip that
wraps -- and then this takes the chosen strips, sizes them for the game, seats the standing bands with a
contact shadow, and reports the geometry the level file needs.

    python scripts/build_layer_set.py defn/art/layer_sets/port_terminal.json
    python scripts/build_layer_set.py defn/art/layer_sets/port_terminal.json --check

It exists because that sizing is arithmetic nobody should be doing by hand twice. A layer is authored as a
fraction of viewport height and stored at a multiple of it; a contact shadow hangs *below* the artwork, so the
layer's bottom edge is no longer where its content stands, and both `height_ratio` and `bottom_ratio` shift by
an amount that depends on the shadow depth. Both numbers were computed by hand during the port-terminal spike
and one of them was wrong, which is exactly the kind of mistake that shows up in the game as "that looks a bit
off" rather than as an error.

The manifest is the authoring source. Layer order in it is **draw order, back to front**, which for a
belt-scroller means the ground comes *before* whatever stands on it -- a band planted on the floor is drawn
onto the floor, exactly as a unit sprite is. Getting that backwards silently clips the standing band at the
ground's far edge, and reads as the band being too small rather than as a layering bug.

Each layer takes:

  * `source`     the tileable strip, usually under `build/art/<biome>/`
  * `screen_height`  the artwork's drawn height, as a fraction of viewport height
  * `base_ratio` where the artwork's bottom edge sits, as a fraction of viewport height from the top
  * `scroll_scale`   camera scroll multiplier; the floor and everything standing on it must be 1.0
  * `autoscroll` optional horizontal drift in pixels per second, independent of the camera -- weather only
  * `density`    stored pixels per screen pixel. 2 for anything with hard edges, 1 for a smooth wash
  * `opaque`     true to drop the alpha channel, for layers nothing shows through
  * `contact_shadow`  optional `{depth, alpha, colour}`, appended below the artwork

Sources live under `build/`, which is not committed, so this rebuilds the shipped PNGs from intermediates
rather than from nothing. What is committed and what actually records the work is the prompt in
`defn/art/prompts/<biome>/`, the `.json` sidecar `gen_art.py` wrote beside each generation, this manifest, and
the shipped asset. Regenerating from scratch means going back to the prompts.

`--check` verifies the existing outputs and their wrap without writing anything.
"""

import argparse
import json
import pathlib
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from check_tiling import DEFAULT_THRESHOLD, columns, seam_ratios  # noqa: E402

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent


def content_crop(image):
    """Trim to the alpha bounding box, so `screen_height` measures the artwork rather than its padding."""
    alpha = np.asarray(image)[..., 3]
    if alpha.min() >= 250:
        return image
    rows = np.nonzero(alpha.max(axis=1) > 2)[0]
    if rows.size == 0:
        return image
    return image.crop((0, int(rows[0]), image.width, int(rows[-1]) + 1))


def blur_columns(field, radius):
    """Box blur across the width, twice, which is close enough to a gaussian and costs two cumulative sums."""
    if radius < 1:
        return field
    for _ in range(2):
        padded = np.pad(field, ((0, 0), (radius + 1, radius)), mode="edge")
        totals = np.cumsum(padded, axis=1)
        field = (totals[:, 2 * radius + 1 :] - totals[:, : -(2 * radius + 1)]) / (2 * radius + 1)
    return field


def contact_shadow(image, depth_fraction, alpha, colour, seed=20260909):
    """Append a soft shadow below the artwork so a standing band seats onto the floor instead of abutting it.

    It has to be far fainter than instinct suggests. The base of a band like a container run is a long
    horizontal line, and *any* consistent darkening along it reads as a drawn rule rather than as shading. The
    first attempt used depth 0.055 at alpha 0.42 in near-black and additionally darkened the artwork's own
    bottom rows by up to 30%; on screen that was a 45px dark stripe across the whole width, and it was the
    first thing anyone noticed. What works is roughly a third of that depth, alpha under 0.2, a colour sampled
    from the surface the shadow falls on rather than from black, and the artwork itself left untouched.

    The lower boundary is jittered with a slow wander, because a shadow that ends on a straight line has
    simply moved the problem down a few pixels.

    It follows each silhouette's own foot rather than the bottom of the canvas, which matters as soon as a band
    stops being a continuous run. The first version appended a full-width rectangle of shading below the
    artwork -- correct for a quay full of containers, where the band really is opaque edge to edge, and wrong
    for anything drawn as separated masses, where it paints a dark bar across the empty gaps the layer exists
    to have. Here the shadow starts at each *column's lowest opaque pixel*, so a boulder gets a pool at its own
    base, a gap between two boulders gets nothing, and a mass standing further back gets its shadow higher up
    the frame where its feet actually are. On a fully opaque band every column's base is the bottom row and
    this reduces to the rectangle it replaces.

    The field is then blurred horizontally, which is what makes it read as a pool rather than a per-column
    curtain: the shadow spreads a little past the ends of each mass and eases across neighbouring columns whose
    bases sit at different heights. It is never allowed to darken the artwork itself -- the `1 - art_alpha`
    factor -- so an overhang casts onto the ground behind it and not onto its own face.
    """
    data = np.asarray(image).astype(np.float32)
    height, width = data.shape[:2]
    depth = max(4, int(round(height * depth_fraction)))

    grown = np.zeros((height + depth, width, 4), dtype=np.float32)
    grown[:height] = data
    art_alpha = grown[..., 3] / 255.0

    rng = np.random.default_rng(seed)
    wander = np.cumsum(rng.normal(0.0, 1.0, width))
    # `same` returns max(len(signal), len(kernel)) samples, so the window has to fit inside a narrow strip.
    window = max(1, min(64, width))
    wander = np.convolve(wander, np.ones(window) / window, mode="same")
    wander = wander / (np.abs(wander).max() + 1e-6)
    extent = depth * (0.62 + 0.38 * (0.5 + 0.5 * wander))

    # Each column's contact point: the lowest pixel that is solidly artwork. Columns holding nothing get no
    # shadow of their own and pick up only what the horizontal blur carries in from their neighbours.
    solid = art_alpha > 0.5
    occupied = solid.any(axis=0)
    base = (height + depth - 1) - np.argmax(solid[::-1], axis=0)

    rows = np.arange(height + depth)[:, None]
    below = rows - base[None, :]
    fade = np.clip(1.0 - below / np.maximum(extent[None, :], 1.0), 0.0, 1.0)
    field = np.where((below >= 0) & occupied[None, :], fade**1.8, 0.0)
    field = blur_columns(field, max(1, int(round(depth * 0.6))))

    shadow = np.clip(field, 0.0, 1.0) * alpha * (1.0 - art_alpha)
    out_alpha = np.clip(art_alpha + shadow, 0.0, 1.0)
    tint = np.asarray(colour, dtype=np.float32)[None, None, :]
    out_rgb = (grown[..., :3] * art_alpha[..., None] + tint * shadow[..., None]) / np.maximum(out_alpha[..., None], 1e-6)

    composed = np.concatenate([out_rgb, out_alpha[..., None] * 255.0], axis=2)
    return Image.fromarray(np.clip(composed, 0, 255).astype(np.uint8)), depth / height


def resize_wrapping(image, width, height):
    """Resize a strip while preserving its horizontal wrap.

    A plain resize quietly breaks a tileable strip. Resampling filters have no data past the left and right
    borders, so they clamp there, and the two edges are reconstructed from different neighbourhoods than a
    wrapped image would give -- the join stops matching. It is a small effect that hides inside the busier
    layers, and it is glaring on the cloud band, which is mostly transparent and so has almost no baseline to
    hide in.

    The strip is therefore tiled three times, resized, and the middle third taken. Padding by a fixed number of
    columns instead is the obvious approach and is worse: the padded width does not scale to an integer, so the
    rounding shifts the content by a fraction of a pixel and misaligns the very join it was meant to protect.
    Tiling three times keeps the scale factor exactly `width / source_width`, with no rounding anywhere.
    """
    source_width, source_height = image.size
    wide = Image.new(image.mode, (source_width * 3, source_height))
    for index in range(3):
        wide.paste(image, (source_width * index, 0))
    resized = wide.resize((width * 3, height), Image.LANCZOS)
    return resized.crop((width, 0, width * 2, height))


def build(manifest_path, check_only):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    viewport = float(manifest.get("viewport_height", 1080))
    out_dir = REPO_ROOT / manifest["output"]
    if not check_only:
        out_dir.mkdir(parents=True, exist_ok=True)

    print(f"{manifest['name']}  ->  {manifest['output']}  (viewport height {viewport:.0f})\n")
    print(f"{'layer':8} {'stored':>13} {'tile on screen':>15} {'scroll':>7} {'height_ratio':>13} {'bottom_ratio':>13}")

    emitted = []
    failures = 0
    for layer in manifest["layers"]:
        name = layer["name"]
        destination = out_dir / f"{name}.png"
        screen_height = float(layer["screen_height"])
        base_ratio = float(layer["base_ratio"])

        if check_only:
            if not destination.exists():
                print(f"{name:8} MISSING {destination}")
                failures += 1
                continue
            image = Image.open(destination).convert("RGBA")
            shadow_fraction = 0.0
            if "contact_shadow" in layer:
                shadow_fraction = float(layer["contact_shadow"]["depth"])
        else:
            source = REPO_ROOT / layer["source"]
            if not source.exists():
                print(f"{name:8} MISSING SOURCE {layer['source']}")
                failures += 1
                continue
            image = content_crop(Image.open(source).convert("RGBA"))
            shadow_fraction = 0.0
            if "contact_shadow" in layer:
                shadow = layer["contact_shadow"]
                image, shadow_fraction = contact_shadow(image, float(shadow["depth"]), float(shadow["alpha"]), shadow["colour"])
            target = int(round(viewport * screen_height * (1.0 + shadow_fraction) * int(layer.get("density", 2))))
            image = resize_wrapping(image, max(1, int(image.width * target / image.height)), target)
            if layer.get("opaque"):
                image = image.convert("RGB")
            image.save(destination, optimize=True)

        # The shadow hangs below the artwork, so the layer is taller than what it draws and its bottom edge is
        # lower than where that artwork stands. Both numbers move, and by different amounts.
        height_ratio = screen_height * (1.0 + shadow_fraction)
        bottom_ratio = base_ratio + screen_height * shadow_fraction
        aspect = image.width / image.height
        print(
            f"{name:8} {image.width:5}x{image.height:<7} {aspect * viewport * height_ratio:14.0f}px"
            f" {layer['scroll_scale']:7.2f} {height_ratio:13.3f} {bottom_ratio:13.3f}"
        )

        # A waiver is a reviewed decision recorded in the manifest, not a threshold anyone can drift. It exists
        # because `check_tiling`'s ratios divide by how much the image varies from column to column, and a
        # layer of separated masses over transparency varies almost not at all -- the desert rock band's
        # interior baseline is 1.08 against the container band's 3.97, so the same absolute join reads nearly
        # four times worse. The number that matters there is the absolute difference across the join, which the
        # ratios deliberately do not report; the manifest note has to give it, and the layer has to have been
        # looked at. Every other layer still fails the build outright.
        local, step, _ = seam_ratios(columns(destination))
        if max(local, step) > DEFAULT_THRESHOLD:
            allowed = layer.get("accepted_seam", {})
            if local <= float(allowed.get("local", 0.0)) and step <= float(allowed.get("step", 0.0)):
                print(f"{'':8} seam local {local:.2f} step {step:.2f} -- accepted by the manifest")
            else:
                print(f"{'':8} SEAM local {local:.2f} step {step:.2f}")
                failures += 1

        drift = float(layer.get("autoscroll", 0.0))
        drift_field = f', "autoscroll": {drift:.1f}' if drift else ""
        emitted.append(
            f'{{"path": "res://{layer["res_path"]}", "scroll_scale": {layer["scroll_scale"]:.2f},'
            f' "height_ratio": {height_ratio:.3f}, "bottom_ratio": {bottom_ratio:.3f}{drift_field}}}'
        )

    print("\nPaste into the level's \"background_layers\" (order is draw order, back to front):\n")
    print("        \"background_layers\": [")
    for index, line in enumerate(emitted):
        print(f"            {line}{',' if index < len(emitted) - 1 else ''}")
    print("        ],")

    if failures:
        print(f"\n{failures} layer(s) failed.", file=sys.stderr)
    return 1 if failures else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("manifest", help="layer set manifest, e.g. defn/art/layer_sets/port_terminal.json")
    parser.add_argument("--check", action="store_true", help="verify the existing outputs and their wrap, write nothing")
    args = parser.parse_args(argv)
    return build(pathlib.Path(args.manifest), args.check)


if __name__ == "__main__":
    sys.exit(main())
