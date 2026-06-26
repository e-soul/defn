#ifndef MATCH_DIRECTOR_H
#define MATCH_DIRECTOR_H

#include "deployment_service.h"
#include "match_session.h"
#include "progression_service.h"
#include "runtime_service_interfaces.h"
#include "score_screen_models.h"
#include "spawn_scheduler.h"
#include "unit_data.h"
#include "unit_spawn_request.h"

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <vector>

namespace defn {

using namespace godot;

class Unit;

struct MatchUpdate {
    std::vector<UnitSpawnRequest> friendly_spawn_requests;
    std::vector<UnitSpawnRequest> enemy_spawn_requests;
    std::optional<int> wave_changed;
    bool resources_changed = false;
    bool hearts_changed = false;
    bool score_changed = false;
    bool match_finished = false;
    int core_resource = 0;
    int hearts = 0;
    int score = 0;
    int bounty_awarded = 0;
    std::optional<ScoreScreenModel> score_screen;
};

class MatchDirector {
  public:
    bool configure(ProgressionService *campaign, const UnitDataLoader *unit_data, const GridQueryService *grid);
    bool load_level(const String &path);
    void load_level_definition(const LevelDefinition &level_definition, const String &level_id);
    void begin_match();
    MatchUpdate update(double delta);
    MatchUpdate handle_deploy_request(const String &unit_type);
    MatchUpdate handle_enemy_died(Unit *unit);
    MatchUpdate handle_base_durability_changed(int current_hp);
    MatchUpdate handle_base_destroyed();
    MatchUpdate handle_core_resource_tick();

    bool is_game_over() const { return match_session_.is_game_over(); }
    int get_core_resource() const { return match_session_.get_core_resource(); }
    int get_base_integrity() const { return match_session_.get_base_integrity(); }
    int get_base_max_health() const { return match_session_.get_base_max_health(); }
    int get_level_number() const { return spawn_scheduler_.get_level_number(); }
    String get_level_name() const { return spawn_scheduler_.get_level_name(); }
    int get_total_waves() const { return spawn_scheduler_.get_total_waves(); }
    String get_background_path() const { return spawn_scheduler_.get_background_path(); }
    std::vector<UnitConfig> build_available_friendlies() const;

    const ScoreScreenModel *get_pending_score_screen() const;
    bool select_upgrade(const String &upgrade_id);
    bool finalize_selected_upgrade();
    void clear_pending_score_screen() { pending_score_screen_.reset(); }

  private:
    MatchUpdate finish_match(bool victory);
    MatchUpdate make_resource_update() const;
    MatchUpdate make_hearts_update() const;
    ScoreScreenRewardModel build_reward_model(bool victory) const;
    String determine_next_level_id(bool victory) const;

    ProgressionService *campaign_ = nullptr;
    const UnitDataLoader *unit_data_ = nullptr;
    const GridQueryService *grid_ = nullptr;
    String level_id_;
    MatchSession match_session_;
    DeploymentService deployment_service_;
    SpawnScheduler spawn_scheduler_;
    std::optional<ScoreScreenModel> pending_score_screen_;
};

} // namespace defn

#endif