#include "test_harness.h"

#include "deployment_service.h"
#include "match_director.h"
#include "match_session.h"
#include "runtime_service_interfaces.h"
#include "spawn_scheduler.h"
#include "unit_data.h"

#include <algorithm>
#include <utility>

namespace defn {

namespace {

Array make_array(std::initializer_list<Variant> values) {
    Array array;
    for (const Variant &value : values) {
        array.push_back(value);
    }
    return array;
}

Dictionary make_global_data() {
    Dictionary global_data;

    Dictionary gameplay_rules;
    gameplay_rules["viewport_width"] = 1280;
    gameplay_rules["viewport_height"] = 720;
    gameplay_rules["world_multiplier"] = 3;
    gameplay_rules["belt_top_y"] = 180.0;
    gameplay_rules["belt_bottom_y"] = 420.0;
    gameplay_rules["breach_x"] = 96.0;
    gameplay_rules["spawn_offset"] = 64.0;
    gameplay_rules["friendly_world_margin"] = 128.0;
    global_data["gameplay_rules"] = gameplay_rules;

    return global_data;
}

Dictionary make_unit_data() {
    Dictionary units_root;
    Dictionary units;

    Dictionary friendly;
    friendly["side"] = "friendly";
    friendly["hp"] = 120;
    friendly["cost"] = 25;
    friendly["ranged_damage"] = 14;
    friendly["move_speed_pixels_per_second"] = 72.0;
    units["operator"] = friendly;

    Dictionary hostile;
    hostile["side"] = "hostile";
    hostile["hp"] = 75;
    hostile["bounty"] = 6;
    units["jackal"] = hostile;

    units_root["units"] = units;
    return units_root;
}

bool contains_string(const std::vector<String> &values, const String &candidate) {
    return std::ranges::any_of(values, [&candidate](const String &value) { return value == candidate; });
}

UnitDataLoader make_unit_loader() {
    UnitDataLoader loader;
    const bool loaded = loader.load_from_data(make_unit_data(), make_global_data());
    DEFN_REQUIRE(loaded);
    return loader;
}

class FakeGridService final : public GridQueryService {
  public:
    real_t deploy_x_value = 64.0F;
    real_t spawn_x_value = 896.0F;
    real_t belt_y_value = 240.0F;

    [[nodiscard]] real_t deploy_x() const override { return deploy_x_value; }
    [[nodiscard]] real_t spawn_x() const override { return spawn_x_value; }
    [[nodiscard]] real_t sample_belt_y() const override { return belt_y_value; }
};

class FakeProgressionService final : public ProgressionService {
  public:
    int total_score = 0;
    PackedStringArray unlocked_units;
    std::vector<LevelUnlock> level_unlocks;
    std::vector<String> completed_levels;
    std::vector<std::pair<String, int>> best_scores;
    String frontier_level_id = "level_01";
    String current_level_id = "level_01";
    int starting_energy_bonus = 0;
    int energy_regen = 1;
    real_t bounty_multiplier = 1.0F;
    int base_integrity_bonus = 0;
    bool allow_level_upgrade = true;
    bool allow_rescue_draft = false;
    bool save_called = false;
    bool level_claimed = false;
    bool rescue_claimed = false;
    std::vector<UpgradeCardViewModel> level_draft;
    std::vector<UpgradeCardViewModel> rescue_draft;
    std::vector<UpgradeCardViewModel> owned_upgrades;

    [[nodiscard]] int get_total_score() const override { return total_score; }
    [[nodiscard]] PackedStringArray get_unlocked_units() const override { return unlocked_units; }
    [[nodiscard]] bool is_level_completed(const String &level_id) const override { return contains_string(completed_levels, level_id); }
    [[nodiscard]] bool is_level_unlocked(const String &level_id) const override {
        for (const auto &unlock : level_unlocks) {
            if (unlock.level_id != level_id) {
                continue;
            }
            return unlock.requires_completed.is_empty() || is_level_completed(unlock.requires_completed);
        }
        return false;
    }
    [[nodiscard]] bool can_claim_level_upgrade(const String & /*level_id*/) const override { return allow_level_upgrade; }
    [[nodiscard]] bool can_claim_rescue_draft(const String & /*level_id*/) const override { return allow_rescue_draft; }
    [[nodiscard]] String get_frontier_level_id() const override { return frontier_level_id; }
    [[nodiscard]] int get_highest_level_score(const String &level_id) const override {
        for (const auto &[candidate, score] : best_scores) {
            if (candidate == level_id) {
                return score;
            }
        }
        return 0;
    }
    [[nodiscard]] UnitConfig get_effective_friendly_unit_config(const UnitConfig &base_config) const override { return base_config; }
    [[nodiscard]] int get_effective_starting_energy(int base) const override { return base + starting_energy_bonus; }
    [[nodiscard]] int get_effective_energy_regen() const override { return energy_regen; }
    [[nodiscard]] real_t get_effective_bounty_multiplier() const override { return bounty_multiplier; }
    [[nodiscard]] int get_effective_base_integrity(int base) const override { return base + base_integrity_bonus; }
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_upgrade_draft_for_level(const String & /*level_id*/) const override { return level_draft; }
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_rescue_draft_for_level(const String & /*level_id*/) const override { return rescue_draft; }
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_owned_upgrade_cards() const override { return owned_upgrades; }
    [[nodiscard]] String get_current_level_id() const override { return current_level_id; }
    [[nodiscard]] const std::vector<LevelUnlock> &get_level_unlock_data() const override { return level_unlocks; }

    void add_score(int amount) override { total_score += amount; }
    void mark_level_completed(const String &level_id, int level_score) override {
        if (!contains_string(completed_levels, level_id)) {
            completed_levels.push_back(level_id);
        }

        for (auto &[candidate, score] : best_scores) {
            if (candidate == level_id) {
                score = std::max(score, level_score);
                return;
            }
        }
        best_scores.emplace_back(level_id, level_score);
    }
    bool claim_level_upgrade(const String & /*level_id*/, const String & /*upgrade_id*/) override {
        level_claimed = true;
        return true;
    }
    bool claim_rescue_draft(const String & /*level_id*/, const String & /*upgrade_id*/) override {
        rescue_claimed = true;
        return true;
    }
    void save() override { save_called = true; }
};

LevelDefinition make_empty_level() {
    LevelDefinition level;
    level.level_id = 1;
    level.name = "Test Level";
    level.starting_core_resource = 100;
    level.base_integrity = 3;
    return level;
}

} // namespace

DEFN_TEST(match_session_calculates_victory_score_and_summary) {
    MatchSession session;
    session.start({
        .starting_core_resource = 50,
        .initial_integrity = 3,
        .bounty_multiplier = 1.5F,
        .energy_regen_rate = 2,
    });
    session.record_enemy_spawned();
    session.record_enemy_died(12);
    session.mark_all_spawns_complete();
    session.set_base_health(200);

    DEFN_CHECK(session.should_end_with_victory());
    DEFN_CHECK_EQ(session.get_core_resource(), 68);
    DEFN_CHECK_EQ(session.calculate_level_score(true), 212);

    const ScoreScreenModel summary = session.build_end_game_summary(true, 400, "level_01", "level_02", PackedStringArray(), {});
    DEFN_CHECK(summary.victory);
    DEFN_CHECK_EQ(summary.level_score, 212);
}

DEFN_TEST(deployment_service_returns_spawn_request_and_spends_energy) {
    UnitDataLoader unit_loader = make_unit_loader();
    FakeProgressionService progression;
    FakeGridService grid;
    MatchSession session;
    session.start({.starting_core_resource = 40, .initial_integrity = 3, .bounty_multiplier = 1.0F, .energy_regen_rate = 1});

    DeploymentService service;
    service.configure(&session, &unit_loader, &progression, &grid);

    const DeploymentResult result = service.deploy_friendly("operator");
    DEFN_REQUIRE(result.succeeded);
    DEFN_REQUIRE(result.spawn_request.has_value());
    DEFN_CHECK_EQ(result.unit_cost, 25);
    DEFN_CHECK_EQ(result.remaining_energy, 15);
    DEFN_CHECK_EQ(result.spawn_request->config.name, String("operator"));
    DEFN_CHECK_CLOSE(result.spawn_request->position.x, 64.0, 0.001);
    DEFN_CHECK_CLOSE(result.spawn_request->position.y, 240.0, 0.001);
}

DEFN_TEST(spawn_scheduler_returns_ordered_spawn_requests_and_wave_changes) {
    UnitDataLoader unit_loader = make_unit_loader();
    FakeGridService grid;

    SpawnScheduler scheduler;
    scheduler.configure(&unit_loader, &grid);

    LevelDefinition level = make_empty_level();
    level.waves.push_back({.wave_number = 1, .spawns = {{.time = 0.5, .type = "jackal"}}});
    level.waves.push_back({.wave_number = 2, .spawns = {{.time = 1.5, .type = "jackal"}}});
    scheduler.load_level_definition(level);
    scheduler.start();

    const SpawnSchedulerUpdate first_update = scheduler.update(0.75);
    DEFN_CHECK_EQ(first_update.spawn_requests.size(), static_cast<size_t>(1));
    DEFN_REQUIRE(first_update.wave_changed.has_value());
    DEFN_CHECK_EQ(*first_update.wave_changed, 1);

    const SpawnSchedulerUpdate second_update = scheduler.update(1.0);
    DEFN_CHECK_EQ(second_update.spawn_requests.size(), static_cast<size_t>(1));
    DEFN_REQUIRE(second_update.wave_changed.has_value());
    DEFN_CHECK_EQ(*second_update.wave_changed, 2);
    DEFN_CHECK(second_update.all_spawns_completed);
}

DEFN_TEST(match_director_victory_produces_reward_and_next_level) {
    UnitDataLoader unit_loader = make_unit_loader();
    FakeGridService grid;
    FakeProgressionService progression;
    progression.current_level_id = "level_01";
    progression.unlocked_units.push_back("operator");
    progression.level_unlocks = {{.level_id = "level_01"}, {.level_id = "level_02", .requires_completed = "level_01"}};
    progression.level_draft.push_back({.id = "upgrade_a", .name = "Upgrade A"});

    MatchDirector director;
    DEFN_CHECK(director.configure(&progression, &unit_loader, &grid));
    director.load_level_definition(make_empty_level(), "level_01");
    director.begin_match();

    const MatchUpdate update = director.update(0.0);
    DEFN_CHECK(update.match_finished);
    DEFN_REQUIRE(update.score_screen.has_value());
    DEFN_CHECK(update.score_screen->victory);
    DEFN_CHECK_EQ(update.score_screen->reward.source, String("first_clear"));
    DEFN_CHECK_EQ(update.score_screen->reward.available_upgrades.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(update.score_screen->next_level_id, String("level_02"));
    DEFN_CHECK(progression.save_called);
    DEFN_CHECK(contains_string(progression.completed_levels, String("level_01")));
}

DEFN_TEST(match_director_defeat_produces_rescue_reward) {
    UnitDataLoader unit_loader = make_unit_loader();
    FakeGridService grid;
    FakeProgressionService progression;
    progression.current_level_id = "level_01";
    progression.frontier_level_id = "level_02";
    progression.allow_level_upgrade = false;
    progression.allow_rescue_draft = true;
    progression.rescue_draft.push_back({.id = "rescue_a", .name = "Rescue A"});

    MatchDirector director;
    DEFN_CHECK(director.configure(&progression, &unit_loader, &grid));
    director.load_level_definition(make_empty_level(), "level_01");
    director.begin_match();

    const MatchUpdate update = director.handle_base_destroyed();
    DEFN_CHECK(update.match_finished);
    DEFN_REQUIRE(update.score_screen.has_value());
    DEFN_CHECK(!update.score_screen->victory);
    DEFN_CHECK_EQ(update.score_screen->reward.source, String("rescue"));
    DEFN_CHECK_EQ(update.score_screen->reward.level_id, String("level_02"));
}

} // namespace defn