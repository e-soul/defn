// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "sim_match.h"
#include "sim_roster.h"

#include <algorithm>

namespace defn {

namespace {

constexpr float BELT_TOP_RATIO = 0.66F;
constexpr float BELT_BOTTOM_RATIO = 0.825F;

std::vector<std::pair<std::string, AnimConfig>> make_match_animations() {
    const AnimConfig looping{.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0};
    const AnimConfig one_shot{.path_template = "", .frame_count = 10, .speed = 10.0, .loop = false, .windup_frames = 3};
    return {{"walk", looping}, {"attack", one_shot}, {"shoot", one_shot}, {"death", one_shot}};
}

UnitConfig make_match_unit(const std::string &name, UnitSide side, int hp, int cost, int bounty) {
    UnitConfig config;
    config.name = name;
    config.side = side;
    config.hp = hp;
    config.cost = cost;
    config.bounty = bounty;
    config.melee_damage = 15;
    config.melee_attack_period_seconds = 1.0;
    config.melee_attack_range = 128.0F;
    config.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.ranged_damage = 8;
    config.ranged_attack_period_seconds = 0.72;
    config.ranged_attack_range = 300.0F;
    config.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.move_speed_pixels_per_second = 60.0F;
    config.animations = make_match_animations();
    return config;
}

SimRoster make_match_roster() {
    SimRoster roster;

    // The base is a turret, as the shipped one is: it out-ranges everything and never moves.
    UnitConfig base = make_match_unit("base", UnitSide::FRIENDLY, 300, 0, 0);
    base.melee_damage = 0;
    base.ranged_damage = 12;
    base.ranged_attack_period_seconds = 1.2;
    base.ranged_attack_range = 520.0F;
    base.move_speed_pixels_per_second = 0.0F;
    roster.add(base);

    UnitConfig breacher = make_match_unit("breacher", UnitSide::FRIENDLY, 400, 20, 0);
    breacher.ranged_attack_range = 245.0F;
    breacher.move_speed_pixels_per_second = 58.0F;
    roster.add(breacher);

    UnitConfig marksman = make_match_unit("marksman", UnitSide::FRIENDLY, 180, 27, 0);
    marksman.ranged_damage = 19;
    marksman.ranged_attack_period_seconds = 1.05;
    marksman.ranged_attack_range = 650.0F;
    marksman.move_speed_pixels_per_second = 74.0F;
    roster.add(marksman);

    UnitConfig grime = make_match_unit("grime", UnitSide::HOSTILE, 95, 0, 4);
    grime.ranged_damage = 5;
    grime.ranged_attack_period_seconds = 0.62;
    grime.ranged_attack_range = 345.0F;
    grime.move_speed_pixels_per_second = 72.0F;
    roster.add(grime);

    return roster;
}

GlobalUnitConfig make_match_globals() {
    GlobalUnitConfig globals;
    globals.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    globals.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    return globals;
}

// A small level in the shape of the shipped ones: two waves of grime, a modest energy start, three hearts.
LevelDefinition make_test_level(int spawns_per_wave = 4) {
    LevelDefinition level;
    level.level_id = 1;
    level.name = "Test Outpost";
    level.starting_core_resource = 44;
    level.base_integrity = 3;
    level.base_position_ratio = {.x = 0.06F, .y = 0.65F};
    level.belt_width_ratio = {.x = BELT_TOP_RATIO, .y = BELT_BOTTOM_RATIO};

    double time = 3.0;
    for (int wave_number = 1; wave_number <= 2; ++wave_number) {
        WaveDefinition wave;
        wave.wave_number = wave_number;
        for (int index = 0; index < spawns_per_wave; ++index) {
            wave.spawns.push_back({.time = time, .type = "grime"});
            time += 1.3;
        }
        level.waves.push_back(wave);
        time += 8.0;
    }

    return level;
}

SimMatchReport run_match(const SimScenario &scenario, const LevelDefinition &level) {
    SimRoster roster = make_match_roster();
    SimMatch match(roster, make_match_globals(), level, scenario, {"breacher", "marksman"}, {});
    return match.run();
}

} // namespace

DEFN_TEST(sim_match_clears_a_small_level_with_the_greedy_policy) {
    SimScenario scenario;
    scenario.level_id = "level_01";
    scenario.seed = 2026U;
    scenario.policy.kind = "greedy";
    scenario.max_seconds = 300.0;

    const SimMatchReport report = run_match(scenario, make_test_level());

    DEFN_CHECK(report.decided);
    DEFN_CHECK(report.victory);
    DEFN_CHECK_EQ(report.policy, std::string("greedy"));
    DEFN_CHECK(report.deployments_total > 0);
    DEFN_CHECK(report.energy_spent > 0);
    DEFN_CHECK(report.clear_time_seconds > 0.0);
    DEFN_CHECK(report.clear_time_seconds < 300.0);
    DEFN_CHECK(report.remaining_integrity > 0);
    DEFN_CHECK(report.level_score > 0);
}

DEFN_TEST(sim_match_loses_when_the_level_is_beyond_the_policy) {
    SimScenario scenario;
    scenario.seed = 7U;
    scenario.policy.kind = "greedy";
    scenario.max_seconds = 300.0;

    // Far more grime than the energy economy can answer.
    const SimMatchReport report = run_match(scenario, make_test_level(40));

    DEFN_CHECK(report.decided);
    DEFN_CHECK(!report.victory);
    DEFN_CHECK_EQ(report.remaining_integrity, 0);
    DEFN_CHECK(!report.leak_events.empty());
    DEFN_CHECK(report.peak_concurrent_enemies > 1);
}

DEFN_TEST(sim_match_replays_identically_from_the_same_seed) {
    SimScenario scenario;
    scenario.seed = 99U;
    scenario.policy.kind = "greedy";

    const SimMatchReport first = run_match(scenario, make_test_level());
    const SimMatchReport second = run_match(scenario, make_test_level());

    DEFN_CHECK_EQ(first.victory, second.victory);
    DEFN_CHECK_CLOSE(first.clear_time_seconds, second.clear_time_seconds, 0.0);
    DEFN_CHECK_EQ(first.level_score, second.level_score);
    DEFN_CHECK_EQ(first.deployments_total, second.deployments_total);
    DEFN_CHECK_EQ(to_jsonl(first), to_jsonl(second));
}

DEFN_TEST(sim_match_reports_a_different_run_for_a_different_seed) {
    SimScenario first_scenario;
    first_scenario.seed = 1U;
    SimScenario second_scenario;
    second_scenario.seed = 2U;

    const SimMatchReport first = run_match(first_scenario, make_test_level());
    const SimMatchReport second = run_match(second_scenario, make_test_level());

    // Belt-Y sampling and range variation both move with the seed, so the runs cannot be bit-identical.
    DEFN_CHECK(to_jsonl(first) != to_jsonl(second));
}

DEFN_TEST(sim_match_scrolls_the_camera_when_a_friendly_reaches_the_trigger) {
    // Nothing to fight for a minute, so a deployed friendly walks all the way to the right trigger strip.
    LevelDefinition quiet_level = make_test_level();
    for (WaveDefinition &wave : quiet_level.waves) {
        for (SpawnDefinition &spawn : wave.spawns) {
            spawn.time += 60.0;
        }
    }

    SimScenario modelled;
    modelled.seed = 5U;
    modelled.max_seconds = 45.0;
    SimScenario fixed = modelled;
    fixed.camera = SimCameraMode::FIXED;

    const SimMatchReport with_camera = run_match(modelled, quiet_level);
    const SimMatchReport without_camera = run_match(fixed, quiet_level);

    DEFN_CHECK(with_camera.camera_scroll_events > 0);
    DEFN_CHECK_EQ(without_camera.camera_scroll_events, 0);
}

// The front line settles wherever the two sides meet, which on a short level is well short of the scroll trigger:
// the camera only moves once the player has actually won ground.
DEFN_TEST(sim_match_leaves_the_camera_alone_while_the_front_line_holds) {
    SimScenario scenario;
    scenario.seed = 5U;

    const SimMatchReport report = run_match(scenario, make_test_level());

    DEFN_CHECK_EQ(report.camera_scroll_events, 0);
    DEFN_CHECK(!report.front_line_trace.empty());
}

// Where a fight happens decides how much a unit is worth. Deploying on sight sends units on a long walk to a fight
// the base cannot reach; holding until the enemy is inside the base's range gives the same unit a short walk and a
// turret fighting beside it. Measured, that is worth roughly twice the damage per deployment.
DEFN_TEST(sim_match_gets_more_from_each_unit_by_holding_the_line_at_the_base) {
    const LevelDefinition level = make_test_level(10);

    SimScenario greedy;
    greedy.seed = 2026U;
    greedy.policy.kind = "greedy";
    SimScenario defensive = greedy;
    defensive.policy.kind = "defensive";

    const SimMatchReport greedy_report = run_match(greedy, level);
    const SimMatchReport defensive_report = run_match(defensive, level);

    const auto damage_per_deployment = [](const SimMatchReport &report) {
        for (const SimUnitStat &unit : report.per_unit) {
            if (unit.unit_id == "breacher") {
                return static_cast<double>(unit.damage_dealt) / std::max(report.deployments_total, 1);
            }
        }
        return 0.0;
    };

    DEFN_CHECK(damage_per_deployment(defensive_report) > damage_per_deployment(greedy_report));
    DEFN_CHECK(defensive_report.remaining_integrity >= greedy_report.remaining_integrity);
    DEFN_CHECK(defensive_report.leak_events.size() <= greedy_report.leak_events.size());
}

DEFN_TEST(sim_match_holds_every_deployment_until_the_enemy_is_in_reach) {
    SimScenario scenario;
    scenario.seed = 31U;
    scenario.policy.kind = "defensive";
    scenario.max_seconds = 12.0; // the first grime spawns at 3s and is still far out at 12s

    const SimMatchReport report = run_match(scenario, make_test_level());

    DEFN_CHECK_EQ(report.deployments_total, 0);
    DEFN_CHECK_EQ(report.energy_spent, 0);
    DEFN_CHECK(report.energy_idle_integral > 0.0);
}

DEFN_TEST(sim_match_runs_every_policy) {
    for (const char *kind : {"greedy", "patience", "scripted", "defensive", "mix"}) {
        SimScenario scenario;
        scenario.seed = 11U;
        scenario.policy.kind = kind;
        scenario.policy.script = {{.time_seconds = 1.0, .unit_id = "breacher"}, {.time_seconds = 12.0, .unit_id = "breacher"}};
        scenario.policy.weights = {{"breacher", 1.0}, {"marksman", 1.0}};

        const SimMatchReport report = run_match(scenario, make_test_level());

        DEFN_CHECK_EQ(report.policy, std::string(kind));
        DEFN_CHECK(report.decided);
        DEFN_CHECK(report.clear_time_seconds > 0.0);
    }
}

// Every other policy ends in "the most expensive thing I can afford", so a sweep of them compares mono-stacks and
// nothing else. This is the one that can play a composition, and the deployment counts are how you tell.
DEFN_TEST(sim_match_mix_policy_deploys_toward_its_target_shape) {
    SimScenario scenario;
    scenario.seed = 17U;
    scenario.policy.kind = "mix";
    scenario.policy.weights = {{"breacher", 3.0}, {"marksman", 1.0}};

    const SimMatchReport report = run_match(scenario, make_test_level(10));

    const auto deployed = [&report](const std::string &unit_id) {
        for (const SimDeploymentStat &deployment : report.deployments) {
            if (deployment.unit_id == unit_id) {
                return deployment.count;
            }
        }
        return 0;
    };

    DEFN_CHECK(deployed("breacher") > 0);
    DEFN_CHECK(deployed("marksman") > 0);
    DEFN_CHECK(deployed("breacher") > deployed("marksman"));
}

// Greedy reaches for the top of the roster whenever it can afford it, so what it ends up buying is decided by the
// price list. A mix that names only the breacher never buys a marksman, however much energy is banked.
DEFN_TEST(sim_match_mix_policy_buys_what_it_was_told_rather_than_the_top_of_the_ladder) {
    SimScenario greedy;
    greedy.seed = 17U;
    greedy.policy.kind = "greedy";

    SimScenario mix = greedy;
    mix.policy.kind = "mix";
    mix.policy.weights = {{"breacher", 1.0}};

    const SimMatchReport greedy_report = run_match(greedy, make_test_level(10));
    const SimMatchReport mix_report = run_match(mix, make_test_level(10));

    const auto deployed = [](const SimMatchReport &report, const std::string &unit_id) {
        for (const SimDeploymentStat &deployment : report.deployments) {
            if (deployment.unit_id == unit_id) {
                return deployment.count;
            }
        }
        return 0;
    };

    DEFN_CHECK(deployed(greedy_report, "marksman") > 0);
    DEFN_CHECK_EQ(deployed(mix_report, "marksman"), 0);
    DEFN_CHECK(deployed(mix_report, "breacher") > 0);
}

// The banking rule and its one exception, pinned together because each is wrong without the other.
//
// Banking is deliberate: a mix that skipped to the next-neediest affordable unit whenever the neediest was out of
// reach would buy the cheap end every time energy crossed its cost and never reach the expensive end at all -- a
// mono-stack claiming to be a composition. But an unbounded bank stalls exactly where a level is decided. On
// `level_01`, `{breacher 2, marksman 1}` bought one breacher and then sat on 24 energy waiting for a 27-cost
// marksman while four hounds crossed the belt at 120px/s: 94 energy spent against ~200 for every other policy, and
// 3 wins in 25.
//
// The exception is `hostile_at_the_gate` and **not** the broader `under_pressure` that `patience` uses. Both were
// measured over 750 paired runs: the broad test also fires on a busy belt, which is the normal condition of
// levels 3 to 5, so it swallowed the rule and handed back the mono-stack -- `level_04` marksman deployments
// 4.5 -> 1.0 per run and 24/25 -> 0/25. The narrow test keeps `level_04` byte-identical and takes `level_01` from
// 3/25 to 11/25, which is the whole of the broad test's gain and none of its cost.
DEFN_TEST(sim_match_mix_policy_spends_rather_than_banking_with_a_hostile_at_the_gate) {
    SimScenario scenario;
    scenario.seed = 23U;
    scenario.policy.kind = "mix";
    // The breacher is affordable long before the marksman, so an unbroken bank shows up as an idle purse.
    scenario.policy.weights = {{"breacher", 1.0}, {"marksman", 3.0}};

    const SimMatchReport report = run_match(scenario, make_test_level(10));

    DEFN_CHECK(report.deployments_total > 0);
    DEFN_CHECK(report.energy_spent > 0);
}

// And the exception does not eat the rule: with nothing near the base, the policy still holds out for the unit its
// shape is short of rather than spending down on the affordable one.
DEFN_TEST(sim_match_mix_policy_still_banks_for_the_expensive_end_when_nothing_is_pressing) {
    SimScenario scenario;
    scenario.seed = 29U;
    scenario.policy.kind = "mix";
    scenario.policy.weights = {{"breacher", 1.0}, {"marksman", 1.0}};
    // The first hostile spawns at 3s and is still far out at 12s, so no pressure test can fire in this window.
    scenario.max_seconds = 12.0;

    const SimMatchReport report = run_match(scenario, make_test_level());

    const auto deployed = [&report](const std::string &unit_id) {
        for (const SimDeploymentStat &deployment : report.deployments) {
            if (deployment.unit_id == unit_id) {
                return deployment.count;
            }
        }
        return 0;
    };

    // Whatever it bought, it did not spend its way past the marksman by stacking the cheap end.
    DEFN_CHECK(deployed("breacher") <= deployed("marksman") + 1);
}

DEFN_TEST(sim_match_report_serializes_to_one_json_line) {
    SimScenario scenario;
    scenario.seed = 3U;
    const std::string line = to_jsonl(run_match(scenario, make_test_level()));

    DEFN_CHECK(line.front() == '{');
    DEFN_CHECK(line.back() == '}');
    DEFN_CHECK(line.find('\n') == std::string::npos);
    DEFN_CHECK(line.find("\"level_id\":") != std::string::npos);
    DEFN_CHECK(line.find("\"victory\":") != std::string::npos);
    DEFN_CHECK(line.find("\"per_unit\":[") != std::string::npos);
    DEFN_CHECK(line.find("\"front_line_trace\":[") != std::string::npos);
}

} // namespace defn
