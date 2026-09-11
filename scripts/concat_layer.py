#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Join several keyed rolls of the same sparse layer into one long strip.

A layer's on-screen period is `source_width * (screen height / content height)`, so a band the model insists
on drawing half a canvas deep can only buy period from width -- and 4:1 is the generation ceiling. Rolls that
end in empty margins can be abutted instead: every join is transparent-to-transparent, so the strip wraps by
construction, and four different arrangements in a row means the eye has four times as much to get through
before anything recurs. This is the scatter planner's job done by hand for one layer.

Segments are aligned on their lowest drawn row, because that is the ground they all lie on, and the canvas is
as tall as the deepest of them.

    python scripts/concat_layer.py a_cut.png b_cut.png c_cut.png d_cut.png -o strip.png

Only use rolls whose two side margins are genuinely empty -- the prompt has to ask for them and about half the
rolls will ignore it. A roll whose artwork reaches the frame edge cannot be a segment: the model draws the two
edges as continuations of each other, so the shape at the left edge is the other half of the one at the right,
and abutting it to a different roll cuts both in half. Cutting such a roll at an interior gap does not rescue
it either; a dense scatter has no column wide enough to cut in, and the measured join cost across two rolls of
the beach scatter ran 12 to 60 per 255 whichever pair was tried.
"""

import argparse

import numpy as np
from PIL import Image


def content_rows(image, threshold=2):
    alpha = np.asarray(image)[:, :, 3]
    rows = np.where((alpha > threshold).any(axis=1))[0]
    return int(rows[0]), int(rows[-1]) + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sources", nargs="+", help="keyed rolls of one layer, in the order they should appear")
    parser.add_argument("-o", "--out", required=True)
    parser.add_argument("--alpha", type=int, default=2, help="alpha above which a pixel counts as artwork")
    args = parser.parse_args()

    pieces = []
    for path in args.sources:
        image = Image.open(path).convert("RGBA")
        top, bottom = content_rows(image, args.alpha)
        pieces.append(image.crop((0, top, image.width, bottom)))
        print(f"{path}  {pieces[-1].width}x{pieces[-1].height}")

    height = max(p.height for p in pieces)
    width = sum(p.width for p in pieces)
    strip = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    x = 0
    for piece in pieces:
        strip.paste(piece, (x, height - piece.height))
        x += piece.width
    strip.save(args.out)
    print(f"-> {args.out}  {width}x{height}")


if __name__ == "__main__":
    main()
