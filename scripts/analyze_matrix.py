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
    """M, its per-cell seed spread, and the mixes each axis is labelled with."""

    def __init__(self, rows, transpose):
        cells = collections.defaultdict(list)
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
            cells[(friendly, hostile)].append(budget if transpose else -budget)

        self.sign = 1.0 if transpose else -1.0
        self.rows = sorted({key[0] for key in list(cells) + list(unbounded)})
        self.cols = sorted({key[1] for key in list(cells) + list(unbounded)})
        self.unbounded = unbounded
        self.samples = cells
        self.m = {}
        self.stderr = {}
        for key, values in cells.items():
            self.m[key] = statistics.fmean(values)
            self.stderr[key] = statistics.stdev(values) / math.sqrt(len(values)) if len(values) > 1 else 0.0

    def complete(self):
        """Rows and columns with a value in every cell. Everything downstream needs a full rectangle."""
        rows = [i for i in self.rows if all((i, j) in self.m for j in self.cols)]
        cols = [j for j in self.cols if all((i, j) in self.m for i in rows)]
        return rows, cols

    def dropped(self):
        return [i for i in self.rows if any((i, j) not in self.m for j in self.cols)]


def decompose(matrix, rows, cols):
    """M[i][j] = mu + a[i] + b[j] + R[i][j], the plain two-way ANOVA."""
    grand = statistics.fmean(matrix.m[(i, j)] for i in rows for j in cols)
    a = {i: statistics.fmean(matrix.m[(i, j)] for j in cols) - grand for i in rows}
    b = {j: statistics.fmean(matrix.m[(i, j)] for i in rows) - grand for j in cols}
    residual = {(i, j): matrix.m[(i, j)] - grand - a[i] - b[j] for i in rows for j in cols}
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


def report_noise(matrix, rows, cols):
    spreads = [matrix.stderr[(i, j)] for i in rows for j in cols if matrix.stderr[(i, j)] > 0.0]
    if not spreads:
        print("noise floor:  single seed per cell -- no confidence interval, so no gap can be called")
        return 0.0

    floor = SIGMA_GATE * statistics.fmean(spreads)
    print(f"noise floor:  {SIGMA_GATE:.0f} sigma is {floor:.3f} in log-budget, about {100.0 * (1.0 - math.exp(-floor)):.0f}% of budget")
    print("              gaps smaller than this do not exist for the player either")
    return floor


def report_balance(matrix, a, rows, cols, floor):
    print("\nbalance -- is any mix dominated or dominant?")
    print(f"{'mix':<24} {'power a[i]':>10} {'geo B*':>9} {'+/-':>7}  argmax columns")
    argmax_owner = collections.Counter()
    for j in cols:
        argmax_owner[max(rows, key=lambda i: matrix.m[(i, j)])] += 1

    for i in sorted(rows, key=lambda i: -a[i]):
        # The geometric mean, because M is a log. Sign follows the row player, so undo it to print energy.
        geometric = math.exp(matrix.sign * statistics.fmean(matrix.m[(i, j)] for j in cols))
        spread = statistics.fmean(matrix.stderr[(i, j)] for j in cols)
        print(f"{i:<24} {a[i]:>10.3f} {geometric:>9.0f} {spread:>7.3f}  {argmax_owner[i]}")

    dead = [i for i in rows if argmax_owner[i] == 0]
    if dead:
        print(f"  dead slots (in no argmax):  {', '.join(dead)}")
    always = [i for i in rows if argmax_owner[i] == len(cols)]
    if always:
        print(f"  auto-includes (every argmax): {', '.join(always)}")
    if not dead and not always:
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


def noise_variance(matrix, rows, cols):
    """How much variance the seed spread alone puts into a cell, from the standard error of each cell mean."""
    return statistics.fmean(matrix.stderr[(i, j)] ** 2 for i in rows for j in cols)


def report_diversity(matrix, a, residual, rows, cols, baseline=None):
    var_a = variance(a.values())
    var_r = variance(residual.values())
    sii = var_r / (var_a + var_r) if var_a + var_r > 0.0 else 0.0

    # Seed noise lands in the residual and inflates every diversity number, because anything the decomposition cannot
    # explain looks like an interaction. Subtract what noise alone would put there: a row mean averages over as many
    # cells as there are columns, and the double-centring costs a degree of freedom on each axis, so the share landing
    # in `a` and the share landing in `R` are not the same.
    row_count, column_count = len(rows), len(cols)
    sigma_squared = noise_variance(matrix, rows, cols)
    noise_in_r = sigma_squared * (row_count - 1) * (column_count - 1) / (row_count * column_count)
    noise_in_a = sigma_squared * (row_count - 1) / (row_count * column_count)
    true_r = max(var_r - noise_in_r, 0.0)
    true_a = max(var_a - noise_in_a, 0.0)
    true_sii = true_r / (true_a + true_r) if true_a + true_r > 0.0 else 0.0
    noise_share = noise_in_r / var_r if var_r > 0.0 else 0.0

    column_means = {j: statistics.fmean(matrix.m[(i, j)] for i in rows) for j in cols}
    centred = [[matrix.m[(i, j)] - column_means[j] for j in cols] for i in rows]
    rank = effective_rank(centred)
    interaction_rank = effective_rank([[residual[(i, j)] for j in cols] for i in rows])

    print("\ndiversity -- does the right answer change with the question?")
    print(f"  SII                  {true_sii:>7.3f}   target >= {SII_TARGET:.2f}   {_verdict(true_sii >= SII_TARGET)}")
    print(f"    raw                {sii:>7.3f}   before removing seed noise")
    print(f"    Var(a) unit power  {true_a:>7.4f}   (raw {var_a:.4f})")
    print(f"    Var(R) matchup     {true_r:>7.4f}   (raw {var_r:.4f})")
    print(f"    seed noise         {noise_share:>6.0%}    of the raw Var(R)")
    print(f"  effective rank       {rank:>7.3f}   target >= {EFFECTIVE_RANK_TARGET:.2f}   {_verdict(rank >= EFFECTIVE_RANK_TARGET)}")
    print(f"    of the residual    {interaction_rank:>7.3f}   (independent axes once power and difficulty are removed)")
    if noise_share > NOISE_SHARE_GATE:
        print(f"  !! over {NOISE_SHARE_GATE:.0%} of the interaction is seed noise, which inflates both ranks above.")
        print("     Raise the seed count before reading either as a count of strategies.")

    if baseline is not None:
        _report_diversity_drift(baseline, true_sii, true_a, true_r)
    return true_sii


def _report_diversity_drift(baseline, sii, var_a, var_r):
    """`SII` against its own numerator and denominator.

    `SII = Var(R) / (Var(a) + Var(R))`, so it rises when the matchup term grows *or* when the power spread shrinks,
    and the two mean opposite things: the first is a new matchup, the second is a flatter roster. Only the first is
    the design target, and a single run cannot tell them apart because in a single run both are just numbers.
    """
    rows, cols = baseline.complete()
    if len(rows) < 2 or len(cols) < 2:
        print("    vs baseline        baseline has no complete rectangle to decompose")
        return

    _, base_a, _, base_residual = decompose(baseline, rows, cols)
    sigma_squared = noise_variance(baseline, rows, cols)
    row_count, column_count = len(rows), len(cols)
    was_r = max(variance(base_residual.values()) - sigma_squared * (row_count - 1) * (column_count - 1) / (row_count * column_count), 0.0)
    was_a = max(variance(base_a.values()) - sigma_squared * (row_count - 1) / (row_count * column_count), 0.0)
    was_sii = was_r / (was_a + was_r) if was_a + was_r > 0.0 else 0.0

    print(f"    vs baseline        SII {was_sii:.3f} -> {sii:.3f}   "
          f"Var(R) {was_r:.4f} -> {var_r:.4f}   Var(a) {was_a:.4f} -> {var_a:.4f}")
    if sii > was_sii + 1e-9 and var_r <= was_r + 1e-9:
        print("  !! SII rose while Var(R) did not: the gain is the denominator, not new matchup. The roster got")
        print("     flatter, which is not the same thing as the right answer changing with the question.")


def report_depth(matrix, rows, cols):
    """Regret: what a pre-mission draft screen would actually be worth to the player."""
    blind = max(rows, key=lambda i: statistics.fmean(matrix.m[(i, j)] for j in cols))
    regret = statistics.fmean(max(matrix.m[(i, j)] for i in rows) - matrix.m[(blind, j)] for j in cols)
    # M is -log B*, so a gap of `regret` in log-budget is this fraction of the blind pick's budget saved.
    saved = 1.0 - math.exp(-regret)

    print("\ndepth -- is the choice worth making, and knowable in advance?")
    print(f"  best blind mix       {blind}")
    print(f"  decision regret      {saved * 100:>6.1f}%  of budget, target {REGRET_BAND[0] * 100:.0f}-{REGRET_BAND[1] * 100:.0f}%   "
          f"{_verdict(REGRET_BAND[0] <= saved <= REGRET_BAND[1])}")

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


def report_premium(matrix, a, residual, rows, cols, baseline=None):
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
    for j in cols:
        best_mono = max(monos, key=lambda i: matrix.m[(i, j)])
        best_mix = max(mixes, key=lambda i: matrix.m[(i, j)])
        total = math.exp(matrix.m[(best_mix, j)] - matrix.m[(best_mono, j)]) - 1.0
        level = math.exp(a[best_mix] - a[best_mono]) - 1.0
        structural = math.exp(residual[(best_mix, j)] - residual[(best_mono, j)]) - 1.0
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
    _, a, b, residual = decompose(matrix, complete_rows, complete_cols)
    report_balance(matrix, a, complete_rows, complete_cols, floor)
    report_difficulty(matrix, b, complete_rows, complete_cols)
    report_diversity(matrix, a, residual, complete_rows, complete_cols, baseline)
    report_depth(matrix, complete_rows, complete_cols)
    report_premium(matrix, a, residual, complete_rows, complete_cols, baseline)
    return 0


if __name__ == "__main__":
    sys.exit(main())
