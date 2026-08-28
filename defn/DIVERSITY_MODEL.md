# Modelling Diversity and Balance

A roster game asks the player to spend a budget on a force drawn from a fixed set of unit types, then sends that
force against an enemy force. This document sets out how to tell, numerically, whether that choice is interesting.

It is self-contained theory. Every term is defined where it first appears.

---

## The three questions

They are constantly confused, and they are not the same:

| | The question | What failure looks like |
|---|---|---|
| **Balance** | Is any unit useless, or mandatory? | A unit nobody should ever bring |
| **Diversity** | Does the *right answer change* with the enemy? | One best unit; the roster is a ladder |
| **Depth** | Does choosing correctly actually pay? | The choice exists but is worth nothing |

Perfect balance with zero diversity is possible and is a worse game than it sounds: several units of identical
strength, any of which works, so the choice is cosmetic.

**A single-number rating cannot express diversity.** Suppose you score each unit with a formula — some combination
of health, damage, range and speed. One number per unit means the units can be *totally ordered*: laid out on a line
from worst to best. A total order has a maximum. Whatever you do to the coefficients, there will be a best unit, and
"better against fast enemies" is not a thing the formula can say, because it has nowhere to say it.

> **Balance is a property of units. Diversity is a property of the payoff matrix.**

A *payoff matrix* is a table with your options down the side, the enemy's down the top, and in each cell a number
saying how that pairing goes. Diversity is a claim about the shape of that table, so the table is what we have to
build.

---

## Step 1 — a number for "how well does A answer B"

Win rate is a poor choice. Given a generous budget almost any force wins, so nearly every cell reads 100% and the
table carries about one bit. This is a **ceiling effect**: the measurement stops varying before the thing you care
about does.

Use **critical budget**, written `B*`: *the smallest budget at which this force beats that enemy half the time.*
Find it by bisection — try a budget, simulate repeatedly, adjust up or down, narrow the bracket.

```
marksmen  vs a rusher wave   ->   B* = 80 energy
breachers vs a rusher wave   ->   B* = 60 energy      breachers answer rushers more cheaply
```

This never saturates — an easy pairing reports a small number rather than "100%" — and it is denominated in the same
currency the player spends.

> The idea predates games analytics. It is how Go handicaps work: rather than asking *who wins*, ask *how many
> stones does the weaker player need to make it even*. Measuring strength as "how much help is required" yields a
> continuous scale where a win/loss bit yields one of two answers.

---

## Step 2 — take the logarithm

Work with `M = −log B*` rather than `B*`. Two reasons:

- **Negated, so bigger is better.** A cheap answer gives a small `B*` and thus a large `M`.
- **Ratios become distances.** "Twice as expensive" is the same gap everywhere on the scale, whether comparing 20
  against 40 or 200 against 400. Everything in Step 4 depends on this.

A consequence worth remembering: **halving a unit's cost shifts its entire row by exactly `log 2`**, the same amount
in every column, because the same budget simply buys twice as many. In this space, price changes are additive
shifts.

---

## Step 3 — build the matrix

Evaluate every force composition against every enemy composition:

```
M[i][j] = −log B*(composition i, enemy composition j)
```

With four unit types, taking single-type stacks and every pair gives `4 + 6 = 10` compositions; the enemy side is
enumerated the same way. Each cell is measured many times, because simulation is stochastic (Step 8).

Note what a *single row* of this table is: one composition measured against every enemy — the familiar "how good is
this unit" question. A single *column* is one enemy measured against every composition — "how hard is this fight".
Both are one-dimensional summaries. **Diversity is not visible in either**, because it is a statement about how
cells differ from what their row and column would jointly predict. That is the next step.

---

## Step 4 — split the matrix into three parts

Write every cell as a sum:

```
M[i][j]  =  mu  +  a[i]  +  b[j]  +  R[i][j]
```

| term | name | meaning |
|---|---|---|
| `mu` | grand mean | the overall difficulty |
| `a[i]` | row effect | how strong composition `i` is, *on average* |
| `b[j]` | column effect | how hard enemy `j` is, *on average* |
| `R[i][j]` | **residual**, or **interaction** | **what is left over — the matchup** |

This is a **two-way ANOVA** (analysis of variance): a standard decomposition splitting a table into row effects,
column effects, and whatever those two together cannot explain. Computing it is only averaging:

```
mu   = average of every cell
a[i] = (average of row i)    − mu
b[j] = (average of column j) − mu
R    = what remains:  M[i][j] − mu − a[i] − b[j]
```

**`R` is the design target.** If `R` is zero everywhere the matrix is *separable*: each composition's value is a
fixed number independent of what it faces, so one composition is best against everything. Non-zero `R` means "this
pairing specifically is unusual" — which is what a matchup is.

### Worked example: two matrices, same numbers, opposite structure

Budgets in energy. Using `log₂` keeps the arithmetic in whole numbers; the base only rescales.

**(a) A matchup.**

```
                 vs rushers   vs snipers
marksman             64           16
breacher             16           64
```

`M = −log₂ B*` gives `−6, −4` and `−4, −6`. Every row averages `−5`, every column averages `−5`, and `mu = −5`. So
**every row effect and every column effect is zero**, and:

```
R  =   −1   +1
       +1   −1
```

Neither composition is stronger on average, neither enemy is harder on average, and yet the cells differ. All the
structure is matchup.

**(b) A ladder.**

```
                 vs rushers   vs snipers
marksman             64           16
breacher             16            4
```

Now `M` is `−6, −4` and `−4, −2`. Working through: `mu = −4`, row effects `(−1, +1)`, column effects `(−1, +1)`, and

```
R  =    0    0
        0    0
```

**Every residual is exactly zero.** The breacher is uniformly twice as efficient as the marksman; snipers are
uniformly twice as cheap to beat as rushers. Nothing is ever worth deciding — always bring breachers, and the
marksman is never the right answer to anything.

> Both tables hold the same four distinct values with the same spread. Only the *structure* differs. **This is why a
> stat sheet cannot tell you whether a roster is diverse.**

One subtlety worth absorbing: `R[i][j]` is not "how good this cell is", it is **how good this cell is relative to
what its row and column already predict**. A composition can be the outright winner of a column and still carry a
negative residual there, if it is even stronger elsewhere.

### Two words for the two halves

The rest of this document needs names for the two things a row can have, so:

- A row's **level** is its height — the single number `a[i]` saying how strong that composition is on average,
  the same against every enemy. Raising a row's level lifts it bodily up the ladder.
- A row's **structure** is its pattern — the residuals `R[i][*]` saying which enemies it is unusually good or bad
  against. Changing a row's structure changes its *shape* without necessarily changing its height.

In example (b) above, the two rows differ purely in level and have no structure at all. In example (a) they are
identical in level and differ purely in structure.

> **"Level" here means the height of a row and has nothing to do with a stage or mission** — an unfortunate
> collision in a game context, so it is worth fixing the sense now. In standard statistics vocabulary these two
> halves are the **main effect** and the **interaction**, which are the terms to search for.

A change to the game can therefore be classified by which half it moves, and this turns out to be the single most
useful thing to know about any proposed change. Step 7 gives the test.

---

## Step 5 — four metrics

*Variance* below means the ordinary statistical one: the average squared distance of a set of values from their own
mean. It measures spread.

### Strategic Interaction Index (`SII`)

Of everything the player's choice explains, what fraction comes from **matching the enemy** rather than from **raw
strength**?

```
SII = Var(R) / (Var(a) + Var(R))
```

`Var(R)` is the spread of the matchup terms; `Var(a)` the spread of the row effects. Example (a) above scores 1.0,
example (b) scores 0.0. A reasonable target is **0.5 or above** — but not 1.0, since some transitive strength is
usually wanted, to make prices and unlocks mean something.

> **It is a ratio, so it also rises when the denominator falls.** Making the strongest composition worse flattens
> `Var(a)` and improves `SII` without creating a single matchup. Always read the numerator `Var(R)` before believing
> the ratio.

### Decision regret

*How much budget is saved by knowing the enemy in advance.* Compare the best answer to each enemy against the single
best fixed choice used against all of them:

```
best answer per enemy, averaged:   60 energy
best single fixed choice:          70 energy
regret = 1 − 60/70 ≈ 14%
```

This is the value of letting the player scout before committing. The name is from decision theory, where regret is
what you lose by not knowing in advance what you would know in hindsight.

The target is a **band — roughly 10% to 30%** — and the upper bound is the interesting half. Below the band the
choice is decoration. Above it, a wrong choice is an unrecoverable loss, which reads as unfair rather than deep.

### Dead slots and auto-includes

A composition is a **dead slot** if it is never the best answer to any enemy: it has no job. Its mirror image is an
**auto-include**, a unit appearing in the best answer to *every* enemy. Both are balance failures; the target for
each is zero.

"Best" must tolerate measurement noise. Taking the strict winner — the **argmax**, the row with the highest value in
a column — has two defects. It awards exactly one winner per column however narrow the margin, so a row that is a
close second everywhere scores the same as one that is hopeless everywhere. And it is bounded by the table's shape
rather than by the design: with more rows than columns, at least `rows − columns` rows are "dead" by arithmetic
alone. Counting a row as alive when it is the best answer **or within measurement noise of it** fixes both.

### Composition premium

`best_mix / best_mono − 1`: how much cheaper the best mixed force is than the best single-type stack, per enemy. It
is the most legible number for a non-specialist — "mixing is worth 25% here".

It shares the weakness of `SII` — weakening the best single unit raises it — but here the fix is exact. Because we
are in log space, the premium **splits into two additive halves**:

```
log premium = (a[mix] − a[mono])  +  (R[mix][j] − R[mono][j])
                    level                    structural
```

`mu` and the enemy's difficulty `b[j]` cancel completely. What remains is exactly the level and structural split of
Step 4: a **level** difference — this mix simply sits higher on the ladder than that single unit, by an identical
amount against every enemy — and a **structural** difference specific to this enemy. Only the second is composition
mattering. For instance:

```
total premium +19%   =   level +34%   ×   structural −11%
```

A 19% premium that looks like mixing paying off is nothing of the kind: the mix is a strong row generally, and
against *this* enemy it underperforms its own average. Gate on the structural half. Note that the halves multiply
rather than add, because they add in logs: `1.34 × 0.89 ≈ 1.19`.

---

## Step 6 — making the numbers trustworthy

Simulation is stochastic, so each cell is measured over many **seeds** (a seed fixes the random number generator so
one run is exactly reproducible).

- **Noise floor.** Each cell carries a confidence interval — a range the true value plausibly occupies. Differences
  smaller than about two standard errors are indistinguishable from noise, and should be treated as non-existent,
  because they are equally invisible to the player.
- **Bootstrap.** To attach an interval to a *derived* quantity like `SII` or a dead-slot count, resample the seeds
  with replacement, recompute the whole decomposition, and repeat several hundred times. The middle 95% of the
  results is the interval. This is essential for counts based on who-won comparisons, which have no closed-form
  error. Draw the seeds once per replicate and apply them to the whole matrix: cells that shared a seed are
  correlated, and resampling each cell independently discards that.
- **Pairing.** When comparing before and after, run both with the same seed list and difference each cell *seed by
  seed*. The randomness the two runs share cancels in the subtraction instead of accumulating, resolving far smaller
  changes than comparing two averages can. This is the standard *common random numbers* technique.

---

## Step 7 — which changes can create a matchup

> **A change moves structure — and therefore `Var(R)` — if and only if what it is worth depends on what it is
> facing.** Otherwise it moves level only.

This follows from the decomposition rather than from observation. Suppose a change adds the same constant `c` to
every cell of one row. Then that row's average rises by `c`, the grand mean rises by `c / rows`, and every column
average rises by `c / rows`. Substituting into `R = M − mu − a − b`, the additions cancel exactly: **every residual
is unchanged.** A change worth the same amount against every enemy is absorbed entirely by that row's level `a[i]`,
by construction, no matter how large the change is.

| kind of change | moves | why |
|---|---|---|
| area damage | **structure** | worth nothing against one target, a great deal against six |
| armour | **structure** | worth nothing against a single large hit, nearly everything against many small ones |
| speed, health | level only | worth the same against every enemy |
| **cost** | level only, provably | halving cost buys twice as many — an identical shift in every column |

### Why this decides which tool to reach for

A dead slot and a missing matchup are opposite problems, and the classification above says which kind of change can
fix which.

**A dead slot is a row sitting too low.** It is never the best answer to anything, so it needs *lifting* — a level
change. Price is the cleanest lever available for that, because the proof above says a price cut is a level change
and nothing else: it moves the row bodily up the ladder by `log(old cost / new cost)` and leaves every residual
exactly where it was. So repricing a neglected unit **cannot break whatever matchups the roster already has**. That
is a strong guarantee, and it is why price is the first thing to try on a dead slot rather than a stat change, which
would move both halves at once and make the result hard to read.

**A missing matchup is the opposite.** No amount of repricing can produce one, because pricing only ever moves
levels and `Var(R)` is built from structure. Creating a matchup requires a mechanic whose value is *conditional* on
what it faces — area damage and armour in the table above, rather than speed and health.

The trap is that **a price cut and a conditional buff can look like the same move on a spreadsheet** — both make an
underused unit stronger — while doing entirely different things to the matrix:

| | can create structure? | can disturb existing structure? |
|---|---|---|
| **price cut** | no | no |
| **conditional buff** | yes | yes |

Each row reads the same in both columns, and that is the point: **creating structure and disturbing it are the same
capability.** A price cut cannot touch the residuals at all, which makes it a safe repair and a useless source of
diversity. A conditional buff is the only thing that can create a matchup, and for exactly that reason the only
thing that can wreck one.

Reach for price when a unit has no job. Reach for a conditional mechanic when the roster has no decisions. Neither
will do the other's work.

---

## Further reading

None of this is novel mathematics; it is standard technique aimed at a game roster.

- **Wellman (2006), *Methods for Empirical Game-Theoretic Analysis*** — building an empirical payoff matrix over
  sampled strategies and analysing it. The general frame for everything above.
- **Balduzzi et al. (2018), *Re-evaluating Evaluation*** (NeurIPS) — splits game payoff matrices into a
  *transitive* component (a strength ladder) and a *cyclic* one (rock-paper-scissors structure). The `a[i]` versus
  `R[i][j]` split is the same idea, adapted to a non-symmetric game where one side is authored rather than played.
- **Czarnecki et al. (2020), *Real World Games Look Like Spinning Tops*** (NeurIPS) — finds that in real games the
  transitive ladder dominates and matchup structure is comparatively thin. Useful for calibrating expectations:
  `Var(R)` is supposed to be hard to move.
- **Jaffe et al. (2012), *Evaluating Balance in Games Through Restricted Play*** (AIIDE) — quantitative balance
  metrics derived from simulated play; the closest design-facing prior work.
- **Efron (1979), *Bootstrap Methods: Another Look at the Jackknife*** — the resampling technique behind the
  confidence intervals.
- **Watson & Pelli (1983), *QUEST: A Bayesian Adaptive Psychometric Method*** — efficient estimation of the input
  level at which a yes/no outcome reaches 50%. Bisecting for `B*` is a crude form of the same problem.
- **Law & Kelton, *Simulation Modeling and Analysis*** — common random numbers and variance reduction; why paired
  comparison repays the bookkeeping.

Any introductory statistics text covers two-way ANOVA. Searching for "two-way ANOVA interaction effect" finds the
decomposition of Step 4, usually explained with crops and fertiliser rather than marksmen and rushers.
