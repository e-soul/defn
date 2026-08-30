#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Read a critical-purse bisection of the tempo lab.

Each line is one (engagement, composition) cell: the smallest starting purse at which that composition wins the
engagement half the time. Unlike a win rate this never saturates, and it is denominated in the same energy the
player spends -- the same reason `scons matrix` reports a critical budget.

    scons sim scenario=res://scenarios/tempo_lab.json seeds=25 bisect=yes out=res://build/purse.jsonl
    python scripts/analyze_tempo.py defn/build/purse.jsonl
    python scripts/analyze_tempo.py defn/build/purse.jsonl --baseline before.jsonl

Deliberately **not** an SII. The matrix decomposition attributes everything it cannot explain to the matchup term,
so folding an economy into it would put tempo, leaks and matchup into one number and call the total diversity. This
reports purses and lets you read them.
"""

import argparse
import json
import pathlib
import sys

# Reference spending heuristics rather than compositions: they vary when to spend, never what to buy.
REFERENCE = {"greedy", "defensive", "patience", "scripted"}


def load(path):
    cells = {}
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        cell = json.loads(line)
        cells[(cell["level_id"], cell["policy"])] = cell
    return cells


def ordered(cells, index):
    seen = []
    for key in cells:
        if key[index] not in seen:
            seen.append(key[index])
    return seen


def purse_of(cell):
    """The reported purse, or None when even the ceiling lost -- an unbounded cell carries no number."""
    return cell["purse"] if cell["bounded"] else None


def format_purse(cell, baseline_cell=None):
    if cell is None:
        return "     -"
    value = purse_of(cell)
    if value is None:
        return "  none"
    if baseline_cell is None:
        return "%6d" % value
    was = purse_of(baseline_cell)
    if was is None:
        return "%6d*" % value
    return "%6d %+d" % (value, value - was)


def report(cells, baseline):
    engagements = ordered(cells, 0)
    policies = ordered(cells, 1)
    compositions = [p for p in policies if p not in REFERENCE]

    width = max(len(p) for p in policies) + 2
    header = "critical purse -- the smallest starting energy that wins half the time"
    print(header)
    print("lower is better; 'none' means even the ceiling lost, so the cell carries no number")
    if baseline:
        print("second column of each pair is the change against the baseline")
    print()
    print("%-*s%s" % (width, "composition", "".join("%-14s" % e for e in engagements)))
    print("-" * (width + 14 * len(engagements)))

    def row(name):
        line = "%-*s" % (width, name)
        for engagement in engagements:
            cell = cells.get((engagement, name))
            base = baseline.get((engagement, name)) if baseline else None
            line += "%-14s" % format_purse(cell, base)
        return line

    for name in compositions:
        print(row(name))
    reference = [p for p in policies if p in REFERENCE]
    if reference:
        print()
        for name in reference:
            print(row(name) + "   (reference heuristic)")

    print()
    print("cheapest answer per engagement")
    winners = []
    for engagement in engagements:
        ranked = [(purse_of(cells[(engagement, c)]), c) for c in compositions
                  if (engagement, c) in cells and purse_of(cells[(engagement, c)]) is not None]
        if not ranked:
            print("  %-18s (every composition unbounded)" % engagement)
            continue
        ranked.sort()
        best_purse, best = ranked[0]
        # Anything within 5% of the cheapest is a tie the player could not feel.
        tied = [c for p, c in ranked if p <= best_purse * 1.05]
        winners.append(best)
        dearest = ranked[-1]
        print("  %-18s %-20s %4d   (dearest %s at %d, %.1fx)"
              % (engagement, "/".join(tied), best_purse, dearest[1], dearest[0], dearest[0] / max(best_purse, 1)))

    print()
    print("  distinct cheapest answers across %d engagements: %d" % (len(engagements), len(set(winners))))
    unbounded = sum(1 for c in compositions for e in engagements
                    if (e, c) in cells and purse_of(cells[(e, c)]) is None)
    print("  unbounded cells: %d of %d" % (unbounded, len(compositions) * len(engagements)))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("path", help="JSONL written by scons sim ... bisect=yes")
    parser.add_argument("--baseline", help="an earlier bisection to difference against")
    arguments = parser.parse_args()

    cells = load(arguments.path)
    if not cells:
        print("no cells in %s" % arguments.path, file=sys.stderr)
        return 1
    baseline = load(arguments.baseline) if arguments.baseline else None
    report(cells, baseline)
    return 0


if __name__ == "__main__":
    sys.exit(main())
