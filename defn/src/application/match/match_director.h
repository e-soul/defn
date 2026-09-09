// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MATCH_DIRECTOR_H
#define MATCH_DIRECTOR_H

#include "deployment_service.h"
#include "match_outputs.h"
#include "match_session.h"
#include "progression_service.h"
#include "random_source.h"
#include "runtime_service_interfaces.h"
#include "spawn_scheduler.h"
#include "unit_definition.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

struct EnemyDefeatedReport {
    int bounty = 0;
};

class MatchDirector {
  public:
    bool configure(ProgressionService *campaign, const UnitCatalog *unit_catalog, const GridQueryService *grid, RandomSource *random = nullptr);
    void load_level_definition(const LevelDefinition &level_definition, std::string level_id);

    // Extends the running spawn timeline. A coordinator that generates its own content stays ahead of the cursor
    // with this rather than the director learning what that content is.
    void append_wave(const WaveDefinition &wave_definition);

    // Scales what a kill pays without touching what it scores.
    void set_bounty_scale(double scale);

    // The standing line the player is allowed to hold from here on.
    void set_supply_cap(int cap);
    void award_survival_bonus(int points);

    void begin_match();
    MatchUpdate update(double delta);
    MatchUpdate handle_deploy_request(const std::string &unit_id);
    MatchUpdate handle_enemy_defeated(const EnemyDefeatedReport &report);

    // A friendly left the field. Only the supply count moves: a friendly pays no bounty and scores nothing, and the
    // count is what decides whether the player may deploy again.
    MatchUpdate handle_friendly_defeated();
    MatchUpdate handle_base_durability_changed(int current_hp);
    MatchUpdate handle_base_destroyed();

    // Closes an idle gap in the spawn timeline, bringing the next spawn to `lead` seconds away, and returns how much
    // time was skipped. Meaningful without endless mode existing: it is the seam a mode that paces itself by what is
    // standing on the belt reaches for, and the director keeps no opinion about when that is warranted.
    double pull_next_spawn_forward(double lead) { return spawn_scheduler_.pull_next_spawn_forward(lead); }

    // Ends the match as a defeat with the base still standing, for a run that stops for a reason other than being
    // overrun.
    MatchUpdate concede_match();
    MatchUpdate handle_core_resource_tick();

    bool is_game_over() const { return match_session_.is_game_over(); }
    int get_core_resource() const { return match_session_.get_core_resource(); }
    int get_living_friendlies() const { return match_session_.get_living_friendlies(); }
    int get_living_enemies() const { return match_session_.get_living_enemies(); }
    bool has_supply_room() const { return match_session_.has_supply_room(); }
    int get_supply_cap() const { return match_session_.get_supply_cap(); }
    int get_energy_cap() const { return match_session_.get_energy_cap(); }
    bool is_energy_ceiling_engaged() const { return match_session_.is_energy_ceiling_engaged(); }
    int get_base_health() const { return match_session_.get_base_health(); }
    int get_base_max_health() const { return match_session_.get_base_max_health(); }
    const std::string &get_level_name() const { return spawn_scheduler_.get_level_name(); }
    int get_total_waves() const { return spawn_scheduler_.get_total_waves(); }
    Vector2 get_base_position_ratio() const { return spawn_scheduler_.get_base_position_ratio(); }
    const std::string &get_background_path() const { return spawn_scheduler_.get_background_path(); }
    const std::vector<BackgroundLayer> &get_background_layers() const { return spawn_scheduler_.get_background_layers(); }
    std::vector<UnitConfig> build_available_friendlies() const;

    const MatchEnded *get_pending_match_end() const;
    bool select_upgrade(const std::string &upgrade_id);
    bool finalize_selected_upgrade();
    void clear_pending_match_end() { pending_match_end_.reset(); }

  private:
    MatchUpdate finish_match(bool victory);
    MatchUpdate make_resource_update() const;
    MatchUpdate make_integrity_update() const;
    IntegrityChanged make_integrity_reading() const;
    MatchRewardOptions build_reward_options(const ProgressionRewardDraft &draft) const;

    ProgressionService *campaign_ = nullptr;
    const UnitCatalog *unit_catalog_ = nullptr;
    const GridQueryService *grid_ = nullptr;
    std::string level_id_;
    MatchSession match_session_;
    DeploymentService deployment_service_;
    SpawnScheduler spawn_scheduler_;
    StdRandomSource default_random_;
    RandomSource *random_ = &default_random_;
    std::optional<MatchEnded> pending_match_end_;
};

} // namespace defn

#endif
