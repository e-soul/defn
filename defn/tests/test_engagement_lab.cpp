// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "sim_engagement_lab.h"
#include "sim_roster.h"
#include "sim_world.h"

namespace defn {

namespace {

std::vector<std::pair<std::string, AnimConfig>> make_standard_animations() {
    const AnimConfig looping{.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0};
    const AnimConfig one_shot{.path_template = "", .frame_count = 10, .speed = 10.0, .loop = false, .windup_frames = 3};
    return {{"walk", looping}, {"attack", one_shot}, {"shoot", one_shot}, {"death", one_shot}};
}

UnitConfig make_unit(const std::string &name, UnitSide side, int hp, int cost) {
    UnitConfig config;
    config.name = name;
    config.side = side;
    config.hp = hp;
    config.cost = cost;
    config.melee_damage = 15;
    config.melee_attack_period_seconds = 1.0;
    config.melee_attack_range = 128.0F;
    config.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.ranged_damage = 10;
    config.ranged_attack_period_seconds = 1.0;
    config.ranged_attack_range = 300.0F;
    config.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.move_speed_pixels_per_second = 60.0F;
    config.animations = make_standard_animations();
    return config;
}

GlobalUnitConfig make_globals() {
    GlobalUnitConfig globals;
    globals.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    globals.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    return globals;
}

// A cheap unit, an expensive one, and something to shoot at.
SimRoster make_priced_roster() {
    SimRoster roster;
    roster.add(make_unit("cheap", UnitSide::FRIENDLY, 200, 20));
    roster.add(make_unit("dear", UnitSide::FRIENDLY, 500, 40));
    roster.add(make_unit("grunt", UnitSide::HOSTILE, 60, 0));
    return roster;
}

int count_of(const ForceMix &mix, const std::string &unit_id) {
    for (const MixEntry &entry : mix) {
        if (entry.unit_id == unit_id) {
            return entry.count;
        }
    }
    return 0;
}

// A short-ranged anchor and a fragile shooter behind it, against enemies that can reach both.
SimRoster make_aggro_roster(float anchor_threat_weight) {
    SimRoster roster;

    UnitConfig anchor = make_unit("anchor", UnitSide::FRIENDLY, 600, 20);
    anchor.ranged_damage = 8;
    anchor.ranged_attack_range = 200.0F;
    anchor.threat_weight = anchor_threat_weight;
    roster.add(anchor);

    UnitConfig shooter = make_unit("shooter", UnitSide::FRIENDLY, 200, 30);
    shooter.ranged_damage = 12;
    shooter.ranged_attack_range = 400.0F;
    roster.add(shooter);

    UnitConfig raider = make_unit("raider", UnitSide::HOSTILE, 120, 0);
    raider.ranged_damage = 8;
    raider.ranged_attack_range = 400.0F;
    roster.add(raider);

    return roster;
}

int damage_taken_by(const std::vector<SimEntity> &entities, const std::string &unit_id) {
    int total = 0;
    for (const SimEntity &entity : entities) {
        if (entity.unit_id == unit_id) {
            total += entity.damage_taken;
        }
    }
    return total;
}

// Measures where damage landed, not who was targeted on one tick: the tank role is only real if it changes the
// distribution over a whole engagement.
int shooter_damage_with_anchor_weight(float anchor_threat_weight) {
    SimRoster roster = make_aggro_roster(anchor_threat_weight);
    StdRandomSource random(4U);
    SimWorld world(roster, make_globals(), random);
    // The shooter stands in front, so distance alone would send every shot at it. Aggro has to pull fire *backwards*
    // onto the anchor to show up at all -- an anchor that is already the nearest target proves nothing, because
    // target selection would have picked it anyway.
    //
    // The two stand close together and the raiders open already inside range of both, which is the condition the
    // mechanic needs: a target held from a previous tick is never re-picked, so aggro weight only ever gets to speak
    // at the moment of acquisition. Spread the line out and the raiders acquire whoever entered range first and stay
    // on it, weight or no weight.
    world.spawn("shooter", UnitSide::FRIENDLY, {.x = 800.0F, .y = 800.0F});
    world.spawn("anchor", UnitSide::FRIENDLY, {.x = 760.0F, .y = 800.0F});
    for (int index = 0; index < 4; ++index) {
        world.spawn("raider", UnitSide::HOSTILE, {.x = 1000.0F + (static_cast<float>(index) * 40.0F), .y = 800.0F});
    }
    world.begin_run();
    run_engagement(world, 60.0);

    return damage_taken_by(world.get_entities(), "shooter");
}

} // namespace

// The tank role, stated as the thing it is supposed to do. This is the one Phase 1 mechanic that changes an outcome
// rather than only a choice: unit A changes where damage lands on unit B.
DEFN_TEST(engagement_lab_threat_weight_moves_damage_onto_the_anchor) {
    const int without_aggro = shooter_damage_with_anchor_weight(1.0F);
    const int with_aggro = shooter_damage_with_anchor_weight(4.0F);

    DEFN_CHECK(without_aggro > 0);
    DEFN_CHECK(with_aggro < without_aggro);
}

DEFN_TEST(engagement_lab_interleaves_a_mix_instead_of_concatenating_it) {
    const ForceMix mix = {{.unit_id = "cheap", .count = 2}, {.unit_id = "dear", .count = 1}};

    const std::vector<std::string> line = expand_mix(mix);

    // Concatenation would put both cheap units in front, so the measurement would be about who stands where.
    DEFN_CHECK_EQ(static_cast<int>(line.size()), 3);
    DEFN_CHECK_EQ(line[0], std::string("cheap"));
    DEFN_CHECK_EQ(line[1], std::string("dear"));
    DEFN_CHECK_EQ(line[2], std::string("cheap"));
}

DEFN_TEST(engagement_lab_spends_a_budget_along_the_shape) {
    const SimRoster roster = make_priced_roster();
    const MixShape shape = {{.unit_id = "cheap", .weight = 1.0}, {.unit_id = "dear", .weight = 1.0}};

    const BudgetAllocation allocation = allocate_budget(roster, shape, 120.0);

    DEFN_CHECK_EQ(count_of(allocation.mix, "cheap"), 3);
    DEFN_CHECK_EQ(count_of(allocation.mix, "dear"), 1);
    DEFN_CHECK_EQ(allocation.energy_spent, 100);
}

DEFN_TEST(engagement_lab_keeps_the_shape_alive_at_a_small_budget) {
    const SimRoster roster = make_priced_roster();
    const MixShape shape = {{.unit_id = "cheap", .weight = 1.0}, {.unit_id = "dear", .weight = 1.0}};

    // Even split: 30 energy each. Naive flooring buys one cheap and nothing else, collapsing the mix to a mono-stack;
    // largest remainder spends the leftover on the slot that was rounded down hardest.
    const BudgetAllocation allocation = allocate_budget(roster, shape, 60.0);

    DEFN_CHECK_EQ(count_of(allocation.mix, "cheap"), 1);
    DEFN_CHECK_EQ(count_of(allocation.mix, "dear"), 1);
    DEFN_CHECK_EQ(allocation.energy_spent, 60);
}

DEFN_TEST(engagement_lab_ignores_weightless_and_unknown_units) {
    const SimRoster roster = make_priced_roster();
    const MixShape shape = {{.unit_id = "cheap", .weight = 1.0}, {.unit_id = "dear", .weight = 0.0}, {.unit_id = "nosuchunit", .weight = 5.0}};

    const BudgetAllocation allocation = allocate_budget(roster, shape, 100.0);

    DEFN_CHECK_EQ(static_cast<int>(allocation.mix.size()), 1);
    DEFN_CHECK_EQ(count_of(allocation.mix, "cheap"), 5);
}

DEFN_TEST(engagement_lab_buys_nothing_with_no_budget) {
    const SimRoster roster = make_priced_roster();
    const MixShape shape = {{.unit_id = "cheap", .weight = 1.0}};

    const BudgetAllocation allocation = allocate_budget(roster, shape, 0.0);

    DEFN_CHECK(allocation.mix.empty());
    DEFN_CHECK_EQ(allocation.energy_spent, 0);
}

DEFN_TEST(engagement_lab_bisects_to_a_budget_that_wins) {
    const SimRoster roster = make_priced_roster();
    const GlobalUnitConfig globals = make_globals();
    const MixShape shape = {{.unit_id = "cheap", .weight = 1.0}};
    const std::vector<std::uint32_t> seeds = default_seeds(3);

    const CriticalBudget budget = critical_budget(roster, globals, shape, mono_mix("grunt", 2), seeds);

    DEFN_CHECK(budget.bounded);
    DEFN_CHECK(budget.win_rate >= 0.5);
    DEFN_CHECK(budget.energy > 0.0);
    DEFN_CHECK(budget.energy_spent > 0);
    DEFN_CHECK(budget.probes > 0);
}

DEFN_TEST(engagement_lab_reports_an_unwinnable_cell_as_unbounded) {
    const SimRoster roster = make_priced_roster();
    const GlobalUnitConfig globals = make_globals();
    const MixShape shape = {{.unit_id = "cheap", .weight = 1.0}};
    const std::vector<std::uint32_t> seeds = default_seeds(1);

    // Sixty grunts against whatever forty energy buys: the ceiling still loses, and a number here would be a lie.
    CriticalBudgetOptions options;
    options.max_budget = 40.0;
    const CriticalBudget budget = critical_budget(roster, globals, shape, mono_mix("grunt", 60), seeds, options);

    DEFN_CHECK(!budget.bounded);
    DEFN_CHECK(budget.win_rate < 0.5);
}

DEFN_TEST(engagement_lab_costs_a_harder_column_more_budget) {
    const SimRoster roster = make_priced_roster();
    const GlobalUnitConfig globals = make_globals();
    const MixShape shape = {{.unit_id = "cheap", .weight = 1.0}};
    const std::vector<std::uint32_t> seeds = default_seeds(3);

    const CriticalBudget easy = critical_budget(roster, globals, shape, mono_mix("grunt", 2), seeds);
    const CriticalBudget hard = critical_budget(roster, globals, shape, mono_mix("grunt", 10), seeds);

    // The point of the scale: unlike win rate, it does not read 100% for both.
    DEFN_CHECK(easy.bounded);
    DEFN_CHECK(hard.bounded);
    DEFN_CHECK(hard.energy > easy.energy);
}

DEFN_TEST(engagement_lab_repeats_a_measurement_exactly) {
    const SimRoster roster = make_priced_roster();
    const GlobalUnitConfig globals = make_globals();
    const ForceMix friendlies = {{.unit_id = "cheap", .count = 2}, {.unit_id = "dear", .count = 1}};

    const EngagementOutcome first = run_engagement_once(roster, globals, friendlies, mono_mix("grunt", 4), 7U);
    const EngagementOutcome again = run_engagement_once(roster, globals, friendlies, mono_mix("grunt", 4), 7U);

    DEFN_CHECK_EQ(first.friendly_won, again.friendly_won);
    DEFN_CHECK_EQ(first.friendly_damage_taken, again.friendly_damage_taken);
    DEFN_CHECK_EQ(first.hostiles_killed, again.hostiles_killed);
    DEFN_CHECK_CLOSE(first.duration_seconds, again.duration_seconds, 1e-9);
}

} // namespace defn
