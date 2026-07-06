#ifndef MATCH_DIRECTOR_H
#define MATCH_DIRECTOR_H

#include "deployment_service.h"
#include "match_outputs.h"
#include "match_session.h"
#include "progression_service.h"
#include "runtime_service_interfaces.h"
#include "spawn_scheduler.h"
#include "unit_data.h"

#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <string>
#include <vector>

namespace defn {

using namespace godot;

struct EnemyDefeatedReport {
    int bounty = 0;
};

class MatchDirector {
  public:
    bool configure(ProgressionService *campaign, const UnitCatalog *unit_catalog, const GridQueryService *grid);
    void load_level_definition(const LevelDefinition &level_definition, const String &level_id);
    void begin_match();
    MatchUpdate update(double delta);
    MatchUpdate handle_deploy_request(const std::string &unit_id);
    MatchUpdate handle_enemy_defeated(const EnemyDefeatedReport &report);
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

    const MatchEnded *get_pending_match_end() const;
    bool select_upgrade(const std::string &upgrade_id);
    bool finalize_selected_upgrade();
    void clear_pending_match_end() { pending_match_end_.reset(); }

  private:
    MatchUpdate finish_match(bool victory);
    MatchUpdate make_resource_update() const;
    MatchUpdate make_hearts_update() const;
    MatchRewardOptions build_reward_options(bool victory) const;
    String determine_next_level_id(bool victory) const;

    ProgressionService *campaign_ = nullptr;
    const UnitCatalog *unit_catalog_ = nullptr;
    const GridQueryService *grid_ = nullptr;
    String level_id_;
    MatchSession match_session_;
    DeploymentService deployment_service_;
    SpawnScheduler spawn_scheduler_;
    std::optional<MatchEnded> pending_match_end_;
};

} // namespace defn

#endif