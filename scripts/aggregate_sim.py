#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Summarise one or more simulator JSONL sweeps.

Each line of a sweep file is one complete match. This reduces a sweep to the numbers a balance decision is actually
made on: win rate, how long a clear takes, what it costs, and how much of the economy went unspent.

    python scripts/aggregate_sim.py defn/build/sweep.jsonl
    python scripts/aggregate_sim.py defn/build/*.jsonl --per-unit
"""

import argparse
import json
import pathlib
import statistics
import sys


def load_runs(paths):
    runs = []
    for path in paths:
        for line_number, line in enumerate(pathlib.Path(path).read_text(encoding="utf-8").splitlines(), start=1):
            line = line.strip()
            if not line:
                continue
            try:
                run = json.loads(line)
            except json.JSONDecodeError as error:
                print(f"{path}:{line_number}: not valid JSON ({error})", file=sys.stderr)
                continue
            run["_source"] = str(path)
            runs.append(run)
    return runs


def summarise(values):
    if not values:
        return "n/a"
    if len(values) == 1:
        return f"{values[0]:.1f}"
    return f"{statistics.mean(values):.1f} +/- {statistics.pstdev(values):.1f}"


def group_key(run):
    return run.get("level_id", "?"), run.get("policy", "?")


def report(runs, show_per_unit):
    groups = {}
    for run in runs:
        groups.setdefault(group_key(run), []).append(run)

    for (level_id, policy), group in sorted(groups.items()):
        victories = [run for run in group if run.get("victory")]
        undecided = [run for run in group if not run.get("decided")]
        win_rate = 100.0 * len(victories) / len(group)

        print(f"{level_id} / {policy}: {len(group)} run(s), {win_rate:.0f}% win rate")
        if undecided:
            print(f"  undecided (hit the time limit): {len(undecided)}")
        if victories:
            print(f"  clear time (s):      {summarise([run['clear_time_s'] for run in victories])}")
            print(f"  integrity left:      {summarise([float(run['remaining_integrity']) for run in victories])}")
            print(f"  level score:         {summarise([float(run['level_score']) for run in victories])}")
        print(f"  deployments:         {summarise([float(run['deployments_total']) for run in group])}")
        print(f"  energy spent:        {summarise([float(run['energy_spent']) for run in group])}")
        print(f"  energy left idle:    {summarise([run['energy_idle_integral'] for run in group])}")
        print(f"  peak enemies:        {summarise([float(run['peak_concurrent_enemies']) for run in group])}")
        print(f"  peak in 5s window:   {summarise([float(run['peak_window_5s']) for run in group])}")
        print(f"  base hits taken:     {summarise([float(len(run['leak_events'])) for run in group])}")

        if show_per_unit:
            totals = {}
            for run in group:
                for unit in run.get("per_unit", []):
                    entry = totals.setdefault(unit["unit_id"], {"spawned": 0, "damage_dealt": 0, "kills": 0, "deaths": 0})
                    for field in entry:
                        entry[field] += unit.get(field, 0)
            print("  per unit (summed over the sweep):")
            for unit_id, entry in sorted(totals.items()):
                print(f"    {unit_id:<10} spawned={entry['spawned']:<5} damage={entry['damage_dealt']:<7} "
                      f"kills={entry['kills']:<4} deaths={entry['deaths']}")
        print()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="+", help="JSONL sweep files written by `scons sim out=...`")
    parser.add_argument("--per-unit", action="store_true", help="also break the sweep down by unit")
    args = parser.parse_args()

    runs = load_runs(args.paths)
    if not runs:
        print("No runs found.", file=sys.stderr)
        return 1

    report(runs, args.per_unit)
    return 0


if __name__ == "__main__":
    sys.exit(main())
