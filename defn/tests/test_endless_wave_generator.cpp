// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "endless_wave_generator.h"
#include "random_source.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace defn {

namespace {

// A three-unit roster priced the way `scons balance` prices the shipped one: a baseline, a mid-cost diver, and a
// heavy. Nothing here depends on the shipped numbers, only on their spread.
std::vector<UnitCost> make_threat_costs() {
    return {{.unit_id = "grime", .cost = 1.0}, {.unit_id = "hound", .cost = 4.76}, {.unit_id = "mason", .cost = 8.78}};
}

EndlessSchedule make_schedule() {
    EndlessSchedule schedule;
    schedule.tuning = {
        .base_budget = 12.0,
        .escalation = 1.14,
        .bounty_decay = 0.985,
        .first_wave_delay = 3.0,
        .wave_interval = 18.0,
        .interval_growth = 1.01,
        .spawn_stagger = 0.8,
        .budget_ceiling = 600.0,
        .wall_clock_ceiling = 3600.0,
        .survival_bonus_per_wave = 25,
    };
    schedule.threat_costs = make_threat_costs();
    schedule.drift = {
        {.wave = 1, .weights = {{.unit_id = "grime", .weight = 4.0}, {.unit_id = "hound", .weight = 1.0}}},
        {.wave = 11, .weights = {{.unit_id = "grime", .weight = 2.0}, {.unit_id = "hound", .weight = 2.0}, {.unit_id = "mason", .weight = 2.0}}},
    };
    return schedule;
}

double wave_threat(const EndlessSchedule &schedule, const WaveDefinition &wave) {
    double total = 0.0;
    for (const SpawnDefinition &spawn : wave.spawns) {
        for (const UnitCost &cost : schedule.threat_costs) {
            if (cost.unit_id == spawn.type) {
                total += cost.cost;
            }
        }
    }
    return total;
}

std::map<std::string, int> counts_of(const WaveDefinition &wave) {
    std::map<std::string, int> counts;
    for (const SpawnDefinition &spawn : wave.spawns) {
        ++counts[spawn.type];
    }
    return counts;
}

double weight_of(const MixShape &shape, const std::string &unit_id) {
    for (const MixWeight &entry : shape) {
        if (entry.unit_id == unit_id) {
            return entry.weight;
        }
    }
    return 0.0;
}

} // namespace

DEFN_TEST(endless_generator_escalates_the_budget_geometrically) {
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());

    DEFN_CHECK_CLOSE(generator.budget(1), 12.0, 1e-9);
    for (int wave = 1; wave < 40; ++wave) {
        DEFN_CHECK(generator.budget(wave + 1) > generator.budget(wave));
    }
    DEFN_CHECK_CLOSE(generator.budget(3), 12.0 * 1.14 * 1.14, 1e-9);
}

DEFN_TEST(endless_generator_spends_the_budget_within_one_unit_cost) {
    const EndlessSchedule schedule = make_schedule();
    EndlessWaveGenerator generator;
    generator.configure(schedule);
    StdRandomSource random(2026);

    // The apportionment stops at the first slot that no longer fits, so a wave may leave under the dearest unit's
    // cost unspent -- and never more, or the wave is silently the wrong size rather than the wrong shape.
    constexpr double DEAREST = 8.78;
    for (const int wave : {1, 2, 5, 12, 25, 40}) {
        const WaveDefinition definition = generator.generate(wave, random);
        const double spent = wave_threat(schedule, definition);
        DEFN_CHECK(spent <= generator.budget(wave));
        DEFN_CHECK(generator.budget(wave) - spent < DEAREST);
    }
}

DEFN_TEST(endless_generator_reads_a_keyframe_as_bodies_rather_than_as_budget) {
    EndlessSchedule schedule = make_schedule();
    // Six grime to two hounds, with a budget that buys exactly that at the authored ratio.
    schedule.drift = {{.wave = 1, .weights = {{.unit_id = "grime", .weight = 6.0}, {.unit_id = "hound", .weight = 2.0}}}};
    schedule.tuning.base_budget = 6.0 + (2.0 * 4.76);
    EndlessWaveGenerator generator;
    generator.configure(schedule);
    StdRandomSource random(11);

    const std::map<std::string, int> counts = counts_of(generator.generate(1, random));

    // Spending the authored numbers as a budget share instead would put 75% of the purse on the cheapest hostile in
    // the game and buy eleven grime and no hound at all -- which is what shipped, and what this pins shut.
    DEFN_CHECK_EQ(counts.at("grime"), 6);
    DEFN_CHECK_EQ(counts.at("hound"), 2);
}

DEFN_TEST(endless_generator_holds_the_shape_at_a_keyframe_and_interpolates_between) {
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());

    const MixShape opening = generator.shape(1);
    DEFN_CHECK_CLOSE(weight_of(opening, "grime"), 4.0, 1e-9);
    DEFN_CHECK_CLOSE(weight_of(opening, "hound"), 1.0, 1e-9);
    DEFN_CHECK_CLOSE(weight_of(opening, "mason"), 0.0, 1e-9);

    const MixShape late = generator.shape(11);
    DEFN_CHECK_CLOSE(weight_of(late, "mason"), 2.0, 1e-9);

    // Halfway between wave 1 and wave 11 is halfway between the two keyframes' weights, per unit.
    const MixShape midway = generator.shape(6);
    DEFN_CHECK_CLOSE(weight_of(midway, "grime"), 3.0, 1e-9);
    DEFN_CHECK_CLOSE(weight_of(midway, "hound"), 1.5, 1e-9);
    DEFN_CHECK_CLOSE(weight_of(midway, "mason"), 1.0, 1e-9);

    // Past the last keyframe the drift holds rather than extrapolating into weights nobody measured.
    DEFN_CHECK_CLOSE(weight_of(generator.shape(40), "mason"), 2.0, 1e-9);
}

DEFN_TEST(endless_generator_lands_set_pieces_on_schedule) {
    EndlessSchedule schedule = make_schedule();
    schedule.set_pieces = {{.period = 5, .offset = 0, .weights = {{.unit_id = "hound", .weight = 1.0}}}};
    EndlessWaveGenerator generator;
    generator.configure(schedule);
    StdRandomSource random(2026);

    const std::map<std::string, int> rush_counts = counts_of(generator.generate(10, random));
    DEFN_CHECK_EQ(static_cast<int>(rush_counts.size()), 1);
    DEFN_CHECK(rush_counts.contains("hound"));

    // The wave either side of it is the drifted shape, not the set piece.
    DEFN_CHECK(counts_of(generator.generate(9, random)).size() > 1);
    DEFN_CHECK(counts_of(generator.generate(11, random)).size() > 1);
}

DEFN_TEST(endless_generator_replays_a_wave_identically_from_the_same_seed) {
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());

    StdRandomSource first(4242);
    StdRandomSource second(4242);
    const WaveDefinition left = generator.generate(9, first);
    const WaveDefinition right = generator.generate(9, second);

    DEFN_REQUIRE(left.spawns.size() == right.spawns.size());
    for (std::size_t index = 0; index < left.spawns.size(); ++index) {
        DEFN_CHECK_EQ(left.spawns[index].type, right.spawns[index].type);
        DEFN_CHECK_CLOSE(left.spawns[index].time, right.spawns[index].time, 1e-12);
    }
}

DEFN_TEST(endless_generator_composes_a_wave_the_same_way_whatever_the_seed) {
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());

    StdRandomSource first(1);
    StdRandomSource second(999);
    DEFN_CHECK(counts_of(generator.generate(14, first)) == counts_of(generator.generate(14, second)));
}

DEFN_TEST(endless_generator_keeps_spawns_sorted_inside_the_wave_window) {
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());
    StdRandomSource random(7);

    for (const int wave : {1, 4, 13, 30}) {
        const WaveDefinition definition = generator.generate(wave, random);
        DEFN_REQUIRE(!definition.spawns.empty());
        DEFN_CHECK_EQ(definition.wave_number, wave);

        const double start = generator.wave_start_time(wave);
        const double end = start + generator.wave_interval(wave);
        DEFN_CHECK(std::ranges::is_sorted(definition.spawns, [](const SpawnDefinition &left, const SpawnDefinition &right) { return left.time < right.time; }));
        DEFN_CHECK(definition.spawns.front().time >= start);
        DEFN_CHECK(definition.spawns.back().time <= end);
    }
}

DEFN_TEST(endless_generator_starts_each_wave_after_the_one_before_it) {
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());

    DEFN_CHECK_CLOSE(generator.wave_start_time(1), 3.0, 1e-9);
    DEFN_CHECK_CLOSE(generator.wave_start_time(2), 3.0 + 18.0, 1e-9);
    for (int wave = 1; wave < 40; ++wave) {
        DEFN_CHECK(generator.wave_start_time(wave + 1) > generator.wave_start_time(wave));
    }
}

DEFN_TEST(endless_generator_decays_the_bounty_without_reaching_zero) {
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());

    DEFN_CHECK_CLOSE(generator.bounty_multiplier(1), 1.0, 1e-9);
    for (int wave = 1; wave < 400; ++wave) {
        DEFN_CHECK(generator.bounty_multiplier(wave + 1) <= generator.bounty_multiplier(wave));
        DEFN_CHECK(generator.bounty_multiplier(wave) > 0.0);
    }
    DEFN_CHECK(generator.bounty_multiplier(2) < 1.0);
    DEFN_CHECK(generator.bounty_multiplier(10000) > 0.0);
}

DEFN_TEST(endless_generator_honours_the_bounty_floor) {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.bounty_decay = 0.5;
    schedule.tuning.bounty_floor = 0.25;
    // Pinned rather than inherited: this reads the floor against a decay it can state in closed form, and the
    // shipped curve is a tuning value that has already moved once.
    schedule.tuning.bounty_decay_curve = 1.0;
    EndlessWaveGenerator generator;
    generator.configure(schedule);

    // The decay lands exactly on the floor at wave 3 and would pass through it at wave 4, so waves 4 and 50 are
    // reading the floor and not the decay.
    DEFN_CHECK_CLOSE(generator.bounty_multiplier(2), 0.5, 1e-9);
    DEFN_CHECK_CLOSE(generator.bounty_multiplier(3), 0.25, 1e-9);
    DEFN_CHECK_CLOSE(generator.bounty_multiplier(4), 0.25, 1e-9);
    DEFN_CHECK_CLOSE(generator.bounty_multiplier(50), 0.25, 1e-9);

    // A floor of zero is a schedule that wants the decay to run all the way down, not a broken one, and a negative
    // floor is a typo that must never pay the player negative income.
    schedule.tuning.bounty_floor = -1.0;
    generator.configure(schedule);
    DEFN_CHECK(generator.bounty_multiplier(200) >= 0.0);
    DEFN_CHECK(generator.bounty_multiplier(200) < 0.25);
}

DEFN_TEST(endless_generator_bounty_curve_is_inert_at_one_and_softens_below_it) {
    EndlessSchedule geometric = make_schedule();
    geometric.tuning.bounty_decay = 0.88;
    geometric.tuning.bounty_floor = 0.0;
    geometric.tuning.bounty_decay_curve = 1.0;
    EndlessSchedule softened = geometric;
    softened.tuning.bounty_decay_curve = 0.6;

    EndlessWaveGenerator plain;
    plain.configure(geometric);
    EndlessWaveGenerator curved;
    curved.configure(softened);

    // `k = 1.0` has to reproduce the geometric decay exactly, or every number measured before the knob existed is
    // invalidated by a knob that was supposed to ship inert.
    for (int wave = 1; wave < 60; ++wave) {
        DEFN_CHECK_CLOSE(plain.bounty_multiplier(wave), std::pow(0.88, wave - 1), 1e-9);
    }

    // Every curve pivots on the same two waves and separates only after them: wave 1 raises the index 0 to a power
    // and wave 2 raises the index 1, so both are `k`-independent by arithmetic. `k` buys nothing before wave 3.
    DEFN_CHECK_CLOSE(curved.bounty_multiplier(1), 1.0, 1e-9);
    DEFN_CHECK_CLOSE(curved.bounty_multiplier(2), plain.bounty_multiplier(2), 1e-9);
    for (int wave = 3; wave < 60; ++wave) {
        DEFN_CHECK(curved.bounty_multiplier(wave) > plain.bounty_multiplier(wave));
        DEFN_CHECK(curved.bounty_multiplier(wave) <= curved.bounty_multiplier(wave - 1));
    }
}

DEFN_TEST(endless_generator_yields_an_empty_wave_when_nothing_is_priced) {
    EndlessSchedule schedule = make_schedule();
    schedule.threat_costs.clear();
    EndlessWaveGenerator generator;
    generator.configure(schedule);
    StdRandomSource random(3);

    DEFN_CHECK(generator.generate(1, random).spawns.empty());
}

DEFN_TEST(endless_generator_ramps_hostile_damage_onto_the_wave_it_generates) {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.hostile_damage_growth = 1.05;
    schedule.tuning.hostile_damage_cap = 3.0;
    EndlessWaveGenerator generator;
    generator.configure(schedule);
    StdRandomSource random(11);

    DEFN_CHECK_CLOSE(generator.hostile_damage_scale(1), 1.0, 1e-9);
    DEFN_CHECK_CLOSE(generator.hostile_damage_scale(3), 1.05 * 1.05, 1e-9);

    // Capped, because it multiplies: uncapped, a long run reaches a point where everything the player owns dies to
    // one hit and composition stops mattering at all.
    DEFN_CHECK_CLOSE(generator.hostile_damage_scale(500), 3.0, 1e-9);

    // And it has to reach the wave, not only the arithmetic -- this is what the spawn paths read.
    DEFN_CHECK_CLOSE(generator.generate(3, random).scale.damage, 1.05 * 1.05, 1e-9);
}

DEFN_TEST(endless_generator_leaves_hostile_damage_alone_by_default) {
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());
    StdRandomSource random(11);

    // Growth of 1.0 is the shipped default and the historical behaviour; an authored campaign wave must never be
    // scaled by a mode it knows nothing about.
    for (int wave = 1; wave < 60; ++wave) {
        DEFN_CHECK_CLOSE(generator.hostile_damage_scale(wave), 1.0, 1e-9);
    }
    DEFN_CHECK_CLOSE(generator.generate(20, random).scale.damage, 1.0, 1e-9);
}

DEFN_TEST(endless_generator_reproduces_the_geometric_ramp_at_curve_one) {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.escalation_curve = 1.0;
    EndlessWaveGenerator generator;
    generator.configure(schedule);

    // a = 1.0 is the historical schedule, and every reading taken before the curve existed has to keep reproducing.
    for (int wave = 1; wave < 60; ++wave) {
        const double geometric = schedule.tuning.base_budget * std::pow(schedule.tuning.escalation, static_cast<double>(wave - 1));
        DEFN_CHECK_CLOSE(generator.budget(wave), geometric, 1e-9);
    }
}

DEFN_TEST(endless_generator_outgrows_its_own_income_above_curve_one) {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.escalation_curve = 1.2;
    EndlessWaveGenerator generator;
    generator.configure(schedule);

    // The opening is what the curve must not disturb: the mode is meant to be won early and lost late.
    DEFN_CHECK_CLOSE(generator.budget(1), schedule.tuning.base_budget, 1e-9);
    DEFN_CHECK(generator.budget(2) / generator.budget(1) < 1.25);

    // The property the whole change exists for. Bounty income is proportional to the budget, so what the player can
    // have bought by wave n is proportional to the sum of every wave before it. Under a geometric ramp that sum is
    // a fixed multiple of the current wave and the army never falls behind; above curve 1.0 the ratio has to fall,
    // and keep falling, or the run cannot be overrun by construction.
    //
    // The convergence is real but slow: the step ratio grows like `n^(a-1)`, so the sum only sheds its multiple
    // gradually. What is asserted here is what actually holds over a run-length number of waves -- the ratio turns
    // over and then falls, and lands well under the geometric constant it would otherwise be pinned to.
    const double geometric_ratio = schedule.tuning.escalation / (schedule.tuning.escalation - 1.0);
    double running_total = 0.0;
    double previous_ratio = 0.0;
    for (int wave = 1; wave <= 40; ++wave) {
        running_total += generator.budget(wave);
        const double ratio = running_total / generator.budget(wave);
        if (wave > 15) {
            DEFN_CHECK(ratio < previous_ratio);
        }
        previous_ratio = ratio;
    }
    DEFN_CHECK(previous_ratio < 0.6 * geometric_ratio);

    // Every step is bigger than the one before it, so the ceiling scan in `EndlessDirector` still terminates.
    for (int wave = 1; wave < 60; ++wave) {
        DEFN_CHECK(generator.budget(wave + 1) > generator.budget(wave));
    }
}

namespace {

// The shipped schedule plus an elite ramp: a quarter of every wave from wave 5, at three times hit points.
EndlessSchedule make_elite_schedule() {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.elite_first_wave = 5;
    schedule.tuning.elite_fraction_growth = 0.05;
    schedule.tuning.elite_fraction_cap = 0.25;
    schedule.tuning.elite_hp = 2.0;
    schedule.tuning.elite_hp_growth = 1.10;
    schedule.tuning.elite_hp_cap = 6.0;
    schedule.tuning.elite_size = 1.35;
    return schedule;
}

int count_elites(const WaveDefinition &wave) {
    return static_cast<int>(std::ranges::count_if(wave.spawns, [](const SpawnDefinition &spawn) { return spawn.scale.hp > 1.0; }));
}

} // namespace

DEFN_TEST(endless_generator_widens_the_supply_allowance_with_the_wave) {
    EndlessWaveGenerator generator;
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.supply_start = 7.0;
    schedule.tuning.supply_growth = 0.4;
    generator.configure(schedule);

    DEFN_CHECK_EQ(generator.supply_cap(1), 7);
    DEFN_CHECK_EQ(generator.supply_cap(11), 11);
    DEFN_CHECK_EQ(generator.supply_cap(41), 23);
}

DEFN_TEST(endless_generator_leaves_the_supply_allowance_alone_by_default) {
    // Zero start means the schedule has no opinion, and the level's own cap stands. Every authored level is this.
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());

    for (int wave = 1; wave < 50; ++wave) {
        DEFN_CHECK_EQ(generator.supply_cap(wave), 0);
    }
}

DEFN_TEST(endless_generator_promotes_no_one_before_the_first_elite_wave) {
    EndlessWaveGenerator generator;
    generator.configure(make_elite_schedule());
    StdRandomSource random(7);

    for (int wave = 1; wave < 5; ++wave) {
        DEFN_CHECK_CLOSE(generator.elite_fraction(wave), 0.0, 1e-9);
        DEFN_CHECK_EQ(count_elites(generator.generate(wave, random)), 0);
    }
}

DEFN_TEST(endless_generator_elites_are_inert_by_default) {
    // Every authored level and the shipped default leave elites off entirely, so a mode that knows nothing about
    // them cannot have its waves quietly reshaped.
    EndlessWaveGenerator generator;
    generator.configure(make_schedule());
    StdRandomSource random(7);

    for (int wave = 1; wave < 60; ++wave) {
        DEFN_CHECK_CLOSE(generator.elite_fraction(wave), 0.0, 1e-9);
        DEFN_CHECK_CLOSE(generator.elite_cost_multiplier(wave), 1.0, 1e-9);
        DEFN_CHECK_EQ(count_elites(generator.generate(wave, random)), 0);
    }
}

DEFN_TEST(endless_generator_grows_the_elite_share_and_caps_it) {
    EndlessWaveGenerator generator;
    generator.configure(make_elite_schedule());

    DEFN_CHECK_CLOSE(generator.elite_fraction(5), 0.0, 1e-9);
    DEFN_CHECK_CLOSE(generator.elite_fraction(7), 0.10, 1e-9);
    DEFN_CHECK_CLOSE(generator.elite_fraction(10), 0.25, 1e-9);
    // Capped, or a long run reaches a wave that is nothing but elites and the composition question stops being asked.
    DEFN_CHECK_CLOSE(generator.elite_fraction(200), 0.25, 1e-9);
}

DEFN_TEST(endless_generator_grows_elite_hit_points_and_caps_them) {
    EndlessWaveGenerator generator;
    generator.configure(make_elite_schedule());

    DEFN_CHECK_CLOSE(generator.elite_scale(5).hp, 2.0, 1e-9);
    DEFN_CHECK_CLOSE(generator.elite_scale(7).hp, 2.0 * 1.10 * 1.10, 1e-9);
    DEFN_CHECK_CLOSE(generator.elite_scale(500).hp, 6.0, 1e-9);

    // The sprite marks the category and does not track the multiple, so an elite is legible at both ends of a run.
    DEFN_CHECK_CLOSE(generator.elite_scale(7).size, 1.35, 1e-9);
    DEFN_CHECK_CLOSE(generator.elite_scale(500).size, 1.35, 1e-9);

    // Nothing about an elite makes it hit harder: hit points multiply every attacker's work by the same factor and
    // so leave the roster's counters where they were measured, which is the whole reason this is the hp channel.
    DEFN_CHECK_CLOSE(generator.elite_scale(30).damage, 1.0, 1e-9);
}

DEFN_TEST(endless_generator_charges_the_budget_for_the_elites_it_promotes) {
    EndlessWaveGenerator plain;
    plain.configure(make_schedule());
    EndlessWaveGenerator elite;
    elite.configure(make_elite_schedule());
    StdRandomSource plain_random(3);
    StdRandomSource elite_random(3);

    const EndlessSchedule schedule = make_schedule();
    const WaveDefinition plain_wave = plain.generate(30, plain_random);
    const WaveDefinition elite_wave = elite.generate(30, elite_random);

    // An elite is priced at its hit-point multiple, so promoting bodies buys *fewer* of them for the same measured
    // threat rather than adding difficulty the schedule never paid for. That is what keeps the late game a wave the
    // screen can hold instead of a body count no ramp can reach.
    DEFN_CHECK(elite_wave.spawns.size() < plain_wave.spawns.size());
    DEFN_CHECK(count_elites(elite_wave) > 0);

    // And the elite wave is worth about what the plain one is, once each elite is counted at its multiple.
    const double plain_threat = wave_threat(schedule, plain_wave);
    double elite_threat = 0.0;
    for (const SpawnDefinition &spawn : elite_wave.spawns) {
        for (const UnitCost &cost : schedule.threat_costs) {
            if (cost.unit_id == spawn.type) {
                elite_threat += cost.cost * spawn.scale.hp;
            }
        }
    }
    DEFN_CHECK(std::abs(elite_threat - plain_threat) < plain_threat * 0.2);
}

DEFN_TEST(endless_generator_promotes_the_same_bodies_for_the_same_wave_whatever_the_seed) {
    // Composition is a pure function of `(schedule, wave)`, and which bodies are elite is part of the composition.
    // The seed decides only who walks in first.
    EndlessWaveGenerator generator;
    generator.configure(make_elite_schedule());

    StdRandomSource first_random(1);
    StdRandomSource second_random(99);
    const WaveDefinition first = generator.generate(24, first_random);
    const WaveDefinition second = generator.generate(24, second_random);

    DEFN_CHECK_EQ(counts_of(first), counts_of(second));
    DEFN_CHECK_EQ(count_elites(first), count_elites(second));
}

DEFN_TEST(endless_generator_spreads_promotions_across_the_mix_rather_than_one_unit_type) {
    // `expand_mix` interleaves the types, so striding through the line lands on a representative sample. Promoting
    // a block would silently turn every elite wave into a set piece of whichever type was listed first.
    EndlessWaveGenerator generator;
    EndlessSchedule schedule = make_elite_schedule();
    schedule.tuning.elite_fraction_cap = 0.5;
    schedule.tuning.elite_fraction_growth = 0.5;
    generator.configure(schedule);
    StdRandomSource random(5);

    const WaveDefinition wave = generator.generate(30, random);
    std::map<std::string, int> elite_types;
    for (const SpawnDefinition &spawn : wave.spawns) {
        if (spawn.scale.hp > 1.0) {
            ++elite_types[spawn.type];
        }
    }

    DEFN_CHECK(elite_types.size() > 1);
}

} // namespace defn
