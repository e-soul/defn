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
