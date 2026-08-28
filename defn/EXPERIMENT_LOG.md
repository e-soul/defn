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
