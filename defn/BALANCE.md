# Balance Strategy

> For the maths behind the measurement -- what the payoff matrix is, why the power formula below cannot express a
> matchup, and what `SII`, regret and the composition premium actually compute -- see
> [`DIVERSITY_MODEL.md`](DIVERSITY_MODEL.md).

## Unit Budget

Comparison formula inside each side of the roster:

$$
\text{power} = \sqrt{\text{hp} \times \text{ranged dps}} \times \text{reach} \times \text{speed}
$$

The multipliers should stay close to 1.0. Range and speed matter a lot, so small changes are enough.

There is no `aoe` term, because there is no `aoe` field. What splash is worth depends on how tightly the *player*
happens to be standing, so it is a free parameter of the engagement rather than a stat of the unit -- which is also
why it is one of the few things in the game that can create a matchup rather than a ranking. Measure it; do not price
it here.

- Breacher: cheapest anchor, highest hp, shortest reach.
- Marksman: longest range, best pick into jackal and mason.
- Impact: fastest unit, aggressive mid-range pressure.
- Operator: safest sustained general-purpose fire.
- Grime: baseline pressure.
- Mason: splash tax on clustered friendlies.
- Wrecker: main direct combat threat.
- Jackal: long-range pressure check.

## Level Tuning

Tune each mission with two numbers:

- Total threat.
- Peak enemy count inside a 5-second window.

Threat points, measured by `scons balance` rather than estimated (see "Threat, remeasured" below):

| Hostile | Measured | Previously estimated |
|---|---|---|
| Grime | 1.0 | 1.0 |
| Mason | 2.9 | 1.4 |
| Wrecker | 3.0 | 1.8 |
| Jackal | 2.9 | 1.6 |

Totals for the shipped levels under those numbers:

| Level | Composition | Spawns | Threat | Peak in 5s |
|---|---|---|---|---|
| 1 | 16 grime | 16 | 16 | 6 |
| 2 | 14 grime, 12 wrecker | 26 | 49 | 7 |
| 3 | 12 grime, 9 mason, 9 wrecker | 30 | 65 | 7 |
| 4 | 10 grime, 10 jackal, 7 mason, 10 wrecker | 37 | 89 | 8 |
| 5 | 14 grime, 7 jackal, 7 mason, 19 wrecker | 47 | 111 | 7 |

If a level is too easy, lower starting energy or tighten spawn spacing first. If a level is too hard, reduce spike density before reducing total threat.


## The simulator

Balance questions used to be answered from the formula above. They are now answered by measurement: the simulator
plays whole matches headless, thousands per second, and reports what actually happened.

It is a lab tool. Nothing in a shippable build contains it, and no balance verdict fails a build. One part of it does
gate: the conformance harness, because a disagreement between the simulator and the game is a bug in one of them.

### What it is

`defn/src/application/simulation` is a second driver for the game's rules, sitting next to the Godot one. It runs a
belt of entities on a fixed 1/60 step with no scene tree, no physics, no assets and no rendering.

It is deliberately **not** a second implementation of the game. Every rule that decides an outcome is called, not
restated:

| Concern | What the simulator calls |
|---|---|
| Combat state machine, command expansion | `advance_combat` |
| Target selection, range classification | `select_target_from_snapshots`, `classify_target_by_distance` |
| Attack timing, windup, shot release | `UnitAnimationState`, `AnimationClock` |
| Projectile flight | `advance_projectile` |
| Splash resolution | `resolve_projectile_impact` |
| Field promotion | `FieldPromotionRuntime` |
| Per-spawn range variation | `resolve_unit_runtime_config` |
| Economy, waves, scoring, end conditions | `MatchDirector`, `SpawnScheduler`, `MatchSession` |
| Camera scrolling | `CameraScrollController` |
| Progression modifiers and unlocks | `progression_rules` |

What it supplies is only the facts those rules would otherwise read off nodes: positions, who can see whom, who is
alive. Change a combat rule and the simulator changes with it, because it is the same function.

### What it models, and what it does not

Modelled: movement, melee, hitscan fire, projectile flight and muzzle offsets, splash, damage and overkill, field
promotion, deaths and bounty, waves and spawn timing, the energy economy, camera scrolling and the spawn positions it
moves, the base as a turret, and progression upgrades.

Not modelled, because none of it changes an outcome: sprites, audio, VFX, muzzle flashes, damage flashes, health bars,
the HUD, and the explosion animation a spent shell lingers to play.

One thing to keep in mind when reading results: **manual repositioning is not modelled.** A player who drags units
around has an option the simulator's policies do not.

### Why you can trust it

`scons conformance` replays five scenarios twice -- once as real Godot units in a real scene, once in the simulator --
and compares the traces: positions within a pixel, health, pose, engagement and attack mode exact, deaths within a
tick, shells in flight exact. It runs as part of `scons test_all`.

If you change combat, movement, or the order things happen in a frame, run it. A failure means the two have diverged,
and the trace names the entity and tick where they first disagree.

### Determinism

A run is reproducible from `(scenario, seed)`. The step is fixed and never wall-clock, entities step in ascending id,
and the only randomness -- belt-Y placement and per-spawn attack-range variation -- goes through the seeded
`RandomSource` port. The same seed gives a byte-identical report; a different seed gives a different one. That is why
every number below is quoted over a spread of seeds rather than a single run.

## Using it

All commands run from the `defn` directory and need Godot, either as `godot_bin=<path>` or `GODOT_BIN`. The engine
loads the shipped JSON through the real loaders and hands plain structs to the kernel, which is why the simulator has
no parser of its own.

| Command | Answers |
|---|---|
| `scons balance seeds=25 out=res://build/balance.txt` | What is each unit worth? |
| `scons sim scenario=<path> seeds=<n> out=<path>` | Is this level winnable, by whom, at what cost? |
| `scons conformance` | Does the simulator still agree with the game? |
| `python ../scripts/aggregate_sim.py <jsonl>... [--per-unit]` | Reduce a sweep to decisions |

### Asking a level question

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
| `seed` | Base seed; `seeds=N` runs `seed`, `seed+1`, ... `seed+N-1`. |
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

Then run it and reduce it:

```
scons sim scenario=res://scenarios/campaign_progression.json seeds=20 out=res://build/sweep.jsonl
python ../scripts/aggregate_sim.py build/sweep.jsonl --per-unit
```

Each run writes one JSON object on one line. The fields worth reading:

| Field | Reads as |
|---|---|
| `victory`, `decided` | Won; and whether the match settled at all before `max_seconds`. |
| `clear_time_s` | How long the match took. |
| `remaining_integrity`, `base_health` | How close it was. Full integrity every seed means the level never threatened. |
| `leak_events` | Every hostile hit on the base, with timestamps. Empty means the threat never arrived. |
| `energy_idle_integral` | Unspent energy integrated over time. High means the player could not spend what they earned. |
| `deployments`, `energy_spent` | What was bought, per unit. |
| `per_unit` | Damage dealt and taken, kills, deaths and mean lifespan, per unit type. |
| `peak_concurrent_enemies`, `peak_window_5s` | Crowding, and the spike density this document tunes against. |
| `front_line_trace` | Where the leading engagement stood, sampled once a second. |
| `camera_scroll_events` | How far the player pushed the belt. |

### Player policies

A balance sim without a player model is a sim that lies, and one policy produces a single number with no meaning.
Always read the spread.

| `kind` | Plays like |
|---|---|
| `greedy` | Deploys the best affordable unit the instant energy allows. The floor: no patience. |
| `defensive` | Banks everything until the leading hostile is inside the base's own weapon range, then commits and keeps spending. This is how the game is actually played well. |
| `patience` | Saves toward the top of the roster, keeps a reserve, spends immediately under pressure. |
| `mix` | Plays a target composition: `"weights": { "breacher": 2, "marksman": 1 }`. Deploys whichever named unit is furthest below its share of the field, and banks when that unit is out of reach. |
| `scripted` | A fixed plan: `"script": [{ "time": 1.0, "unit_id": "breacher" }]`. Exact reproduction of one line of play. |

`greedy`, `defensive` and `patience` all end in "the most expensive affordable unit" -- they vary *when* to spend,
never *what* to buy, so a sweep of them compares mono-stacks and nothing else. `mix` is the only one that can express
a composition, and it is what any level-scale claim about composition has to be measured with.

The gap between `greedy` and `defensive` on the same content is usually the most informative number in a sweep. If a
level is winnable only by `defensive`, it is asking for knowledge the game has not taught yet.

Adding a policy is a subclass of `PlayerPolicy` in `application/simulation/policies`, plus a line in `make_policy`.
It sees only what a player sees: energy, integrity, wave, the roster, and what is on the belt.

### Asking a roster question

`scons balance` runs two fixed experiments and prints both tables.

**Threat** puts six breachers against four of each hostile and reports what the player pays per kill, normalised to
grime. The reference matters more than anything else here: it has to beat every hostile type *and still bleed doing
it*. A marksman wall out-ranges the entire roster and takes literally zero damage, which reports every threat as zero
and looks like a working measurement.

**Roster** spends a fixed 120 energy on each friendly against eight grime. Counts differ so that spend does not --
comparing three cheap units with three expensive ones says nothing about whether either is worth its cost.

Both rosters are arguments with defaults, so a driver can hand `measure` any slice of the catalog.

### Asking a diversity question

Both tables above are a single line through a much bigger object. The threat table fixes the defence at six breachers
-- one row. The roster table fixes the enemy at eight grime -- one column. The campaign sweep compares mono-stacks.
**No measurement in this document, taken on its own, can detect diversity in either direction**, because diversity
lives strictly in the off-diagonals of

    M[i][j] = -log B*(friendly mix i, hostile mix j)

where `B*` is the *critical budget*: the smallest energy budget at which mix `i` beats hostile mix `j` half the time,
found by bisection. Budget replaces win rate as the payoff scale because win rate has saturated -- levels 2 to 5 read
100% at full integrity across every policy, so that entire table carries about one bit. A budget never saturates, is
denominated in the same energy the player spends, and is approximately additive in the log, which is what makes the
decomposition below mean something.

```
scons matrix out=res://build/matrix.jsonl
python ../scripts/analyze_matrix.py build/matrix.jsonl
python ../scripts/analyze_matrix.py build/matrix.jsonl --baseline build/matrix_before.jsonl
```

The third form is the one to use when judging a change rather than taking a reading. `SII` and the composition
premium are both ratios, and both can be moved by making the denominator worse instead of the numerator better --
flattening the roster rather than creating a matchup, or nerfing the best single unit rather than improving mixes.
The premium report splits each column into a **level** half (this mix is stronger everywhere) and a **structural**
half (this mix answers *this* column), and the gate counts only the structural half; that needs no baseline. `SII`
does: with one the script prints `Var(R)` beside it and warns when the ratio moved and the numerator did not.

With no `spec=`, the matrix is every friendly mono-stack and every friendly pair against every hostile mono-stack and
every hostile pair. A `spec=res://scenarios/matrix_smoke.json` file names the mixes explicitly, and sets the budget
ceiling, tolerance, iteration cap and seed count.

The analysis is a plain two-way decomposition, `M[i][j] = mu + a[i] + b[j] + R[i][j]`:

| Term | Reads as |
|---|---|
| `a[i]` | Unit power. The transitive axis, and all the power formula at the top of this document can express. |
| `b[j]` | Content difficulty. What "threat points" measures. |
| `R[i][j]` | Matchup interaction. **This is diversity, and it is the design target.** |

| Metric | Reads as | Target |
|---|---|---|
| `SII` = `Var(R) / (Var(a) + Var(R))` | Of the variation the roster choice explains, how much comes from *matching* rather than raw strength. Near zero means the roster is a power ladder. | >= 0.5 |
| Effective rank | How many independent strategic axes exist. Rank 1 means one best unit per budget. | >= 2.5 |
| Decision regret | What a pre-mission draft screen would be worth: the budget saved by knowing the enemy in advance. | 10-30% of budget |
| Support size and usage | A unit in no argmax is a dead slot; a unit in every argmax at constant weight is an auto-include. Both are the same bug. | neither |
| Composition premium | How much cheaper the best mix is than the best mono-stack. | >= 20% on two hostile mixes, with *different* winners |

Two things the report will not let you skip. Every cell carries a seed-variance confidence interval, and a gap
narrower than two sigma does not exist for the player either. And `--transpose` runs the same decomposition the other
way round: composition is an *answer*, and identical questions cannot have distinct answers, so diversifying the
friendly roster is wasted work until the hostile roster asks different things.

### Asking a one-off question

For a single engagement, drive `SimWorld` directly from a native test. No Godot, no content loading, about 0.3 ms per
engagement:

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

## Designing a unit that has a job

The formula at the top says whether a unit is *strong*. The simulator says whether it is *distinct* -- whether there
is a fight it wins that the others do not. A roster where one entry is never the right answer is a roster with a dead
slot, however well-priced it is.

The loop:

1. **Write the job down first**, as a fight. "Answers massed short-range pressure." "Trades cheaply into long-range
   chip damage." A job you cannot phrase as a matchup is not a job.

2. **Add the unit to `data/unit_data.json`** and give it animations. Nothing else is needed: the simulator reads the
   shipped catalog, so a new entry is immediately measurable.

3. **Measure it against the whole hostile roster.** Add it to `DEFAULT_FRIENDLIES` in `defn_balance_runner.cpp` and
   run `scons balance`. Read the table for the fight you named. If it does not win that column, the stats do not
   match the intent yet.

4. **Check it is not dominated.** Compare its row against the others at equal energy. Losing on every axis to a
   cheaper unit means the slot is dead. Winning on every axis means something else's slot is. `scons matrix` answers
   this directly: a unit in no argmax is a dead slot, a unit in every argmax is an auto-include.

5. **Check it does not trivialise a level.** Add it to a sweep and compare win rates and `remaining_integrity` with
   and without it unlocked. A unit that takes a level from 30% to 100% across every policy is not a new option, it is
   a new difficulty setting. Include a `mix` policy that names it, or the sweep can only tell you whether it is the
   most expensive thing on the roster.

6. **Pin what you learned.** A test in `tests/test_sim_world.cpp` for the matchup that defines the job.

7. **Run `scons test_all`.** Conformance included.

The same loop tunes an existing unit: change the numbers, re-run steps 3 to 5, and look at whether the distinctness
you wanted actually appeared.

### Keeping a level honest

1. Recompute total threat and peak from the measured coefficients above.
2. Sweep it against every policy, at the upgrades a player would reach it with.
3. Read `leak_events` and `remaining_integrity` before win rate. A level cleared at full integrity by every policy
   never happened, whatever its threat total says.
4. Read `energy_idle_integral`. Large means the economy, not the content, is what is limiting the player.
5. If it is too easy, lower starting energy or tighten spawn spacing first. If it is too hard, reduce spike density
   before reducing total threat.

## What the simulator says today

Every number below is a measurement, reproducible from the scenario and seed named with it.

### Where you fight is worth more than what you deploy

Twenty seeds per policy on level 1, from a fresh save -- breacher only, 44 starting energy, 1 energy per second:

| Policy | Win rate | Deployments | Breachers lost | Damage per deployment | Base hits taken |
|---|---|---|---|---|---|
| Defensive | **100%** | 8.0 | 1.6 | 161 | 5.5 |
| Greedy | 0% | 6.2 | 6.0 | 79 | 60 |
| Composition | 0% | 6.0 | 5.1 | 63 | 60 |

The defensive policy banks its energy until the leading grime is inside the base's own 520-pixel range, then spends
everything and keeps spending. Nothing else differs -- same units, same economy, same level.

It is worth exactly twice as much damage per deployment, and that is entirely about geography. Deployments land at
`deploy_x`, which is 100 pixels off the left edge of the camera. Meeting the enemy where they first appear means a
breacher spends about fifteen of its twenty-three seconds walking, and fights alone. Letting them come means a short
walk, a base turret firing alongside for another 233 damage a run, and a concentrated group instead of a trickle.

So the honest reading is not that level 1 is too hard. It is that **playing forward is close to twice as expensive as
holding the line**, and the naive policies all play forward. Whether that gap is intended is a design question: a new
player who deploys on sight loses level 1, and nothing on screen explains why.

Two cross-checks that the model reads the level correctly: the sweep reports `peak_window_5s` of exactly 6, matching
the target above, and sixteen grime is exactly the 16-threat total above.

Either of two single changes also takes greedy to a 100% win rate, if the forward-playing line is the one being tuned
for: `battery_pack` + `reserve_cells` + `overclocked_capacitors` (+35 starting energy, +1 regen), or
`sharpshooter_contract` alone (marksman unlocked).

### The cheapest anchor is not an answer

One friendly against three grime, starting 500 to 800 pixels away, from `tests/test_sim_world.cpp`:

| Friendly | Cost | Outcome | Time | Left standing |
|---|---|---|---|---|
| Breacher | 20 | Loses | 24.9s | One grime on 71 of 95 hp |
| Marksman | 27 | Wins | 14.7s | Marksman on 45 of 180 hp, never moved |

Read the second row carefully: the marksman engages at 650 while grime stall at 345, so it stops on the spot and never
takes a step. Reach is doing the work the power formula credits it with.

### Threat, remeasured

`scons balance` puts six breachers against four of each hostile and reports what the player pays per kill. The
reference has to beat every type and still bleed doing it: a marksman wall out-ranges the whole roster and takes
literally zero damage, which measures nothing. Averaged over twenty-five seeds:

| Hostile | Win | Seconds | HP per kill | Threat | Defenders lost |
|---|---|---|---|---|---|
| Grime | 100% | 16.2 | 64 | **1.00** | 0.0 |
| Mason | 100% | 17.7 | 185 | **2.89** | 0.0 |
| Wrecker | 100% | 23.1 | 189 | **2.95** | 0.8 |
| Jackal | 100% | 20.6 | 187 | **2.92** | 1.0 |

Two things the estimates had wrong.

**Grime is not one third of a wrecker, it is one third of everything else.** The estimated spread ran 1.0 to 1.8;
measured, there is a cliff after grime and then a plateau.

**Mason, wrecker and jackal cost the same.** Within 2% of each other, despite estimates of 1.4, 1.8 and 1.6 and
completely different stat lines. They get there differently -- the jackal out-ranges the defence and kills one of it,
the wrecker grinds through the longest fight, the mason taxes the cluster and loses nobody -- but the bill is
identical. Treating them as interchangeable at 2.9 is closer to the truth than the old spread was, and if they are
meant to be distinct threats, that is a design change rather than a coefficient change.

One earlier measurement, kept because it is the same statement from a different angle: one breacher against three of
each dies in 24.9s to grime and 17.7s to mason, a ratio of 1.41. That matched the old estimate of 1.4, which is why
it was believed. It measures offence only. Cost per kill also prices durability and reach, and those are where the
advanced hostiles actually earn their keep.

### The roster, measured

The same command spends a fixed 120 energy on each friendly against eight grime. Counts differ so that spend does
not -- comparing three cheap units with three expensive ones says nothing about whether either is worth its cost.

| Friendly | Cost | Bought | Spent | Seconds | HP lost | Units lost |
|---|---|---|---|---|---|---|
| Breacher | 20 | 6 | 120 | 24.3 | 617 | 0.2 |
| Marksman | 27 | 4 | 108 | 12.5 | **0** | 0.0 |
| Impact | 23 | 5 | 115 | 17.9 | 290 | 0.2 |
| Operator | 25 | 4 | 100 | 14.8 | 98 | 0.0 |

**The breacher is dominated by the operator** on this test: less total energy, 40% faster, and a sixth of the damage
taken. The marksman is better still and does the job untouched, because 650 reach against 345 means the grime die
during the approach and never fire.

Read the caveat with it: this is one unit type at a time, on open ground, against the shortest-ranged hostile. It
gives the breacher's 400 hp nothing to do. The honest conclusion is not "delete the breacher" but "the breacher has
no measurable job in this fight", which is a prompt to give it one -- or to price it lower.

### The campaign curve

`scons sim scenario=res://scenarios/campaign_progression.json seeds=20` plays every level with the upgrades a player
would plausibly have reached it with -- one claimed per completed level, taking the biggest lever available: the
marksman unlock, then energy, then the impact unlock, then regen. Twenty seeds per cell.

| Level | Threat | Greedy | Defensive | Composition | Base hits, best policy |
|---|---|---|---|---|---|
| 1 | 16 | 0% | **100%** | 0% | 5.5 |
| 2 | 49 | 100% | 100% | 0% | 0.1 |
| 3 | 65 | 100% | 100% | 100% | 0.0 |
| 4 | 89 | 100% | 100% | 100% | 0.0 |
| 5 | 111 | 100% | 100% | 100% | 0.0 |

**The curve runs backwards.** Level 1 is the only one that resists, and only the policy that holds its ground beats
it. From level 2 onward every policy clears with full integrity, and from level 3 the base is never touched at all --
not one hit across sixty runs.

Threat does climb steeply, 16 to 111 across the campaign, nearly seven-fold. Player power climbs faster: a single
unit unlock replaces a dominated 20-energy unit with one that clears the same threat untouched, and that happens
after level 1. The content is not too easy in absolute terms; the reward curve outruns it.

For contrast, the same sweep with no upgrades at all
(`scons sim scenario=res://scenarios/campaign_matrix.json`) gives the defensive policy 100%, 95%, 30%, 0%, 0% -- a
sane difficulty ramp. The whole inversion lives in the upgrades.

Three places to spend the next tuning pass, in order of how much the measurements support them:

1. **Level 1 is the wall.** It is the only level a new player can lose, they lose it to the natural way of playing,
   and nothing on screen explains that holding ground is worth twice deploying forward.
2. **The first unlock is worth more than three levels of threat growth.** Either the unlock should come later, or
   levels 2 to 5 need to be built for a roster that has it.
3. **The advanced hostiles are one threat, not three.** Give them distinct costs or accept they are reskins.

### The splash tax, measured

One mason shell into three stationary targets, spacing the only variable:

| Spacing | Victims | Damage |
|---|---|---|
| 20 px (clustered inside the blast) | 2 | 17 |
| 320 px (spread outside it) | 1 | 10 |

So clustering costs 70% more per shell. Note what caps it: `affected_fraction` of 0.5 over three candidates rounds to
two victims, and the game picks those two in the order it walks the entity container -- spawn order -- not by how close
they stand to the impact. A tighter cluster does not widen the blast, it only fills it.

Every row above is pinned as a test, so a rules change that moves them shows up as a failure.

## Checklist

**0. Sanity screen, before anything else:** compare units with the power formula above, then check it against
`scons balance`. This step can only catch a unit that is obviously mispriced. The formula is a product, so it is
scalar-valued and totally ordered, and it therefore *cannot* express a matchup however the numbers move -- it will
never tell you whether the right answer changes with the question. Everything below is what does.

1. Check that every unit still has a distinct job, and that the roster table shows it.
1. Recompute total threat and peak 5-second spike from the measured coefficients.
1. Re-run `scons sim scenario=res://scenarios/campaign_progression.json seeds=20` and confirm the curve still rises.
