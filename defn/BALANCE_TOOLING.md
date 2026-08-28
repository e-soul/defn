# The Measurement Instruments

How to run the balance and diversity measurements, and how to read what they print.

- Why these quantities and not others: [`DIVERSITY_MODEL.md`](DIVERSITY_MODEL.md).
- What they currently say, and what is still open: [`DIVERSITY_AND_BALANCE.md`](DIVERSITY_AND_BALANCE.md).
- How the simulation kernel is built: [`ARCH.md`](../ARCH.md), "Simulation kernel" and "The engagement lab".

---

## The instruments

| Command | Answers | Output |
|---|---|---|
| `scons matrix` | What does every friendly composition cost against every hostile composition? | JSONL, one row per cell per seed |
| `python scripts/analyze_matrix.py` | Is the roster balanced, diverse, deep? | The decomposition and every gate |
| `scons balance` | What is one hostile worth, and what does one friendly buy for its energy? | Two tables |
| `scons sim` | Is this level winnable, by whom, at what cost? | JSONL, one row per match |
| `python scripts/aggregate_sim.py` | Reduce a sweep to decisions | Win rates, clear times, spend |
| `scons conformance` | Does the kernel still agree with the game? | Pass/fail, and the first diverging tick |
| Native `SimWorld` tests | What happens in one engagement? | Whatever the test asserts, in ~0.3 ms |

Everything except `scons conformance` is a lab tool: no balance verdict fails a build. Conformance gates, because a
disagreement between the kernel and the game is a bug in one of them.

## Running them

All `scons` targets run from the `defn` directory and need Godot, either as `godot_bin=<path>` or `GODOT_BIN`. The
Python scripts take a path and can be run from anywhere; the examples below are written from the repository root.

> **Go through the `scons` targets, never the runner scripts by hand.** `scons matrix`, `scons balance`, `scons sim`
> and `scons conformance` depend on the extension library and rebuild it. `scons test` builds only the native binary,
> so invoking `godot --script tests/godot_matrix_runner.gd` after a source change measures **the previous build** and
> reports it as a clean null.

Cost is linear in seeds and the noise floor falls with their square root, so the seed count is the only dial that
trades runtime against resolution.

---

## `scons matrix` — the payoff matrix

Measures `B*(friendly mix i, hostile mix j)`, the smallest energy budget at which mix `i` beats hostile mix `j` half
the time, by bisection. It emits one JSONL row per `(friendly mix, hostile mix, seed)`: each seed is bisected on its
own, so the spread across seeds is a real confidence interval rather than an average that already threw the noise
away.

```
scons matrix seeds=51 out=res://build/matrix.jsonl
scons matrix seeds=51 separation=2400 out=res://build/matrix_far.jsonl
scons matrix spec=res://scenarios/matrix_smoke.json out=res://build/smoke.jsonl
```

| Argument | Default | Meaning |
|---|---|---|
| `seeds` | 15 | Seeds per cell. **Use 51 for any judgement** — see the noise floor below. |
| `out` | stdout | `res://` path for the JSONL. |
| `spec` | — | A JSON file naming the mixes explicitly and overriding the bisection settings. |
| `separation` | 800px | How far apart the two lines start. |
| `spacing` | 200px | Gap between friendly ranks. |

**The default rectangle** is every friendly mono-stack and every friendly pair (4 units, 10 rows) against every
hostile mono-stack and every hostile pair (5 units, 15 columns). Monos give the transitive axis `a[i]`; the pairs are
the only rows that can carry an interaction, so a matrix of monos alone would answer the diversity question "no" by
construction. Every hostile column fields six units, split evenly when it is a pair, so the columns stay comparable.

**Bisection settings** (`CriticalBudgetOptions`, overridable in a spec): ceiling 400 energy, tolerance 2, 8
iterations, win threshold 0.5. A cell that still loses at the ceiling reports `unbounded` rather than a bogus large
number. If many cells come back unbounded, raise `max_budget`.

**The lab parameters are part of the measurement, not scenery.**

| Parameter | Default | Why it matters |
|---|---|---|
| `separation` | 800px | Friendlies advance at their own speeds, so over a longer walk a mix with a wide speed spread arrives strung out rather than as a line. One separation cannot see that. |
| `friendly_spacing` | 200px | **Must stay wider than the largest melee reach (128px)**, or a unit arriving at the front rank finds the second rank already in contact, and anything meaning to walk past the first rank has nothing to walk past. The original 70px measured every positional mechanic as a flat null. |
| `hostile_spacing` | 110px | Six hostiles at 110px span 550px, inside the marksman's 650 reach, so a marksman engages the whole hostile force at once. Widening it changes the value of the dominant friendly unit in every cell — an open question, not a settled default. |
| belt band | 750–850px | Units are scattered across the belt as in the game, sampled from a *second* random stream (`seed ^ 0x9E3779B9`) so that adding depth does not shift every range variation after it. Splash is the only rule that reads a 2-D distance. |

A spec file names the mixes and sets the bisection:

```json
{
  "seeds": 2,
  "max_budget": 300.0,
  "tolerance": 10.0,
  "max_iterations": 5,
  "win_threshold": 0.5,
  "friendly_mixes": [{ "label": "breacher+marksman", "weights": { "breacher": 1, "marksman": 1 } }],
  "hostile_mixes": [{ "label": "grime", "units": { "grime": 6 } }]
}
```

Friendly mixes are *shapes* — relative weights that a budget is spent along, by largest-remainder apportionment,
because naive flooring collapses a 2:1 mix into a mono-stack at small budgets. Hostile mixes are explicit counts.
Both are interleaved round-robin when they take the field, so a 2:1 mix is not silently measured as "whichever unit
was listed first is the whole front line".

---

## `scripts/analyze_matrix.py` — reading the matrix

```
python scripts/analyze_matrix.py defn/build/matrix.jsonl
python scripts/analyze_matrix.py defn/build/matrix.jsonl --transpose      # is the content diverse?
python scripts/analyze_matrix.py after.jsonl --baseline before.jsonl      # judging a change
python scripts/analyze_matrix.py after.jsonl --bootstrap 2000             # 400 by default, 0 to skip
```

Pure stdlib, like the rest of `scripts/`. It decomposes `M[i][j] = -log B*` into `mu + a[i] + b[j] + R[i][j]` and
prints six blocks.

| Block | Reads as |
|---|---|
| `noise floor` | 2 sigma in log-budget, and the same figure as a share of budget. Gaps narrower than this do not exist for the player either. |
| `balance` | Per row: power `a[i]`, its geometric `B*`, how many columns it strictly wins, how many it is alive in. Then the dead-slot and auto-include counts. |
| `content` | Per column: difficulty `b[j]`, its best answer, and how many *distinct* best answers exist across all columns. |
| `diversity` | `SII`, `Var(a)`, `Var(R)`, what share of `Var(R)` is seed noise, and effective rank. |
| `depth` | The best blind mix, decision regret, support size, and per-unit usage across the argmaxes. |
| `composition premium` | Per column: best mono, best mix, and the premium split into its **level** and **structural** halves. |

**Read them in this order: regret, then `Var(R)`, then the ratios.** `SII` and the raw premium are both ratios, and a
ratio improves when its denominator gets worse just as readily as when its numerator gets better. Both have been
reported here as wins on the strength of a degradation.

- **The premium is fixed at the source.** `log premium(j) = (a[mix] - a[mono]) + (R[mix][j] - R[mono][j])` — `mu` and
  the column difficulty `b[j]` cancel, leaving a level term identical in every column and a structural term specific
  to this one. The gate counts structural columns only and needs no baseline.
- **`SII` cannot be, so it takes `--baseline`.** With one, the script prints `Var(R)` and `Var(a)` before and after
  next to it, and warns when the ratio moved and the numerator did not.

**Dead slots are counted twice.** The strict argmax count is discontinuous — one winner per column however narrow the
win — and is bounded below by the matrix's shape at `max(rows - columns, 0)`. The noise-tolerant count treats a row as
alive in a column if it is the best answer there **or within the noise floor of it**; it has no such floor, so zero is
reachable on both sides, and it is the one to gate on. The strict count is printed alongside it, labelled, because it
appears throughout the project's history.

**Effective rank over-reports.** It is entropy over singular values and counts any direction with variance, noise
included. It has read PASS since the very first baseline, including when every other metric said the roster was a
one-unit ladder. Never trust it alone.

**`--transpose` flips the sign of `M` as well as the axes**, so "higher is better for the row player" holds both ways.
Without the flip every transposed argmax names the *weakest* hostile and the dead-slot reading comes out backwards.

### Seeds and the noise floor

| Seeds | 2-sigma floor, as a share of budget |
|---|---|
| 5 | 13% |
| 15 | 5–6% |
| 51 | 2–3% |

The decision-regret gate sits at 10%. Below 51 seeds the instrument cannot resolve its own pass mark, so **51 is the
number to reproduce with**. The composition premium is the noisiest number on the board — 13% at 15 seeds against 19%
at 51 on identical content — and nothing about it should be believed under 51.

### Bootstrap intervals

Every gated number carries a 95% percentile interval over resampled seeds, 400 replicates by default. The seeds are
drawn once per replicate and applied to the **entire matrix**, because a seed is a whole-simulation seed: cells that
shared one are correlated, and a per-cell resample would discard that correlation and overstate the uncertainty on
every comparison *between* cells — which is what regret, the dead-slot count and the premium are all made of. The
resampling is itself seeded, so a gate gives the same answer twice.

### `--baseline`, and why it is paired

Both runs draw from the same fixed seed list, so cells are differenced seed by seed rather than mean against mean. The
seed noise the two runs share cancels instead of adding, which resolves changes several times smaller than two
independent means can. It prints:

- a **per-row shift table** with the paired standard error and a noise-corrected **structural sd** — the
  column-to-column spread of the shift with the seed noise taken back out, which is exactly zero for a pure level
  move;
- each row classified `level` / `structural, under the floor` / `structural`, the middle case being structure that
  pairing can resolve but the gates cannot act on;
- **paired bootstrap deltas** on every gated number, with the share of replicates agreeing on the sign, so a change is
  reported as resolved or not rather than as a difference of two point estimates.

A delta whose interval spans zero is not a result, however good the point estimate looks. When the two runs' complete
rectangles differ, the run under test is re-measured over the shared rectangle for the comparison only; mismatched
seed sets and censored cells fall back rather than silently comparing the wrong things.

---

## `scons balance` — the two roster tables

```
scons balance seeds=25 out=res://build/balance.txt
```

**Threat** puts six breachers against four of each hostile and reports what the player pays per kill, normalised to
grime. **Roster** spends a fixed 120 energy on each friendly against eight grime; the counts differ so that the spend
does not, because comparing three cheap units with three expensive ones says nothing about whether either is worth
its cost. Both rosters are arguments with defaults, so a driver can hand `measure` any slice of the catalog.

**The reference force is the whole method.** It has to beat every hostile type *and still bleed doing it*: a marksman
wall out-ranges the entire hostile roster and takes literally zero damage, which reports every threat as zero and
looks like a working measurement.

Both tables are a single line through the matrix — threat fixes the defence at one row, roster fixes the enemy at one
column — so neither can see an off-diagonal, and neither says anything about diversity.

---

## `scons sim` — whole matches

```
scons sim scenario=res://scenarios/campaign_progression.json seeds=20 out=res://build/sweep.jsonl
python scripts/aggregate_sim.py defn/build/sweep.jsonl --per-unit
```

A scenario is a JSON file in `defn/scenarios/`. The smallest useful one:

```json
{
  "level_id": "level_01",
  "seed": 2026,
  "policy": { "kind": "greedy" },
  "owned_upgrades": [],
  "camera": "modelled",
  "max_seconds": 300.0
}
```

| Field | Meaning |
|---|---|
| `level_id` | Which level to load, from `data/levels/`. |
| `seed` | Base seed; `seeds=N` runs `seed`, `seed+1`, … `seed+N-1`. |
| `policy` | How the player plays. See below. |
| `owned_upgrades` | Upgrade ids from `data/upgrades.json`, as if already claimed. |
| `camera` | `modelled` (units push it, as in game) or `fixed` (pinned, to isolate scroll pacing). |
| `max_seconds` | Give up and report the match undecided after this long. |

For a sweep, replace the singular keys with lists. `levels` and `policies` form a matrix; `runs` is an explicit list
where each entry overrides the defaults, which is how a sweep gives each level the upgrades a player would actually
have reached it with:

```json
{
  "seed": 2026,
  "policies": [{ "kind": "greedy" }, { "kind": "defensive" }],
  "runs": [
    { "level_id": "level_01", "owned_upgrades": [] },
    { "level_id": "level_02", "owned_upgrades": ["sharpshooter_contract"] }
  ]
}
```

### Player policies

A balance sim without a player model is a sim that lies, and one policy produces a single number with no meaning.
Always read the spread.

| `kind` | Plays like |
|---|---|
| `greedy` | Deploys the best affordable unit the instant energy allows. The floor: no patience. |
| `defensive` | Banks everything until the leading hostile is inside the base's own weapon range, then commits and keeps spending. |
| `patience` | Saves toward the top of the roster, keeps a reserve, spends immediately under pressure. |
| `mix` | Plays a target composition: `"weights": { "breacher": 2, "marksman": 1 }`. Deploys whichever named unit is furthest below its share of the field, and banks when that unit is out of reach. |
| `scripted` | A fixed plan: `"script": [{ "time": 1.0, "unit_id": "breacher" }]`. Exact reproduction of one line of play. |

`greedy`, `defensive` and `patience` all end in "the most expensive affordable unit" — they vary *when* to spend,
never *what* to buy, so a sweep of them compares mono-stacks and nothing else. **`mix` is the only policy that can
express a composition**, and it is what any level-scale claim about composition has to be measured with.

Adding a policy is a subclass of `PlayerPolicy` in `application/simulation/policies`, plus a line in `make_policy`. It
sees only what a player sees: energy, integrity, wave, the roster, and what is on the belt.

### Reading a run

Each run writes one JSON object on one line.

| Field | Reads as |
|---|---|
| `victory`, `decided` | Won; and whether the match settled at all before `max_seconds`. |
| `clear_time_s` | How long the match took. |
| `remaining_integrity`, `base_health` | How close it was. Full integrity every seed means the level never threatened. |
| `leak_events` | Every hostile hit on the base, with timestamps. Empty means the threat never arrived. |
| `energy_idle_integral` | Unspent energy integrated over time. High means the player could not spend what they earned. |
| `deployments`, `energy_spent` | What was bought, per unit. |
| `per_unit` | Damage dealt and taken, kills, deaths and mean lifespan, per unit type. |
| `peak_concurrent_enemies`, `peak_window_5s` | Crowding, and the spike density levels are tuned against. |
| `front_line_trace` | Where the leading engagement stood, sampled once a second. |
| `camera_scroll_events` | How far the player pushed the belt. |

Two standing caveats. **Manual repositioning is not modelled** — a player who drags units around has an option the
policies do not. And this sweep is still read as two unpaired point estimates at a low seed count, which makes it the
least resolved gate in use; raise the seeds and difference by seed before letting it decide anything.

---

## `scons conformance`

Replays five scenarios twice — once as real Godot units in a real scene, once in the kernel — and compares the
traces: positions within a pixel, health, pose, engagement and attack mode exact, deaths within a tick, shells in
flight exact. It runs as part of `scons test_all` and gates like any other correctness test.

Run it on anything touching combat, movement, or the order things happen in a frame. A failure names the entity and
the tick where the two first disagree.

## Native engagement probes

For a single engagement, drive `SimWorld` directly from a test — no Godot, no content loading, about 0.3 ms:

```cpp
SimRoster roster;              // or the shipped catalog, in a hosted test
roster.add(breacher_config);
roster.add(grime_config);

StdRandomSource random(2026U);
SimWorld world(roster, globals, random);
world.spawn("breacher", UnitSide::FRIENDLY, {.x = 1000.0F, .y = 800.0F});
world.spawn("grime", UnitSide::HOSTILE, {.x = 1500.0F, .y = 800.0F});
world.begin_run();

const SimEngagementReport report = run_engagement(world, 60.0);
```

`tests/test_sim_world.cpp` is full of these. Pin the ones that matter: a measurement written down as a test fails when
a rules change moves it, which is how a balance fact stops rotting.

> **Put anything that reads shipped stats in the hosted suite, not the native one.** The native suite cannot read
> `res://`, so everything in it is a fixture and nothing in it is a claim about the catalog. It used to carry a
> `make_shipped_roster`, a hand-written copy of `unit_data.json` that nothing enforced, and the copy drifted: it
> omitted `armour` entirely, which silently **inverted** every matchup pinned against it — a breacher that beats
> three grime in the game lost to them there, and a marksman that loses won. Those pins now live in
> `tests/test_shipped_content.cpp`, which reads the real catalog and is the pattern to copy.
