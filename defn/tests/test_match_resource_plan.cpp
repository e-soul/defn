// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "match_resource_plan.h"
#include "test_harness.h"
#include <algorithm>

namespace defn {
DEFN_TEST(match_roster_contains_only_unlocked_friendlies_and_all_authored_waves) {
    const LevelDefinition level{.waves = {{.spawns = {{.type = "hound"}, {.type = "hound"}}}, {.spawns = {{.type = "grime"}}}}};
    const std::vector<std::string> friendlies{"breacher"};
    const auto ids = match_unit_ids(level, friendlies);
    DEFN_CHECK((ids == std::vector<std::string>{"base", "breacher", "grime", "hound"}));
    DEFN_CHECK(std::ranges::find(ids, "marksman") == ids.end());
}

DEFN_TEST(endless_loading_includes_late_drift_and_set_pieces_but_excludes_unspendable_units) {
    const EndlessSchedule schedule{
        .drift = {{.wave = 1, .weights = {{"grime", 1}, {"unused", 0}}}, {.wave = 20, .weights = {{"hound", 2}, {"unpriced", 1}}}},
        .set_pieces = {{.period = 5, .weights = {{"wrecker", 1}}}, {.period = 0, .weights = {{"disabled", 1}}}},
        .threat_costs = {{"grime", 1}, {"unused", 1}, {"hound", 2}, {"wrecker", 4}, {"unpriced", 0}, {"disabled", 1}},
    };
    const auto ids = match_unit_ids({}, {}, &schedule);
    DEFN_CHECK((ids == std::vector<std::string>{"base", "grime", "hound", "wrecker"}));
}

DEFN_TEST(match_resource_manifest_follows_selected_units_and_deduplicates_dependencies) {
    const LevelDefinition level{.background_path = "unused.png", .background_layers = {{.path = "sky.png"}, {.path = "ground.tscn"}}};
    const UnitConfig unit{
        .muzzle = {.path_template = "muzzle_%03d.png"},
        .shoot_sfx = {.path = "fire.wav"},
        .projectile_attack = ProjectileAttackConfig{.explosion_sfx = ShootSfxConfig{.path = "fire.wav"},
                                                    .projectile_animation = {.path_template = "bullet.png", .frame_count = 1},
                                                    .explosion_animation = {.path_template = "burst_%02d.png", .frame_count = 2}},
        .animations = {{"walk", {.path_template = "walk_%03d.png", .frame_count = 2}}, {"death", {.path_template = "dead_%03d.png", .frame_count = 2}}},
    };
    const std::vector<UnitConfig> units{unit, unit};
    const std::vector<std::string> presentation{"fire.wav", "victory.wav", ""};
    const auto paths = match_resource_paths(level, units, presentation);
    DEFN_CHECK_EQ(paths.size(), std::size_t{21});
    DEFN_CHECK(std::ranges::find(paths, "unused.png") == paths.end());
    for (const auto *required : {"sky.png", "ground.tscn", "walk_001.png", "dead_001.png", "muzzle_009.png", "bullet.png", "burst_01.png", "victory.wav"}) {
        DEFN_CHECK(std::ranges::find(paths, required) != paths.end());
    }
}

DEFN_TEST(animation_resource_paths_support_static_and_numbered_assets) {
    DEFN_CHECK_EQ(animation_frame_path("tower.png", 0), std::string("tower.png"));
    DEFN_CHECK_EQ(animation_frame_path("Walk__%03d.png", 9), std::string("Walk__009.png"));
    DEFN_CHECK_EQ(animation_frame_path("Walk__%d.png", 10), std::string("Walk__10.png"));
}

DEFN_TEST(endless_loading_excludes_drift_types_when_set_pieces_replace_every_reachable_wave) {
    const EndlessSchedule schedule{
        .tuning = {.base_budget = 10, .escalation = 2, .budget_ceiling = 20},
        .drift = {{.wave = 1, .weights = {{"grime", 1}}}},
        .set_pieces = {{.period = 1, .weights = {{"hound", 1}}}},
        .threat_costs = {{"grime", 1}, {"hound", 2}},
    };
    DEFN_CHECK((match_unit_ids({}, {}, &schedule) == std::vector<std::string>{"base", "hound"}));
}
} // namespace defn
