# Experiment Log

Every measured change to the roster or the rules, whether or not it shipped. Newest first.

A negative result costs the same to measure as a positive one and is worth the same next time, so entries are kept
for changes that were reverted — the point of the log is that nobody re-runs an experiment the numbers already
answered. What the instruments are and how to read them is in [`BALANCE_TOOLING.md`](BALANCE_TOOLING.md); the model
behind them is in [`DIVERSITY_MODEL.md`](DIVERSITY_MODEL.md); where the roster currently stands is in
[`DIVERSITY_AND_BALANCE.md`](DIVERSITY_AND_BALANCE.md).

Each entry records the **hypothesis** as a fight, the **result** against the gates, and — the part worth writing
down — **what is now known** that does not depend on the change being kept. Gate numbers are 51 seeds, paired
against the baseline, decomposed both ways, unless the entry says otherwise.

---

## 2026-08-30 — Marksman rate of fire, swept: structure that moves no answers

**Not shipped; `ranged_attack_period_seconds` stays at 1.05.** Six points swept, 51 seeds each, paired.

Started as a diagnosis of a local edit — marksman period 1.05 → 1.38 with `cost` 31 → 30 — and turned into the
sweep, because the two levers could not be separated while the catalog itself was wrong. See the entry below on the
stat stowaways; every number here is measured at the intended catalog, `hp` 180 and `cost` 27.

**Hypothesis.** Rate of fire ought to be structural. The marksman's whole relationship to a hostile is the *free
approach window* — `(650 − where it stops) / its speed` of unanswered fire — and that window holds a different
number of shots for each hostile, so a period change should be worth different amounts in different columns.

### Result

| period | `Var(R)` | `Var(a)` | `SII` | regret | dead | premium | mono shift | structural sd |
|---|---|---|---|---|---|---|---|---|
| 0.85 | 0.0374 | 0.0312 | 0.545 | 13.9% | 3 | 3/3 | +0.159 | 0.154 |
| 0.95 | **0.0390** | 0.0232 | 0.627 | 15.8% | 3 | 3/3 | +0.067 | 0.101 |
| **1.05 (shipped)** | 0.0367 | 0.0212 | 0.634 | 14.0% | 3 | 3/3 | — | — |
| 1.15 | 0.0365 | 0.0205 | 0.640 | 12.4% | 3 | 3/3 | −0.031 | 0.052 |
| 1.25 | 0.0367 | 0.0205 | 0.642 | 11.7% | 3 | 3/3 | −0.096 | 0.125 |
| 1.38 | 0.0352 | 0.0211 | 0.626 | 9.9% MISS | 3 | 5/3 | −0.170 | 0.135 |

The hypothesis is confirmed on classification and refuted on value. Every point reads `structural`, and the
column-to-column spread is **as large as the mean shift or larger** — 79% to 168% of it, against roughly 2% for
`cost`. On the test this project has used since the beginning, that is the most structural lever measured.

It buys nothing. `Var(R)` spans 0.0352 to 0.0390 across a 62% swing in fire rate; only 0.95 is a resolved gain
(+0.0023, 100%), and 1.38 is resolved the wrong way. The dead-slot count is **3 at every point**, never moving,
only swapping identity (`operator` below 1.25, `marksman+operator` above).

**0.95 was written up as a candidate and then withdrawn, which is the entry.** `Var(R)` 0.0390 is higher than any
value previously recorded here, and regret rose 14.0% → 15.8% resolved — the two numbers `How to judge a change`
says to read first, both moving the right way, neither a ratio. Then the winner-by-column table:

| | 1.05 | 0.95 | 0.85 |
|---|---|---|---|
| `breacher` | 3 columns | 3 | **1** |
| `breacher+marksman` | 4 | 4 | **6** |
| `breacher+operator` | 3 | 3 | 3 |
| `marksman` | 3 | 3 | 3 |
| `marksman+impact` | 1 | 1 | 1 |
| `impact+operator` | 1 | 1 | 1 |
| distinct winners | 6 | 6 | 6 |

**Not one of the fifteen columns changes hands at 0.95.** The residual grew; no matchup resolved differently. The
regret rise has a mechanism that is worth naming on its own: the blind-best mix is `breacher+operator`, which
carries no marksman, so buffing the marksman widens the gap between the default and the marksman-bearing column
winners automatically. The decision did not get richer — the default got relatively worse.

At 0.85 columns genuinely do change hands, and it is the failure mode: the blind-best mix itself becomes
`breacher+marksman`, taking 6 of 15 while `breacher` collapses 3 → 1, `Var(a)` +47%, `SII` 0.545. The doc's own
warning — a conditional buff reads as raw power when given to a unit that is already strong — with the
qualification that in the lab the marksman is *not* strong: at cost 27 its mono row sits seventh of ten at
`a[i]` = 0.019. Its in-game dominance is survival-shaped (it out-ranges the roster, takes zero from a mason, does
not die) and `a[i]` is power per energy.

### What is now known

- **Structural classification is necessary, not sufficient.** The rule "a lever moves `Var(R)` iff its worth depends
  on what it is facing" needs a qualifier: the dependence must differ in **sign or saturation**, not merely in
  magnitude. Rate of fire is worth more against everything, just differently more, so its spread lands back in
  `a[i]` and in the size of the residual — never in an argmax.
- **Diff the winner-by-column table before believing any headline.** It is the only reading that separates "the
  residual got bigger" from "the answer changed", it costs nothing, and here it contradicted `Var(R)`, regret and
  the structural classification simultaneously.
- **Regret is not immune to the denominator problem.** It is not a ratio, which is why it is read first, but it is a
  comparison against the blind pick and inflates when that pick gets relatively worse. Recorded in
  [`DIVERSITY_MODEL.md`](DIVERSITY_MODEL.md).
- **The period's response is saturating, one-sided and quantised**, so no single pair of readings characterises it:
  a dead zone from 1.05 to 1.25 where four of five columns move by exactly nothing, a mason column that is saturated
  on the fast side (+0% at every speed-up), and a breakpoint at 1.38 where five free approach shots at 19 damage
  stop killing an 82hp mason before contact. Damage taken from six masons runs 0 → 60 → 214 → **0** across
  1.05/1.15/1.25/1.38 — the invariant erodes at constant budget, then is bought back by spending 49% more.
- **A rate change is not a way to tune a level.** It moves all 150 matrix cells and every marksman cell in the tempo
  lab to adjust one mission. Hostile volume in the level's own spawn table is the local lever; that is what levels
  are tuned with.

---

## 2026-08-30 — Two stat stowaways, and the catalog the doc was measured at

**Fixed by reverting the value; nothing else changed.** Found while diagnosing the rate sweep above.

The scoreboard in [`DIVERSITY_AND_BALANCE.md`](DIVERSITY_AND_BALANCE.md) did not reproduce against `HEAD`: measured
`SII` 0.653, regret 10.8%, `Var(a)` 0.0195, 2 friendly dead slots against a documented 0.633 / 14.0% / 0.0212 / 3.
The doc was not stale. Two commits about other things had each carried a marksman stat:

| commit | subject | stowaway |
|---|---|---|
| `c460d0c` | Fix animation transitions. | `hp` 180 → **580** |
| `a2d1c13` | Fix export presets. | `hp` restored, `cost` 27 → **31** |

Rebuilding the intended catalog (`hp` 180, `cost` 27, period 1.05) reproduced the scoreboard **exactly** — 0.634,
14.0%, 0.0367, 0.0212, 3 dead, premium 3/3, every bootstrap interval matching.

The cost stowaway is a textbook pure price move in the matrix — every marksman row `structural, under the floor`,
`Var(R)` +0.0001 — and it still cost **regret 14.0% → 10.8%**, resolved, most of the margin above the band's floor.
In the tempo lab, where price is not a level lever, four energy did this to the marksman mono's critical purse:

| | rush | spike | grind | escalation |
|---|---|---|---|---|
| intended (27) | 66 | 100 | **26** | 62 |
| `HEAD` (31) | 82 | 143 | **56** | 75 |

### What is now known

- **The scoreboard is a regression test that nobody runs.** It reproduces to three decimals and to the bootstrap
  interval when the catalog is right, so a mismatch means the catalog moved — diff it against the last commit that
  touched balance deliberately before concluding the document needs an update.
- **`hp` 580 should have failed `shipped_breacher_answers_three_grime_and_the_marksman_does_not`**, which existed at
  that commit; a 580hp marksman does not die at 12.7s to three grime. That commit went in red and the next one
  healed it by accident. Nothing pinned `cost` at all.
- **A one-line catalog value can be a large regression in an instrument the matrix cannot see.** +4 energy reads as
  a harmless level move in the decomposition and as a 2.2x critical purse on the grind.

---

## 2026-08-30 — The tempo lab, measured as a critical purse

**Shipped.** `bisect=yes` on `scons sim`, `scripts/analyze_tempo.py`, three hosted tests.

The lab shipped the day before read a win rate at a fixed purse, and said this:

| composition | rush | spike | grind | escalation |
|---|---|---|---|---|
| `breacher+marksman` | 100% | 100% | 100% | 100% |
| `marksman` | 100% | 100% | 100% | 100% |
| `marksman+operator` | 100% | 96% | 100% | 100% |
| `marksman+impact` | 100% | 88% | 100% | 100% |

Four compositions tied at the top of every engagement, which is not a result — it is the ceiling effect
`DIVERSITY_MODEL.md` rejects win rate for, arriving in the timed instrument as well. **Win rate is a step function
of the purse**: sweeping it showed cells jumping 0% to 100% between adjacent probes, which is also why the shipped
purses had to be hand-calibrated per engagement to land anywhere informative.

So the lab now makes the same measurement `scons matrix` does, on the quantity this instrument actually varies:
**the smallest starting purse at which a composition wins the engagement half the time.** Same contract as
`critical_budget` — bisect under a ceiling, report a cell that never wins as `bounded: false` rather than handing it
a number, carry the probe count so a cell that ran out of iterations is visible.

### Result, 25 seeds, ten compositions x four engagements

| composition | rush | spike | grind | escalation |
|---|---|---|---|---|
| `breacher+marksman` | **50** | **81** | 44 | 56 |
| `marksman` | 66 | 100 | **26** | 62 |
| `marksman+operator` | 75 | 109 | 65 | **50** |
| `marksman+impact` | 91 | 100 | 71 | 63 |
| `operator` | 100 | 159 | 84 | 73 |
| `breacher+operator` | 110 | 176 | 78 | 75 |
| `breacher` | 115 | 143 | 88 | 84 |
| `breacher+impact` | 140 | 279 | 100 | 91 |
| `impact+operator` | 141 | 288 | 93 | 93 |
| `impact` | 156 | 284 | 90 | 88 |
| *`defensive`, reference* | *41* | *28* | *6* | *12* |

**Three distinct cheapest answers across four engagements, no unbounded cells, and spreads of 1.9x to 3.8x between
the cheapest and dearest answer.** The win-rate table showed none of that: it could not separate its own top four.

Worth reading off it:

1. **`marksman` mono is the cheapest answer to the grind at 26 and one of the dearest to the spike at 100.** A
   trickle lets a sniper work for free; twenty bodies arriving at once do not. That is a tempo matchup, and no
   arrangement of the matrix can express it, because the matrix has no arrival order.
2. **`marksman+operator` is the cheapest answer to the escalation.** First time the `operator` has appeared in a
   cheapest answer on any instrument. It is not a job yet — the margin over `breacher+marksman` at 56 is one probe
   step — but it is the first sign that anything rewards the unit, and it appears only once a clock exists.
3. **`defensive` is far cheaper than every composition on every engagement** (28 on the spike against 81 for the
   best composition). Banking until contact then committing is worth more than any composition choice measured here.
   That is a statement about the *policies*, not the roster, and it means composition results should be read against
   each other rather than against the reference lines.

### What is now known

- **The ceiling effect is a property of the measure, not the instrument.** Win rate saturated in the matrix, the
  campaign sweep and the tempo lab in turn. Each time the fix was the same: bisect the resource the player spends.
  **Reach for a critical quantity before calibrating a fixed one** — the hand-tuned purses from the day before
  became dead weight the moment the bisection existed.
- **A floor is as blinding as a ceiling.** The first bisection returned purse `1` for the grind and escalation:
  spread over two minutes, income alone (120 energy) swamped any starting purse. Compressing those two schedules
  from six-second to three-second spacing put them back on scale. **A cell reading the minimum of the search range
  is not a cheap answer, it is an unmeasured one.**
- **Cost of the whole thing:** about 120 lines in `defn_sim_runner.cpp`, one flag through SConstruct and the `.gd`
  runner, a 130-line reader, three hosted tests. The 4x12 table takes 203 seconds at 25 seeds.

---

## 2026-08-30 — Deleting every instrument denominated in content

**Shipped.** No code paths removed; nine scenario files and one matrix spec deleted, two fixtures repointed.

The day before, the campaign sweep was demoted from a roster gate to a level instrument and the tempo lab was built
to replace it. That was half a decision. **Levels are narrative — some are written to be lost — so a win rate over
one is not a weak measurement of the roster, it is a measurement of something else**, and leaving the scenarios in
place left a gate that would be reached for again.

Deleted: `campaign_matrix.json`, `campaign_progression.json`, the seven `level_01_*.json` sweeps, and
`matrix_levels.json` — the one-off that reconciled the two instruments, whose result is recorded above along with
the compositions it used, so the reading stays reproducible without the file.

Kept, because neither touches a level: `matrix_smoke.json` (unit mixes only) and `tempo_lab.json` with its four
synthetic engagements in `data/lab/`.

Two references were load-bearing and had to move rather than go:

- `godot_sim_runner.gd`'s default scenario, and
- `defn_sim_runner_runs_a_checked_in_scenario_and_writes_jsonl`, a hosted plumbing test that needs *a* checked-in
  scenario and does not care which,

both now on a new `tempo_smoke.json` — one synthetic engagement, one policy, the counterpart of `matrix_smoke.json`.
A third apparent reference was a false positive: `content_validator.cpp` contains a function called
`validate_campaign_progression` that has nothing to do with the scenario file of that name.

### What is now known

- **Demoting an instrument is not the same as removing it.** A gate that is documented as "for levels only" is still
  a gate sitting where somebody will reach for it, and its numbers still look like results. If content should not
  decide roster questions, the content-denominated files have to go, not be relabelled.
- **Check what a fixture is actually for before deleting it.** Of eleven scenario files, nine were unreferenced, one
  was the runner's default *and* a hosted test's input, and one match was a name collision in production code. The
  cost of checking was one `grep`; the cost of not checking would have been a red suite and a wrong guess about why.

---

## 2026-08-29 — Plating the wrecker: armour's inverse, measured across seven settings

**Content reverted; the mechanic is kept in code, wired and tested but carried by no unit.** `damage_cap` is now a
`UnitConfig` field applied in both damage paths, exactly as `armour` was before anything used it.

The `wrecker` was the strongest hostile on the board and completely flat — worth the same against every friendly
composition, so its whole contribution was to the power term `a[i]` and none of it to the matchup term `Var(R)`.
Every armour value in the roster also pushes the *same* way: armour subtracts, so it punishes rate and spares burst,
and `breacher 4 / grime 4 / jackal 2` therefore all say "bring the marksman". Reach-and-burst answers every question
on the board at once, which is what totally orders the friendly roster.

- **`damage_cap`, plating** — a ceiling on any single hit, applied before armour. Armour's exact inverse: it costs a
  19-damage round seven points and a 6-damage round nothing. In this roster it is an anti-marksman stat exclusively,
  since nothing else friendly exceeds 8.
- **The intended fight**: *the wrecker shrugs off single heavy rounds, so killing it cheaply needs sustained fire
  delivered from outside its reach* — and the `operator` is the only friendly gun that is both, passing the cap
  untouched at 6 damage and out-ranging the wrecker 380 to 330. The operator has no job, and nothing in the roster
  rewards rate as such; this was meant to be the thing that does.

### Result

Seven settings, 51 seeds each, paired, decomposed both ways. Baseline `Var(R)` 0.0367, friendly dead 3/10, hostile
dead 5/15, hostile premium 2 columns.

| cap | hp | dmg | `Var(R)` change | friendly dead | hostile dead change | hostile premium | wrecker row shift |
|---|---|---|---|---|---|---|---|
| 12 | 160 | 13 | +0.0026 resolved | **2** | **+3.8** [3, 5] | 1 col MISS | +0.032 |
| 12 | 180 | 12 | +0.0029 resolved | **2** | **+3.3** [2, 5] | 1 col MISS | +0.053 |
| 12 | 180 | 11 | +0.0054 resolved | 3 | **+2.9** [2, 4] | 1 col MISS | −0.017 |
| 12 | 150 | 13 | +0.0034 resolved | **2** | **+2.4** [1, 3] | 2 col PASS | −0.007 |
| 14 | 180 | 13 | +0.0014 resolved | 3 | **+3.8** [3, 4] | 1 col MISS | +0.036 |
| 15 | 170 | 13 | +0.0004 resolved | **2** | +1.3 [0, 2] *not resolved* | 2 col PASS | +0.005 |
| 16 | 180 | 13 | +0.0006 resolved | 3 | +1.7 [1, 2] | 2 col PASS | +0.012 |

**The trade is monotone and roughly one-for-one: every point of `Var(R)` the cap buys is paid for in hostile dead
slots.** The dead-slot count is the only gate the roster currently misses, so this is the wrong currency to spend.
The one setting that costs nothing resolvable — cap 15 — buys `Var(R)` +1%, which is under any bar worth shipping
for.

The mechanic itself is not at fault and did precisely what the arithmetic said. The friendly paired block splits
exactly along the axis it was designed to split: every marksman row falls (`marksman` −0.058 with the largest
structural sd on the board, 0.107) and every other row rises (`impact+operator` +0.028, `operator` +0.015), each
classified `structural`. On the hostile side every row without a wrecker in it reads **+0.000, structural sd 0.000,
level** — byte-identical, as it must be, which is the cleanest wiring confirmation available.

### What is now known

1. **A conditional mechanic on a hostile unit spreads the friendly side and concentrates the hostile side at the same
   time, and they are the same effect seen from two sides.** The cap varies by *which friendly gun* faces it, which
   is real structure and shows up as a resolved `Var(R)` gain. But it is worth the same to every hostile mix that
   contains a wrecker, so all 5 of the 15 wrecker rows rise together — and they were already the strong end. The
   wrecker mono then wins all three marksman columns outright and its own pairs stop adding anything over it:
   `wrecker+jackal`, `wrecker+hound` and `grime+wrecker` died, and `grime+hound` / `grime+jackal` died without
   moving at all, purely because rows above them improved. **Read the transpose before believing a spread.**
2. **The regression is not under-payment.** At cap 12 / hp 150 the wrecker's own row shift is −0.007 — its power is
   neutralised outright — and hostile dead still rises +2.4 [+1.0, +3.0], resolved. Paying for a conditional buff
   removes the level component; it does not touch which rows the structure concentrates in.
3. **Cutting `hp` and cutting `ranged_damage` are not interchangeable payments.** `hp` scales the durability term the
   plating multiplies, so it shrinks the structure along with the power (cap 12 / hp 150 keeps `Var(R)` +0.0034);
   `ranged_damage` touches only the offensive term and leaves the structure intact (cap 12 / hp 180 / dmg 11 is the
   largest `Var(R)` of the seven at +0.0054). **Pay for a durability mechanic out of the offence.**
4. **The `operator` came off the noise-floor dead list in four of the seven settings, and never resolvably.** Its own
   row moves +0.002 to +0.031 with a structural sd under the floor in the settings that revive it: the row sits on
   the edge of the floor and the cap nudges it across rather than giving it a job. Consistent with the standing
   diagnosis — the operator's budget is survival-bound, and plating is a damage-side lever.
5. **The friendly premium is insensitive to this whole family.** Three settings read 4 columns and three read 3, and
   the paired delta spans zero in all seven. Nothing here moves it either way.

### The same mechanic on the mason, which is the losing end

The entry above predicted that plating a *losing* hostile row would reduce dead slots where plating a strong one
raised them. That is exactly what happened, and the mechanic failed anyway.

| mason cap | `Var(R)` | friendly dead | **hostile dead** | friendly regret | hostile premium |
|---|---|---|---|---|---|
| baseline | 0.0367 | 3 | 5 | 14.0% | 2 col |
| 15 | 0.0339 (**−0.0028**) | 4 (+0.9 *n/r*) | 5 (−0.1 *n/r*) | 12.9% | 2 col PASS |
| 12 | 0.0304 (**−0.0063**) | 4 (+1.0) | 5 (−0.1 *n/r*) | 12.9% | 2 col PASS |
| 10 | 0.0281 (**−0.0086**) | 4 (+1.0 *n/r*) | **2 (−2.8** [−3, −2]**)** | **9.9% MISS** | 1 col MISS |

**The carrier lesson is confirmed outright.** Cap 12 on the wrecker costs +3.3 hostile dead slots, resolved; cap 12
on the mason costs nothing resolvable, and cap 10 on the mason *buys* 2.8 of them — from 5 of 15 down to **2 of 15**,
the lowest hostile dead count ever recorded here, against a target of zero. Lifting a losing row spreads the hostile
side; lifting a strong one concentrates it.

**And `Var(R)` falls, resolved, at every setting** — the deepest cap costs 23% of the matchup term and takes friendly
regret out of band at 9.9%. This is the flattening signature, and the mechanism is stated in the entry that rejected
`mason` reach 400 → 600: *the marksman is the mason's one clean answer*, out-ranging it 650 to 400 and taking zero
damage. Plating blunts that answer — the marksman row shifts −0.057 at cap 12 and −0.082 at cap 10, structural, with
the largest column spreads on the board — so the sharpest matchup in the matrix is the one being paid out.

### The same mechanic on the operator, which is a friendly

Point 8 below, taken the same afternoon. Hostile damage is spread where friendly damage is not — grime 5, mason 10
(splash 12), wrecker 13, jackal 18, hound melee 20 — so a cap on a *friendly* unit is not a single-unit dial at all.
At 14 it is worth 22% against a `jackal`, 30% against a `hound`'s melee, and **exactly nothing** against grime, mason
or wrecker. Unpaid, catalog-only.

| operator cap | `Var(R)` | friendly dead | hostile dead | friendly regret | premium |
|---|---|---|---|---|---|
| baseline | 0.0367 | 3 — incl. `operator` | 5 | 14.0% | 3 col / 3 win |
| **14** | 0.0384 (**+0.0017**) | 3 — `operator` **out**, `marksman+impact` in | 5, unchanged | 13.4% PASS | 2 col / 2 win PASS |
| 12 | 0.0391 (**+0.0025**) | 3 — same set as 14 | 5, unchanged | 14.5% PASS | 1 col MISS |
| 10 | 0.0387 (+0.0020) | **4 (+1.3** [+1, +2]**)** | 5, unchanged | 13.2% PASS | 1 col MISS |

**Cap 14 is the first setting measured in this whole investigation that costs no resolved regression on any gate.**
`Var(R)` rises resolvedly, both regrets stay in band, the hostile side does not move at all — no concentration,
because the carrier is friendly — and the premium falls from 3 columns to 2, which still passes and is not resolved.

**And it lands in the right row, which is the thing that has failed every previous attempt.** The paired shifts at
cap 14, in order:

| row | shift | structural sd |
|---|---|---|
| `operator` | **+0.109** | 0.115 |
| `impact+operator` | +0.062 | 0.078 |
| `marksman+operator` | +0.045 | 0.081 |
| `breacher+operator` | +0.023 | 0.043 |

The buff is worth **five times more to the operator alone than to `breacher+operator`**, which is exactly the
inversion the 2026-08-28 pierce entry asked for and could not produce — there, the operator's `a[i]` improved while
the row stayed dead and `breacher+operator` collected the gain. The mechanism is plain once stated: a breacher
already absorbs what the plating would have stopped, so plating pays least where a tank is standing in front.

Read against the pierce, which paid in the same 9 of 15 columns and *lowered* `Var(R)`: **column count is not
sufficient — the spread of the payoff within those columns is what matters.** Pierce paid a flat fraction against
anything armoured; this pays 30% against a hound that reaches, 22% against a jackal, and nothing at all otherwise.

What it costs, and neither is resolved: `marksman+impact` takes the operator's place on the dead list, so the count
is unchanged and the dead row has moved rather than gone; and the premium's margin, already zero, is spent. `Var(a)`
also falls 0.0025, so any `SII` gain here is partly a denominator effect and should not be quoted as diversity.

**Not shipped, because it cannot yet be validated.** No policy in `campaign_matrix.json` buys an operator, so
`scons sim` can neither confirm nor reject this — the same blocker recorded on 2026-08-28. Fixing that is a
prerequisite, not an optional extra, and it is a scenario change rather than a design one.

### Through the campaign gate — after making the gate able to see an operator at all

`scons sim` could not judge this, because no policy bought an operator. The cause is sharper than "the policies do
not like it": `best_affordable` takes the most expensive affordable unit with a **strict** `>`, so on a cost tie the
earlier catalog entry wins. `breacher` and `operator` both cost 20 and the breacher is listed first, so the operator
is unbuyable at every energy level by `greedy`, `defensive` and `patience` — and `mix` named only breacher and
marksman. **This became true when the operator was repriced 25 → 20**, a shipped change that silently removed a unit
from a gate, and it went unnoticed because the sweep was read as one number.

Two mix policies were added, plus an optional `label` on a policy spec so two policies of the same kind are not
pooled into one row by `aggregate_sim.py`. 6 policies x 5 levels x 25 seeds = 750 runs, **paired by
`(level, policy, seed)`**:

| policy | before | after | delta |
|---|---|---|---|
| `defensive` | 125/125 | 125/125 | **+0** |
| `greedy` | 85/125 | 85/125 | **+0** |
| `patience` | 91/125 | 91/125 | **+0** |
| `mix` (breacher 2, marksman 1) | 99/125 | 99/125 | **+0** |
| `mix_balanced` (breacher 1, marksman 1, operator 1) | **107/125** | 105/125 | −2 |
| `mix_operator` (breacher 1, operator 2) | 27/125 | 28/125 | +1 |
| total | 534/750 | 533/750 | −1 |

**Plating the operator is invisible at level scale.** ±2 cells out of 125 on the two policies that can see it, net −1
across 750 runs, and both moves are in `level_02`. The four policies that buy no operator are **exactly** +0 with
zero integrity drift, which is the isolation check: the mechanic touches what it should and nothing else.

**So cap 14 is not shipped.** The matrix says `Var(R)` +0.0017 resolved and the operator row alive; the campaign says
nothing happened. Against that, the matrix costs are real — the dead row moved to `marksman+impact` rather than
going, and the premium's zero margin is spent. A change that improves no gate and softens two does not ship on a
structural argument alone.

**The instrument fix is the part worth keeping.** `mix_balanced` wins 107/125 against `mix` at 99, which looked at
first like the campaign contradicting the lab's "the operator has no job". **It does not — see the reconciliation
below.** Both of those totals are averages over five levels that disagree with each other, and the average is the
one number in this sweep that carries no information.

### Reconciling the two instruments, which turned out not to disagree

The aggregate win rates suggested the campaign rated the operator where the lab did not. Read per level, the
suggestion evaporates — and the totals turn out to be hiding a genuine matchup:

| policy | level_01 | level_02 | level_03 | level_04 | level_05 | total |
|---|---|---|---|---|---|---|
| `mix` (breacher 2, marksman 1) | **3/25** | 22/25 | 25/25 | 24/25 | 25/25 | 99 |
| `mix_balanced` (1/1/1) | 16/25 | 17/25 | 25/25 | 25/25 | 24/25 | 107 |
| `mix_operator` (breacher 1, operator 2) | **25/25** | 2/25 | 0/25 | 0/25 | 0/25 | 27 |

`mix` and `mix_operator` are near-perfect complements. **That is composition diversity at level scale, and reading
the sweep as one number per policy destroyed it** — the whole 107-against-99 gap is level 1, where `mix` collapses.

The lab was then run against the campaign's own content rather than against even mixes of the roster: each hostile
column one shipped level's spawn composition scaled to a 12-body force, each friendly row one of the policies the
campaign played, 51 seeds. The spec file has since been deleted along with the rest of the content-denominated
instruments, so the columns are recorded here instead:

| column | 12-body force | from spawn shares |
|---|---|---|
| `level_01` | grime 8, hound 4 | grime 67%, hound 33% |
| `level_02` | grime 5, wrecker 5, hound 2 | grime 45%, wrecker 39%, hound 16% |
| `level_03` | grime 4, wrecker 3, mason 3, hound 2 | grime 33%, wrecker 25%, mason 25%, hound 17% |
| `level_04` | grime 3, wrecker 3, jackal 2, mason 2, hound 2 | grime 23%, wrecker 23%, jackal 23%, mason 16%, hound 16% |
| `level_05` | wrecker 4, grime 3, hound 2, mason 2, jackal 1 | wrecker 33%, grime 25%, hound 18%, mason 12%, jackal 12% |

| column | lab's best answer | campaign's winner | |
|---|---|---|---|
| `level_01` | `breacher` | `mix_operator` 25/25, `mix` 3/25 | **disagrees** |
| `level_02` | `mix` | `mix` 22/25 | agrees |
| `level_03` | `mix_balanced` | `mix_balanced` 25/25, `mix` 25/25 | agrees |
| `level_04` | `marksman` | `mix_balanced` 25/25, `mix` 24/25 | agrees |
| `level_05` | `marksman` | `mix` 25/25, `mix_balanced` 24/25 | agrees |

**Levels 2-5 agree outright**, and across these five columns the lab ranks `mix_operator` at `a = 0.009` against
`mix` at 0.265 — it is never a strict winner, which is what 27/125 says too. The disagreement is **level 1 alone**.

Its mechanism, from the sweep's own fields:

| policy | level_01 win | leaks | energy spent | deployments per run |
|---|---|---|---|---|
| `mix_operator` | **100%** | 0.0 | 206 | operator 6.0, breacher 4.3 |
| `greedy` | **100%** | 1.6 | 209 | breacher 8.3, marksman 1.0 |
| `defensive` | **100%** | 0.0 | 180 | breacher 4.0, impact 2.0, marksman 2.0 |
| `mix_balanced` | 64% | 19.4 | 182 | marksman 3.4, breacher 2.8, operator 1.7 |
| `mix` | **12%** | 15.4 | **94** | breacher 2.4, marksman 1.7 |

`mix` spends **94 energy** where everything else spends about 200. `MixPolicy` banks when the unit furthest below its
target share cannot be afforded, so on `{breacher 2, marksman 1}` it buys one breacher, then sits on 24 energy
waiting for a 27-cost marksman while four hounds cross the belt from t=3s at 120px/s. **The variable is not "has an
operator", it is "has something affordable at t=3s"** — the breacher costs 20 as well, and `greedy` wins the level
100% on 8.3 of them.

**So the two instruments agree, the lab's "the operator has no job" stands, and the level-1 gap is the missing clock
(open problem 6) with a measured instance at last.** What the lab cannot represent is not *composition* but the
interaction of a unit's price with a wave's arrival time — and that is a property of `MixPolicy` and the level's
opening, not of the roster.

Three things follow that are worth more than the plating was:

1. **Never read this sweep as one number per policy.** Two of its six policies are complements whose totals differ
   by 8 and whose per-level records differ by 25. Read `(level, policy)`.
2. **`MixPolicy` had a banking pathology, and it was not a roster fact.** A ratio naming an expensive unit stalled
   against an early rush; the policy would not buy the affordable unit it also named. `mix`'s 3/25 on level 1 was
   that, not a verdict on breacher-plus-marksman. **Fixed and measured — see below.**
3. **The lab can be pointed at any columns you like**, which costs one spec file and no code, and turns "does the
   lab predict the game" into a table. Worth doing once when the two instruments seem to disagree — and worth
   deleting afterwards rather than leaving a content-denominated gate standing.

### Fixing `MixPolicy`'s bank, and the variant that had to be rejected first

Not a bug so much as a rule with no exception. The banking is deliberate and the reason is written in the source: a
mix that skipped to the next-neediest *affordable* unit whenever the neediest was out of reach would buy the cheap
end every time energy crossed its cost and never reach the expensive end at all — a mono-stack claiming to be a
composition. What was missing is that a plan is worth nothing to a base being hit while the purse fills.

Two exceptions were tried, both over 750 paired runs against the same baseline:

| | `mix` | `mix_balanced` | others | `level_01` mix | `level_04` mix | total |
|---|---|---|---|---|---|---|
| baseline | 99/125 | 107/125 | — | 3/25 | 24/25 | 534/750 |
| **broad** — `under_pressure`, as `patience` uses it | 51/125 | 81/125 | +0 | **11/25** | **0/25** | **460** |
| **narrow** — hostile inside base engage range | **106/125** | **113/125** | +0 | **11/25** | 24/25 | **547** |

**The broad test swallowed the rule.** It also fires on "three or more hostiles live", which is the normal condition
of a level with forty spawns, so on levels 3-5 it never stopped firing and handed back exactly the mono-stack the
original comment warned about: `level_04` marksman deployments **4.5 → 1.0 per run, 24/25 → 0/25**. The author's
reasoning was right and my first patch was the thing it predicted.

**The narrow test takes the whole gain and none of the cost.** `level_01` reaches 11/25 either way; `level_04` comes
back byte-identical at 24/25 with marksmen restored to 4.5. Net **+13 over 750 runs**, entirely on level 1, with the
four policies that never bank at exactly +0 — `mix_operator` included, because every unit it names costs 20 and it
never had a bank to stall in.

Three things worth keeping:

1. **A policy's rule and its exception are a matched pair, and the exception's *width* is the whole design.** Same
   direction, same seam, one line apart: one variant is +13 and the other is −74.
2. **`patience`'s pressure test does not generalise.** It is fine for a policy choosing *when* to spend a fixed
   ladder and ruinous for one choosing *what* to buy. The two now sit next to each other in the source as
   `hostile_at_the_gate` and `under_pressure`, named so the difference is visible at the call site.
3. **`level_01` is still only 11/25 for `mix`, and that is now a level question rather than a policy one.** The
   override cannot fire until something reaches the base, by which point a 27-cost opening has already cost the
   player the wave. The level is teaching the rush on purpose; whether it should also punish a mix naming a marksman
   this hard is open problem 8's play test, not a sweep.

**Every campaign figure recorded before this is denominated in the stalling policy**, including the 400/500 and
534/750 baselines and the plating readings above. The plating verdict is unaffected — it moved `mix_operator` and
`mix_balanced` by ±2 and neither is sensitive to the bank — but re-baseline before comparing anything new.

### Retiring the campaign as a roster gate, and building the tempo lab

**Levels are content, and some of them are written to be lost.** A win-rate gate over narrative is measuring the
story; worse, it makes roster work hostage to content churn — `grime` armour 5 is blocked today purely because
`level_02` happens to be grime-heavy, a rejection already flagged as reversible the moment level 2 is retuned.

Reviewing every use of the gate in this log: **five uses, one decision, and that decision was the content-driven
one.** The other four returned +0, +6-then-reverted, confirmatory, and −1.

But the clock cannot go with it. The recorded measurement is that the same `operator` 20 → 25 shifts
`breacher+operator` *uniformly* in the matrix (structural sd 0.012, under the floor) and *by column* in a timed lab
(0.172, fourteen times the spread). Price is the most-used lever in the game; with no timed instrument no repricing
can be judged at all. So the clock stays and the content goes.

**The tempo lab.** Four synthetic engagements in `data/lab/`, outside `data/levels/` so nothing here can be mistaken
for narrative. Same base and the *same twenty hostiles* in each — grime 8, wrecker 5, hound 3, jackal 2, mason 2 —
so the only variable is the arrival schedule: `tempo_rush` (all twenty in 30s), `tempo_spike` (a minute of nothing,
then all twenty), `tempo_grind` (one every six seconds), `tempo_escalation` (cheap first, heavy last). Rows are
compositions rather than spending heuristics. `scons sim scenario=res://scenarios/tempo_lab.json`.

| composition | rush | spike | grind | escalation |
|---|---|---|---|---|
| `breacher+marksman` | 100% | 100% | 100% | 100% |
| `marksman` | 100% | 100% | 100% | 100% |
| `marksman+operator` | 100% | 96% | 100% | 100% |
| `marksman+impact` | 100% | 88% | 100% | 100% |
| `operator` | 68% | **0%** | 100% | 100% |
| `breacher+operator` | **4%** | 28% | 100% | 100% |
| `breacher` | 0% | 0% | 84% | 100% |
| `breacher+impact` | 0% | 0% | 0% | 100% |
| `impact` | 0% | 0% | 20% | 48% |
| `impact+operator` | 0% | 0% | 40% | 20% |

**With a clock, reach dominates harder than it does in the matrix.** The top four rows are marksman-bearing and clear
everything; the table is close to a ladder. The inversions that exist are all in the lower half: `operator` beats
`breacher+operator` on the rush 68% to 4% and loses to it on the spike 0% to 28%; `impact+operator` beats `impact` on
the grind and loses on the escalation.

### What this instrument still needs

1. **Critical purse, not win rate.** Sweeping `starting_core_resource` showed the transitions are step functions: at
   any fixed purse most cells read 0% or 100%, which is precisely the ceiling effect `DIVERSITY_MODEL.md` rejects win
   rate for. The purses shipped here (rush 100, spike 120, grind 40, escalation 40) were hand-calibrated to the band
   where compositions come apart, and they rank but do not measure distance. **Bisect the purse per
   (composition, engagement) the way `scons matrix` bisects budget** and the top four rows separate.
2. **One purse cannot serve four schedules.** The same force delivered faster is harder, so a shared purse was
   abandoned. That is not a flaw to fix — the purse *is* the measurement, once it is bisected.

### What is now known

- **Check `owned_upgrades` before believing any composition table.** The roster is `base_units` — breacher alone —
  plus owned `unit_unlock` cards. The lab's first run had `owned_upgrades: []`, so all twelve composition policies
  silently played the same breacher-only force and produced a full, plausible, meaningless table. **Third instance
  today of an instrument quietly measuring something else**, after the unbuyable operator and the pooled `mix`
  label. The tell was `energy_spent: 0` with an empty deployment list, and it is worth checking first every time.
- **A gate denominated in content inherits the content's churn.** This is the general form of the `grime` armour 5
  block, and it is why the campaign sweep is now documented as a level instrument only.

### What is now known, from both carriers together

6. **In this roster, hostile plating is a marksman dial and nothing else.** Friendly ranged damage is 6, 8, 8 and
   **19**: the marksman's is the only shot any useful cap truncates. So plating a hostile unit means exactly "make
   the marksman worse against that unit", and where it is pointed decides which gate moves:
   - at a hostile the marksman answers *partially* (`wrecker`), it **creates** a matchup — `Var(R)` +7%, at the cost
     of concentrating five already-strong rows;
   - at a hostile the marksman answers *uniquely and totally* (`mason`), it **destroys** one — hostile dead slots
     3 of 5 recovered, at the cost of 17-23% of `Var(R)` and, at cap 10, regret itself.

   Both carriers rob one gate to pay another, and the two failures are the same fact seen twice.
7. **`grime` mono is dead in all ten matrices measured today** — every wrecker setting, every mason setting, and the
   baseline. It is the most robustly dead row on the board and nothing aimed at another unit reaches it.
8. **The friendly side is where this mechanic has not been tried.** Hostile damage is far more spread than friendly:
   grime 5, mason 10 (splash 12), wrecker 13, jackal 18, hound melee 20. A cap on a *friendly* unit is therefore not
   a single-unit dial but an anti-`jackal`, anti-`hound` stat — and it is survival-side, which is what the standing
   diagnosis says the `operator` actually needs. **Measured above, and it is the only setting of the three carriers
   that costs no resolved regression**: the mechanic is not at fault on either hostile, the side it is on is.

### Cost of the attempt

`damage_cap` needed a `UnitConfig` field, a loader line, a `SimEntity` field, and one call swapped in each of the two
damage paths — `SimWorld::take_damage` and `HealthComponent::take_damage`, both now routed through a single
`damage_after_mitigation` so the ordering cannot drift either. About 25 lines across 8 files, conformance-clean on
the first run, clang-tidy clean after one divide-by-zero guard in a test. Seven 51-seed matrices at roughly 105
seconds each. **The mechanic is left in place**: it is the only lever in the rules that runs opposite to armour, and
the reason it failed here is the carrier, not the arithmetic.

### What to try next

Aim it at a *losing* hostile instead. Four of the five baseline dead hostile rows involve `mason` or `grime`, and the
`mason` is the sharpest case: it is dead as a mono, and its only answer is the marksman, which out-ranges it 650 to
400 and takes **zero** damage doing so. Plating the mason blunts that answer without deleting it — the marksman still
out-ranges and still kills, only slower — which is the shape a question is supposed to have. Concentration into a
weak row is a gain where concentration into a strong one was the whole of this entry's cost.

---

## 2026-08-28 — Giving the lab an economy: measured, then reverted

**Reverted. The finding is kept and the code is not.** No content changed.

Every matrix here is a *set-piece*: `allocate_budget` spends a lump sum and the whole friendly force stands on the
belt at `t=0`. The player never receives a budget that way. `MatchSession::tick_energy` pays one energy per second,
`record_enemy_died` pays a bounty back, and friendly costs are 20–27, so a composition is **delivered over a minute
or more**, not fielded. A `B*` of 120 is 76 seconds of income at level 1.

A prototype added three `LabSetup` fields modelling the discipline a player actually uses — bank until contact,
commit the bank at once, then buy the next unit of the planned line as income and bounties allow. The mix bought at a
given `B*` was identical either way and every unit kept its rank, so the two labs differed in *when a unit arrived*
and nothing else. Six 51-seed matrices, paired.

### What it found

1. **Cost is a level lever only because the lab hands over a lump.** The one result worth the whole exercise. Same
   `operator` 20 → 25, both labs, 51 seeds, paired, on the fifteen columns fully bounded in both:

   | lab | `breacher+operator` shift | structural sd |
   |---|---|---|
   | lump sum | −0.099 | 0.012 (under the floor) |
   | timed | −0.188 | **0.172** (structural) |

   Fourteen times the column-to-column spread. The lump lab reproduces the published figure exactly (mono shift
   −0.214 against a predicted −0.223, spread 0.006), so it is the economy, not a rebuild. **In the game a price is
   also a delay, and a delay is conditional on what it is late against.** The supporting row does not survive the
   same check — `marksman+operator` reads 0.026 lump against 0.021 timed — so this rests on one clean row plus the
   `operator` mono row ceasing to be measurable at cost 25 at all.
2. **`Var(R)` rose on every reading and the size was never resolved.** +0.0112 [+0.0088, +0.0139] across all cells,
   +0.0011 (about +4%) on the sub-rectangle where every seed is bounded in both runs. The gap is censored cells, and
   the rows that lose their reading are exactly the rows that censor (`marksman` 0.124 → 0.013).
3. **Two explanations for that censoring were wrong, and both were cheap to falsify.** Not the clock: 117 censored
   runs re-measured at a 1200s cap instead of 300s came back identical in kills, losses *and* damage taken — 117 of
   117, no damage at all in the extra fifteen minutes. Not the ceiling: `impact` vs `jackal` reads win rates 0.00,
   0.00, 0.13, 0.07, 0.20 at ceilings of 120 to 400.
4. **The real cause is that the lab has no base.** `SimWorld::move` clamps a friendly at `world_width - margin` but
   decrements a hostile's x with no lower bound, and targeting is forward-only, so a hostile that gets past the line
   walks away for ever and neither side can re-acquire. In the game it would hit the base; here the engagement never
   resolves, is recorded as undecided, and so reads as a loss at every budget. **This is true of the lump-sum lab
   too** — it just almost never triggers, because the whole line is present at `t=0`. A timed line leaks constantly,
   because the opening commitment is thin. It is the same forward-only problem as open problem 7.

### Why it was reverted

Not because it was wrong — because of what it would cost the instrument. The decomposition attributes everything it
cannot explain to `R`, and the whole project reads `R` as *"the right answer changes with the question"*. An economy
in the same instrument puts tempo, leaks and matchup into that one number. Fixing the censoring properly means a
base, leak damage, `deploy_x` instead of fixed ranks, and a policy choosing between defence and pressure — at which
point it is `scons sim` rebuilt in a second kernel, and two engagement models that are supposed to agree is the
`make_shipped_roster` failure at a larger scale.

**`scons matrix` measures composition. `scons sim` measures the game.** Keeping that line is worth more than the
`Var(R)` this would have added.

### What is now known, and does not depend on the code

- **`scons matrix` cannot rank anything time-sensitive, and a price is the clearest case.** Validate repricings in
  `scons sim`. Recorded against the cost section of [`DIVERSITY_AND_BALANCE.md`](DIVERSITY_AND_BALANCE.md).
- **Raising `scons sim`'s resolution (open problem 4) is what closes that gap** — not a better matrix lab.
- **A leaked hostile is unreachable for ever, and the lab silently reports it as an undecided run.** Rare today; it
  would become the dominant failure of any future lab where friendlies arrive over time.
- **Re-run any structural verdict on the uncensored sub-rectangle before believing its size.** A cell censored on
  some seeds contributes only the seeds it won, which both truncates its shift and adds column-to-column spread that
  reads as structure. Half the headline here was that.
- **Suspect a mechanism you have not tested, however well it fits.** "The clock is the limit" fitted every censored
  run sitting at exactly the cap, and was wrong. So was the ceiling. Both took one pass over existing JSONL or one
  re-run to kill.

---

## 2026-08-28 — The native "shipped" roster was not the shipped roster

**No content change. An instrument correction — and it inverts two facts this suite had recorded as measured.**

`tests/test_sim_world.cpp` carried a `make_shipped_roster` helper: a hand-written copy of four `unit_data.json`
entries, needed because the native suite cannot read `res://`. Three tests pinned matchup outcomes against it and
described them as balance facts. The copy omitted `armour`, a stat that did not exist when it was written.

### Result

One friendly against three hostiles at 1500/1650/1800, seed 2026, shipped globals, re-measured in the hosted suite:

| | pinned against the copy | measured against the catalog |
|---|---|---|
| breacher vs 3 grime | **loses**, 0 alive, 24.9s | **wins**, 227/400 hp left, 57.3s |
| marksman vs 3 grime | **wins**, survives under half hp, 14.7s | **loses**, 2 grime still up, 12.7s |
| breacher vs 3 mason | loses, 17.7s | wins, 64/400 hp left, 30.4s |

### What is now known

- **Armour is the whole of the level-1 matchup.** Grime's rifle does 5, so the breacher's `armour: 4` floors it at 1
  and three grime need four hundred shots to kill it. The marksman has none and takes the full 5 from each. The cheap
  anchor outlasts the expensive sniper by a factor of four against the threat level 1 is made of.
- **The counter-relationship runs opposite to the old pins.** Massed grime is answered by armour, not by reach: the
  marksman's 650 buys nothing once three shooters close to 345, because 19 against armour 4 cannot kill them fast
  enough to matter.
- **A fixture that borrows shipped names is a liability.** These pins stayed green straight through the change that
  introduced armour, precisely because they read a copy rather than the catalog. The helper is deleted, the pins now
  live in `tests/test_shipped_content.cpp` against the real catalog, and the native fixtures no longer reuse shipped
  unit names.

The matrix gates were not re-run: neither the roster nor the rules changed, only what the tests were reading.

---

## 2026-08-28 — Give the operator a job by piercing armour, and make the wrecker hunt it

**Reverted. Neither half did what it was for.**

Two changes aimed at the same target from opposite ends: the `operator` is a dead slot with no job and price is
exhausted at cost 20, while the `wrecker` is the strongest hostile and completely flat, so its whole contribution is
`Var(a)`.

- **`operator` `armour_pierce: 2`** — a new attacker-side stat cancelling that much of a target's armour. Armour
  subtracts per hit, so it costs a high-rate shooter the most; pierce is its inverse and should therefore have paid
  for rate, which nothing in the roster does. Predicted: +100% dps against `grime` (armour 4), +50% against `jackal`
  (armour 2), exactly 0% against `mason`, `wrecker`, `hound`.
- **`wrecker` `preferred_roles: {specialist: N}`** — make the wrecker pick the operator out of a line instead of
  trading with the tank in front of it, so a breacher cannot cover an operator.

### Result

| | `Var(R)` | `Var(a)` | regret | dead | premium | campaign |
|---|---|---|---|---|---|---|
| baseline | 0.0367 | 0.0212 | 14.0% PASS | 3/10 | 3 col / 3 win | 400/500 |
| `armour_pierce: 2` | **0.0332** ↓resolved | 0.0194 | **8.8% MISS** | 3/10 | 2 / 2 | 400 (+0) |
| wrecker `specialist: 2.5` | *byte-identical to baseline* | | | | | |
| wrecker `5.0` + `aggro_range: 700` | 0.0393 ↑resolved | 0.0195 | 13.8% PASS | 3/10 | 3 / 3 | 406 (+6) |

The pierce is a resolved regression. The wrecker change passes every gate — and was still reverted, because the
effect runs opposite to its intent: the wrecker declines the tank, *stops shooting* and walks, so every operator row
got **better** (`impact+operator` +0.069). It is a lure, not an assassin, and what it feeds is `breacher+operator`,
already the strongest row. Neither change moved the dead-slot count, on either side.

### What is now known

1. **A row shift classified `structural` is not automatically a diversity gain.** The paired block read every
   operator row as structural (mono shift +0.151, structural sd 0.192) and every other row as an exact zero — the
   mechanic did precisely what the model says it should. But 9 of the 15 hostile columns contain `grime` or `jackal`,
   so most of the row moved together and the level component dominated. **Check how many columns a conditional
   mechanic actually pays in before believing it will move `Var(R)`; a condition that most of the roster satisfies
   is a price.**
2. **The carrier lesson applies to mixes, not just units.** "Give composition-dependent mechanics to the units that
   are losing" is not enough. The operator is losing, but its carrier in practice is `breacher+operator`, which is
   winning: the pierce took the operator's `a[i]` from −0.105 to +0.008 and left it **still a dead slot**, while
   `breacher+operator` went from alive in 6 columns to 9 and regret collapsed to 8.8%. Ask which *row* a unit buff
   lands in, not which unit.
3. **Pierce is monotonically subtractive here.** Holding everything else fixed: pierce 0 → `Var(R)` 0.0513,
   1 → 0.0409, 2 → 0.0384.
4. **`preferred_roles` has two thresholds, and below either one it is byte-identical to absent.**
   - *Score.* The bias multiplies a candidate's threat weight, so it has to outbid what the tank already pulls. Under
     the wrecker's `highest_hp` the breacher scores 400 hp × 2.0 = 800 against the operator's 225 × bias: the bias
     must exceed 3.56 to be picked at all, and **4.44** to take a target already locked, through the 25% retarget
     margin. 2.5 produced an exactly identical matrix.
   - *Sensor.* `resolve_aggro_range` floors the sensor at the gun, so at the wrecker's 330 reach and 200px friendly
     ranks, an operator standing behind a stopped breacher is **outside it**. The wrecker never saw both at once, so
     there was no choice for a bias to change. The preference is inert without `aggro_range` and the sensor does
     nothing without the preference.
5. **The campaign sweep cannot see the operator at all.** Across all 500 runs of `campaign_matrix.json` and all four
   policies, `operator` is spawned **zero** times — `mix` is `{breacher: 2, marksman: 1}`, and the three heuristic
   policies buy breacher, impact and marksman. That is why the pierce campaign run came back with not one cell
   changed. **The campaign gate can neither validate nor reject an operator change until a policy buys one.**
6. **The recorded reason for rejecting `grime` armour 5 is wrong.** `DIVERSITY_AND_BALANCE.md` attributes it to
   "`MixPolicy` buys operators, which is what armour punishes"; the mix policy buys no operators. The real mechanism
   is that armour 5 takes the breacher's 8-damage rifle to 3, and level 2 is grime-heavy. The rejection still
   stands — it is the explanation that was wrong.
7. **Incidental, and the largest result measured that day:** wrecker lure + `grime` armour 5 gives `Var(R)` **0.0513
   (+40%, resolved)**, regret 18.4% mid-band, premium **5 columns / 3 winners**, with `Var(a)` *rising* — so for once
   the `SII` gain is not a denominator effect. It still fails the campaign at 384/500, `level_02/mix` 22/25 → 13/25,
   reproducing the previously recorded 22 → 14. **Retuning level 2 is worth more than anything else tried here.**

### Cost of the attempt

`armour_pierce` needed a new `UnitConfig` field, a loader line, and threading an attacker-side value through both
damage paths (`SimWorld::apply_damage` and `DamageDispatcher::apply` → `AttackTarget::take_damage`) — about 30 lines
across 9 files, conformance-clean on the first run. The wrecker half was catalog-only. Neither was expensive; the
measurement is what took the time, and it is the only reason the ideas were not shipped on the strength of the
arithmetic, which looked right for both.
