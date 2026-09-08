// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ENDLESS_WAVE_GENERATOR_H
#define ENDLESS_WAVE_GENERATOR_H

#include "force_mix.h"
#include "hostile_scaling.h"
#include "level_definition.h"
#include "random_source.h"

#include <span>
#include <vector>

namespace defn {

// The three knobs the endless mode is tuned on, plus the pacing they are spent through. Every number here is a
// measurement rather than a preference -- `r`, `d` and the drift keyframes are swept in the simulator until the
// run-length, not-solved and economy readings land where the design wants them. See `ENDLESS_MODE.md`.
struct EndlessTuning {
    double base_budget = 12.0; // B0
    double escalation = 1.14;  // r
    // The curve the wave index is raised to before the escalation is applied: `B(n) = B0 * r^((n-1)^a)`.
    //
    // `a = 1.0` is the plain geometric ramp and the historical behaviour. It cannot overrun a competent player at
    // any `r`, and that is arithmetic rather than tuning: bounty income is proportional to `B(n)`, so the army the
    // player has standing integrates the budget and settles at `B(n) * r/(r-1)` -- a ratio bounded below by 1
    // whatever `r` is. Sweeping `r` over 1.06, 1.10 and 1.14 moved the strongest composition by zero integrity; it
    // only moved the ceiling it ran out of schedule at.
    //
    // For `a > 1` the step ratio `B(n)/B(n-1)` grows without bound, so the multiple shrinks instead of holding. The
    // convergence is asymptotic and slow -- the step ratio grows like `n^(a-1)` -- so this is a tightening rather
    // than a cliff: at `a = 1.2` the multiple falls from 8.1 to about 3.6 by wave 40 and keeps going. That is the
    // direction the geometric ramp cannot supply at any `r`, but where it becomes enough to overrun a given
    // composition is a measurement, not an argument. Sweep it.
    double escalation_curve = 1.0; // a

    // What a hostile hits for, per wave: `damage_scale(n) = hostile_damage_growth^(n-1)`, capped.
    //
    // This is the half of the escalation that does not cost bodies. Budget growth buys more hostiles and runs into a
    // body-count wall long before it can overrun a competent line -- driving the army-to-wave multiple to 1 by wave
    // size alone needs waves of order 10^21. Damage growth is unbounded at a constant wave size, and it removes the
    // player's standing army rather than adding targets for it, which is the mechanism the mode was missing.
    //
    // Capped because it multiplies rather than adds: uncapped, a long run reaches damage numbers where every
    // friendly dies to one hit and the composition question stops being asked at all.
    double hostile_damage_growth = 1.0; // per wave
    double hostile_damage_cap = 8.0;

    // Elites: a share of each wave arrives with a multiple of its catalog hit points and a larger sprite.
    //
    // This is the escalation that fits a game whose core mechanic is the battle line. Every lever has to act *at*
    // the line -- on what arrives there and on how much work it is to hold -- because nothing may route around it
    // or treat a base as a destination (`GDD.md`). Buying more bodies runs into a body-count wall; buying tougher
    // ones does not, and an elite is priced in the same measured threat as the bodies it replaces, so a wave of
    // elites is a *smaller* wave that costs the same. That is the whole trick: the threat curve keeps climbing
    // while the number of things on screen stays playable.
    //
    // Hit points rather than damage, because hit points multiply every attacker's time-to-kill by the same factor
    // and so leave the roster's counter structure exactly where `DIVERSITY_AND_BALANCE.md` measured it, while the
    // damage ramp crosses one-shot thresholds and is knife-edge between inert and instant.
    //
    // Inert by default: `elite_first_wave = 0` disables elites entirely, which is what every authored level wants.
    int elite_first_wave = 0;
    // Added to the elite share for each wave past the first, then clamped. A share is a fraction of the bodies in
    // the wave, so 0.25 is one body in four.
    double elite_fraction_growth = 0.0;
    double elite_fraction_cap = 0.0;
    // What an elite's hit points are multiplied by: `elite_hp * elite_hp_growth^(n - elite_first_wave)`, clamped to
    // `elite_hp_cap`. Capped for the same reason the damage ramp is: uncapped, a long run reaches a body no line can
    // kill inside a wave interval, and the fight stops being a fight.
    double elite_hp = 1.0;
    double elite_hp_growth = 1.0;
    double elite_hp_cap = 1.0;
    // The sprite multiple that telegraphs an elite. A category marker rather than a readout of the hit-point
    // multiple: the player has to be able to see which bodies are the hard ones before they are in contact, and a
    // size that tracked the multiple exactly would be illegible at both ends of a long run.
    double elite_size = 1.0;

    // How large a line the player may hold, as a function of the wave: `supply_start + supply_growth * (n - 1)`,
    // never above the level's own `supply_cap`.
    //
    // A flat cap forces a choice the mode should not have to make. The dead stretch a player reports as boredom is
    // the wave budget being small against a *full* line, and it is early -- waves 6 to 12 -- so nothing that scales
    // with the wave number reaches it: steepening the curve, tightening the spacing and promoting more elites all
    // leave those waves byte-identical. Measured, the only flat cap with no dead stretch is about 8, and it ends
    // the run in eight minutes; the caps that give a full-length run all restore the dead stretch.
    //
    // Growing the allowance answers both. The line starts small enough that the opening waves are a fight, and it
    // widens roughly as fast as the budget does, so the ratio that decides whether anything dies stays put instead
    // of collapsing the moment the player fills a fixed allowance. Zero growth reproduces the flat cap exactly.
    double supply_start = 0.0;
    double supply_growth = 0.0;
    // What a kill pays, as a function of the wave: `bounty(n) = max(d^((n-1)^k), bounty_floor)`. It scales what a
    // kill *pays* and never what it *scores*.
    //
    // Income compounds with difficulty on its own -- a bounty scales with kills and kills scale with `B(n)` -- so
    // without a counter-pressure here the mode resolves into a runaway defence.
    double bounty_decay = 0.985; // d

    // The curve the wave index is raised to before the decay is applied, exactly as `escalation_curve` is applied to
    // the budget ramp.
    //
    // `k = 1.0` is the plain geometric decay and the historical behaviour; 0.85 ships. `k < 1` spreads the same
    // reduction over a longer run, `k > 1` front-loads it harder. Swept 2026-09-08: 1.0 -> 0.85 buys +6.4%
    // deployments and +6.4% energy spent over 12 seeds with median run length unchanged, which is relief in what
    // the player can afford rather than in how long they survive. `k = 0` is degenerate rather than useful -- it
    // pays `d` flat from wave 1, including wave 1.
    //
    // Waves 1 and 2 are `k`-independent whatever it is set to, because the index is raised to the power before the
    // rate is, and 0 and 1 are both fixed points of that. So `k` is a knob on wave 3 onwards and the opening pair
    // stays exactly where it was tuned; `d` is the only thing that moves them.
    //
    // **Read this against the quantization before sweeping it.** A kill pays `ceil(base_bounty * multiplier)` on
    // bounties of 4 to 7 (`MatchSession::record_enemy_died`), so the curve the player experiences is a staircase
    // and a multiplier below 0.25 pays 1 for every hostile in the game. Two `k` values that never cross a step are
    // the same game. See `ENDLESS_MODE.md`.
    double bounty_decay_curve = 0.85; // k

    // Income decays but never below this. Not the "never a countdown" guarantee its old comment claimed -- that is
    // already supplied by the `ceil` on the award, which pays 1 energy at any positive multiplier -- but a hard
    // stop that props income at a constant from the wave the rate falls through it.
    //
    // Measured inert as shipped, for two arithmetic reasons. `4 * 0.25` is exactly 1.0 and grime is 41% of the
    // drift, so no floor below 0.26 changes what the commonest body pays; and at `k = 1` the 0.05 floor was not
    // reached until wave 25 while runs end at 24-26. Swept at values that do clear the step, it works -- 0.30 binds
    // from wave 11 and buys +5.7% deployments, 0.40 buys +9.5% -- but it substitutes for `k` rather than adding to
    // it, and a constant floor is a snowball risk if run length is ever retuned longer. Raise it only then.
    double bounty_floor = 0.05;
    double first_wave_delay = 3.0; // when wave 1 opens
    double wave_interval = 18.0;
    double interval_growth = 1.01;
    double spawn_stagger = 0.8; // seconds between spawns within a wave

    // How long the player is left looking at an empty belt. Once nothing hostile is standing, the gap to the next
    // arrival is closed to this rather than run out in full.
    //
    // The interval is a difficulty knob for a player who is still fighting -- it decides how much of a wave is
    // still alive when the next one opens. For a player who has already cleared it, it is dead air, and dead air is
    // the one thing a wave-based mode has no way to make interesting. So the spacing is authored for the fight and
    // spent only while there is one.
    //
    // It closes only gaps already longer than itself, so the stagger inside a wave is untouched at any sane value,
    // and it moves the *schedule* clock rather than the wall clock: the run reaches the same wave against the same
    // budget, sooner, having earned only the income of the seconds actually played. Clearing fast is therefore a
    // real trade -- more waves per hour, less regeneration between them -- rather than a free skip.
    //
    // Zero keeps the schedule's own spacing exactly, which is the right answer for a schedule being measured.
    double cleared_field_grace = 0.0;

    double budget_ceiling = 600.0; // hard stop; above this the run is unwinnable by construction
    double wall_clock_ceiling = 3600.0;
    int survival_bonus_per_wave = 25;
};

// A point the drift passes through. Waves between two keyframes interpolate; waves outside the outermost pair hold
// the nearest one.
//
// Weights are **relative unit counts**, not budget shares: `{grime: 6, hound: 2}` means six bodies to two. The
// generator converts to budget share against the threat costs before spending, so a keyframe reads as what it puts
// on the belt rather than as what it costs.
struct ShapeKeyframe {
    int wave = 1;
    MixShape weights;
};

// A wave that overrides the drifted shape whenever `wave % period == offset`. A set piece is the same generator
// called with a degenerate shape, which is why an all-`hound` rush costs no new code and no new content.
struct SetPiece {
    int period = 0;
    int offset = 0;
    MixShape weights;
};

struct EndlessSchedule {
    EndlessTuning tuning;
    std::vector<ShapeKeyframe> drift;
    std::vector<SetPiece> set_pieces;
    std::vector<UnitCost> threat_costs;
};

// Generates endless waves by spending an escalating threat budget along a drifting composition shape.
//
// Composition is a pure function of `(schedule, wave_number)`: the same schedule asked for the same wave twice
// returns the same units. `RandomSource` decides only the order they take the field in and the jitter on their spawn
// times, which mirrors how `SpawnScheduler` already treats an injected source.
class EndlessWaveGenerator {
  public:
    void configure(const EndlessSchedule &schedule);

    [[nodiscard]] const EndlessTuning &tuning() const { return schedule_.tuning; }

    // What wave `wave_number` is worth, in measured threat. Geometric, and unclamped: a caller compares it against
    // `budget_ceiling` to decide the run is over rather than being handed a silently capped wave.
    [[nodiscard]] double budget(int wave_number) const;

    // When wave `wave_number` opens, in seconds from the start of the run.
    [[nodiscard]] double wave_start_time(int wave_number) const;

    // How long wave `wave_number` has the field to itself.
    [[nodiscard]] double wave_interval(int wave_number) const;

    // The authored shape for this wave, in relative unit counts: a set piece if one lands on it, otherwise the
    // interpolated drift.
    [[nodiscard]] MixShape shape(int wave_number) const;

    [[nodiscard]] WaveDefinition generate(int wave_number, RandomSource &random) const;

    // Income decays as difficulty compounds, or the mode resolves into a runaway defence. Never below
    // `bounty_floor`, and never negative.
    [[nodiscard]] double bounty_multiplier(int wave_number) const;

    // What every hostile in this wave hits for, as a multiple of its catalog damage. Never below 1: the ramp only
    // ever makes the run harder.
    [[nodiscard]] double hostile_damage_scale(int wave_number) const;

    // The line the player may hold during this wave. Zero means the schedule has no opinion and the level's own cap
    // stands unchanged.
    [[nodiscard]] int supply_cap(int wave_number) const;

    // The share of this wave's bodies that arrive as elites, in [0, 1].
    [[nodiscard]] double elite_fraction(int wave_number) const;

    // What one elite of this wave is multiplied by. Identity before `elite_first_wave`.
    [[nodiscard]] HostileScale elite_scale(int wave_number) const;

    // What the average body of this wave costs, as a multiple of its threat cost, once the elite share is priced in.
    // The budget is divided by this before it is spent, which is what makes an elite cost the bodies it is worth
    // instead of arriving free on top of a wave that was already paid for.
    //
    // It prices what an elite costs to kill and not what it deals while dying, so it is an *under*-estimate that
    // grows with the multiple. `elite_hp` is a difficulty knob, not a texture knob -- see the implementation.
    [[nodiscard]] double elite_cost_multiplier(int wave_number) const;

  private:
    // Rescales an authored count ratio into the budget ratio `allocate_budget` spends along.
    [[nodiscard]] static MixShape to_budget_shape(const MixShape &counts, std::span<const UnitCost> costs);

    EndlessSchedule schedule_;
};

} // namespace defn

#endif
