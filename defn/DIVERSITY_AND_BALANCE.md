# Diversity and Balance

Where the roster and the content stand against the diversity targets, which levers were measured and what each one
did, and how to judge the next change.

- The model behind the numbers — payoff matrix, the decomposition, what each metric means:
  [`DIVERSITY_MODEL.md`](DIVERSITY_MODEL.md).
- How to run the instruments and read their output: [`BALANCE_TOOLING.md`](BALANCE_TOOLING.md).

Every number here is a measurement over 51 seeds on the shipped catalog, at the default lab settings. Reproduce with:

```
scons matrix seeds=51 out=res://build/matrix.jsonl
python scripts/analyze_matrix.py defn/build/matrix.jsonl
python scripts/analyze_matrix.py defn/build/matrix.jsonl --transpose
```

---

## The scoreboard

10 friendly mixes x 15 hostile mixes, 800px separation, 200px friendly spacing, 110px hostile spacing. Bracketed
figures are 95% bootstrap intervals over resampled seeds.

| Metric | Target | Friendly side | | Hostile side (`--transpose`) | |
|---|---|---|---|---|---|
| `SII` | >= 0.50 | 0.633 [0.625, 0.643] | PASS | 0.633 [0.627, 0.643] | PASS |
| Decision regret | 10-30% of budget | 14.0% [12.7, 15.3] | PASS | 14.5% [14.0, 15.0] | PASS |
| `Var(R)` matchup | — | 0.0367 [0.0360, 0.0375] | | same by construction | |
| `Var(a)` power | — | 0.0212 | | 0.0212 | |
| Effective rank | >= 2.50 | 5.99 | PASS, but it over-reports | 6.1 | PASS |
| **No dead slot** (noise floor) | zero, reachable both sides | **3 of 10** [2, 3] | **MISS by 3** | **5 of 15** [5, 6] | **MISS by 5** |
| No auto-include | zero | none | PASS | none | PASS |
| Composition premium | >= 20% *structural*, two columns, *differing* winners | 3 columns, 3 winners [2, 4] / [2, 3] | PASS, no margin | — | |
| `scons sim` campaign | — | 83/100 | | — | |

**The dead-slot count is the only MISS, and it is the whole of what is left.** Genuinely dead today:

- **Friendly (3):** `operator`, `impact`, `breacher+impact`
- **Hostile (5):** `grime`, `mason`, `mason+hound`, `mason+jackal`, `jackal+hound`

**The premium PASS has no margin.** Its lower bound sits exactly on the gate, so anything costing one column loses it.
`SII`, regret and `Var(R)` are all comfortably resolved by comparison.

**`Var(R)` is the row that could not have been faked.** It is 2.5x its starting value across two content changes —
the mason repricing and the armour profile — both levers whose payoff depends on the opposing composition. Six levers
whose payoff does not depend on it moved it by nothing at all.

---

## What is established

### A lever moves `Var(R)` if and only if what it is worth depends on what it is facing

The single most reliable finding here, and it follows from the decomposition rather than from observation: a change
worth the same amount against every column is absorbed entirely by that row's level `a[i]`, however large it is.

| Lever | Moves | Measured |
|---|---|---|
| `affected_fraction`, `splash_damage` | **structure** | Pays nothing against one target and a great deal against six. Doubled `Var(R)` on both sides at once. |
| `armour` | **structure** | Pays nothing against a 19-damage round and almost everything against a stream of 5s. `Var(R)` +21% as a roster profile. |
| New archetype (melee rusher) | **structure** | The only lever that ever moved regret into band on its own. |
| `move_speed`, `hp` | level only | Tried on four units each; moved `Var(R)` by nothing while moving `a[i]` more than any other stat. |
| **`cost`** | **level only, provably** | See below. |
| Reach (hostile out-ranges) | structure, wrong sign | The largest structural change of any lever, but it *flattened* the matrix and halved regret. Reverted. |
| Target preference (hostile) | none | Null. Hostile strength is set by bulk and reach, not by choice of target. |
| Target preference (friendly), aggro weight, minimum range | none alone | Inert until a rusher existed to enter the dead zone and reach the front rank first. |
| Shot-count rounding | none | Null. Overkill changes kill speed, and the dominant unit's budget was survival-bound. |

**Before reaching for a stat, ask what the candidate is worth against a *different* enemy mix than the one it was
designed for.** If the answer is "the same", it will move `a[i]` and nothing else.

### Cost is the purest level lever in the game

`allocate_budget` spends a budget along energy shares, so for a mono shape `B*` scales exactly with cost and `log B*`
shifts by `log(cost_old / cost_new)` — the same amount in every column. Sweeping the `operator` from 25 down, 51 seeds
each, on the raw cells of its mono row:

| cost | predicted shift | measured | spread across the 10 columns |
|---|---|---|---|
| 22 | 0.128 | 0.118 | 0.003 |
| 20 | 0.223 | 0.214 | 0.006 |
| 18 | 0.329 | 0.315 | 0.005 |
| 16 | 0.446 | 0.440 | 0.005 |

The spread is a fifth of the noise floor: the shift really is uniform. The 4-7% shortfall against prediction is
integer unit counts, not a failure of the model. `Var(R)` across the whole sweep: 0.0373, 0.0368, 0.0367, 0.0370,
0.0363 — paired, the 25 -> 20 cut moves it by -0.0006 [-0.0008, -0.0004], which is 1.6% of `Var(R)` against `Var(a)`
moving 40% in the same change.

Two consequences worth acting on:

- **Repricing cannot break the matchups the roster already has**, which makes it the safe first tool on a dead slot.
- **Repricing cannot create one either**, and it fakes `SII` more easily than any other lever, purely as a denominator
  effect. A cost cut also *lowers* regret, because cheapening the weakest row flattens the roster.

> **All of this is a fact about the lab, and it does not transfer to the game.** The proof depends on the budget
> arriving as a lump, so that cost decides only how many the budget buys. In the game energy arrives at one per
> second, so a cost is also a *delay* — a 27-cost marksman is 27 seconds of tempo — and a delay pays differently
> against an enemy that punishes arriving piecemeal than against one that does not. Measured once, with a throwaway
> lab that delivered the same line out of a bank plus income: the same `operator` 20 → 25 that shifts
> `breacher+operator` uniformly here (structural sd **0.012**, under the floor) shifts it by column there
> (**0.172**, fourteen times the spread), on the same fifteen columns, 51 seeds, paired. **Price is a level lever
> only for as long as the instrument hands the player the whole purse at once.**
>
> So: `scons matrix` cannot rank a price change against anything time-sensitive. **Validate repricings in
> `scons sim`**, which has the clock, the bounties and the base. See the entry of 2026-08-28 in
> [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md) for the measurement and for why the lab was left alone.

The one exception is small: a *mix* row is not immune, because cheaper `u` buys more `u` for the same energy share and
the realised composition drifts. Measured on `marksman+operator`, the per-column spread of the shift is 0.010 to
0.060 — four to twelve times the mono row's, and still under the floor the gates are read at.

### Archetypes buy diversity; numbers do not

Six levers applied to the existing units moved nothing that survived. One new archetype moved regret from 8.8% to
11.1% and raised `Var(R)` by a third. The reason: every unit in the game was the same archetype — a ranged shooter
that walks forward and stops to trade — and reach totally orders such units, so no arrangement of their numbers can
produce a matchup matrix.

**`hound`, the rusher.** 110 hp, melee 20 per 0.8s at 128px, no gun, speed 120. Sized from the approach arithmetic: it
survives one marksman's approach fire (79 damage against 110 hp), reaches a breacher in 1.0s, and loses to a breacher
in contact. It gave the marksman its first fight, at twice the budget of the one it wins — and note *how*: it takes
only 153 damage and loses 0.33 units. It is not dying, it is ineffective, because it cannot shoot inside 200px.
Minimum range, aggro weight and the composition premium all became live measurements the moment it existed.

**The role mechanism.** `UnitRole`, a per-shooter `preferred_roles` bias table and an `aggro_range`, all folded into
the existing threat weight as `effective_weight = threat_weight * role_bias(role)`. A role is a *targeting handle*,
read only by the enemy, so a role on one side does nothing until something on the other side names it. Exactly one
edge is on: `hound -> sniper`, which makes the hound decline the front line to reach a marksman.

That edge is the clearest single matchup in the game, and it is conditional exactly where it was designed to be:

| friendly mix | vs `hound` | vs `jackal+hound` | vs `mason+hound` | |
|---|---|---|---|---|
| `breacher+marksman` | +21% | +21% | +10% | tank in front of a sniper — the dive answers it |
| `marksman+impact` | -6% | -10% | -19% | no tank to walk past; the hound strands itself |

**`impact`, the counter-puncher.** The asymmetric answer to a diver is not another diver — friendlies are *answers*
and hostiles are *questions*, and an answer shaped like the question is how a payoff matrix collapses toward rank 1.
The answer that worked was making the pass expensive: `target_preference` `highest_hp` -> `nearest` and melee 15 -> 30.
The preference was the load-bearing half and needed no code — under `highest_hp` an impact stood plinking a grime
while a hound ate it, because the closing threat never cleared the 25% retarget margin. The effect is conditioned
where it should be: -28% against `hound`, -25% / -19% / -17% against the mixed hound columns, and **0%** against
`jackal` and `wrecker`, the two hostiles that never close. `marksman+impact` went from dead slot to the strongest row
on the board.

The tune was swept rather than argued, and `ranged_damage` settled back at 8: at 4 the unit is simply weak (`Var(a)`
sextuples, `SII` fails at 0.407, and `Var(R)` is the *highest* of the four values tried — interaction variance rising
while the roster gets worse); at 9 an unintended flat buff compresses `Var(a)` and costs regret.

### The answer cannot be specific while the question is only easier or harder

Decomposing the same matrix transposed for the first time showed the friendly side passing while the hostile side
failed both diversity metrics: `SII` 0.286, regret 7.6%, and ten times the power spread. The hostile roster was a
ladder, and every friendly-side change was pushing on the wrong end.

Pricing **one** hostile unit into contention then moved every headline number on **both** sides — hostile `SII` 0.284
-> 0.523, regret 7.4% -> 10.5%, `Var(R)` doubled, friendly distinct answers 5 -> 7 — without one friendly stat
changing. **Run the decomposition transposed as well; hostiles are the cheaper end to fix, because they carry no
cost-balance constraint.**

### The carrier matters more than the value

The same armour sweep on the `wrecker` raises `Var(R)` too — and quadruples `Var(a)` (0.028 -> 0.131) while regret
collapses to 0.0%. The wrecker becomes the answer to everything. The wrecker starts at `a = +0.192` and grime at
-0.430; same lever, opposite outcome.

> **A conditional buff still reads as raw power when it is given to a unit that is already strong. Give
> composition-dependent mechanics to the units that are losing.**

And **more is not better**: armouring five units beat armouring two on no metric at all. Armour on both sides partly
cancels — the differential is what creates structure, not the amount.

### Measure a unit property at the granularity it lives at

Armour was nearly abandoned on a level-1 regression that turned out to be an artifact of measuring it one unit at a
time. Armour is a *roster* property: armouring `grime` alone breaks level 1, because armour's counter is the
marksman's 19-damage shot and the marksman is not unlocked yet — but armour on the **breacher** pays for armour on
grime in exactly that place, because grime's 5-damage rifle is the most blunted attack in the game (5 -> 1 at armour
4, the floor, while mason splash keeps 8 of 12 and jackal 14 of 18).

### Damage-side levers cannot move a budget that survival bounds

Damage taken by each mono force at its own critical budget, with units lost:

| Friendly | vs grime | vs mason | vs wrecker | vs jackal |
|---|---|---|---|---|
| marksman | 260 / 1.00 | **0 / 0.00** | 75 / 0.00 | 133 / 0.07 |
| operator | 250 / 0.80 | 370 / 0.87 | 386 / 1.07 | 817 / 2.20 |
| breacher | 595 / 0.27 | 1130 / 2.00 | 1439 / 0.33 | 1308 / 2.13 |
| impact | 304 / 0.53 | 661 / 2.00 | 739 / 0.60 | 838 / 1.67 |

The marksman does not die: its budget is set by how many are needed to clear the field before the clock, not by
whether they survive. **Reach is not a damage advantage, it is a survival advantage**, and no damage-side breakpoint
touches survival. The mechanism is that units stop moving the instant they can attack, so a longer-ranged unit gets a
free window of `(range gap) / (approaching unit's speed)` — five free marksman shots against a mason, which is
exactly why that cell reads zero.

A related standing constraint: **a force whose combat output is additive in its members has `R ~ 0` by
construction.** The original rules were almost exactly additive, which is why the design question was never "what
stats should the units have" but "which non-additivity to introduce".

---

## Shipped changes, and what each bought

### Pricing the mason — `affected_fraction` 0.5 -> 1.0, `splash_damage` 7 -> 12

The mason carries the only mechanic in the game that is non-additive by construction, and it was correctly shaped and
merely underpriced: heavy damage into a cheap massed line, zero against a marksman that out-ranges it. Only the levers
that pay when several targets are caught could fix that without deleting the counter.

- `affected_fraction` is the strong lever and `splash_damage` saturates: splash 13 -> 16 moves `mason a` by 0.004,
  fraction 0.5 -> 1.0 moves it by 0.164.
- `affected_fraction: 0.65` is a **no-op** — with nearest rounding, half and two-thirds of the candidate counts that
  actually occur round to the same integer. It is a continuous-looking knob with a step function underneath.
- The window is narrow: `SII` rises monotonically with `splash_damage` while regret *falls*, so the two gates close
  from opposite directions and both pass only at 11-13. 12 ships for margin on both.

What it did to the player's bill against six masons, before -> after, 51 seeds:

| hostile row | vs `breacher` | vs `impact` | vs `breacher+impact` | vs any marksman column |
|---|---|---|---|---|
| `mason` | 81 -> 141 (+74%) | 79 -> 147 (+85%) | 69 -> 144 (**+109%**) | **+0%** |
| `mason+wrecker` | 104 -> 167 (+61%) | 97 -> 163 (+69%) | 90 -> 152 (+68%) | +0% to +3% |
| `grime+mason` | 67 -> 117 (+74%) | 70 -> 112 (+59%) | 64 -> 106 (+66%) | +0% to +2% |

`breacher+impact` is the single most affected cell in the matrix: a mixed short-range line is the most clustered thing
the player can field, which is precisely what the unit is for. The marksman column is **exactly** unchanged at 51
seeds — splash is paid only to targets that are not the direct one, so the counter is untouchable by these levers.
`mason+wrecker` now wins 5 of the 10 friendly columns, and they are precisely the five with no marksman in them.

Also measured, and pinned as tests: one mason's output **saturates at six defenders** (it is dead before the seventh
engages), and its output is ordered by how long it lives rather than by body count — against an impact line closing at
98px/s it does less than half what it does against breachers.

### The armour profile — `breacher` 4, `grime` 4, `jackal` 2, everything else 0

Armour was fully wired the whole time and set to zero on every unit. Flat subtraction per hit, so it is strong against
many small shots and weak against few big ones — and the friendly roster is split hard by exactly that axis: breacher
8, impact 8, operator 6, **marksman 19**.

| | before | after |
|---|---|---|
| `Var(R)` matchup | 0.0309 | **0.0373 (+21%)** |
| `Var(a)` power | 0.0282 | 0.0214 |
| Hostile regret | 10.5% | **14.5%** |
| Friendly regret | 13.4% | **16.3%** |
| Composition premium | 1 column, 1 winner MISS | **4 columns, 3 winners PASS** |
| `scons sim` | 72/100 | **82/100**, nothing regressed |

The first premium PASS that survives the level/structural split. Each value has a job: `breacher 4` takes grime's
rifle to the floor, which is the whole of the level-1 answer; `grime 4` halves the operator's shot and costs the
marksman a fifth of its own, so burst answers it and volume does not; `jackal 2` exists because every profile that
cleared the premium gate killed exactly one unit, and at `breacher 4 / grime 4` it was the jackal. The `wrecker`
staying at 0 is deliberate: its durability is 180hp, flat and unconditional, and grime's is armour, which is
conditional — two kinds of toughness, countered differently, are worth more than two units that are simply hard to
kill.

### Repricing the operator — 25 -> 20

The cost sweep, read under the noise-floor gate:

| operator cost | `Var(R)` | dead, strict | dead, noise floor | friendly regret | premium |
|---|---|---|---|---|---|
| 25 | 0.0373 | 5 of 10 | 4 of 10 | 16.3% | 4 cols, 3 winners PASS |
| 22 | 0.0368 | 5 of 10 | 4 of 10 | 16.3% | 4 cols, 4 winners PASS |
| **20 (shipped)** | 0.0367 | 4 of 10 | **3 of 10** | 14.0% | 3 cols, 3 winners PASS |
| 18 | 0.0370 | 4 of 10 | 4 of 10 | 11.7% | 1 col, 1 winner MISS |
| 16 | 0.0363 | 4 of 10 | 4 of 10 | 11.4% | 2 cols, 2 winners PASS |

**20 is a strict optimum, not a tie**, and it shows the limit of the tool. At 18 and 16 the `operator` mono does come
alive — but `impact+operator` and `marksman+impact` die in exchange, so the dead rows only move. **A price can stop a
unit being overpriced; it cannot manufacture a job, and past a point it spends other rows' jobs to buy one.** `SII`
rose 0.555 -> 0.633 on this change with the script's denominator warning firing correctly: that gain is `Var(a)`
falling, and is not a diversity improvement.

---

## Measured and not shipped

Kept because a negative result costs the same to measure as a positive one and is worth exactly as much next time.

| Candidate | Why not |
|---|---|
| `grime` armour 5 | `Var(R)` +31% resolved, and the premium goes from a bare PASS to a fully resolved [4, 4] columns / [3, 3] winners — but **both dead-slot counts are unchanged** (the sweep's predicted gain was against a weaker baseline), and it costs `MixPolicy` 10 paired wins in 125, almost all of it `level_02` 22/25 -> 14/25. Level 2 is grime-heavy and `MixPolicy` buys operators, which is what armour punishes. **Becomes a one-line change again the moment level 2 is retuned.** |
| `grime` speed +50% | Raises hostile `SII` to 0.605, the highest ever recorded here. `Var(R)` does not move at any speed, regret is invariant to four decimal places, and the whole gain is `Var(a)` falling 29%. |
| `mason` speed 48 -> 72 | Passes the premium gate outright — while `Var(R)` *falls* 0.0309 -> 0.0254. Both answers got worse and the mono just got worse faster. The gate now splits level from structure and no longer reports this as a PASS. |
| Armour on the `operator` | `Var(R)` falls and the premium drops to zero columns. The operator is a high-rate, low-damage shooter, which is precisely the profile armour punishes: it is armour's victim, and arming it muddles the mechanic's only clean counter-relationship. |
| Armour on the `wrecker` | `Var(a)` quadruples, regret collapses to 0.0%. |
| Hostiles out-ranging friendlies | Largest structural change of any lever measured, but it flattened the matrix and halved regret. |
| `mason` reach 400 -> 600 | Hits breachers harder *and* starts hurting marksmen (0 -> 98 damage), which deletes the one clean answer to it. The shipped ordering is load-bearing: breacher 245 < impact 320 < operator 380 < **mason 400** < marksman 650. |
| Hostile spacing 110 -> 200px | Hostile regret 14.5% -> 8.8% MISS and the premium falls to 1 column, in exchange for one hostile dead row. The mechanism is the *marksman's* reach, not melee reach: six hostiles at 110px span 550px and are engaged all at once, at 200px they span 1000px and are engaged piecemeal. Open question rather than a rejected change — see below. |

---

## How to judge a change

1. **`scons conformance`** — mandatory on anything touching combat, movement, or frame order.
2. **`scons matrix` before and after, at 51 seeds**, decomposed **both ways**. Read **regret first, then `Var(R)`,
   then `SII` and the premium**: the last two are ratios and both can be moved by making the denominator worse.
3. **Always pass `--baseline`** and read the paired block. A delta whose interval spans zero is not a result. Check
   the per-row `level` / `structural` classification before believing a lever did what you wanted — if the rows read
   `level`, the lever is a price however it is spelled.
4. **Gate the dead-slot count on the noise-floor reading, not the strict argmax.** No mix outside every column's noise
   floor; none present in every argmax at constant weight.
5. **`scons sim` with `MixPolicy` against the mono policies**, at enough seeds to resolve what it is being asked.
6. **Pin each job's defining matchup as a test**, in the hosted suite if it is composition-sensitive. Catalog values
   that encode a breakpoint (`hp: 96` is five marksman shots plus one) must be pinned, because nothing about the
   number says what it is for.
7. **On a null result, add a wiring test before concluding the mechanic is inert.** A matrix that comes back
   byte-identical looks the same whether the mechanic does nothing or was never connected;
   `sim_world_carries_a_minimum_range_from_the_catalog` is the pattern.
8. **Check what a probe measured against, and against how many compositions.** A unit benchmarked against its own
   counter always looks broken; against its best matchup it always looks fine. **Three compositions is the working
   minimum: the two extremes and the mix.** The mason reads 206 damage against a breacher line and 92 against an
   impact line of the same size.
9. **Suspect a flat result.** A sweep once returned identical rows for five configurations because a string replace
   silently failed, and an `affected_fraction` step turned out to be a genuine no-op for the same reason a bug would.
   Rows agreeing to three decimals on every metric at once did not happen by chance.

### Designing a unit that has a job

1. **Write the job down first, as a fight** — "answers massed short-range pressure", "makes closing expensive". A job
   you cannot phrase as a matchup is not a job, and a job that pays the same against every enemy is a price, not a job.
2. **Add it to `data/unit_data.json`** and give it animations. The kernel reads the shipped catalog, so a new entry is
   immediately measurable.
3. **Measure it against the whole hostile roster** (`scons balance`, adding it to `DEFAULT_FRIENDLIES`). If it does
   not win the column you named, the stats do not match the intent yet.
4. **Check it is not dominated and does not dominate** — `scons matrix`: a mix in no column's noise floor is a dead
   slot, a mix in every argmax is an auto-include.
5. **Check it does not trivialise a level** — sweep with and without it unlocked, including a `mix` policy that names
   it, or the sweep can only tell you whether it is the most expensive thing on the roster.
6. **Pin the matchup that defines the job**, then run `scons test_all`.

A sanity screen exists for step 3 — `sqrt(hp * ranged dps) * reach * speed` will catch a grossly mispriced unit — but
it is a product, therefore scalar-valued and totally ordered, so it **cannot express a matchup whatever the numbers
do**. Unit power is now read off `a[i]`, which is measured.

### Keeping a level honest

Tune each mission with two numbers: **total threat**, from the measured coefficients `scons balance` reports, and
**peak enemy count inside a 5-second window** (`peak_window_5s`).

1. Sweep it against every policy, at the upgrades a player would reach it with.
2. Read `leak_events` and `remaining_integrity` **before** win rate. A level cleared at full integrity by every policy
   never happened, whatever its threat total says.
3. Read `energy_idle_integral`. Large means the economy, not the content, is what is limiting the player.
4. If it is too easy, lower starting energy or tighten spawn spacing first. If it is too hard, reduce spike density
   before reducing total threat.

---

## Open problems

**1. The dead-slot count, on both sides.** Three friendly rows and five hostile ones, listed in the scoreboard above.
The hostile side is the larger half and the cheaper one, since hostiles carry no cost-balance constraint. Four of the
five involve the mason or grime.

**2. The operator has no job.** It is dead in 1 of its 4 rows after the repricing, and prices cannot manufacture a job
— past cost 20 they only move the dead rows around. It is a high-rate, low-damage mid-range shooter, so its job cannot
come from durability (armour on it is measurably wrong), and **nothing in the roster currently rewards rate as such**.
This is a design pass, not a sweep. The catalog also declares it `SPECIALIST` while it has no special ability; nothing
should *prefer* that role until the ability exists, or the preference will be aimed at a fiction.

**3. The level tables are stale, and level 2 is now blocking a matrix improvement.** They have never been revisited,
and the content has moved a long way from them: the hound was added to every wave in levels 1-5 (roughly +20% spawns,
with level 1's opening wave retyped so the game opens by teaching the rush), the mason's measured threat went 1.89 ->
3.84 while it appears 9, 7 and 7 times in levels 3-5, `impact` became a melee counter-puncher, and the base no longer
pulls fire (`threat_weight: 0.25`). Regenerate the coefficients before using them. Retuning **level 2** specifically
is what unblocks `grime` armour 5, and it is worth doing on its own terms either way.

**4. The campaign sweep is the least resolved gate in use.** It is still read as two unpaired point estimates at 5
seeds, and it is the gate that rejected `grime` armour 5 — at 5 seeds it read 83 -> 81, the right size to dismiss as
noise; at 25 seeds paired by seed it is an unambiguous -10 on one policy. Raise its default seed count and pair it
against a baseline, as `analyze_matrix.py` already does. A scripting change, not a design one.

**5. Hostile line spacing.** 110px is inside the 128px melee reach, and widening it to match the friendly side costs
two passing gates while buying one hostile row. It is not obvious which spacing is *right*: 200px is arguably closer
to a real level, where hostiles arrive over time and strung out, and the lab models no reinforcement-travel cost at
all. Doing it properly means widening the spacing **and** retuning against the new baseline, as one deliberate piece
of work.

**6. The lab has no clock, so it cannot see tempo — and that is deliberate.** Every number above comes from a
set-piece in which the friendly force is already standing on the belt, because the budget was handed over as a lump.
The player never receives one that way: energy arrives at one per second, bounties pay back on kills, and a
composition is *delivered* over a minute rather than fielded. A throwaway lab that modelled this raised `Var(R)` on
every reading taken, and reclassified `cost` from a level lever to a structural one (above). It was **not** kept: the
decomposition attributes everything it cannot explain to `R`, so an economy in the same instrument would put tempo,
leaks and matchup into one number and call the total diversity. The realistic instrument already exists and is
`scons sim`.

What this costs, and it is a standing cost rather than a bug: **anything whose value depends on arriving early is
invisible here**, and a price is the clearest case. Judge those in `scons sim` — which makes raising its resolution
(problem 4) the thing that closes this gap, not another lab.

**7. Divers bypass tanks completely, and nothing stops them.** Combat and movement are forward-only, so once a diver
crosses a tank's x they are permanently invisible to each other, and `threat_weight` cannot help because a gunless
unit never reads it. The breacher's counter to a hound — it wins in contact, 7.3s against 16s — disappears the moment
the hound stops stopping. The counter-puncher taxes the pass rather than blocking it, which is the only answer that
exists today. The hostile side also has no `TANK`, so a future friendly diver would have nothing to decline on its way
to a jackal: the two dives are not symmetric.

**8. Two game-feel changes are unreviewed on screen.** The retarget margin — ranged units now re-ask every tick and
switch when a candidate beats the current target by 25%, where they previously held a target for as long as it stayed
reachable — and `impact` fighting in melee. The hound and its dive have been played and approved. The rusher in level
1 is a difficulty decision rather than a measurement result and is awaiting a play test: `DefensivePolicy` banks
energy until a hostile reaches base engage range, and against a 120px/s rusher the bank never gets spent in time.
