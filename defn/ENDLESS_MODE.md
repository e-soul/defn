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
alongside difficulty. The counter is a per-wave bounty decay `d^(n-1)` applied to what a kill *pays*, never to what
it *scores*. Base integrity does not regenerate; the run is strict attrition.

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
| **28 (shipped)** | 4-5 | 9 | 53 | 53 |
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
untouched, because the flow of replacements grinds each wave down before anything arrives. The target for a roster
change is therefore not "make friendlies die" -- starvation, boons and banes and attrition generally all do that,
and none of them reach the base -- but **letting hostiles through a line that is still fighting**: bypassing,
outranging or outrunning the front rather than grinding against it. `defensive` does degrade with decay (41 -> 33
waves), so the lever is not inert; it just cannot touch the one composition that decides the gate.

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
4. `escalation_curve`, `hostile_damage_growth` and `hostile_damage_cap` all default to inert. Do not set any of them
   in `data/endless.json` without a sweep in the same commit: the first has never been measured at all, and the
   second is knife-edge between "no effect" and "dead by wave 6".

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
