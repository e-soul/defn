#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Decompose the payoff matrix written by `scons matrix` into balance, diversity and depth.

Balance is a property of units; diversity is a property of the payoff matrix. Comparing stat lines cannot tell you
whether the right answer changes with the question -- only the off-diagonal structure of

    M[i][j] = -log B*(friendly mix i, hostile mix j)

can, where `B*` is the smallest energy budget at which mix `i` beats hostile mix `j` half the time. Higher M means
`i` answers `j` more cheaply.

The two-way decomposition

    M[i][j] = mu + a[i] + b[j] + R[i][j]

splits that into unit power (`a`, the transitive axis -- all the power formula can express), content difficulty (`b`,
what "threat points" measures) and the matchup interaction `R`, which is the design target.

    python scripts/analyze_matrix.py defn/build/matrix.jsonl
    python scripts/analyze_matrix.py defn/build/matrix.jsonl --transpose   # is the content diverse?
    python scripts/analyze_matrix.py after.jsonl --baseline before.jsonl   # did a change earn its numbers?

**`SII` and the composition premium are both ratios, and a ratio improves when its denominator gets worse just as
readily as when its numerator gets better.** Both have been reported as wins here on the strength of a degradation.
The two are handled differently, because only one of them can be fixed within a single run:

- **The premium decomposes, so it is fixed outright.** `log premium(j) = (a[mix] - a[mono]) + (R[mix][j] -
  R[mono][j])`: `mu` and the column difficulty `b[j]` cancel, leaving a *level* term identical in every column and a
  *structural* term specific to this one. Nerfing the best mono globally lands wholly in the level term, so the gate
  counts structural columns only and needs no baseline. The `hound` column is the standing example -- a 19% raw
  premium that is level +34% and structural -11%, which is why it never moved however the roster changed.
- **`SII` cannot be, so it takes `--baseline`.** Whether `Var(R)` grew or `Var(a)` merely shrank is only visible
  against a previous run, so with a baseline the script prints both terms and warns when the ratio moved and the
  numerator did not.

Every gated number is also reported with a **bootstrap** interval: the seeds are resampled, the whole decomposition
is recomputed, and the 95% percentile interval says how much of a number was ever real. The seeds are drawn once per
replicate and applied to the entire matrix, because a seed is a whole-simulation seed and cells that shared one are
correlated. This matters most for the argmax-based gates -- the dead-slot count, the premium's column and winner
counts -- which are discontinuous in the data and have no analytic standard error.

The **dead-slot count is reported twice**: on the strict argmax, and on rows that are inside the noise floor of a
column's winner. The strict count is discontinuous, and it is also bounded below by the matrix's shape -- one argmax
per column means `max(rows - columns, 0)` rows are dead by arithmetic whatever the design does, which is why the
hostile side could never reach zero. The noise-tolerant count has no such floor and is the one to gate on.

With `--baseline`, the comparison is **paired**: both runs use the same deterministic seed list, so cells are
differenced seed by seed rather than mean against mean, and the seed noise the two runs share cancels instead of
adding. That is a much sharper instrument than two independent means -- it resolves changes several times smaller --
and it separates the two things a change can do to a row: shift it by the same amount in every column, which the
decomposition absorbs into `a[i]`, or restructure it, which is the only kind of change that can reach `R`.

`--transpose` flips the sign of M as well as the axes, so that "higher is better for the row player" holds in both
directions: forward, a row is a friendly mix and winning cheaply is good; transposed, a row is a hostile mix and
costing the player dearly is good. Without the flip every transposed argmax names the *weakest* hostile, and the
dead-slot and auto-include readings come out exactly backwards.

Pure stdlib on purpose: the rest of scripts/ has no third-party dependency and neither should this.
"""

import argparse
import collections
import json
import math
import pathlib
import random
import statistics
import sys

# Of the variation the player's roster choice explains, how much comes from matching rather than raw strength.
SII_TARGET = 0.5
# How many independent strategic axes a four-unit roster should offer.
EFFECTIVE_RANK_TARGET = 2.5
# Knowing the enemy in advance should be worth this much budget, and no more: below it the draft screen is
# decorative, above it a wrong pick is an auto-loss, which reads as unfair rather than deep.
REGRET_BAND = (0.10, 0.30)
# A mix should beat the best mono-stack by this much, on at least two hostile mixes with different winners.
PREMIUM_TARGET = 0.20
# Gaps smaller than this many standard errors are seed noise, and do not exist for the player either.
SIGMA_GATE = 2.0
# Above this share of seed noise in the residual, the effective rank counts noise rather than strategies.
NOISE_SHARE_GATE = 0.25
# Bootstrap replicates, and the interval reported from them. 400 is enough for a 95% percentile interval to be
# stable to about a point; the metrics here are read to two or three significant figures.
BOOTSTRAP_REPLICATES = 400
BOOTSTRAP_INTERVAL = 0.95
# A gate has to give the same answer twice, so the resampling is deterministic rather than clock-seeded.
BOOTSTRAP_SEED = 20260828


def load_rows(paths):
    rows = []
    for path in paths:
        try:
            text = pathlib.Path(path).read_text(encoding="utf-8")
        except OSError as error:
            print(f"{path}: cannot read ({error.strerror})", file=sys.stderr)
            continue
        for line_number, line in enumerate(text.splitlines(), start=1):
            line = line.strip()
            if not line:
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as error:
                print(f"{path}:{line_number}: not valid JSON ({error})", file=sys.stderr)
    return rows


class Matrix:
    """M, its per-cell seed spread, and the mixes each axis is labelled with.

    The per-seed values are kept rather than collapsed on the way in. Two things need them: the bootstrap, which
    resamples seeds to put a confidence interval on every headline number, and the paired comparison against a
    baseline, which needs to line up cells seed for seed rather than mean against mean.
    """

    def __init__(self, rows, transpose):
        cells = collections.defaultdict(lambda: collections.defaultdict(list))
        self.weights = {}
        unbounded = collections.Counter()

        for row in rows:
            friendly, hostile = row["friendly_mix"], row["hostile_mix"]
            if transpose:
                friendly, hostile = hostile, friendly
            self.weights.setdefault(friendly, row["hostile_units" if transpose else "friendly_weights"])
            if not row.get("bounded", False):
                # A cell that loses at the ceiling has no budget to report. Substituting the ceiling would understate
                # how bad it is, so it is excluded and counted instead.
                unbounded[(friendly, hostile)] += 1
                continue
            # Sign convention: higher is always better *for the row player*. Forward, the row is a friendly mix and
            # winning cheaply is good, so M = -log B*. Transposed, the row is a hostile mix and costing the player
            # dearly is good, so the sign flips. Without this, every transposed argmax names the *weakest* hostile.
            budget = math.log(max(float(row["critical_budget"]), 1e-9))
            cells[(friendly, hostile)][int(row["seed"])].append(budget if transpose else -budget)

        self.sign = 1.0 if transpose else -1.0
        self.rows = sorted({key[0] for key in list(cells) + list(unbounded)})
        self.cols = sorted({key[1] for key in list(cells) + list(unbounded)})
        self.unbounded = unbounded
        # One value per seed. Repeated rows for the same seed -- two files of the same configuration concatenated on
        # the command line -- are averaged, so a seed stays one observation and the resampling stays a seed draw.
        self.samples = {key: {seed: statistics.fmean(values) for seed, values in per_seed.items()}
                        for key, per_seed in cells.items()}
        self.seeds = sorted({seed for per_seed in self.samples.values() for seed in per_seed})
        self.m = {}
        self.stderr = {}
        for key, per_seed in self.samples.items():
            values = list(per_seed.values())
            self.m[key] = statistics.fmean(values)
            self.stderr[key] = statistics.stdev(values) / math.sqrt(len(values)) if len(values) > 1 else 0.0

    def complete(self):
        """Rows and columns with a value in every cell. Everything downstream needs a full rectangle."""
        rows = [i for i in self.rows if all((i, j) in self.m for j in self.cols)]
        cols = [j for j in self.cols if all((i, j) in self.m for i in rows)]
        return rows, cols

    def dropped(self):
        return [i for i in self.rows if any((i, j) not in self.m for j in self.cols)]


def decompose(m, rows, cols):
    """M[i][j] = mu + a[i] + b[j] + R[i][j], the plain two-way ANOVA."""
    grand = statistics.fmean(m[(i, j)] for i in rows for j in cols)
    a = {i: statistics.fmean(m[(i, j)] for j in cols) - grand for i in rows}
    b = {j: statistics.fmean(m[(i, j)] for i in rows) - grand for j in cols}
    residual = {(i, j): m[(i, j)] - grand - a[i] - b[j] for i in rows for j in cols}
    return grand, a, b, residual


def variance(values):
    values = list(values)
    return statistics.pvariance(values) if len(values) > 1 else 0.0


def singular_values(table):
    """Singular values of a small dense matrix, by cyclic Jacobi on its Gram matrix."""
    columns = len(table[0]) if table else 0
    gram = [[sum(row[x] * row[y] for row in table) for y in range(columns)] for x in range(columns)]

    for _ in range(60):
        off = sum(gram[x][y] ** 2 for x in range(columns) for y in range(columns) if x != y)
        if off < 1e-18:
            break
        for x in range(columns):
            for y in range(x + 1, columns):
                if abs(gram[x][y]) < 1e-15:
                    continue
                theta = 0.5 * math.atan2(2.0 * gram[x][y], gram[x][x] - gram[y][y])
                cos, sin = math.cos(theta), math.sin(theta)
                for k in range(columns):
                    gxk, gyk = gram[x][k], gram[y][k]
                    gram[x][k], gram[y][k] = cos * gxk + sin * gyk, -sin * gxk + cos * gyk
                for k in range(columns):
                    gkx, gky = gram[k][x], gram[k][y]
                    gram[k][x], gram[k][y] = cos * gkx + sin * gky, -sin * gkx + cos * gky

    return sorted((math.sqrt(max(gram[x][x], 0.0)) for x in range(columns)), reverse=True)


def effective_rank(table):
    """exp(H) over the normalised singular values: how many independent strategic axes the matrix actually has."""
    values = [value for value in singular_values(table) if value > 1e-12]
    total = sum(values)
    if total <= 0.0:
        return 0.0
    entropy = -sum((value / total) * math.log(value / total) for value in values)
    return math.exp(entropy)


def is_mono(weights):
    return len([unit for unit, weight in weights.items() if weight > 0]) <= 1


# --- the metrics, as plain functions of a cell table -------------------------------------------------------------
#
# Each headline number is computed here from a bare `{(i, j): value}` and nothing else, so that the bootstrap can
# recompute all of them on a resampled table without going back through the printing.


def noise_floor(stderr, rows, cols):
    spreads = [stderr[(i, j)] for i in rows for j in cols if stderr[(i, j)] > 0.0]
    return SIGMA_GATE * statistics.fmean(spreads) if spreads else 0.0


def diversity_terms(m, stderr, rows, cols):
    """`Var(a)`, `Var(R)` and `SII`, each with the seed noise taken back out.

    Seed noise lands in the residual and inflates every diversity number, because anything the decomposition cannot
    explain looks like an interaction. Subtract what noise alone would put there: a row mean averages over as many
    cells as there are columns, and the double-centring costs a degree of freedom on each axis, so the share landing
    in `a` and the share landing in `R` are not the same.
    """
    _, a, _, residual = decompose(m, rows, cols)
    var_a, var_r = variance(a.values()), variance(residual.values())
    row_count, column_count = len(rows), len(cols)
    sigma_squared = statistics.fmean(stderr[(i, j)] ** 2 for i in rows for j in cols)
    noise_in_r = sigma_squared * (row_count - 1) * (column_count - 1) / (row_count * column_count)
    noise_in_a = sigma_squared * (row_count - 1) / (row_count * column_count)
    true_r = max(var_r - noise_in_r, 0.0)
    true_a = max(var_a - noise_in_a, 0.0)
    true_sii = true_r / (true_a + true_r) if true_a + true_r > 0.0 else 0.0
    return true_sii, true_a, true_r, var_a, var_r, (noise_in_r / var_r if var_r > 0.0 else 0.0)


def regret_saved(m, rows, cols):
    """The fraction of budget a pre-mission draft screen would save over the best blind pick."""
    blind = max(rows, key=lambda i: statistics.fmean(m[(i, j)] for j in cols))
    regret = statistics.fmean(max(m[(i, j)] for i in rows) - m[(blind, j)] for j in cols)
    # M is -log B*, so a gap of `regret` in log-budget is this fraction of the blind pick's budget saved.
    return 1.0 - math.exp(-regret), blind


def alive_counts(m, rows, cols, floor):
    """Which rows are a real answer to which columns, strictly and within the noise floor.

    A strict argmax is a discontinuous statistic: exactly one row wins each column however narrow the win, so a row
    that is second everywhere by a thousandth reads identically to one that is second everywhere by a mile, and a
    seed can flip it either way. `alive` widens "best answer" to "best answer, or inside the noise floor of it" --
    the same 2-sigma gap the rest of the script already refuses to call, applied to the gate it matters most for.
    """
    strict, alive = collections.Counter(), collections.Counter()
    for j in cols:
        best = max(m[(i, j)] for i in rows)
        strict[max(rows, key=lambda i: m[(i, j)])] += 1
        for i in rows:
            if m[(i, j)] >= best - floor:
                alive[i] += 1
    return strict, alive


def premium_rows(m, a, residual, weights, rows, cols):
    """Per column: the best mono, the best mix, and the premium split into its level and structural halves."""
    monos = [i for i in rows if is_mono(weights.get(i, {}))]
    mixes = [i for i in rows if not is_mono(weights.get(i, {}))]
    if not monos or not mixes:
        return
    for j in cols:
        best_mono = max(monos, key=lambda i: m[(i, j)])
        best_mix = max(mixes, key=lambda i: m[(i, j)])
        total = math.exp(m[(best_mix, j)] - m[(best_mono, j)]) - 1.0
        level = math.exp(a[best_mix] - a[best_mono]) - 1.0
        structural = math.exp(residual[(best_mix, j)] - residual[(best_mono, j)]) - 1.0
        yield j, best_mono, best_mix, total, level, structural


def premium_gate(m, rows, cols, weights):
    """How many columns the *structural* premium clears, and how many distinct mixes win them."""
    _, a, _, residual = decompose(m, rows, cols)
    winners = set()
    columns = 0
    for _, _, best_mix, _, _, structural in premium_rows(m, a, residual, weights, rows, cols):
        if structural >= PREMIUM_TARGET:
            columns += 1
            winners.add(best_mix)
    return columns, len(winners)


def measure(m, stderr, rows, cols, weights):
    """Every gated number, from one cell table. The unit of work the bootstrap repeats."""
    sii, var_a, var_r, _, _, _ = diversity_terms(m, stderr, rows, cols)
    saved, _ = regret_saved(m, rows, cols)
    strict, alive = alive_counts(m, rows, cols, noise_floor(stderr, rows, cols))
    columns, winners = premium_gate(m, rows, cols, weights)
    return {
        "SII": sii,
        "Var(a)": var_a,
        "Var(R)": var_r,
        "regret": saved,
        "dead slots": sum(1 for i in rows if strict[i] == 0),
        "dead slots (noise-tolerant)": sum(1 for i in rows if alive[i] == 0),
        "premium columns": columns,
        "premium winners": winners,
    }


# --- the bootstrap -----------------------------------------------------------------------------------------------


def percentile(values, fraction):
    ordered = sorted(values)
    if not ordered:
        return 0.0
    position = (len(ordered) - 1) * fraction
    low, high = math.floor(position), math.ceil(position)
    if low == high:
        return ordered[low]
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def resample(matrix, rows, cols, draw):
    """Cell means and standard errors over one bootstrap draw of seeds.

    Seeds are drawn **once and applied to the whole matrix**, not independently per cell. A seed is a whole-simulation
    seed, so cells that shared it are correlated; resampling each cell on its own would throw that correlation away
    and overstate the uncertainty on every comparison *between* cells -- which is what regret, the dead-slot count
    and the premium are all made of.
    """
    m, stderr = {}, {}
    for i in rows:
        for j in cols:
            per_seed = matrix.samples[(i, j)]
            values = [per_seed[seed] for seed in draw if seed in per_seed]
            if not values:
                # Every drawn seed was unbounded in this cell. Fall back to the whole-cell reading rather than
                # dropping the row and changing the matrix's shape mid-bootstrap.
                m[(i, j)], stderr[(i, j)] = matrix.m[(i, j)], matrix.stderr[(i, j)]
                continue
            count = len(values)
            mean = math.fsum(values) / count
            m[(i, j)] = mean
            spread = math.fsum((value - mean) ** 2 for value in values) / (count - 1) if count > 1 else 0.0
            stderr[(i, j)] = math.sqrt(spread / count)
    return m, stderr


class Bootstrap:
    """Percentile intervals for every gated number, and for its movement against a baseline.

    The point estimates elsewhere in this script are single numbers with no interval, and several of them --
    the dead-slot count, the premium column count, the identity of each column's winner -- are argmax-based and so
    discontinuous in the data. Resampling the seeds says how much of any movement was ever real.
    """

    def __init__(self, matrix, rows, cols, replicates, baseline=None, rectangle=None):
        self.replicates = replicates
        self.values = collections.defaultdict(list)
        self.deltas = collections.defaultdict(list)
        self.paired = baseline is not None and rectangle is not None
        generator = random.Random(BOOTSTRAP_SEED)
        seeds = matrix.seeds
        shared = set(seeds) & set(baseline.seeds) if baseline is not None else set()
        if not shared:
            self.paired = False
        # Both sides of a difference have to be measured over the same rows and columns, or the delta is one
        # rectangle against another rather than one run against another. When the two runs' complete rectangles
        # differ -- a row unbounded in only one of them -- the run under test is re-measured over the shared
        # rectangle for the comparison, and its own reported interval stays on its own full rectangle.
        paired_rows, paired_cols = rectangle if self.paired else (rows, cols)
        # The run under test can be reused for the comparison only when both sides really are measuring the same
        # thing: the same rectangle, and the same drawn seeds. If the runs do not share every seed, the paired draw
        # is a subset of the full one and the two sides would otherwise be drawn differently.
        reusable = (list(paired_rows) == list(rows) and list(paired_cols) == list(cols)
                    and set(seeds) <= shared)

        for _ in range(replicates):
            draw = [seeds[generator.randrange(len(seeds))] for _ in seeds]
            after = measure(*resample(matrix, rows, cols, draw), rows, cols, matrix.weights)
            for name, value in after.items():
                self.values[name].append(value)
            if not self.paired:
                continue
            # The same draw on both runs. Sharing the seeds is what makes the difference paired: the seed noise the
            # two runs have in common cancels in the subtraction instead of adding to it.
            paired_draw = [seed for seed in draw if seed in shared]
            if not paired_draw:
                continue
            before = measure(*resample(baseline, paired_rows, paired_cols, paired_draw),
                             paired_rows, paired_cols, baseline.weights)
            comparable = after if reusable else measure(
                *resample(matrix, paired_rows, paired_cols, paired_draw), paired_rows, paired_cols, matrix.weights)
            for name, value in comparable.items():
                if name in before:
                    self.deltas[name].append(value - before[name])

    def interval(self, name):
        values = self.values.get(name)
        if not values:
            return None
        tail = (1.0 - BOOTSTRAP_INTERVAL) / 2.0
        return percentile(values, tail), percentile(values, 1.0 - tail)

    def note(self, name, decimals=3):
        bounds = self.interval(name)
        if bounds is None:
            return ""
        return f"  [{bounds[0]:.{decimals}f}, {bounds[1]:.{decimals}f}]"

    def delta(self, name):
        """The paired change, its interval, and the share of replicates that agreed on its sign."""
        values = self.deltas.get(name)
        if not values:
            return None
        tail = (1.0 - BOOTSTRAP_INTERVAL) / 2.0
        middle = statistics.fmean(values)
        agree = sum(1 for value in values if (value > 0) == (middle > 0)) / len(values)
        return middle, percentile(values, tail), percentile(values, 1.0 - tail), agree


def report_bootstrap(bootstrap, matrix):
    if bootstrap is None:
        return
    print(f"\nbootstrap -- {bootstrap.replicates} resamples of the {len(matrix.seeds)} seeds, "
          f"{BOOTSTRAP_INTERVAL:.0%} percentile intervals")
    print("             the seeds are drawn once per replicate and applied to every cell, so the correlation")
    print("             between cells that shared a seed is kept rather than averaged away")
    formats = {"SII": 3, "Var(a)": 4, "Var(R)": 4, "regret": 3,
               "dead slots": 1, "dead slots (noise-tolerant)": 1, "premium columns": 1, "premium winners": 1}
    for name, decimals in formats.items():
        values = bootstrap.values.get(name)
        if not values:
            continue
        low, high = bootstrap.interval(name)
        print(f"  {name:<28} {statistics.fmean(values):>8.{decimals}f}   [{low:.{decimals}f}, {high:.{decimals}f}]")

    if not bootstrap.deltas:
        return
    print("\n  paired against the baseline -- the same seed draw on both runs, so the shared seed noise cancels")
    print(f"  {'':<28} {'change':>8}   {BOOTSTRAP_INTERVAL:.0%} interval           agreed on sign")
    for name, decimals in formats.items():
        found = bootstrap.delta(name)
        if found is None:
            continue
        middle, low, high, agree = found
        if low == 0.0 == high:
            print(f"  {name:<28} {middle:>+8.{decimals}f}   unchanged in every replicate")
            continue
        mark = "" if not low <= 0.0 <= high else "   interval spans zero: not resolved"
        print(f"  {name:<28} {middle:>+8.{decimals}f}   [{low:>+.{decimals}f}, {high:>+.{decimals}f}]   "
              f"{agree:>5.0%}{mark}")


def report_paired_cells(matrix, baseline, rows, cols, floor=0.0):
    """Per-row shift against the baseline, measured seed for seed.

    Both runs use the same deterministic seed list, so every cell can be differenced against its own counterpart
    rather than one mean against another. The paired difference drops the seed variance the two runs share, which is
    most of it, and it also separates the two things a change can do to a row: shift it (the same amount in every
    column, which the decomposition absorbs into `a[i]`) or restructure it (a shift that differs by column, which is
    the only kind that can reach `R`).
    """
    shared = sorted(set(matrix.seeds) & set(baseline.seeds))
    if len(shared) < 2:
        print("\npaired against baseline -- the two runs share no seeds, so only the unpaired comparison is possible")
        return
    usable = [i for i in rows if all((i, j) in baseline.m for j in cols)]
    if not usable:
        return

    print(f"\npaired against baseline -- {len(shared)} seeds in common, differenced cell by cell and seed by seed")
    print(f"{'mix':<24} {'mean shift':>11} {'+/-':>7} {'structural sd':>14}  reads as")
    for i in sorted(usable):
        per_column, per_column_error = [], []
        for j in cols:
            after, before = matrix.samples[(i, j)], baseline.samples[(i, j)]
            differences = [after[seed] - before[seed] for seed in shared if seed in after and seed in before]
            if len(differences) < 2:
                continue
            per_column.append(statistics.fmean(differences))
            per_column_error.append(statistics.stdev(differences) / math.sqrt(len(differences)))
        per_seed_row = []
        for seed in shared:
            values = [matrix.samples[(i, j)][seed] - baseline.samples[(i, j)][seed]
                      for j in cols if seed in matrix.samples[(i, j)] and seed in baseline.samples[(i, j)]]
            if values:
                per_seed_row.append(statistics.fmean(values))
        if len(per_column) < 2 or len(per_seed_row) < 2:
            continue

        shift = statistics.fmean(per_column)
        error = statistics.stdev(per_seed_row) / math.sqrt(len(per_seed_row))
        # A shift the same size in every column is a level move and lands wholly in `a[i]`; one that varies by column
        # is the only kind that can reach the interaction. Under a pure level move the columns still differ, but only
        # by their own noise, so the test is whether the spread across columns exceeds what that noise would produce.
        # Comparing a raw range against the standard error of the *mean* would call every row structural: a range
        # over this many columns is several single-column sigmas wide before anything has happened.
        noise_across_columns = statistics.fmean(value ** 2 for value in per_column_error)
        structural = math.sqrt(max(variance(per_column) - noise_across_columns, 0.0))
        if structural <= math.sqrt(noise_across_columns):
            reads = "level"
        elif structural < floor:
            # Pairing is a much sharper instrument than the unpaired reading the gates use, so it resolves structure
            # the gates cannot act on. Saying so is the difference between "this lever restructured the row" and
            # "this lever left a residue only the paired comparison can see" -- integer unit counts, usually.
            reads = "structural, under the floor"
        else:
            reads = "structural"
        print(f"{i:<24} {shift:>+11.3f} {error:>7.3f} {structural:>14.3f}  {reads}")
    print("  a shift identical in every column is a level move and lands wholly in a[i]; only a shift that varies")
    print("  by column can reach Var(R). The structural sd is the column-to-column spread of the shift with the")
    print("  seed noise taken back out, so it is zero for a pure level move. Cost is that pure case.")
    if floor > 0.0:
        print(f"  'under the floor' means the spread is real but smaller than the {floor:.3f} noise floor the gates")
        print("  are read at, so it is below the resolution of every verdict above.")


def report_noise(matrix, rows, cols):
    floor = noise_floor(matrix.stderr, rows, cols)
    if floor <= 0.0:
        print("noise floor:  single seed per cell -- no confidence interval, so no gap can be called")
        return 0.0

    print(f"noise floor:  {SIGMA_GATE:.0f} sigma is {floor:.3f} in log-budget, about {100.0 * (1.0 - math.exp(-floor)):.0f}% of budget")
    print("              gaps smaller than this do not exist for the player either")
    return floor


def report_balance(matrix, a, rows, cols, floor, bootstrap=None):
    """Dead slots and auto-includes, counted twice: on the strict argmax, and inside the noise floor.

    The strict count is the one this document has always quoted, and it has two defects. It is discontinuous -- one
    row wins each column however narrow the margin -- and it is bounded below by the *shape* of the matrix rather
    than by the design: each column has exactly one argmax, so at most `min(rows, columns)` rows can ever be alive
    and `max(rows - columns, 0)` are dead by arithmetic whatever the roster does. Both are reported here, because a
    gate stated against zero on a matrix with more rows than columns can never be met.
    """
    print("\nbalance -- is any mix dominated or dominant?")
    strict, alive = alive_counts(matrix.m, rows, cols, floor)
    print(f"{'mix':<24} {'power a[i]':>10} {'geo B*':>9} {'+/-':>7}  {'argmax':>6} {'within floor':>13}")

    for i in sorted(rows, key=lambda i: -a[i]):
        # The geometric mean, because M is a log. Sign follows the row player, so undo it to print energy.
        geometric = math.exp(matrix.sign * statistics.fmean(matrix.m[(i, j)] for j in cols))
        spread = statistics.fmean(matrix.stderr[(i, j)] for j in cols)
        print(f"{i:<24} {a[i]:>10.3f} {geometric:>9.0f} {spread:>7.3f}  {strict[i]:>6} {alive[i]:>13}")

    arithmetic_floor = max(len(rows) - len(cols), 0)
    dead = [i for i in rows if strict[i] == 0]
    really_dead = [i for i in rows if alive[i] == 0]
    print(f"  dead on the strict argmax:  {len(dead)} of {len(rows)}"
          f"{'  (' + ', '.join(dead) + ')' if dead else ''}")
    if arithmetic_floor > 0:
        print(f"    of which {arithmetic_floor} are dead by arithmetic: {len(rows)} rows against {len(cols)} columns, and each")
        print(f"    column has exactly one argmax, so the strict count can never fall below {arithmetic_floor}.")
        print("    That floor binds the strict count only. Several rows can be inside one column's floor, so the")
        print("    noise-tolerant count below is not bounded by the matrix's shape and zero is reachable either way.")
    print(f"  dead within the noise floor: {len(really_dead)} of {len(rows)}"
          f"{'  (' + ', '.join(really_dead) + ')' if really_dead else ''}   <-- the gate")
    if floor > 0.0 and len(dead) != len(really_dead):
        recovered = [i for i in dead if alive[i] > 0]
        print(f"    {len(recovered)} row(s) are never the strict winner but are inside the floor of one: "
              f"{', '.join(recovered)}.")
        print("    Those are not dead slots -- the gap between them and the winner does not exist for the player.")
    if bootstrap is not None:
        print(f"    bootstrap{bootstrap.note('dead slots (noise-tolerant)', 1)} over resampled seeds")

    always = [i for i in rows if strict[i] == len(cols)]
    if always:
        print(f"  auto-includes (every argmax): {', '.join(always)}")
    if not really_dead and not always:
        print("  no dead slot and no auto-include")
    if floor > 0.0:
        close = sum(1 for j in cols if _within_noise(matrix, rows, j, floor))
        print(f"  columns whose winner is inside the noise floor: {close}/{len(cols)}")


def _within_noise(matrix, rows, column, floor):
    ranked = sorted((matrix.m[(i, column)] for i in rows), reverse=True)
    return len(ranked) > 1 and ranked[0] - ranked[1] < floor


def report_difficulty(matrix, b, rows, cols):
    print("\ncontent -- is every hostile mix asking the same question?")
    print(f"{'column':<24} {'difficulty b[j]':>15} {'best answer':>28}")
    for j in sorted(cols, key=lambda j: -b[j]):
        best = max(rows, key=lambda i: matrix.m[(i, j)])
        print(f"{j:<24} {b[j]:>15.3f} {best:>28}")

    distinct = len({max(rows, key=lambda i: matrix.m[(i, j)]) for j in cols})
    print(f"  distinct best answers across {len(cols)} hostile mixes: {distinct}")
    if distinct == 1:
        print("  composition is an answer; identical questions cannot have distinct answers")


def report_diversity(matrix, a, residual, rows, cols, baseline=None, bootstrap=None):
    true_sii, true_a, true_r, var_a, var_r, noise_share = diversity_terms(matrix.m, matrix.stderr, rows, cols)
    sii = var_r / (var_a + var_r) if var_a + var_r > 0.0 else 0.0

    column_means = {j: statistics.fmean(matrix.m[(i, j)] for i in rows) for j in cols}
    centred = [[matrix.m[(i, j)] - column_means[j] for j in cols] for i in rows]
    rank = effective_rank(centred)
    interaction_rank = effective_rank([[residual[(i, j)] for j in cols] for i in rows])

    note = bootstrap.note if bootstrap is not None else (lambda name, decimals=3: "")
    print("\ndiversity -- does the right answer change with the question?")
    print(f"  SII                  {true_sii:>7.3f}   target >= {SII_TARGET:.2f}   {_verdict(true_sii >= SII_TARGET)}{note('SII')}")
    print(f"    raw                {sii:>7.3f}   before removing seed noise")
    print(f"    Var(a) unit power  {true_a:>7.4f}   (raw {var_a:.4f}){note('Var(a)', 4)}")
    print(f"    Var(R) matchup     {true_r:>7.4f}   (raw {var_r:.4f}){note('Var(R)', 4)}")
    print(f"    seed noise         {noise_share:>6.0%}    of the raw Var(R)")
    print(f"  effective rank       {rank:>7.3f}   target >= {EFFECTIVE_RANK_TARGET:.2f}   {_verdict(rank >= EFFECTIVE_RANK_TARGET)}")
    print(f"    of the residual    {interaction_rank:>7.3f}   (independent axes once power and difficulty are removed)")
    if noise_share > NOISE_SHARE_GATE:
        print(f"  !! over {NOISE_SHARE_GATE:.0%} of the interaction is seed noise, which inflates both ranks above.")
        print("     Raise the seed count before reading either as a count of strategies.")

    if baseline is not None:
        _report_diversity_drift(baseline, true_sii, true_a, true_r, bootstrap)
    return true_sii


def _report_diversity_drift(baseline, sii, var_a, var_r, bootstrap=None):
    """`SII` against its own numerator and denominator.

    `SII = Var(R) / (Var(a) + Var(R))`, so it rises when the matchup term grows *or* when the power spread shrinks,
    and the two mean opposite things: the first is a new matchup, the second is a flatter roster. Only the first is
    the design target, and a single run cannot tell them apart because in a single run both are just numbers.
    """
    rows, cols = baseline.complete()
    if len(rows) < 2 or len(cols) < 2:
        print("    vs baseline        baseline has no complete rectangle to decompose")
        return

    was_sii, was_a, was_r, _, _, _ = diversity_terms(baseline.m, baseline.stderr, rows, cols)
    print(f"    vs baseline        SII {was_sii:.3f} -> {sii:.3f}   "
          f"Var(R) {was_r:.4f} -> {var_r:.4f}   Var(a) {was_a:.4f} -> {var_a:.4f}")
    if sii > was_sii + 1e-9 and var_r <= was_r + 1e-9:
        print("  !! SII rose while Var(R) did not: the gain is the denominator, not new matchup. The roster got")
        print("     flatter, which is not the same thing as the right answer changing with the question.")
    if bootstrap is None:
        return
    for name, decimals in (("SII", 3), ("Var(R)", 4), ("Var(a)", 4)):
        found = bootstrap.delta(name)
        if found is None:
            continue
        middle, low, high, _ = found
        resolved = "" if low <= 0.0 <= high else "   resolved"
        print(f"    paired change      {name:<7} {middle:>+8.{decimals}f}   "
              f"[{low:>+.{decimals}f}, {high:>+.{decimals}f}]{resolved}")


def report_depth(matrix, rows, cols, bootstrap=None):
    """Regret: what a pre-mission draft screen would actually be worth to the player."""
    saved, blind = regret_saved(matrix.m, rows, cols)

    print("\ndepth -- is the choice worth making, and knowable in advance?")
    print(f"  best blind mix       {blind}")
    interval = ""
    if bootstrap is not None:
        bounds = bootstrap.interval("regret")
        if bounds is not None:
            interval = f"   [{bounds[0] * 100:.1f}%, {bounds[1] * 100:.1f}%]"
    print(f"  decision regret      {saved * 100:>6.1f}%  of budget, target {REGRET_BAND[0] * 100:.0f}-{REGRET_BAND[1] * 100:.0f}%   "
          f"{_verdict(REGRET_BAND[0] <= saved <= REGRET_BAND[1])}{interval}")
    if bootstrap is not None:
        bounds = bootstrap.interval("regret")
        if bounds is not None and not (REGRET_BAND[0] <= bounds[0] and bounds[1] <= REGRET_BAND[1]):
            print("    the interval leaves the band: the verdict above is not resolved at this seed count")

    support = [len([unit for unit, weight in matrix.weights.get(max(rows, key=lambda i: matrix.m[(i, j)]), {}).items() if weight > 0]) for j in cols]
    print(f"  support size         {statistics.fmean(support):>6.1f}  units in the winning mix, averaged over hostile mixes")

    usage = collections.Counter()
    for j in cols:
        for unit, weight in matrix.weights.get(max(rows, key=lambda i: matrix.m[(i, j)]), {}).items():
            if weight > 0:
                usage[unit] += 1
    if usage:
        print("  per-unit usage across the argmaxes:")
        for unit, count in sorted(usage.items(), key=lambda item: -item[1]):
            print(f"    {unit:<20} {count}/{len(cols)}")
    return saved


def report_premium(matrix, a, residual, rows, cols, baseline=None, bootstrap=None):
    """How much cheaper the best mix is than the best mono-stack, split into the half that means something.

    The ratio on its own is the most easily faked number on the board: `best_mix(j) / best_mono(j)` has no floor under
    it, so making the best single unit worse raises it exactly as reliably as making mixes better. But the premium
    decomposes, and the decomposition separates the two:

        log premium(j) = M[mix][j] - M[mono][j] = (a[mix] - a[mono]) + (R[mix][j] - R[mono][j])

    `mu` and the column difficulty `b[j]` cancel outright, leaving a **level** term -- how much stronger this mix is
    than this mono on average, identical in every column -- and a **structural** term specific to this column. Only
    the second is composition mattering. A globally nerfed mono lands wholly in the level term, and a column that
    simply got harder cancels, so the structural premium is immune to both and needs no baseline to read.
    """
    monos = [i for i in rows if is_mono(matrix.weights.get(i, {}))]
    mixes = [i for i in rows if not is_mono(matrix.weights.get(i, {}))]
    print("\ncomposition premium -- what mixing is worth over the best single unit")
    if not monos or not mixes:
        print("  the matrix has no mono/mix pair to compare; add mixed rows to the spec")
        return

    print(f"{'hostile mix':<22} {'best mono':>14} {'best mix':>20} {'total':>7} {'level':>7} {'structural':>11}")
    winners = set()
    structural_columns = 0
    total_clearing = 0
    for j, best_mono, best_mix, total, level, structural in premium_rows(matrix.m, a, residual, matrix.weights, rows, cols):
        total_clearing += total >= PREMIUM_TARGET
        earned = structural >= PREMIUM_TARGET
        if earned:
            structural_columns += 1
            winners.add(best_mix)
        print(f"{j:<22} {best_mono:>14} {best_mix:>20} {total * 100:>6.0f}% {level * 100:>+6.0f}% "
              f"{structural * 100:>+10.0f}%{'   <-- earned' if earned else ''}")

    ok = structural_columns >= 2 and len(winners) >= 2
    print(f"  columns whose *structural* premium clears {PREMIUM_TARGET * 100:.0f}%: {structural_columns}, "
          f"with {len(winners)} distinct winner(s)   {_verdict(ok)}")
    if bootstrap is not None:
        print(f"    bootstrap: columns{bootstrap.note('premium columns', 1)}, "
              f"winners{bootstrap.note('premium winners', 1)} over resampled seeds")
        low = bootstrap.interval("premium columns")
        winner_bounds = bootstrap.interval("premium winners")
        if low is not None and winner_bounds is not None and (low[0] < 2 or winner_bounds[0] < 2):
            print("    the interval reaches below the gate: this PASS is not resolved at this seed count.")
    if total_clearing != structural_columns:
        print(f"  (the raw ratio clears in {total_clearing} column(s), the structural half in {structural_columns}. The two")
        print("   differ wherever the level term carries a column, or cancels one the structure has earned.)")
    if structural_columns >= 2 and len(winners) < 2:
        print("  one mix wins every fight: that is a recipe, not a decision")

    if baseline is not None:
        _report_premium_drift(matrix, baseline, monos, mixes, cols)


def _report_premium_drift(matrix, baseline, monos, mixes, cols):
    """Absolute cost of each column's winning mix against the baseline.

    Not part of the verdict -- the structural premium already strips level effects, and this is a *difficulty*
    reading, not a structural one. It is printed because a change can raise the structural premium while making every
    answer more expensive, and whether that is acceptable is a design call rather than a gate.
    """
    moved = []
    for j in cols:
        best_mix = max(mixes, key=lambda i: matrix.m[(i, j)])
        if (best_mix, j) not in baseline.m:
            continue
        change = math.exp(matrix.sign * (matrix.m[(best_mix, j)] - baseline.m[(best_mix, j)])) - 1.0
        if abs(change) >= 0.05:
            moved.append((j, best_mix, change))
    if not moved:
        return
    print("\n  and what those winning mixes now cost against the baseline (difficulty, not structure):")
    for j, best_mix, change in moved:
        print(f"    {j:<22} {best_mix:<20} its own B* moved {change * 100:+.0f}%")


def _verdict(passed):
    return "PASS" if passed else "MISS"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="+", help="JSONL files written by `scons matrix out=...`")
    parser.add_argument("--transpose", action="store_true", help="decompose the other way round: are the hostile mixes asking different questions?")
    parser.add_argument("--baseline", help="a matrix to compare against, so ratio metrics can be shown to have earned their movement")
    parser.add_argument("--bootstrap", type=int, default=BOOTSTRAP_REPLICATES, metavar="N",
                        help=f"resample the seeds N times for a confidence interval on every gated number "
                             f"(default {BOOTSTRAP_REPLICATES}, 0 to skip)")
    args = parser.parse_args()

    rows = load_rows(args.paths)
    if not rows:
        print("No matrix rows found.", file=sys.stderr)
        return 1

    baseline = None
    if args.baseline:
        baseline_rows = load_rows([args.baseline])
        if not baseline_rows:
            print(f"No matrix rows found in the baseline {args.baseline}.", file=sys.stderr)
            return 1
        # Same orientation as the run under test, or the two are not comparable cell for cell.
        baseline = Matrix(baseline_rows, args.transpose)

    matrix = Matrix(rows, args.transpose)
    complete_rows, complete_cols = matrix.complete()
    if len(complete_rows) < 2 or len(complete_cols) < 2:
        print("Not enough complete rows and columns to decompose. Raise max_budget so fewer cells come back unbounded.", file=sys.stderr)
        return 1

    side = "hostile" if args.transpose else "friendly"
    print(f"{len(complete_rows)} {side} mix rows x {len(complete_cols)} columns, from {len(rows)} measurements")
    if baseline is not None:
        print(f"baseline:     {args.baseline}")
        missing = [i for i in complete_rows if any((i, j) not in baseline.m for j in complete_cols)]
        if missing:
            print(f"              no baseline cells for {', '.join(missing)}; those rows cannot be judged as earned")
    if matrix.unbounded:
        print(f"unbounded:    {sum(matrix.unbounded.values())} measurement(s) still lost at the budget ceiling")
    dropped = matrix.dropped()
    if dropped:
        print(f"dropped:      {', '.join(dropped)} (no bounded budget against every column)")

    floor = report_noise(matrix, complete_rows, complete_cols)
    _, a, b, residual = decompose(matrix.m, complete_rows, complete_cols)

    bootstrap = None
    if args.bootstrap > 0 and len(matrix.seeds) > 1:
        rectangle = None
        paired_baseline = None
        if baseline is not None:
            baseline_rows, baseline_cols = baseline.complete()
            shared_rows = [i for i in complete_rows if i in baseline_rows]
            shared_cols = [j for j in complete_cols if j in baseline_cols]
            if len(shared_rows) >= 2 and len(shared_cols) >= 2 and set(matrix.seeds) & set(baseline.seeds):
                paired_baseline, rectangle = baseline, (shared_rows, shared_cols)
        bootstrap = Bootstrap(matrix, complete_rows, complete_cols, args.bootstrap, paired_baseline, rectangle)
    elif args.bootstrap > 0:
        print("bootstrap:    one seed per cell, so there is nothing to resample")

    report_balance(matrix, a, complete_rows, complete_cols, floor, bootstrap)
    report_difficulty(matrix, b, complete_rows, complete_cols)
    report_diversity(matrix, a, residual, complete_rows, complete_cols, baseline, bootstrap)
    report_depth(matrix, complete_rows, complete_cols, bootstrap)
    report_premium(matrix, a, residual, complete_rows, complete_cols, baseline, bootstrap)
    if baseline is not None:
        report_paired_cells(matrix, baseline, complete_rows, complete_cols, floor)
    report_bootstrap(bootstrap, matrix)
    return 0


if __name__ == "__main__":
    sys.exit(main())
