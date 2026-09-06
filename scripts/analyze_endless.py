#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Read the three things that decide whether the endless mode is any good out of `scons endless`.

The mode's quality is entirely a question of whether the escalation `r`, the bounty decay `d` and the drift schedule
are right, and none of the three can be judged by playing a handful of runs. Each line of the sweep is one run:
`(escalation, bounty_decay, policy, seed) -> waves_reached`, plus the energy held when each wave opened.

    python scripts/analyze_endless.py defn/build/endless.jsonl
    python scripts/analyze_endless.py defn/build/endless.jsonl --target-wave 40
    python scripts/analyze_endless.py defn/build/endless.jsonl --interval 18 --growth 1.008

Three readings, in the order they should be acted on:

1. **Run length.** The wave-reached distribution for a competent policy should centre on a 15-25 minute run. The
   script converts waves to minutes from the same interval schedule the generator uses, so the band is read in the
   unit the design states it in rather than in waves.

2. **Not solved.** No single friendly mix may reach the target wave across all seeds. This is the ship gate, and it
   is the reason the mode is worth building: a mode beaten by one composition is arithmetic forever. The script also
   prints the *spread* -- the best mix's median over the field's -- because a mix that merely lasts three times
   longer than every other is solved in practice whatever the target wave says.

   **If this gate fails, fix the drift keyframes before touching `r`.** A faster ramp shortens every run including
   the winning one; it does not make a solved mode unsolved.

3. **Economy.** Energy held when each wave opens should stay bounded. An upward trend means `d` is too weak and the
   bounty snowball is live. Reported as the slope of a least-squares line through the trace, averaged over runs: a
   positive slope is the snowball.

Pure stdlib on purpose: the rest of scripts/ has no third-party dependency and neither should this.
"""

import argparse
import collections
import json
import pathlib
import statistics
import sys

# What the design asks a competent run to last. Minutes, not waves: the wave count is an artefact of the interval.
TARGET_MINUTES = (15.0, 25.0)

# A mix that outlasts the field by more than this is solved in practice, whatever the target wave says.
SOLVED_SPREAD = 2.0

# Waves a run must have opened before its energy trace says anything about trend. A run that died on wave 4 has two
# points, and two points always have a slope -- reading one as a snowball is reading noise.
MINIMUM_ECONOMY_WAVES = 6


def load_runs(path):
    runs = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        if not stripped:
            continue
        try:
            runs.append(json.loads(stripped))
        except json.JSONDecodeError as error:
            print(f"{path}:{line_number}: not JSON ({error})", file=sys.stderr)
    return runs


def wave_start_seconds(wave, first_delay, interval, growth):
    """When a wave opens, from the same schedule `EndlessWaveGenerator::wave_start_time` walks."""
    start = first_delay
    for index in range(1, max(wave, 1)):
        start += interval * (growth ** (index - 1))
    return start


def is_mix(policy):
    """The compositions the not-solved gate is read over, as `DefnEndlessRunner` labels them."""
    return policy.startswith("mono:") or policy.startswith("mix:")


def group_by(runs, key):
    grouped = collections.defaultdict(list)
    for run in runs:
        grouped[key(run)].append(run)
    return grouped


def median_waves(runs):
    return statistics.median(run["waves_reached"] for run in runs)


def report_run_length(runs, first_delay, interval, growth):
    print("run length -- the wave a competent policy reaches, in minutes")
    print("knobs            policy        runs  waves med/min/max      minutes  band")
    for (escalation, decay), knob_runs in sorted(group_by(runs, lambda run: (run["escalation"], run["bounty_decay"])).items()):
        for policy, policy_runs in sorted(group_by(knob_runs, lambda run: run["policy"]).items()):
            if is_mix(policy):
                continue
            waves = sorted(run["waves_reached"] for run in policy_runs)
            median = statistics.median(waves)
            minutes = wave_start_seconds(int(median), first_delay, interval, growth) / 60.0
            band = "PASS" if TARGET_MINUTES[0] <= minutes <= TARGET_MINUTES[1] else ("SHORT" if minutes < TARGET_MINUTES[0] else "LONG")
            print(f"r={escalation:.3f} d={decay:.3f}  {policy:<12}  {len(policy_runs):>4}  {median:>5.0f} {waves[0]:>3} {waves[-1]:>4}  {minutes:>11.1f}  {band}")
    print()


def report_not_solved(runs, target_wave):
    print(f"not solved -- no single friendly mix reaches wave {target_wave} on every seed  [SHIP GATE]")
    print("knobs            mix                     runs  med  max  reached target")
    failed = False
    for (escalation, decay), knob_runs in sorted(group_by(runs, lambda run: (run["escalation"], run["bounty_decay"])).items()):
        mixes = {policy: policy_runs for policy, policy_runs in group_by(knob_runs, lambda run: run["policy"]).items() if is_mix(policy)}
        if not mixes:
            continue

        medians = {policy: median_waves(policy_runs) for policy, policy_runs in mixes.items()}
        for policy, policy_runs in sorted(mixes.items(), key=lambda entry: -medians[entry[0]]):
            waves = [run["waves_reached"] for run in policy_runs]
            every_seed = all(wave >= target_wave for wave in waves)
            failed = failed or every_seed
            print(f"r={escalation:.3f} d={decay:.3f}  {policy:<22}  {len(waves):>4}  {medians[policy]:>3.0f}  {max(waves):>3}  {'SOLVED' if every_seed else 'no'}")

        best = max(medians.values())
        field = statistics.median(sorted(medians.values())[:-1]) if len(medians) > 1 else best
        spread = best / field if field > 0 else float("inf")
        verdict = "PASS" if spread <= SOLVED_SPREAD else "MISS"
        print(f"r={escalation:.3f} d={decay:.3f}  best mix outlasts the field by {spread:.2f}x  {verdict}")
    print()
    return not failed


def slope(values):
    """Least-squares slope of `values` against their index. Zero for anything shorter than two points."""
    if len(values) < 2:
        return 0.0
    mean_index = (len(values) - 1) / 2.0
    mean_value = statistics.fmean(values)
    numerator = sum((index - mean_index) * (value - mean_value) for index, value in enumerate(values))
    denominator = sum((index - mean_index) ** 2 for index in range(len(values)))
    return numerator / denominator if denominator else 0.0


def report_economy(runs):
    print("economy -- energy held when each wave opened; an upward slope is the bounty snowball")
    print("knobs            policy        mean slope  mean held  verdict")
    for (escalation, decay), knob_runs in sorted(group_by(runs, lambda run: (run["escalation"], run["bounty_decay"])).items()):
        for policy, policy_runs in sorted(group_by(knob_runs, lambda run: run["policy"]).items()):
            if is_mix(policy):
                continue
            traces = [run.get("energy_at_wave", []) for run in policy_runs]
            traces = [trace for trace in traces if len(trace) >= MINIMUM_ECONOMY_WAVES]
            if not traces:
                print(f"r={escalation:.3f} d={decay:.3f}  {policy:<12}  {'too short to read':>22}  (died inside {MINIMUM_ECONOMY_WAVES} waves)")
                continue
            mean_slope = statistics.fmean(slope(trace) for trace in traces)
            mean_held = statistics.fmean(statistics.fmean(trace) for trace in traces)
            verdict = "PASS" if mean_slope <= 0.0 else "SNOWBALL"
            print(f"r={escalation:.3f} d={decay:.3f}  {policy:<12}  {mean_slope:>10.3f}  {mean_held:>9.1f}  {verdict}")
    print()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("jsonl", type=pathlib.Path, help="the file `scons endless out=<path>` wrote")
    parser.add_argument("--target-wave", type=int, default=40, help="the wave the not-solved gate is read at")
    parser.add_argument("--first-delay", type=float, default=3.0, help="tuning.first_wave_delay from data/endless.json")
    parser.add_argument("--interval", type=float, default=18.0, help="tuning.wave_interval from data/endless.json")
    parser.add_argument("--growth", type=float, default=1.008, help="tuning.interval_growth from data/endless.json")
    arguments = parser.parse_args()

    runs = load_runs(arguments.jsonl)
    if not runs:
        print(f"{arguments.jsonl}: no runs", file=sys.stderr)
        return 1

    print(f"{len(runs)} run(s) from {arguments.jsonl}\n")
    report_run_length(runs, arguments.first_delay, arguments.interval, arguments.growth)
    not_solved = report_not_solved(runs, arguments.target_wave)
    report_economy(runs)
    return 0 if not_solved else 1


if __name__ == "__main__":
    sys.exit(main())
