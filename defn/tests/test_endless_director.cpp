// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "endless_director.h"
#include "level_definition.h"
#include "match_director.h"
#include "random_source.h"
#include "sim_grid.h"
#include "sim_progression.h"
#include "sim_roster.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace defn {

namespace {

constexpr double TICK_SECONDS = 1.0 / 60.0;

std::vector<std::pair<std::string, AnimConfig>> make_animations() {
    const AnimConfig looping{.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0};
    return {{"walk", looping}, {"attack", looping}, {"shoot", looping}, {"death", looping}};
}

UnitConfig make_unit(const std::string &name, UnitSide side, int cost, int bounty) {
    UnitConfig config;
    config.name = name;
    config.side = side;
    config.hp = 100;
    config.cost = cost;
    config.bounty = bounty;
    config.animations = make_animations();
    return config;
}

SimRoster make_roster() {
    SimRoster roster;
    roster.add(make_unit("base", UnitSide::FRIENDLY, 0, 0));
    roster.add(make_unit("breacher", UnitSide::FRIENDLY, 20, 0));
    roster.add(make_unit("grime", UnitSide::HOSTILE, 0, 4));
    roster.add(make_unit("hound", UnitSide::HOSTILE, 0, 5));
    return roster;
}

LevelDefinition make_endless_level() {
    LevelDefinition level;
    level.name = "Standing Engagement";
    level.starting_core_resource = 105;
    level.base_integrity = 4;
    // Deliberately empty: an endless level authors no waves, which is also what makes the HUD read the wave count as
    // unbounded instead of as a denominator.
    return level;
}

EndlessSchedule make_schedule() {
    EndlessSchedule schedule;
    schedule.tuning = {
        .base_budget = 12.0,
        .escalation = 1.1,
        .bounty_decay = 0.9,
        .first_wave_delay = 2.0,
        .wave_interval = 10.0,
        .interval_growth = 1.0,
        .spawn_stagger = 0.5,
        .budget_ceiling = 1.0e9,
        .wall_clock_ceiling = 1.0e9,
        .survival_bonus_per_wave = 25,
    };
    schedule.threat_costs = {{.unit_id = "grime", .cost = 1.0}, {.unit_id = "hound", .cost = 4.76}};
    schedule.drift = {
        {.wave = 1, .weights = {{.unit_id = "grime", .weight = 3.0}, {.unit_id = "hound", .weight = 1.0}}},
        {.wave = 20, .weights = {{.unit_id = "grime", .weight = 1.0}, {.unit_id = "hound", .weight = 3.0}}},
    };
    return schedule;
}

// A whole endless run, headless and without a world: every hostile that spawns is reported dead on the same tick, so
// the only thing standing between the match and a victory is the timeline never running out.
struct EndlessHarness {
    SimRoster roster = make_roster();
    StdRandomSource random{2026};
    GameplayRules rules;
    SimGrid grid{rules, random};
    SimProgression progression;
    MatchDirector director;
    EndlessDirector endless;
    int bounty_awarded_total = 0;
    int last_bounty_awarded = 0;
    // Every hostile that reached a spawn intent, with the damage scale it was carrying.
    std::vector<std::pair<int, double>> hostile_damage_scales;

    explicit EndlessHarness(const EndlessSchedule &schedule) {
        progression.configure({"breacher"}, {}, {});
        director.configure(&progression, &roster, &grid, &random);
        director.load_level_definition(make_endless_level(), "endless");
        endless.configure(&director, &progression, schedule, &random);
        endless.seed_first_wave();
        director.begin_match();
    }

    // Returns the match end, if the run finished on this tick.
    std::optional<MatchEnded> tick() {
        MatchUpdate update = endless.update(TICK_SECONDS);
        endless.finalize_ended_run(update);
        for (const SpawnUnitIntent &intent : update.spawn_unit_intents) {
            if (intent.side != MatchUnitSide::Hostile) {
                continue;
            }
            hostile_damage_scales.emplace_back(endless.current_wave(), intent.damage_scale);
            const auto config = roster.get_unit(intent.unit_id);
            const MatchUpdate death = director.handle_enemy_defeated({.bounty = config.has_value() ? config->bounty : 0});
            if (death.score_changed.has_value()) {
                last_bounty_awarded = death.score_changed->bounty_awarded;
                bounty_awarded_total += last_bounty_awarded;
            }
        }
        return update.match_ended;
    }

    std::optional<MatchEnded> run_for(double seconds) {
        const auto ticks = static_cast<int>(seconds / TICK_SECONDS);
        for (int index = 0; index < ticks; ++index) {
            if (const auto ended = tick(); ended.has_value()) {
                return ended;
            }
        }
        return std::nullopt;
    }
};

} // namespace

DEFN_TEST(endless_director_carries_the_wave_damage_scale_onto_every_hostile_spawn_intent) {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.hostile_damage_growth = 1.5;
    schedule.tuning.hostile_damage_cap = 100.0;
    EndlessHarness harness(schedule);

    harness.run_for(120.0);

    // A wiring test, not an arithmetic one. `with_damage_scale` and `hostile_damage_scale` are both unit-tested, but
    // the value has to survive WaveDefinition -> SpawnTimelineWave -> FlatSpawn -> DueSpawn -> SpawnUnitIntent to
    // reach a spawner, and a null result in the sweep is indistinguishable from a break anywhere along that chain.
    DEFN_CHECK(!harness.hostile_damage_scales.empty());

    bool saw_a_scaled_wave = false;
    for (const auto &[wave, scale] : harness.hostile_damage_scales) {
        const double expected = std::pow(1.5, static_cast<double>(std::max(wave, 1) - 1));
        DEFN_CHECK_CLOSE(scale, expected, 1e-9);
        saw_a_scaled_wave = saw_a_scaled_wave || scale > 1.0;
    }
    DEFN_CHECK(saw_a_scaled_wave);
}

DEFN_TEST(endless_director_leaves_the_damage_scale_at_one_when_the_ramp_is_off) {
    EndlessHarness harness(make_schedule());

    harness.run_for(120.0);

    DEFN_CHECK(!harness.hostile_damage_scales.empty());
    for (const auto &[wave, scale] : harness.hostile_damage_scales) {
        DEFN_CHECK_CLOSE(scale, 1.0, 1e-9);
    }
}

DEFN_TEST(endless_director_never_lets_the_match_end_in_victory) {
    EndlessHarness harness(make_schedule());

    // Thirty-odd waves at a ten second interval, with the field emptied on every tick -- the exact state a campaign
    // level ends victorious in.
    const std::optional<MatchEnded> ended = harness.run_for(330.0);

    DEFN_CHECK(!ended.has_value());
    DEFN_CHECK(!harness.director.is_game_over());
    DEFN_CHECK(harness.endless.current_wave() >= 30);
}

DEFN_TEST(endless_director_keeps_one_wave_ahead_of_the_spawn_cursor) {
    EndlessHarness harness(make_schedule());

    DEFN_CHECK_EQ(harness.endless.appended_through_wave(), 1);
    harness.run_for(60.0);
    // Wave 6 is running; wave 7 is already on the timeline and has not started.
    DEFN_CHECK_EQ(harness.endless.appended_through_wave(), harness.endless.current_wave() + 1);
}

DEFN_TEST(endless_director_ends_the_run_at_the_budget_ceiling) {
    EndlessSchedule schedule = make_schedule();
    // 12 * 1.1^(n-1) first passes 20 at wave 7, so waves 1 to 6 are fought and the run stops when 7 would open.
    schedule.tuning.budget_ceiling = 20.0;
    EndlessHarness harness(schedule);

    const std::optional<MatchEnded> ended = harness.run_for(200.0);

    DEFN_REQUIRE(ended.has_value());
    DEFN_CHECK(!ended->victory);
    DEFN_CHECK_EQ(harness.endless.current_wave(), 6);
    DEFN_CHECK(harness.director.is_game_over());
}

DEFN_TEST(endless_director_ends_the_run_at_the_wall_clock_ceiling) {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.wall_clock_ceiling = 45.0;
    EndlessHarness harness(schedule);

    const std::optional<MatchEnded> ended = harness.run_for(200.0);

    DEFN_REQUIRE(ended.has_value());
    DEFN_CHECK(!ended->victory);
    DEFN_CHECK(harness.endless.elapsed_seconds() >= 45.0);
}

DEFN_TEST(endless_director_decays_what_a_kill_pays_without_decaying_what_it_scores) {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.bounty_decay = 0.5;
    EndlessHarness harness(schedule);

    harness.run_for(9.0);
    const int first_wave_bounty = harness.last_bounty_awarded;
    harness.run_for(30.0);
    const int later_bounty = harness.last_bounty_awarded;

    DEFN_CHECK(first_wave_bounty > 0);
    DEFN_CHECK(later_bounty < first_wave_bounty);
}

DEFN_TEST(endless_director_pays_a_survival_bonus_per_wave) {
    EndlessSchedule schedule = make_schedule();
    schedule.tuning.wall_clock_ceiling = 55.0;
    EndlessHarness harness(schedule);

    const std::optional<MatchEnded> ended = harness.run_for(200.0);

    DEFN_REQUIRE(ended.has_value());
    // Waves open at 2, 12, 22, 32, 42 and 52 seconds, at 25 points each.
    DEFN_CHECK_EQ(ended->summary_model.survival_bonus, 6 * 25);
    DEFN_CHECK_EQ(ended->summary_model.wave_reached, 6);
    DEFN_CHECK(ended->summary_model.level_score >= ended->summary_model.survival_bonus);
}

} // namespace defn
