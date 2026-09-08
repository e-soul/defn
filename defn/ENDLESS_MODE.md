# Endless Mode

One continuous match with no wave cap, unlocked by completing `level_05`. Waves are not authored: each is generated
by spending an escalating **threat budget** along a **composition shape** that drifts as the run goes on. The run
ends when base integrity reaches zero, or when the escalation passes the ceiling above which a wave is unwinnable by
construction. The record kept is the wave reached and the score at that point.

A run is fully determined by `(seed, tuning)`.

- Where the tuning numbers come from, and how to re-measure them: `scons endless` in
  [`BALANCE_TOOLING.md`](BALANCE_TOOLING.md).
- What one hostile is worth: the `threat` column of `scons balance`, copied into `data/endless.json`.
- Where the pieces live: [`ARCH.md`](../ARCH.md), Module 1.

---

## Why a budget rather than a wave list

A hand-authored endless mode is a contradiction — the content runs out and the last authored wave repeats forever.
The obvious alternative, "pick N random hostiles and scale N", is worse than it looks: a `wrecker` costs the player
roughly seven times what a `grime` costs, so a random draw makes wave difficulty a lottery over composition rather
than a curve. Two waves of identical size can differ by a factor of eight in what they actually demand.

Budget fixes that by making difficulty a single scalar the generator spends, and the project already had the
currency. `scons balance` reports a `threat` column — cost per kill against a fixed six-breacher reference,
normalised to `grime` — which is exactly "what one of these costs the player". Priced in those units, wave `n` is:

```
wave(n) = allocate_budget(threat_costs, shape(n), B(n))
```

`allocate_budget` is the largest-remainder apportionment the engagement lab already used, moved inward to
`domain/content/force_mix.{h,cpp}` and generalised to fractional costs. Largest remainder is the part that is easy to
get wrong: naive flooring collapses a 2:1 shape into a mono-stack at small budgets, so early waves would silently be
the wrong composition rather than a small one.

## The three knobs

**1. Escalation `B(n) = B0 * r^(n-1)`.** `r` is swept in the simulator until the run-length distribution lands where
the design wants it. A wave interval that also grows slightly keeps the late game from becoming a spawn-rate test
rather than a composition test.

**2. Shape drift `shape(n)`.** The relative unit counts a wave is composed of, interpolated across keyframes: early
waves lean `grime`/`hound`, later waves shift weight to `wrecker`, `jackal` and `mason`. Drift alone is still a
treadmill, so the schedule also injects **set-piece waves** at intervals — an all-`hound` rush, a `jackal` firing
line, a `mason` cluster. A set piece is the same generator called with a degenerate shape; it costs no new code and
no new content. Keyframes are authored as bodies and converted to budget share before spending -- see the first
finding below for why that distinction is load-bearing.

**3. Economy counter-pressure.** Bounty income scales with kills and kills scale with `B(n)`, so income compounds
alongside difficulty. The counter is a per-wave bounty decay applied to what a kill *pays*, never to what it
*scores*: `bounty(n) = max(d^((n-1)^k), bounty_floor)`. Base integrity does not regenerate; the run is strict
attrition.

The decay has a **rate** `d`, a **shape** `k` and a **floor**. `k = 1.0` is the geometric decay every measurement
above was taken on; `k = 0.85` ships, for the reasons in the next section. `k < 1` spreads the same reduction over a
longer run, and the floor is a hard stop the rate cannot fall through.

## What keeps it from being solved

`DIVERSITY_AND_BALANCE.md` records 3 of 10 friendly mixes dead and the composition premium passing with no margin. A
mode built on a flat difficulty ramp against a static shape will be beaten by one composition and then it is
arithmetic forever.

Shape drift is the answer to that, and it is the reason to build this mode at all: it is the first content in the
game that changes what the player is facing *within* a single match. A campaign level lets the player pick a
composition at the start and never revisit the decision. Endless makes the decision recur, which is what turns the
off-diagonals of the payoff matrix from a measured property into a played one.

The success criterion is therefore testable, and it is the gate on shipping: **no single friendly mix may survive to
the target wave across all seeds.**

## Failure modes this design is built against

| Failure | Cause | Countermeasure |
|---|---|---|
| Bounty snowball | Income compounds with difficulty | Per-wave bounty decay `d^(n-1)`, swept |
| Solved composition | Static shape, flat ramp | Shape drift + set pieces; gated on the mix-survival sweep |
| Difficulty lottery | Equal-count waves of unequal threat | Waves priced in measured threat cost |
| Flat pacing | Pure geometric ramp reads as monotone | Set pieces at intervals; wave interval growth |
| Unbounded runtime | No cap on a run | A wall-clock ceiling and a `B(n)` ceiling |

---

## What the sweep found

**The keyframes are a ratio of bodies, and that is a trap worth naming.** The first version of this file authored
them as a ratio of *budget*, which reads identically and behaves nothing like it: grime is the cheapest hostile by a
factor of five, so `{grime: 6, hound: 2}` spent as budget share buys twelve grime and no hound at all. The mode
shipped once that way and the first six waves were a grime swarm. `EndlessWaveGenerator::to_budget_shape` now does
the conversion, and `endless_generator_reads_a_keyframe_as_bodies_rather_than_as_budget` pins it shut.

**The opening wave decides more runs than the ramp does.** Sweeping `B0` at `r = 1.06`, 2-3 seeds per cell:

| `B0` | `greedy` | `mono:breacher` | `breacher+marksman` | `defensive` |
|---|---|---|---|---|
| 22 | 5 or 57 | 57 | 57 | 57 |
| 28 (shipped until 2026-09-06) | 4-5 | 9 | 53 | 53 |
| 34 | 4 | 7 | 6-7 | 50 |
| 48 | 3 | 5-6 | 5 | 44 |

At `B0 = 48` the escalation is inert — 1.06, 1.09 and 1.12 all kill everything inside two minutes, because a cold
start with 105 energy cannot absorb a 48-threat opening whatever happens afterwards. Twelve threat either way moves
a run from "nothing dies" to "everything dies", so `B0` is swept first and `r` second.

**At the shipped values, composition and tempo both decide the run.** 3 seeds, `B0 = 28`, `r = 1.06`:

| composition | wave reached | minutes |
|---|---|---|
| `mono:marksman` | 3, 3, 3 | 0.8 |
| `patience` | 4, 4, 5 | 1.0 |
| `greedy` | 4, 5, 5 | 1.3 |
| `mono:breacher` | 9, 9, 9 | 2.8 |
| `breacher+impact` | 5, 10, 53 | 1.6-20.9 |
| `breacher+marksman` | 53, 53, 53 | 20.9 |
| `breacher+operator` | 53, 53, 53 | 20.9 |
| `defensive` | 53, 53, 53 | 20.9 |

**Three of those rows are stale.** Re-measured 2026-09-06 at the same knobs, after the belt change:
`breacher+operator` reads **9, 9, 8** and `breacher+impact` **8, 9, 10**. Only `breacher+marksman` still reaches the
ceiling. See the belt-change note below, which drew the wrong conclusion from re-measuring one row.

Run length passes: competent play lands at 20.9 minutes, inside the 15-25 band. A wall with no damage behind it
(`mono:breacher`) dies at wave 9 and damage with no wall (`mono:marksman`) dies at wave 3, so the mode does demand a
composition rather than a unit.

**Making the belt endless cost `defensive` twelve waves -- and, it turns out, three other compositions too.** The
original note here read "nothing else moved", on the strength of re-measuring `defensive` alone. Re-running the full
slate on 2026-09-06 shows `breacher+operator` 53 -> 9 and `breacher+impact` 53 -> 8 as well: the change knocked out
every strong composition except `breacher+marksman`. `defensive` reads 53, 53, 53 before and **41, 41, 41** after --
19.3 minutes down to 14.1, which drops it just under the band. The far edge used to be a free rally line: once the
camera hit its cap a friendly stopped at `world_width - margin`, so a held line bunched into a wall at a fixed spot a
known distance from
the base. With no edge to stop at, the line keeps walking and spreads out by unit speed, which is the composition
question the mode is supposed to ask. **`B0` and `r` have not been re-tuned for this** -- the reading above is the
record of what the change did, not a tuning pass.

**The not-solved gate MISSES, and the drift schedule cannot fix it.** `breacher+marksman` reaches the budget ceiling
at full integrity on every seed. (As originally written this read "every `breacher` pair with a damage partner",
which the 2026-09-06 re-measurement above corrects: the other two pairs now die in single digits.)

The design's rule is to fix the drift keyframes before touching `r`,
so that is what was tried: `hound` weight 2 -> 3 at wave 18 and 2 -> 4 at wave 35, and the hound rush every 5 waves
instead of every 7 -- aimed at the marksman line, because `hound -> sniper` is the one live role edge in the roster
and `DIVERSITY_AND_BALANCE.md` measures it as the counter to exactly that composition. **It moved the result by
zero waves.** The change was reverted rather than committed on hope; this paragraph is the record of it.

The reason it cannot work is structural. Drift changes *which hostiles arrive*, and a breacher wall is not a
composition-specific answer — it is a durability answer, and durability persists across waves while the hostiles do
not. Every wave the wall survives makes it bigger. No reweighting of the arriving side reaches that, because the
thing being answered is not what arrives but what is left standing when it does.

**And no escalation rate reaches it either, which is arithmetic rather than tuning.** Bounty income is
proportional to `B(n)`, so what the player has standing integrates the budget and settles at `B(n) * r/(r-1)` -- a
ratio bounded below by 1 for any `r`. Steepening the ramp raises both sides of the fight at once. Swept over
`r = 1.06, 1.10, 1.14` (117 runs, 2026-09-06): `breacher+marksman` finishes at **4/4 integrity in all nine runs**,
and the wave it reaches is exactly the ceiling wave at each `r` (53, 33, 24). Raising `r` never shortened that run;
it only moved the finish line in front of it. The measured army-to-wave multiple falls 17.7 -> 11.0 -> 8.1 across
those values, and even an 8x advantage is far past sufficiency.

**What would reach it is attrition on the player's side** -- except that attrition, measured directly, turns out not
to be enough either. See the bounty finding below. A hostile that punishes a static wall specifically -- something
that gains value against a line that has not moved -- remains the answer, and is a roster change rather than a
schedule one.

**The bounty decay is a real lever on spend and a weak one on survival.** Sweeping `d` from 0.985 to 0.94 halves
what a run deploys -- 358 units down to 178 for `greedy` over 55 waves -- and moves the wave reached by nothing at
all, because the line is held by the army that already exists rather than by the one being bought. `d` is therefore
tuned against the economy reading, not against run length.

**Pushed much harder, it still does nothing -- and that result generalises past the economy.** Swept to `d = 0.90`
and `0.80` on 2026-09-06, `mix:breacher+marksman`:

| `d` | waves | integrity | deployments | energy spent | `breacher` spawned / died |
|---|---|---|---|---|---|
| 0.985 | 53 | 4/4 | 324 | 7607 | 163 / **5** |
| 0.90 | 53 | 4/4 | 155 | 3401 | 112 / **89** |
| 0.80 | 53 | 4/4 | 132 | 2801 | 109 / **109** |

Cutting income by 80% halves deployments and kills *everything the player builds* -- at `d = 0.80` every breacher
and every marksman that spawns also dies -- and the base still takes **zero damage across 53 waves**. At the shipped
decay the line is genuinely inert: 8 deaths against 324 deployments.

**So making the player's army die is not sufficient.** At `d = 0.90` the line is churning hard and the base is still
untouched, because the flow of replacements grinds each wave down before anything arrives. Starvation, boons and
banes and attrition generally all make friendlies die, and none of them reach the base. The first reading of this
result was that a hostile should bypass, outrange or outrun the front. **That reading is withdrawn on design
grounds (2026-09-06).** The battle line is the game's core mechanic and units do not pass through it in either
direction; the base is a property of a level rather than of the game, a mode may have none or several, and nothing
may treat it as a destination -- see the battle-line section of [`GDD.md`](../GDD.md). Every endless lever
therefore acts *at the line*: on what arrives there, on how much it takes to kill, and on how large a line the
player can hold. What the measurement above then says is that removing the standing army is not enough on its own;
what has to shrink is the player's ability to *replace* the line faster than the wave arrives at it. `defensive`
does degrade with decay (41 -> 33 waves), so the lever is not inert; it just cannot touch the one composition that
decides the gate.

---

## Re-tuned for the burst/volume roster (2026-09-06)

Everything under "What the sweep found" above was measured on the roster that had one answer. The roster changed
on 2026-09-06 — evasion on `grime`, `hound` and `impact`, armour on `wrecker` and `jackal`, the cap applying to
shots only; see [`DIVERSITY_AND_BALANCE.md`](DIVERSITY_AND_BALANCE.md) — and this section is what the mode does
against it. Two things were re-measured and one was changed.

**The threat costs compressed.** `scons balance` now reads `grime` 1.00, `hound` 2.39, `wrecker` 4.07,
`jackal` 3.37, `mason` 3.57 (was 1 / 4.76 / 6.77 / 7.22 / 8.78). Nothing but the grime got cheaper in absolute terms:
the *unit* got dearer, because its rifle now lands 3 on the reference breacher where it landed 1 and its evasion
caps the breacher's round at 6. Copied into `data/endless.json` as the protocol says.

**The opening wave had to shrink by more than half.** With the old `B0 = 28` priced in the new grime, wave 1 is
nineteen evasive grime and four hounds against a cold 105 energy, and every composition on the slate dies inside
five waves except `mono:operator`, which walks to wave 38. Swept `B0` at 3 seeds, `r = 1.06`, `d = 0.985`, the
shipped drift; the cell is waves reached per seed and the ceiling wave at each `B0` is where the budget cap lands:

| composition | `B0 = 6` (ceiling 71) | `B0 = 9` | **`B0 = 12` (shipped)** | `B0 = 16` |
|---|---|---|---|---|
| `mono:operator` | 55 / 48 / 50 | 39 / 40 / 40 | **39 / 39 / 39** | 36 / 36 / 36 |
| `breacher+operator` | 53 / 48 / 54 | 38 / 38 / 38 | 36 / 36 / 36 | 33 / 29 / 32 |
| `impact+operator` | 57 / 56 / 57 | 40 / 40 / 44 | 36 / 38 / 4 | 3 / 3 / 3 |
| `breacher+impact` | 57 / 58 / 57 | 45 / 45 / 43 | 5 / 42 / 4 | 4 / 5 / 4 |
| `mono:breacher` | 43 / 43 / 44 | 35 / 35 / 34 | 8 / 8 / 8 | 5 / 5 / 5 |
| `mono:impact` | 60 / 60 / 60 | 41 / 6 / 48 | 3 / 4 / 3 | 3 / 3 / 3 |
| `marksman+operator` | **71 / 71 / 71 at 4/4** | 70 / 4 / 71 | 3 / 5 / 3 | 3 / 3 / 3 |
| `breacher+marksman` | **71 / 71 / 71 at 4/4** | 5 / 6 / 8 | 4 / 5 / 4 | 4 / 3 / 4 |
| `impact+marksman` | **71 / 71 / 71 at 4/4** | 3 / 3 / 3 | 3 / 3 / 3 | 3 / 3 / 3 |
| `mono:marksman` | 4 / 4 / 4 | 3 / 3 / 3 | 2 / 3 / 2 | 2 / 2 / 2 |
| `defensive` | 54 / 54 / 54 | 44 / 44 / 45 | 36 / 41 / 42 | 3 / 2 / 3 |

Three readings, in the order the protocol asks for them:

1. **Run length.** At 12 the best fixed compositions reach the high thirties, which is about 14 minutes — just
   under the 15-25 band for a *fixed* line, and a fixed line is the floor: see the third reading.
2. **Not solved.** At 12 **no composition on the slate reaches wave 40 on any seed**, and the ceiling is never
   touched. At 9 `impact+operator` clears 40 on every seed and `marksman+operator` reaches the ceiling at full
   integrity on two of three; at 6 every marksman-bearing pair reaches it — the opening is small enough that the
   sniper line is standing before the swarm is big, which is the old wall reappearing. The gate is a knife-edge on
   the number (`mono:operator` at 39 / 39 / 39), so read the shape rather than the threshold: **every fixed line
   dies, and they die at two different places.** Marksman-bearing lines die at waves 3-5 to the evasive opening;
   operator-bearing lines die at 36-39, where the drift has brought the armoured share up and the mason set piece
   lands. The `outlasts the field` reading reports 8.5x for that reason, and it is the wrong reading here: the
   "field" it divides by is the marksman lines dying at wave 3, which is the roster working.
3. **Economy.** `defensive` −0.38 and `patience` −0.26 energy per wave, `greedy` flat; the snowball is off.

**What this means, and what it does not.** The slate is fixed compositions, and a fixed composition is exactly what
this roster is built to punish: the light half of the roster answers the first eighteen waves and the heavy half is
needed after. A player who *transitions* — operators and breachers into the swarm, marksmen and impacts as wreckers
and jackals arrive — is not on the slate, and would outlast every row of the table. That is the composition
decision recurring inside one match, which is what the mode exists for; the sweep cannot see it because no policy
changes its mind. The gate therefore passes for the reason the design wanted, and the mode's real ceiling is
unmeasured until the slate carries a transitioning policy.

**Not changed, and why.** The drift keyframes are where they were. The evasive opening is what kills a sniper-first
line and the armoured tail is what kills an operator-only one, and both of those are the roster's questions being
asked in order; pulling the heavy share forward would shorten the operator-only phase, which is the obvious next
knob, but the protocol asks for a sweep in the same commit and the `B0` sweep was the one that decided runs. The
army-to-wave multiple remains the mode's standing limitation for any composition the waves do not specifically
punish, exactly as recorded above.

**Next.** A `transition` policy for the slate (a mix whose weights are a function of the wave), so the gate can
read the thing the mode is actually about; then the drift, swept against it.

---

## Escalation that does not cost bodies

Spending a larger budget buys more hostiles, and that runs into a wall that is not a design question. Driving the
army-to-wave multiple down to 1 by wave size alone needs `B(n) = B0 * r^((n-1)^a)` with `a` near 2, which is waves of
order 10^21 bodies; at a playable wave size the reachable multiple is about 9, against 17.7 for the plain geometric
ramp. Two knobs exist for this, **both inert by default**, and they are not equally well evidenced:

- **`escalation_curve` (`a`)** bends the budget ramp super-exponentially: `B(n) = B0 * r^((n-1)^a)`, with `a = 1.0`
  reproducing the shipped geometric schedule exactly. The motivation is the `r/(r-1)` bound above, and the body-count
  table is computed rather than simulated. **It has never been swept.** The knob and its unit tests exist; no run
  length, gate reading or economy reading has ever been taken with `a > 1`.

- **`hostile_damage_growth` / `hostile_damage_cap`** scale what each hostile hits for: `damage_scale(n) =
  growth^(n-1)`, clamped. This is unbounded at a constant body count, and it is applied through one shared
  `with_damage_scale` called from both spawn paths -- `SimWorld::spawn` and `UnitFactory::materialize` -- because the
  conformance suite compares them tick for tick. The scale rides on `WaveDefinition` rather than on a scheduler
  setter: a wave's spawns and its wave-changed signal arrive on the same tick, so a setter would scale part of a wave
  and not the rest.

**The damage ramp works mechanically and is not a difficulty curve.** Measured 2026-09-06 on `breacher+marksman`:

| growth | waves | integrity | scale at wave 5 | scale from wave 16 |
|---|---|---|---|---|
| 1.0 | 53 | 4/4 | 1.00 | 1.00 |
| 1.15 | 53 | 4/4 | 1.75 | **8.00** |
| 1.30 | **5** | **0/4** | 2.86 | 8.00 |

Eight times hostile damage for thirty-seven consecutive waves changes nothing; 2.86x at wave 5 ends the run. It is a
third opening-wave lever alongside `B0` and `r`, not a late-game one. Probing between them, `1.22` and `1.26` kill
that composition at wave 5-7 while `1.18` is bimodal across seeds -- dead at 6 on one, untouched at 53 on the other.
**There is no value that produces a middling run**, so nothing here is shipped set.

---

## The mode became losable (2026-09-06)

Everything above is the record of trying to overrun a competent player by changing what *arrives* -- the budget
ramp, the escalation rate, the bounty decay, a uniform damage ramp -- and finding that none of them reach. This
section is the change that does, and the measurement that finally saw the problem.

**The slate could not see the mode's real failure until it carried a transitioning player.** Every policy the sweep
ran fixed its composition at the start, and a fixed composition is what a drifting schedule is built to punish, so
the table above reads as "every fixed line dies" and was taken as the gate passing. It is not the gate passing. With
a `transition` policy on the slate -- operators and breachers into the evasive opening, impacts and marksmen as the
armoured tail arrives, four keyframes stepping at waves 1, 12, 22 and 32 -- the mode reads:

| policy | waves reached | integrity | minutes | peak friendlies |
|---|---|---|---|---|
| `greedy` | 3, 4, 3 | 0/4 | 0.9 | 5 |
| `mix:breacher+marksman` | 4, 5, 4 | 0/4 | 1.1 | 6 |
| `defensive` | 36, 41, 42 | 0/4 | 12.9 | 61 |
| **`transition`** | **68, 68, 68** | **4/4** | **28.5** | **212** |

68 is the budget ceiling. A player who changes composition inside the match runs out of *schedule* at full integrity
with two hundred and twelve friendlies standing. That is the mode's actual state, it was true the whole time, and no
row of any earlier sweep could show it.

**The unbounded quantity was never the wave; it was the line.** Friendlies persist between waves and hostiles do
not, so the standing army integrates the entire difficulty ramp. Bounty income is proportional to `B(n)`, so the
army settles at a fixed multiple of any wave -- which is why the `r/(r-1)` bound holds for every escalation rate and
why bounty decay kills the player's whole line without the base taking a point. Every lever tried before acted on
one side of a ratio whose other side rose with it.

**A supply cap is the rule that bounds it, and it acts at the battle line.** The most friendlies that may stand at
once, as a level property rather than an endless one. Nothing routes around the front, nothing outruns it, nothing
treats a base as a destination -- the fight stays where it was, and what changes is how much line the player is
allowed to hold while the waves keep growing. Swept 3 seeds at the shipped ramp:

| `supply_cap` | `transition` | `defensive` | minutes | peak hostiles |
|---|---|---|---|---|
| none | **68, 68, 68 at 4/4** | 36, 41, 42 | 28.5 | 351 |
| 30 | 53, 54, 56 | 61, 61, 61 | 22.0-24.6 | 1140 |
| **20 (shipped)** | 44, 46, 46 | 51, 52, 52 | 17.5-19.9 | 553 |
| 14 | 32, 37, 41 | 33, 37, 40 | 14.9-15.1 | 221 |
| 10 | 25, 25, 25 | 23, 23, 24 | 8.3-8.6 | 104 |

**Every capped cell ends at 0/4 integrity.** The mode is losable by construction now rather than by tuning, which
is the thing four earlier attempts could not buy at any setting.

**And the cap alone puts the wrong number on screen.** Read the last column: with the line bounded, wave size is
what carries the ramp, and the late game reaches 553 hostiles at cap 20 and 1140 at cap 30. That is the body-count
wall from the section above arriving from the other direction -- the budget has to keep climbing, and bodies were
the only thing it could buy.

**Elites are what the budget buys instead.** A share of each wave arrives with a multiple of its catalog hit points
and a 1.35x sprite, and is *charged its multiple against the wave budget* -- so a wave of elites is a smaller wave
worth the same measured threat. Hit points rather than damage, because hit points multiply every attacker's
time-to-kill by the same factor and leave the roster's counters exactly where `DIVERSITY_AND_BALANCE.md` measured
them, while the damage ramp crosses one-shot thresholds and is knife-edge between inert and instant. Swept at
`supply_cap = 20`, 3 seeds:

| share cap | hp growth | `transition` | `defensive` | peak hostiles | minutes |
|---|---|---|---|---|---|
| 0 (off) | -- | 27, 40, 42 | 44, 44, 44 | 325 | 15.6-16.6 |
| 0.25 | 1.00 | 29, 27, 34 | 38, 44, 45 | 291 | 12.0-16.8 |
| 0.25 | 1.06 | 28, 37, 40 | 45, 46, 46 | 159 | 14.9-17.6 |
| 0.45 | 1.00 | 29, 27, 34 | 41, 44, 45 | 249 | 12.1-16.8 |
| **0.45 (shipped)** | **1.06** | 28, 37, 46 | 49, 46, 53 | **107** | 17.4-20.4 |

Peak hostiles falls from 325 to 107 while the run stays in the 15-25 minute band and every cell still ends at 0/4.
The share cap and the hit-point growth do different jobs and both are needed: the share decides how many bodies the
budget stops buying, and the growth decides how much each promoted body is worth. At growth 1.00 the elites are only
twice a normal body, so the budget barely notices them and the wave hardly shrinks.

**The energy cap is the third rule, and it is aimed at the transitioning player specifically.** A ceiling on the
reserve, engaging the first time the player spends down to it rather than clamping from the first tick -- so the 105
opening grant is theirs until they spend it, and from then on 100 is a hard ceiling on regeneration and bounty
alike. Swept at the shipped caps and elite ramp, 3 seeds:

| `energy_cap` | `transition` | `defensive` | `greedy` | transition spend | transition deployments |
|---|---|---|---|---|---|
| none | 49, 54, 54 | 51, 53, 54 | 3, 4, 3 | 2784 | 123 |
| **100 (shipped)** | 28, 37, 46 | 46, 49, 53 | 3, 4, 3 | 1962 | 88 |

It costs the transitioning line 30% of its spend and a third of its deployments, and leaves `defensive` almost
untouched. That asymmetry is the point rather than a side effect: banking across waves is exactly what a player
switching composition does, and a reserve that can only ever be refilled to the same number turns every kill into
"spend it or lose it". It is also where `transition`'s seed spread comes from -- 49-54 becomes 28-46 -- because
whether a keyframe switch lands with a full purse now depends on what died just before it.

**What the not-solved gate now means.** It was written to ask whether any composition survives forever, and the
answer is now structurally no rather than measured no: an unbounded run against a bounded line ends. The remaining
question is pacing -- whether the loss arrives inside the 15-25 minute band and reads as being overrun rather than
as a switch being thrown -- so read the run-length table first and treat the target wave as derived from it.

**What is not yet tuned.** `transition` spreads 28 to 46 waves across three seeds at the shipped values, which is a
wider band than any fixed composition shows. That is the schedule's set pieces landing at different points in a
transitioning player's keyframes, and it is a pacing question rather than a correctness one. `B0` and `r` have not
been re-swept against the capped line at all -- everything above holds them at the values tuned for an unbounded
one.

---

## The dead zone, and why a flat cap caused it (2026-09-07)

The supply cap made the mode losable. It did not make it *interesting*, and the difference is the whole of this
section. Played to wave 27, the run reads: the opening is a fight, the line fills at wave 6-7, and then roughly
twenty waves pass in which the hostiles evaporate on contact. The run still ends at zero integrity in the right
number of minutes, so **every reading the sweep took said it was fine**.

**No metric here could see it, and that is the first thing that had to change.** Run length, integrity, the
economy trend and the not-solved gate are all end-of-run readings; a run can end correctly and be twenty waves of
nothing happening. `friendly_deaths_at_wave` -- friendlies lost during each wave -- is the reading that sees it,
with `first_capped_wave` to date the moment the line stopped changing. Rendered as one character per wave, `.` for
a wave in which the player lost nobody, the shipped mode looked like this:

```
supply 20 flat   ...1.......1.2.###11.11342#1.#2.1.3##211111111
                     ^^^^^^^ seven consecutive waves with no losses at all
```

**Three plausible fixes were measured and all three do nothing.** Each was tried because it sounds like it should
work, and the traces are the argument that it does not.

| lever | swept over | longest quiet stretch |
|---|---|---|
| `escalation_curve` | 1.0, 1.15, 1.20, 1.25 | 6, 6, 6, 6 |
| `interval_growth` | 1.008, 0.99, 0.97, 0.95 | 6, 6, 6, 6 |
| elite share and start | to 55% from wave 6 | unchanged |

`escalation_curve` cannot reach it *by construction*: it raises `(n-1)` to a power, so its divergence is late by
design -- the knob's own comment says the divergence has to sit late -- and the dead zone is waves 6 to 12. Wave
spacing compresses the run in wall-clock time and changes nothing about what happens in it: at `0.95` the same
run finishes in a quarter of the time with an identical loss trace. Elites are charged their hit-point multiple
against the wave budget, so they are threat-neutral by construction: they change what a wave is made of, never
what it is worth. **A lever that is neutral by design cannot be a difficulty lever, however it is tuned.**

**The dead zone was never in the schedule.** It is the ratio of wave size to line size, and the line is what moves:
the player goes from about three units at wave 3 to a full twenty at wave 6, quadrupling their power in three waves,
while the budget grows six percent per wave. It does not reach twice its wave-5 value until wave 17. Nothing done to
the hostile side closes a gap that size, and the arithmetic says how much would be needed -- pressuring twenty units
at wave 6 takes roughly 70 threat against an opening of 12, and an opening anywhere near that kills every
composition in three waves (`B0 = 16` at a cap of 8: `defensive` dies at waves 2-4).

**Swept flat, the cap forces a choice between the two halves of a good run.**

| flat `supply_cap` | `transition` waves | minutes | longest quiet stretch |
|---|---|---|---|
| 8 | 22, 23, 22 | 7.8 | **2** |
| 12 | 25, 25, 27 | 9.3 | 7 |
| 16 | 26, 28, 30 | 10.7 | 7 |
| 20 (was shipped) | 28, 37, 46 | 17.4 | 7 |

Eight is the only value with no dead stretch and it ends the run in eight minutes, under the band. Everything long
enough restores the dead stretch. Lowering the escalation to buy back the length restores it too, harder: at `8` and
`r = 1.03` the run reaches 24.7 minutes and the quiet stretch is back to 7.

**A growing allowance answers both, because it holds the ratio still.** `supply_start` and `supply_growth` make the
line the player may hold a function of the wave, bounded above by the level's own `supply_cap`. The allowance widens
at roughly the rate the budget does, so the quantity that decides whether anything dies stops collapsing the moment
a fixed allowance is filled. Zero growth reproduces the flat cap exactly, so authored levels are untouched. Swept at
5 seeds, `transition`:

| `supply_start` | `supply_growth` | waves | minutes | longest quiet |
|---|---|---|---|---|
| 6 | 0.35 | 24-40 | 14.9 | 4 |
| 6 | 0.40 | 36-41 | 15.3 | 5 |
| 7 | 0.35 | 35-42 | 15.7 | 5 |
| **7** | **0.40 (shipped)** | **40-44** | **16.6** | **4** |
| 8 | 0.35 | 36-43 | 16.1 | 7 |
| 8 | 0.40 | 42-45 | 16.8 | 5 |

Below 7 the opening turns on the seed -- `defensive` reads 7, 4, 8, 8, 39 at a start of 6 -- and at 8 the dead zone
is already back, which dates the failure precisely: **a starting line of eight is the point at which the opening
waves stop being a fight.** At the shipped pair the run is 40-44 waves across every seed, inside the 15-25 band, and
the trace is the point of the whole exercise:

```
was   supply 20 flat  ...1.......1.2.###11.11342#1.#2.1.3##211111111
now   start 7, +0.4   ...1..1...11.1113#..2..11.124#33#111111111
```

**What this costs, and what it does not.** The player's line is smaller for most of the run and reaches 24 rather
than 20 by the end, so the late-game army is bigger than before and the early one much smaller. The elite ramp was
brought forward to wave 6 and up to a 55% share in the same commit: it does not close the dead zone and was never
going to, but it keeps the body count readable now that waves are being cleared under pressure rather than
instantly. Wave spacing was left alone -- it costs run length and buys no measured pressure, and the run is already
at the bottom of the band.

**The standing warning.** `quiet%` -- the share of post-cap waves with no losses -- sits at 32-36% both before and
after, while the longest *consecutive* quiet stretch fell from 7 to 4. Read the consecutive figure. A third of waves
being free is fine and always was; seven in a row is the run going to sleep, and only one of those two numbers
noticed.

---

## Tougher hostiles, more masons, and what elite hit points really cost (2026-09-07)

Three changes asked for after a play session, and one of them turned out not to be the knob it looks like.

**The elite price is an under-estimate, and it grows with the multiple.** `elite_cost_multiplier` charges an elite
its hit-point multiple, on the theory that hit points are what a body costs to kill. A body that lives 2.5 times as
long also *shoots* for 2.5 times as long, and the price counts none of that. The comment here used to say the
approximation errs "slightly" toward the harder side. Measured: raising `elite_hp` from 2.0 to 3.0 -- which the
pricing calls a wash, since the wave simply buys proportionally fewer bodies -- cut a transitioning run from **40-44
waves to 18-25** at an unchanged escalation. **`elite_hp` is a difficulty knob. Re-sweep `escalation` in the same
commit, every time.**

**So the rate came down with the roster going up.** Swept against the tougher roster:

| `escalation` | `transition` waves | minutes | reading |
|---|---|---|---|
| 1.02-1.04 | 71, 71, 71 | 30.0 | hits the wall clock at 4/4 -- the hostiles cannot win |
| 1.048 | 19-56 | 22.2 | in band on the median, wild across seeds |
| **1.055 (shipped)** | **29-44** | **12.6-16.7** | overrun on every seed |
| 1.06 | 18-40 | 14.8 | median falls to 26 |

One hundredth on the rate moves a run by more than ten minutes here, which is much tighter than the same sweep was
before the roster changed -- concentrating a wave's threat into fewer, harder bodies narrows the window between
"the line holds forever" and "the line folds fast".

**The elite multiple is 2.5 rather than 3.0 for variance, not for difficulty.** At 3.0 the run landed anywhere from
wave 22 to 52 on identical settings, a 2.4x spread, because an all-mason set piece carrying 3x elites can take a
supply-capped line apart in a single wave. At 2.5 the same slate reads 29, 31, 35, 36, 44 -- a 1.5x spread. Both are
"tougher"; only one of them is a difficulty curve rather than a lottery. Some spread is wanted in an endless mode,
and this is the reading to watch if the mason share rises again.

**Masons arrive earlier and heavier**, since splash is the roster's question to a line parked in one lane and a
supply-capped line is packed by construction: into the wave-8 keyframe at weight 1, tripled to 3 at wave 18, 5 at
wave 35, and the mason set piece every 9 waves instead of 13.

---

## Both toughness knobs have a peak, and the shipped values sit on it (2026-09-07)

Asked for maximum toughness with short runs explicitly accepted. The finding is that "more" was not available from
the levers it looks like it should come from: **`elite_hp` and `escalation` each have a difficulty maximum, and past
it more is less.**

| lever | swept | `transition` waves | direction |
|---|---|---|---|
| `elite_hp` | 3, 4, 6, 8 | 23-42, 38-43, 42-44, 42-44 | **easier** above 3 |
| `escalation` | 1.06, 1.07, 1.08 | 18-25, 24-26, 26-27 | **easier** above 1.06 |
| `elite_fraction_cap` | 0.55, 0.80 | identical | inert |

**Why `elite_hp` turns around.** A hostile threatens the line in two ways: how long it takes to kill, and how much
damage it deals meanwhile. `elite_cost_multiplier` prices only the first. Multiplying hit points scales durability
linearly and leaves per-body damage flat, while the wave shrinks in proportion -- so total incoming damage per wave
*falls*. At 3x the two effects roughly cancel; above it the wave is weaker. 6 and 8 are byte-identical because
`elite_hp_cap` truncates at 8.

**Why `escalation` turns around.** A bigger wave pays more bounty than it costs. Income is proportional to what
dies, so past a point a faster ramp funds replacements faster than the extra hostiles kill.

**Why the share cap is inert.** `elite_fraction_growth` is 0.03 a wave from wave 6, so a 27-wave run reaches about
0.57 and never touches a cap of 0.55, let alone 0.80. **Raise the growth, not the cap.**

**What did work: starving the economy.** `bounty_decay` 0.985 -> 0.88, swept at a fixed line of 7:

| `bounty_decay` | `transition` waves | minutes | deployments |
|---|---|---|---|
| 0.985 | 23, 26, 42 | 15.6 | 63 |
| 0.94 | 23, 26, 33 | 11.9 | 45 |
| **0.88 (shipped)** | 18, 21, 23, 27, 28 | 9.6 | 36 |

**This reverses an earlier finding in this document, and the reversal is the point.** The bounty sweep above --
"a real lever on spend and a weak one on survival" -- was measured when the standing army was unbounded, and an
unbounded army holds the line whatever the income is. With `supply_cap` in place the player's only recovery is
buying replacements, so income *is* the line. **A measurement taken before a structural change is evidence about
the old structure, not the new one.**

**A `supply_start` of 5 was rejected.** It reads 17-25 waves, barely harder than 7, and kills `defensive` at wave 3
on every seed -- the opening stops being a fight and becomes a coin toss.

**What this costs, stated plainly.** The longest quiet stretch goes back up to 6-7 waves, from 4. Under a starved
economy the player fields far fewer units, so there are fewer bodies to lose and `friendly_deaths_at_wave` reads
quiet -- **but quiet here is not idle.** The metric cannot tell "nothing is threatening me" from "I cannot afford to
answer", and under starvation it is usually the second. That is a real limitation of the instrument at this end of
the tuning range, and a run trace should be read alongside `deployments_total` before concluding the mode has gone
back to sleep.

---

## Closer waves and more bodies (2026-09-07)

Two asks, and for each the obvious lever was the wrong one.

**Tightening the interval from the first wave makes waves *less* dense, not more.** Swept at a fixed budget:

| spacing | `transition` waves | minutes | peak hostiles | gap at wave 20 | `defensive` |
|---|---|---|---|---|---|
| 13s, -1%/wave | 17-22 | 4.3 | 27 | 10.6s | dies wave 4 |
| 16s, -1%/wave | 22-28 | 6.4 | 29 | 13.2s | dies wave 3-7 |
| 19s, -1.5%/wave | 23-32 | 8.1 | 41 | 14.1s | 4-32 |
| **19s, -2.5%/wave (shipped)** | 20-31 | 6.9 | **45** | **11.8s** | 7-33 |

A wave arriving sooner is not more bodies on the belt; each wave is still the same size, and the run ends before it
reaches the waves where the budget is large enough to be crowded. **The gentler opening that tapers hard beats the
uniformly tight one on every reading** -- more hostiles on screen, closer late waves, and an opening that survives.

**Body count is set by the drift, not the budget.** The composition drifts toward expensive units, so the cost of a
body climbs as the run goes: 1.23 threat per body at wave 1 against 3.13 by wave 35. A growing budget was buying
steadily fewer hostiles. Raising the `grime` and `hound` weights across the keyframes brings the wave-35 figure to
2.51 -- about **25% more bodies for the same measured threat** -- with the mason weights untouched, so the splash
pressure the drift was changed for in the first place is unaffected.

**`base_budget` stays at 12.** At 16 the extra bodies arrive and immediately end the run: `defensive` at wave 3 on
every seed and `transition` by wave 8.

---

## Spacing is spent only while there is a fight (2026-09-08)

The interval is a difficulty knob for a player who is still fighting: 19 seconds decides how much of one wave is
still alive when the next one opens. For a player who has already cleared it, it is dead air -- and dead air is the
one thing a wave-based mode has no way to make interesting. Both readings are true of the same number, which is why
tuning could never satisfy them at once: every spacing that makes a contested wave tense makes a cleared one a wait.

`cleared_field_grace` separates them. Once nothing hostile is standing, the gap to the next arrival is closed to
**1.5 seconds** instead of run out in full. It closes only gaps already longer than itself, so `spawn_stagger` at
0.8s is untouched and a wave still walks in as a group rather than as a drip feed; and it is gated on the first wave
having opened, so the authored `first_wave_delay` is not itself read as dead air.

**It moves the schedule clock, not the wall clock.** `EndlessDirector` now keeps the two apart: `elapsed_seconds` is
what the player has actually played and is what `wall_clock_ceiling` reads, and `schedule_seconds` is that plus every
gap closed, and is what wave start times, `top_up` and the budget ceiling read. So a run reaches the same wave
against the same budget, sooner. That is also what makes it a trade rather than a free skip: the skipped seconds are
energy regeneration the player does not collect, so clearing fast buys tempo at the price of income.

**Measured across the shipped sweep, 70 paired runs, the rule off against on:**

| | rule off | rule on |
|---|---|---|
| mean waves reached | 10.6 | 10.1 |
| deepest wave | 33 | 33 |
| mean seconds per wave | 16.0 | 15.9 |
| runs identical to the baseline | -- | 51 of 70 |

**Most of the sweep does not move at all, and that is the finding rather than a null result.** The sim policies are
not fast players: they leave hostiles standing almost continuously, so there is no dead air to close and the rule
never fires. It fires in the long `mono:operator` and `mix:breacher+operator` runs, where seconds per wave falls from
around 13.8 to 12.9 -- and where the wave count moves in both directions, by up to twelve either way, which is the
income trade landing on top of the ordinary seed variance in exactly the runs that already had the widest spread.
The not-solved gate is unaffected: nothing reaches wave 40 with the rule on either.

**Manually verified in play, 2026-09-08, and that is the evidence that counts here.** The rule reads as intended at
the shipped 1.5s: a cleared wave rolls straight into the next one and the dead stretch it was written to remove is
gone, with no sense that a wave is being rushed onto a field the player has not finished with.

This is deliberately the primary evidence rather than a supplement to the table above, because **the sweep is
structurally unable to test this rule.** It only fires when the field is clear, and the sim policies leave hostiles
standing almost continuously -- 51 of 70 runs are byte-identical with it on. The measurement's real job was to show
that the rule does not distort the runs it does touch, and it does that; whether closing the gap *feels* right is a
question about a fast player, and no policy in the slate plays like one. A future policy that clears waves quickly
would let the sweep say more.

The knob is inert at `0.0`, which is what a schedule under measurement wants -- the baseline column above is that
value.

---

## The income curve, swept (2026-09-08)

`bounty_decay_curve` and `bounty_floor` were pulled out of `endless_wave_generator.cpp`, where the shape was fixed
at geometric and the floor was a `constexpr` of 0.05. Asked for a slight relief on an economy that felt too harsh.
**Shipping `bounty_decay_curve` 1.0 -> 0.85 and leaving `bounty_floor` at 0.05.** `transition`, 12 seeds:

| `k` | median wave | deployments | energy spent |
|---|---|---|---|
| **1.0** (was) | 24 | 28.7 | 595 |
| **0.85 (shipped)** | 24 | 30.5 | 633 |

+6.4% on both economy readings with run length unchanged. That is the shape of relief that was wanted: the player
affords more replacements, and the mode does not get easier to *survive*. The full slate at 5 seeds keeps the
not-solved gate passing, leaves the economy slope negative, and does not move the 6.25x best-mix spread.

**Read `deployments_total` and `energy_spent` here, not `waves_reached`.** Both knobs move what the player can
afford and neither moves how long they last, and run length at 5 seeds is mostly seed noise -- the first sweep of
this pair read medians of 26/27/26 across the shape axis and concluded, wrongly, that nothing was happening.

**Bounty is quantized, and it dominates both knobs.** A kill pays `ceil(base_bounty * multiplier)` -- integer
energy, minimum 1 -- and hostile bounties are 4 to 7. So the curve the player experiences is a staircase with seven
steps, and below a multiplier of 0.25 every hostile in the game pays exactly 1:

| multiplier | grime (4) | hound (5) | mason/jackal (6) | wrecker (7) |
|---|---|---|---|---|
| 0.50 | 2 | 3 | 3 | 4 |
| 0.30 | 2 | 2 | 2 | 3 |
| 0.25 | **1** | 2 | 2 | 2 |
| 0.15 | 1 | 1 | 1 | 2 |
| 0.10 and below | 1 | 1 | 1 | 1 |

`4 x 0.25 = 1.0` exactly, so a floor of 0.25 does nothing whatever for grime, which is 41% of the drift. **A floor
below 0.26 cannot change what the commonest body pays.** The first sweep of the floor used 0.05/0.15/0.25 and read
as flat for precisely this reason; it is an artefact of the instrument, not a finding about the knob.

**Why the floor is not shipped, though it works.** Swept at values that clear the step: 0.30 (binds from wave 11)
gives +5.7% deployments and 0.40 gives +9.5%, against `k = 0.85`'s +6.4%. They are *substitutes* -- combining
`k = 0.85` with a floor of 0.30 reads identically to `k = 0.85` alone, because both are lifting the same stretch of
the run. `k` was preferred because a floor props income at a constant forever: 0.30 is six times the current floor,
and if run length is ever retuned longer that constant becomes a snowball risk, where a shape keeps decaying at
every wave. Reach for the floor only if runs lengthen.

**Where the harshness actually is.** The energy trace says it plainly. Mean energy held at each wave open,
`transition`, shipped tuning: waves 4-10 sit at 95-100 against an `energy_cap` of 100, then wave 11 collapses to 49
and the run spends waves 14-26 under 20 energy, when a deployable costs 20-27.

**So relief before wave 11 is thrown away** -- it overflows the cap -- and the mid-game is `supply_cap`-bound rather
than income-bound anyway (`deployments_blocked` counts only supply refusals, and reads ~10,000). Only waves 11+ are
genuinely income-starved. Any future income lever should be judged on what it does after wave 11 and nowhere else.

**Run length is 6 minutes, against the 15-25 the design asks for.** Every cell in every sweep here reads `SHORT`,
including the shipped one, so this is not a regression from these knobs -- but the band in this document predates
both `interval_growth` of 0.975, which shrinks the spacing as the run goes on, and `cleared_field_grace`, which
closes the gaps a fast player would otherwise wait out. Left alone deliberately: run length is a different decision
from income relief and wants its own sweep, and the band itself may be what is wrong rather than the tuning.

## Where the mode lives

Two entry points, both inside the campaign flow, and deliberately none in the main menu: endless is the campaign's
terminus, and a game-menu entry would present it as a parallel mode a new player can wander into before it means
anything.

### The campaign map — a header button, not a sixth mission

There is no free ground east of `level_05` at (0.66, 0.77): the campaign already ends at the map's usable right
edge, with the whole right quarter under the operation dossier (x 1408..1872 of 1920). Rather than squeeze a
beacon into a free pocket of the map art, endless gets its own control on the header bar that already carries the
map's title and the "N / 5 SECURED" count:

- **A themed button, centred on the header.** The breadcrumb and the secured count carry equal expand weight, so
  the button lands on the header's centre line whatever either label's width happens to be. It wears the wave mark
  (`icons.wave`) followed by the label "Endless Mode" — a mode switch, not a mission preview card.
- **Hidden until `level_05` is completed.** A disabled button would spoil nothing useful and would clutter a header
  that only needs to say something once the mode exists.
- **Pressed, it deploys straight into the run.** There is no dossier stop first: a mission choice has siblings
  worth comparing, but the button already answers the only question endless has, so it calls `endless_action_`
  directly rather than opening a panel to confirm what pressing it already meant.
- **No new art.** The button reuses the existing `wave` icon and button theme — endless is fought on `level_05`'s
  ground, so the run itself is the narrative reuse, not a shortcut taken here.

**One trap, avoided.** The header's "N / 5 SECURED" reads `missions.size()`. Endless is therefore
`std::optional<CampaignEndlessViewModel> endless` on the view model and **not** an entry in `missions`;
`test_campaign_map_presenter.cpp` pins that it is not counted and produces no mission route.

### The score screen after level 5

The run that first clears `level_05` announces the unlock in the existing `new_unlocks` list, and the footer gains an
**Endless** button that launches a run directly.

The two are gated differently on purpose: the announcement is **edge-triggered** (only on the run that actually
unlocks it, so replaying level 5 does not re-announce), while the button shows **whenever endless is available**, so
a later replay can still jump straight in. After level 5 there is no next level, so the footer reads
`[Endless] [Retry] [Campaign]` and Endless lands in the slot the eye already expects the forward action to occupy.

A finished run reads `RUN OVER` rather than `DEFEAT` — a run that was never going to be completed cannot be lost —
and shows the wave reached, the survival bonus, the run score and the best run, with `BEST` against either half that
moved. Its footer is `[Retry] [Campaign]`, where Retry is the same call the Endless button makes.

---

## Scoring

The existing kill and integrity scoring plus a per-wave survival bonus, so the wave number is legible in the score
rather than living on a separate axis. No completion bonus — there is no completion. The profile records best wave
and best score per threat level (`std::map<int, int>` each, so an ascension ladder needs no second save migration);
there is one threat level today.

An endless run completes under the level id `endless`, which names no content. `complete_level` on an id that is in
no unlock table adds the run's score to the career total and produces neither an unlock nor a reward draft, which is
exactly what an endless run should do.

---

## Tuning protocol

The constants in `data/endless.json` are measurements, not preferences, and they go stale the same way the balance
tables do. When unit stats change:

1. `scons balance` → refresh `threat_costs` from the `threat` column.
2. `scons endless` → re-check run length, the not-solved gate and the economy trend.
3. If the not-solved gate fails, **fix the drift keyframes before touching `r`**. A faster ramp shortens runs; it
   does not make a solved mode unsolved -- and against `breacher+marksman` it does not even do that, so treat `r` as
   a pacing knob only.
4. `escalation_curve`, `bounty_floor`, `hostile_damage_growth` and `hostile_damage_cap` all ship at the values the
   code used to hard-code, so all four are inert until moved. Do not set any of them in `data/endless.json` without
   a sweep in the same commit: `escalation_curve` has never been measured at all, and `hostile_damage_growth` is
   knife-edge between "no effect" and "dead by wave 6". `bounty_decay_curve` is swept and ships at 0.85.
5. **Every lever acts at the battle line.** No hostile, elite or schedule rule may route around the front, outrun it,
   or target the base as a destination, and none may assume a base exists at all (`GDD.md`, the battle-line
   section). The diver is the one standing exception and is not a template.
6. **Run the slate with `transition` on it, and read that row first.** A slate of fixed compositions cannot see
   whether the mode has a ceiling -- it measures how long each wrong answer survives. This is how the mode shipped
   unwinnable-for-the-hostiles for as long as it did.
7. **`supply_cap` and the elite ramp are tuned together.** The cap decides whether the run ends; the elite share
   decides whether the wave that ends it is a number of bodies the screen can hold. Moving one without re-reading
   `peak_enemies` on the other trades a losable mode for an unreadable one.
8. **Read `friendly_deaths_at_wave` before any of the end-of-run numbers.** A run that ends at the right wave, in
   the right minutes, at zero integrity can still be twenty waves of nothing happening, and every other reading
   here will call that a pass. Read the longest *consecutive* run of waves with no losses, not the share.
9. **`elite_hp` is a difficulty knob, not a texture knob.** The budget prices an elite at what it costs to kill and
   never at what it deals while dying, so the error grows with the multiple. Re-sweep `escalation` in the same
   commit, and read the seed *spread* as well as the median: concentrating threat into fewer bodies raises variance
   faster than it raises difficulty.
10. **Sweep both directions before concluding a knob is maxed.** `elite_hp` and `escalation` both have a difficulty
    peak, and past it more is less -- an elite shrinks the wave in proportion to its toughness, and a bigger wave
    pays more bounty than it costs. Neither turnaround is visible from a one-sided sweep.
11. **A reading taken before a structural change is evidence about the old structure.** Bounty decay was measured
    as weak on survival when the army was unbounded; with the line capped it is the strongest lever there is.
    **`d`, `bounty_decay_curve` and `bounty_floor` are three parameters of one curve and substitute for each
    other**, so sweep them together and expect a value that read correct against one to be wrong against another.
12. **"More hostiles" is a drift question before it is a budget question.** Cost per body climbs as the mix drifts
    to heavy units, so a growing budget can buy *fewer* bodies. Read threat-per-body across the keyframes before
    reaching for `base_budget`, which at any useful size ends the run in the opening.
13. **Bounty is integer and rounds up, so read a multiplier against the staircase before sweeping it.** A kill pays
    `ceil(base_bounty * multiplier)` on bounties of 4 to 7, so a multiplier below 0.25 pays 1 for every hostile in
    the game and two "different" income settings can be the same game. Grime is 41% of the drift and `4 x 0.25` is
    exactly 1.0, so the first useful floor is 0.26. A sweep whose cells straddle none of the steps reads as flat and
    means nothing.
14. **Income relief before wave 11 is thrown away.** The trace holds 95-100 energy against a cap of 100 through
    wave 10, and the mid-game is supply-bound rather than income-bound. Judge any economy lever on waves 11+.
15. **The line is a difficulty knob, and usually the decisive one.** When the middle of a run goes quiet, suspect
   `supply_start` and `supply_growth` before touching anything on the hostile side: what decides whether a wave is
   a fight is its size against the line's, and the line is the half that moves in steps.

Treat `data/endless.json` the way `DIVERSITY_AND_BALANCE.md` asks the catalog to be treated: when a sweep disagrees
with the committed numbers, suspect a stat that rode along in an unrelated commit before concluding the file is
stale.

---

## Out of scope, and better as follow-ups

Each of these composes with the generator rather than changing it, which was the reason to land the generator first.

- **Boon/bane drafts between waves.** Still the highest-value addition over the upgrade effect system -- but note
  that the case for it as a *difficulty* fix is now measured and does not hold: the bounty sweep above kills the
  player's entire line without the base taking a point of damage, so player-side attrition alone does not overrun a
  composition that keeps fighting.
- **Threat levels / ascension ladder.** The save format already keys the records by threat level.
- **Seeded daily runs and leaderboards.**
- **Elite and boss hostiles.**
