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

It reproduces exactly — to three decimals and to the bootstrap interval — so **treat this table as a regression test
on the catalog**. When a fresh run disagrees, diff `data/unit_data.json` against the last commit that touched
balance deliberately before concluding the document is stale; twice now the difference has been a unit stat riding
along in a commit about something else.

---

## The roster, and the one axis it is built on

Every mitigation in the game used to be armour, and armour asks for burst. That is the whole reason one unit answered
every question: reach-and-burst was the best answer to every armour value on the board, nothing rewarded rate of
fire, and every counter the roster had was a *gradient* — 4 points of armour, 30px of reach, a 25% aggro bias —
which a larger army simply outnumbers. The endless mode showed the end state: a breacher wall with marksmen behind
it, untouched to the budget ceiling.

The roster now sits on one axis with two ends, both of which were already wired and tested:

| profile | rule | blunts | is answered by |
|---|---|---|---|
| **armoured** (`armour: 4`) | subtract 4 from every hit, floor 1 | a stream of light rounds (6 -> 2) | burst (19 -> 15) |
| **evasive** (`damage_cap: 6`) | no single *shot* lands more than 6 | a heavy round (19 -> 6) | volume (6 -> 6), or a swing in contact |

The cap reads the hit's delivery and armour does not: **a melee swing is never capped.** That one clause is what
lets evasion give the sniper a weakness without deleting the counter-puncher's 30-damage swing, and it gives the
roster its second answer to evasion for free — the breacher in contact beats a hound, the marksman at 8 does not.

Every hostile carries one profile or neither, and every friendly gun sits at one end of the axis:

| unit | profile | gun | job, as a fight | its question |
|---|---|---|---|---|
| `breacher` | armoured | light, 245 | holds the front against light fire | heavy rounds, splash |
| `marksman` | plain | **burst**, 650 | kills armoured and out-ranges everything | `grime`, `hound` |
| `operator` | plain | **volume**, 380 | kills evasive | `wrecker`, `jackal` |
| `impact` | evasive | light, 320, swing 30 | soaks heavy rounds, kills divers in contact | `grime` volume |
| `grime` | evasive | light, 345 | the swarm: a sniper line cannot thin it | operator, breacher |
| `hound` | evasive | swing 20, dives snipers | reaches a marksman through its own approach fire | impact, operator, breacher |
| `wrecker` | armoured | 13, 330 | the bruiser: volume cannot dent it | marksman, impact soaks it |
| `jackal` | armoured | 18, 620 | the sniper: volume cannot dent it, out-ranges all but one | marksman, impact closes |
| `mason` | plain | splash, 400 | punishes a clustered short-range line | marksman, out-ranged |

Read down the last two columns and the decision is visible without a matrix: **light enemies want breachers and
operators; heavy enemies want impacts and marksmen**; a hound wants anything but a marksman; a mason wants a
marksman. A wave with both ends wants both pairs in some proportion, and the proportion is the decision. The old
wall, `breacher+marksman`, is now a mismatched pair — the breacher soaks the fire the marksman cannot answer, and
nobody kills what is firing it.

The numbers are in [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md), 2026-09-06, including the two catalog values the
instruments overruled on the way (wrecker armour 5 floors the operator; an evasive *and* burst impact takes the
marksman's job).

---

## The scoreboard

10 friendly mixes x 15 hostile mixes, 800px separation, 200px friendly spacing, 110px hostile spacing. Bracketed
figures are 95% bootstrap intervals over resampled seeds. The "before" column is the catalog as of the belt-slide
commit, the last one before the roster change, paired on the same 51 seeds.

| Metric | Target | Before | Friendly side | | Hostile side (`--transpose`) | |
|---|---|---|---|---|---|---|
| `SII` | >= 0.50 | 0.640 | **0.788** [0.783, 0.793] | PASS | 0.694 [0.689, 0.700] | PASS |
| Decision regret | 10-30% of budget | 12.5% | **18.3%** [17.6, 19.1] | PASS | — | |
| `Var(R)` matchup | — | 0.0375 | **0.0529** [0.0523, 0.0537] | +41%, resolved | same by construction | |
| `Var(a)` power | — | 0.0211 | 0.0143 | | 0.0281 | |
| Effective rank | >= 2.50 | 5.86 | 5.83 | PASS, but it over-reports | 5.80 | PASS |
| **No dead slot** (noise floor) | zero, reachable both sides | 3 of 10 | **0 of 10** [0, 1] | **PASS** | **12 of 15** [11, 12] | MISS, see below |
| No auto-include | zero | none | none | PASS | none | PASS |
| Composition premium | >= 20% *structural*, two columns, *differing* winners | 3 cols, 2 winners | 2 columns, 2 winners [1, 2] / [1, 2] | PASS, unresolved | — | |
| Distinct best answers | — | 5 of 15 | **8 of 15** | | 3 of 10 | |
| Per-unit usage in argmaxes | every unit | breacher 11, marksman 7, operator 3, impact 1 | impact 8, breacher 7, operator 5, marksman 5 | | | |

**Every friendly-side gate passes, and every paired change is resolved with 100% sign agreement.** Eleven of the
fifteen columns changed hands. Winner by column, which is the only reading that separates "the residual grew" from
"the answer changed":

| column | answer | column | answer | column | answer |
|---|---|---|---|---|---|
| `grime` | `breacher+operator` | `hound` | `impact+operator` | `wrecker` | `marksman+impact` |
| `grime+hound` | `breacher+operator` | `jackal` | `impact` | `wrecker+hound` | `marksman+impact` |
| `grime+mason` | `breacher+operator` | `jackal+hound` | `impact` | `wrecker+jackal` | `impact` |
| `grime+jackal` | `breacher+impact` | `mason` | `breacher+marksman` | `mason+wrecker` | `marksman` |
| `grime+wrecker` | `breacher` | `mason+hound` | `breacher+operator` | `mason+jackal` | `marksman+impact` |

And the two rows that were the whole complaint, as median critical budget:

| | grime | grime+hound | hound | jackal | mason | wrecker | mason+wrecker |
|---|---|---|---|---|---|---|---|
| `marksman`, before | 81 | 109 | 109 | 81 | 55 | 109 | 81 |
| `marksman`, now | **136** | **189** | **244** | 81 | 55 | 109 | 81 |
| `operator`, before | 100 | 81 | 81 | 141 | 100 | 81 | 100 |
| `operator`, now | **61** | **61** | 81 | **200** | 100 | **200** | **241** |

A row that reads 81 against nearly everything is the numerical form of "the answer to everything". Both rows now
span a factor of four, in opposite directions.

**The hostile-side dead count is the one MISS, and it is a property of the lab's columns rather than of the
questions.** The three hardest questions are `hound` (for `marksman`, `breacher+marksman` and `marksman+impact`),
`mason+wrecker` (for every operator- or breacher-bearing line) and `wrecker`. Every grime, mason and jackal column is
now easy, because the lab fields **six bodies** per column whatever they are, and a hard counter makes one heavy
body worth several light ones: six grime are 576hp of evasion that the cheapest friendly clears, six wreckers are
1080hp of armour that only burst can. Priced in threat rather than in bodies — which the endless generator already
does — the same columns would be comparable. Read as content, the reading is the design: the hound is the sniper's
question and the armoured pair is the gunner's, and a level that wants to punish a composition now has a named tool
for each. Read as the gate, it is twelve questions nobody needs to answer, and the fix is in the instrument: give
`scons matrix` a threat-priced hostile column and re-read this row. See open problem 1.

**The clock agrees.** The tempo lab (25 seeds, bisected purse, the same twenty hostiles — grime 8, wrecker 5,
hound 3, jackal 2, mason 2 — under four schedules) is the one instrument with an economy, and it is where the wall
used to be cheapest: `breacher+marksman` answered the rush and the spike, `marksman` alone answered the grind at a
purse of 26. Now:

| engagement | cheapest answer | purse | dearest | ratio |
|---|---|---|---|---|
| `tempo_rush` | `marksman+operator` / `breacher+marksman` | 94 | `marksman` 178 | 1.9x |
| `tempo_spike` | `marksman+operator` | 156 | `impact+operator` 271 | 1.7x |
| `tempo_grind` | `impact` / `impact+operator` / `marksman+operator` | 81 | `breacher` 118 | 1.5x |
| `tempo_escalation` | `operator` | 37 | `marksman` 148 | 4.0x |

The twenty hostiles carry both ends of the axis, so the pair that carries both guns is the cheapest answer to a
schedule that delivers them all at once, and the single-gun sniper line is the dearest answer to every schedule but
one — 1.9x to 4.0x off the best, where it used to *be* the best. Every purse rose (the same twenty hostiles are
simply dearer now: an evasive 7-damage grime is not a 5-damage one that armour floors), which is the level content
drifting, not the roster; see open problem 4.

**Endless** was re-tuned against the roster: the opening budget went 28 -> 12, and at 12 every fixed composition on
the slate dies — sniper-first lines at waves 3-5, operator lines at 36-39 — with nothing reaching wave 40 and
nothing touching the ceiling. The compositions die in two different places because the drift asks the light
question first and the heavy one later, which is the decision recurring inside a match; what survives is a line
that transitions, and no policy on the slate does. The full table and the reasoning are in
[`ENDLESS_MODE.md`](ENDLESS_MODE.md).

---

## What is established

### A counter has to be a breakpoint, and it has to be carried by the unit the dominant answer farms

Everything below this heading was learned on a roster whose every counter was a gradient, and the two facts that
finally moved the board are worth stating ahead of them.

**Gradients are outnumbered; breakpoints are not.** Armour 4 against a 5-damage rifle, 30px of reach over the
jackal, a 3x aggro bias — each made one matchup *cheaper* and none made it *different*, so a large enough army
answered every question with the same units. `damage_cap: 6` on the grime turns a 19-round into a 6-round: no number
of marksmen makes that shot worth more, so the counter holds at any army size, which is the property the endless
wall was exploiting the absence of.

**The carrier decides whether a mechanic reads as a counter or as a nerf.** Plating was measured on three carriers
before it shipped and rejected on each, and each was either a unit the marksman was *meant* to beat (`wrecker`,
`mason`) or a friendly (`operator`). On those carriers the cap blunted the roster's one clean matchup and the gates
said so. On `grime` and `hound` — the two units the marksman farmed for free — the identical mechanic moved eleven
columns. Ask which cell a mechanic changes the *sign* of, not how much it changes the mean.

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
| **`ranged_attack_period`** | **structure that buys nothing** | Column spread larger than the mean shift itself — more structural in shape than any other lever — yet swept 0.85 to 1.38 it never changes which mix answers which column. See below. |
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
> So: `scons matrix` cannot rank a price change against anything time-sensitive. **Validate repricings in the tempo
> lab**, which has the clock, the bounties and the base without borrowing a level's story. See the entry of 2026-08-28 in
> [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md) for the measurement and for why the lab was left alone.

The one exception is small: a *mix* row is not immune, because cheaper `u` buys more `u` for the same energy share and
the realised composition drifts. Measured on `marksman+operator`, the per-column spread of the shift is 0.010 to
0.060 — four to twelve times the mono row's, and still under the floor the gates are read at.

### Rate of fire is the opposite of cost, and buys just as little

`ranged_attack_period` is the most structurally *shaped* lever measured here and one of the least productive, which
makes it the standing counter-example to reading a structural classification as a result. Swept on the `marksman` at
its shipped price, 51 seeds per point, paired against 1.05:

| period | `Var(R)` | `Var(a)` | regret | dead | mono shift | structural sd | sd / shift |
|---|---|---|---|---|---|---|---|
| 0.85 | 0.0374 | 0.0312 | 13.9% | 3 | +0.159 | 0.154 | 97% |
| 0.95 | **0.0390** | 0.0232 | 15.8% | 3 | +0.067 | 0.101 | 151% |
| **1.05 (shipped)** | 0.0367 | 0.0212 | 14.0% | 3 | — | — | — |
| 1.15 | 0.0365 | 0.0205 | 12.4% | 3 | −0.031 | 0.052 | 168% |
| 1.25 | 0.0367 | 0.0205 | 11.7% | 3 | −0.096 | 0.125 | 130% |
| 1.38 | 0.0352 | 0.0211 | 9.9% MISS | 3 | −0.170 | 0.135 | 79% |

**The column-to-column spread is as large as the average move, or larger** — against roughly 2% for `cost`, the pure
level case. By the test at the top of this section it is emphatically structural. And it buys nothing: a 62% swing in
fire rate moves `Var(R)` from −4% to +6%, never changes the dead-slot count, and — the reading that settles it — **at 0.95 all
fifteen columns keep the winner they had at 1.05**, six distinct winners before and after. The residual grew without
one matchup resolving differently.

Push it far enough and the change is real but is the failure mode, not the goal: at 0.85 the blind-best mix itself
becomes `breacher+marksman`, which takes 6 of 15 columns while `breacher` collapses from 3 to 1, and `Var(a)` rises
47%. Concentration, not diversity.

The response is also **saturating, one-sided and quantised**, which is why no single pair of readings characterises
it. Marksman mono budget by column, against 1.05:

| period | grime | mason | wrecker | jackal | hound |
|---|---|---|---|---|---|
| 0.85 | −33% | **+0%** | −26% | −26% | −23% |
| 0.95 | −13% | **+0%** | −12% | −14% | −2% |
| 1.15 | +0% | +0% | +0% | +1% | +0% |
| 1.25 | +0% | +0% | +0% | +6% | +0% |
| 1.38 | +0% | **+49%** | +7% | +16% | +6% |

- **1.05 to 1.25 is a dead zone**: four of five columns move by exactly nothing. The marksman can be slowed 19% and
  the matrix cannot see it.
- **The mason column is saturated on the fast side** and is where all the structure comes from there — the marksman
  already kills a mason before it can answer, so extra rate is worth nothing against it and a great deal elsewhere.
- **1.38 is a breakpoint, not a slope.** Five free approach shots at 19 damage kill an 82hp mason before contact;
  four do not. Damage taken from six masons goes 0 → 60 → 214 → **0** across 1.05/1.15/1.25/1.38: the invariant
  erodes at constant budget, then is bought back by spending 49% more on more marksmen.

> **A lever can be structural in shape and still move no answers.** The rule at the top of this section is
> necessary, not sufficient: the payoff has to differ across columns in *sign or saturation*, not merely in
> magnitude. Rate of fire is worth more against everything, just differently more — so the spread it creates lands
> back in `a[i]` and in the residual's size, never in an argmax. **Check the winner-by-column table before believing
> a `Var(R)` gain**; it costs nothing and it is the only reading that distinguishes the two.

### Archetypes buy diversity; numbers do not

Six levers applied to the existing units moved nothing that survived. One new archetype moved regret from 8.8% to
11.1% and raised `Var(R)` by a third. The reason: every unit in the game was the same archetype — a ranged shooter
that walks forward and stops to trade — and reach totally orders such units, so no arrangement of their numbers can
produce a matchup matrix.

**`hound`, the rusher.** 110 hp, melee 20 per 0.8s at 100px, no gun, speed 120. Sized from the approach arithmetic: it
survives one marksman's approach fire (79 damage against 110 hp), reaches a breacher in 1.0s, and loses to a breacher
in contact. It gave the marksman its first fight, at twice the budget of the one it wins — and note *how*: it takes
only 153 damage and loses 0.33 units. It is not dying, it is ineffective, because it cannot shoot inside 200px.
Minimum range, aggro weight and the composition premium all became live measurements the moment it existed.

Its reach was 128px when it was sized here and was cut to 100 on 2026-09-05 so that a swing lands on the
target rather than in the air in front of it. The `hound` is the only row that moved — threat 5.38 -> 4.76 — because
it is the only unit with no gun, and so the only one that ever enters a melee band on purpose. See [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md).

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

### The burst/volume roster — evasion on `grime`, `hound` and `impact`, armour on `wrecker` and `jackal`

One rule (the cap applies to shots only) and nine catalog lines; the design is at the top of this document and the
full measurement is the 2026-09-06 entry of [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md). What it bought, 51 seeds
paired: `SII` 0.640 -> 0.788, `Var(R)` +41%, regret 12.5% -> 18.3%, friendly dead slots 3 -> 0, distinct answers
5 -> 8, eleven columns changing hands, every paired delta resolved. What it cost: the hostile-side dead count
5 -> 12, for the reason given under the scoreboard, and the composition premium's margin. It supersedes the grime
half of the armour profile below — `grime` is evasive now, not armoured — and closes the operator question.

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

### The armour profile — `breacher` 4, `grime` 4, `jackal` 2, everything else 0 (superseded in part)

> Kept as measured. Since 2026-09-06 `grime` carries `damage_cap: 6` and no armour, `jackal` carries 4 and
> `wrecker` 4; the breacher's 4 is unchanged and is still the reason the front rank survives light fire. The
> reasoning below about *why* a differential profile creates structure still holds; the specific values do not.

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
| `wrecker` armour 5 (2026-09-06) | Floors the operator's round at 1. `operator` alone cannot beat `mason+wrecker` under a 400 budget and the analyzer drops the whole row as unbounded. 4 leaves the round at 2 and every row bounded. |
| `impact` evasive **and** carrying a 16-damage shotgun (2026-09-06) | Better headline numbers than what shipped (`SII` 0.851, `Var(R)` 0.0636, regret 20.2%) and worse on every gate: the marksman becomes the one dead row, because a unit that soaks heavy rounds *and* deals burst answers `wrecker`, `jackal` and `hound` at once. With its light rifle the impact soaks and the marksman kills, and the two have distinct jobs. |
| `grime` armour 5 | `Var(R)` +31% resolved, and the premium goes from a bare PASS to a fully resolved [4, 4] columns / [3, 3] winners — but **both dead-slot counts are unchanged** (the sweep's predicted gain was against a weaker baseline), and it costs `MixPolicy` 10 paired wins in 125, almost all of it `level_02` 22/25 -> 14/25. Level 2 is grime-heavy and `MixPolicy` buys operators, which is what armour punishes. **Becomes a one-line change again the moment level 2 is retuned.** |
| `grime` speed +50% | Raises hostile `SII` to 0.605, the highest ever recorded here. `Var(R)` does not move at any speed, regret is invariant to four decimal places, and the whole gain is `Var(a)` falling 29%. |
| `mason` speed 48 -> 72 | Passes the premium gate outright — while `Var(R)` *falls* 0.0309 -> 0.0254. Both answers got worse and the mono just got worse faster. The gate now splits level from structure and no longer reports this as a PASS. |
| Armour on the `operator` | `Var(R)` falls and the premium drops to zero columns. The operator is a high-rate, low-damage shooter, which is precisely the profile armour punishes: it is armour's victim, and arming it muddles the mechanic's only clean counter-relationship. |
| Armour on the `wrecker` | `Var(a)` quadruples, regret collapses to 0.0%. |
| Hostiles out-ranging friendlies | Largest structural change of any lever measured, but it flattened the matrix and halved regret. |
| `mason` reach 400 -> 600 | Hits breachers harder *and* starts hurting marksmen (0 -> 98 damage), which deletes the one clean answer to it. The shipped ordering is load-bearing: breacher 245 < impact 320 < operator 380 < **mason 400** < marksman 650. |
| `wrecker` plating (`damage_cap`) | Armour's inverse — a per-hit ceiling, so it punishes burst where armour punishes rate. Buys `Var(R)` resolvedly at every setting swept, and pays for it in hostile dead slots at roughly one for one: +0.0026 `Var(R)` costs +3.8 dead, and the one setting costing nothing resolvable buys +1%. Not under-payment — at a setting where the wrecker's own row shift is −0.007 the regression is still +2.4. The mechanic concentrates hostile strength into the 5 wrecker rows, which were already the strong end. **The mechanic was wired and tested but carried by no unit** until 2026-09-06, when it went to `grime`, `hound` and `impact` as the evasive profile — the "re-aim it at `grime`" this row recommended, with the shots-only clause added. See the entry of 2026-08-29 in [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md). |
| `operator` plating (`damage_cap: 14`) | **Measured through both gates; not shipped.** The same mechanic on a friendly carrier, where hostile damage is spread (grime 5, mason 10, wrecker 13, jackal 18, hound melee 20) so a cap is an anti-`jackal`/anti-`hound` stat rather than a single-unit dial. `Var(R)` +0.0017 resolved, both regrets in band, hostile side untouched, and it lands **five times harder on `operator` than on `breacher+operator`** — the row inversion every previous operator lever failed to get. Costs: `marksman+impact` takes the operator's place on the dead list, and the premium's zero margin is spent. And then, once the campaign gate could see an operator at all, **nothing happened there**: ±2 cells of 125 on the two policies that buy one, net −1 over 750 paired runs, with the other four at exactly +0. Improves no gate and softens two. See 2026-08-29 in [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md). |
| `mason` plating (`damage_cap`) | The carrier test for the row above, and it confirms it: cap 12 on the mason costs nothing resolvable in dead slots where the same value on the wrecker costs +3.3, and cap 10 *buys* 2.8 of them — hostile dead 5 of 15 down to **2**, the lowest recorded. But `Var(R)` falls resolvedly at every setting (−7.6% to −23%) and cap 10 takes friendly regret out of band at 9.9%. The marksman is the mason's one clean answer; plating blunts it, so the sharpest matchup in the matrix is what pays. See 2026-08-29 in [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md). |
| `marksman` `ranged_attack_period` 0.85 / 0.95 / 1.15 / 1.25 / 1.38 | **The whole range measured; 1.05 stands.** 0.95 looked like the best result on the board — `Var(R)` 0.0390, higher than anything recorded here, regret 14.0% -> 15.8%, both resolved, neither a ratio — and **all fifteen columns kept the winner they already had**. 1.38 takes regret out of band and lowers `Var(R)`; 0.85 makes `breacher+marksman` the blind-best mix and 6 of 15 columns while `breacher` falls to 1. Dead slots are 3 at every point. See above and the entry of 2026-08-30 in [`EXPERIMENT_LOG.md`](EXPERIMENT_LOG.md). |
| Hostile spacing 110 -> 200px | Hostile regret 14.5% -> 8.8% MISS and the premium falls to 1 column, in exchange for one hostile dead row. The mechanism is the *marksman's* reach, not melee reach: six hostiles at 110px span 550px and are engaged all at once, at 200px they span 1000px and are engaged piecemeal. Open question rather than a rejected change — see below. |

---

## How to judge a change

1. **`scons conformance`** — mandatory on anything touching combat, movement, or frame order.
2. **`scons matrix` before and after, at 51 seeds**, decomposed **both ways**. Read **regret first, then `Var(R)`,
   then `SII` and the premium**: the last two are ratios and both can be moved by making the denominator worse.
   Regret is not immune either — buffing a row that already wins columns inflates it against a blind pick that does
   not contain that row, with nothing else changing.

   Then **diff the winner-by-column table**, which is the only reading that separates "the residual got bigger" from
   "the answer changed". Every number above is a summary and all of them can move while each question keeps the
   answer it had: the marksman rate sweep moved `Var(R)` *and* regret resolvedly, in the right directions, with all
   fifteen winners unchanged. **If no column changed hands, no diversity was bought, whatever the headlines say.**
3. **Always pass `--baseline`** and read the paired block. A delta whose interval spans zero is not a result. Check
   the per-row `level` / `structural` classification before believing a lever did what you wanted — if the rows read
   `level`, the lever is a price however it is spelled.
4. **Gate the dead-slot count on the noise-floor reading, not the strict argmax.** No mix outside every column's noise
   floor; none present in every argmax at constant weight.
5. **The tempo lab, bisected** — `scons sim scenario=res://scenarios/tempo_lab.json seeds=25 bisect=yes`, read
   with `analyze_tempo.py --baseline`. It is the only instrument with a clock, so **anything whose value depends on
   arriving early has to be judged here, a price above all**. Read the critical purse, never a win rate at a fixed
   purse: the latter is a step function and four compositions tie at 100% across every engagement. Nothing here
   runs on a shipped level — levels are narrative, some are written to be lost, and a gate denominated in content
   inherits the content's churn.
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

**1. The hostile-side dead count reads 12 of 15, and the instrument is the reason.** `scons matrix` fields six
bodies per hostile column regardless of what they are. Under gradient counters that was tolerable; under hard
counters a heavy body is worth several light ones, and every grime, mason and jackal column is easy at six. The
endless generator already prices hostiles in measured threat (`data/endless.json`, `threat_costs`), so the fix is a
threat-priced column option in the matrix runner — the same `allocate_budget` over the same costs — and a re-read of
the transposed gate. Until then the hostile side is read from the "distinct hardest questions" line (3: `hound`,
`wrecker`, `mason+wrecker`) and from the winner-by-column table, both of which say the questions are real.

**2. The composition premium passes without margin.** Two structural columns (`grime`, `mason+jackal`), two
winners, interval [1, 2]. The reason is structural rather than a tuning miss: with hard counters a *mono* is often the
right answer to a mono column (`impact` to `jackal`, `operator` to `hound`), and the strongest pairs are strong on
average as well as by matchup, so their premium lands in the level term. The premium is a gate written for a roster
where mixing was the only source of structure; whether it should keep its weight on a roster where units carry
structure on their own is a question for the model, not the catalog.

**3. Hound packs.** Six hounds at 110px is the hardest column for every marksman-bearing line and the hardest for
`impact` too, because a pack that arrives together focuses one body at a time and the impact's swing is one target
per second. The single hound is exactly where it was designed to be (the marksman's question, everyone else's
prey); the pack is a content decision — the endless set piece and the level-1 opening both use it — and it wants a
play test, not a stat.

**4. The level tables are stale, and are now further from the roster than ever.** Grime's rifle is 7 and evasive,
so the level-1 opening no longer has a free answer; the tempo lab's twenty-hostile column got dearer for every
composition by 20-50 energy. Levels are content, and some are written to be lost, so nothing here gates on them —
but their threat totals were computed on a different game. Regenerate the coefficients before touching them.

**5. Endless.** Re-tuned against the new roster in [`ENDLESS_MODE.md`](ENDLESS_MODE.md); what the re-tune found
and what it still cannot reach is recorded there.

**6. Hostile line spacing** (unchanged from before). 110px used to sit inside the melee reach and now sits just
outside it; widening it to match the friendly side is a deliberate piece of work that needs a retune against the new
baseline.

**7. The lab has no clock, so it cannot see tempo — and that is deliberate.** Unchanged. A price is the clearest
case; judge those in the tempo lab.

**8. Divers bypass tanks completely, and nothing stops them.** Unchanged in mechanism. The hound's cap makes the
bypass matter more, because approach fire from a sniper line no longer kills it; the answer is the counter-puncher's
swing, and it is the answer only one hound at a time (problem 3).

**9. Two game-feel changes are unreviewed on screen**, and there is now a third: the evasive profile has no
presentation at all. A 19 landing as 6 is invisible unless the damage number or the flash says so, and a player who
cannot see the cap will read a marksman line failing against grime as a bug. A per-profile hit flash, or a tag on
the deploy card, is the cheapest fix and is presentation work rather than balance.
