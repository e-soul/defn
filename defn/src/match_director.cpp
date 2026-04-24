#include "match_director.h"

#include "progression_presentation.h"
#include "unit.h"

namespace defn {

bool MatchDirector::configure(ProgressionService *campaign, const UnitDataLoader *unit_data, const GridQueryService *grid) {
    campaign_ = campaign;
    unit_data_ = unit_data;
    grid_ = grid;
    spawn_scheduler_.configure(unit_data_, grid_);
    return campaign_ != nullptr && unit_data_ != nullptr && grid_ != nullptr;
}

bool MatchDirector::load_level(const String &path) {
    if (campaign_ == nullptr || !spawn_scheduler_.load_level(path)) {
        return false;
    }

    level_id_ = campaign_->get_current_level_id();
    return true;
}

void MatchDirector::load_level_definition(const LevelDefinition &level_definition, const String &level_id) {
    spawn_scheduler_.load_level_definition(level_definition);
    level_id_ = level_id;
}

void MatchDirector::begin_match() {
    if (campaign_ == nullptr) {
        return;
    }

    const MatchConfig match_config{
        .starting_core_resource = campaign_->get_effective_starting_energy(spawn_scheduler_.get_starting_core_resource()),
        .initial_integrity = campaign_->get_effective_base_integrity(spawn_scheduler_.get_base_integrity()),
        .bounty_multiplier = campaign_->get_effective_bounty_multiplier(),
        .energy_regen_rate = campaign_->get_effective_energy_regen(),
    };

    match_session_.start(match_config);
    pending_score_screen_.reset();
    deployment_service_.configure(&match_session_, unit_data_, campaign_, grid_);
    spawn_scheduler_.start();
}

std::vector<UnitConfig> MatchDirector::build_available_friendlies() const {
    std::vector<UnitConfig> friendlies;
    if (campaign_ == nullptr || unit_data_ == nullptr) {
        return friendlies;
    }

    const PackedStringArray unlocked_units = campaign_->get_unlocked_units();
    const auto all_friendlies = unit_data_->get_friendly_units();
    friendlies.reserve(all_friendlies.size());
    for (const auto &config : all_friendlies) {
        if (unlocked_units.has(config.name)) {
            friendlies.push_back(campaign_->get_effective_friendly_unit_config(config));
        }
    }

    return friendlies;
}

MatchUpdate MatchDirector::update(double delta) {
    MatchUpdate update_result;
    if (match_session_.is_game_over()) {
        return update_result;
    }

    const SpawnSchedulerUpdate scheduler_update = spawn_scheduler_.update(delta);
    update_result.enemy_spawn_requests = scheduler_update.spawn_requests;
    update_result.wave_changed = scheduler_update.wave_changed;

    for (const UnitSpawnRequest &enemy : update_result.enemy_spawn_requests) {
        if (!enemy.config.name.is_empty()) {
            match_session_.record_enemy_spawned();
        }
    }

    if (scheduler_update.all_spawns_completed) {
        match_session_.mark_all_spawns_complete();
    }

    if (match_session_.should_end_with_victory()) {
        return finish_match(true);
    }

    return update_result;
}

MatchUpdate MatchDirector::handle_deploy_request(const String &unit_type) {
    MatchUpdate update_result;
    const DeploymentResult result = deployment_service_.deploy_friendly(unit_type);
    if (result.succeeded && result.spawn_request.has_value()) {
        update_result.friendly_spawn_requests.push_back(*result.spawn_request);
        update_result.resources_changed = true;
        update_result.core_resource = result.remaining_energy;
    }

    return update_result;
}

MatchUpdate MatchDirector::handle_enemy_died(Unit *unit) {
    if (unit == nullptr || unit->is_queued_for_deletion()) {
        return {};
    }

    match_session_.record_enemy_died(unit->get_bounty());
    MatchUpdate update_result = make_resource_update();
    update_result.score_changed = true;
    update_result.score = match_session_.get_kill_score();
    return update_result;
}

MatchUpdate MatchDirector::handle_base_durability_changed(int current_hp) {
    match_session_.set_base_health(current_hp);
    return make_hearts_update();
}

MatchUpdate MatchDirector::handle_base_destroyed() {
    match_session_.set_base_health(0);
    return finish_match(false);
}

MatchUpdate MatchDirector::handle_core_resource_tick() {
    match_session_.tick_energy();
    return make_resource_update();
}

const ScoreScreenModel *MatchDirector::get_pending_score_screen() const { return pending_score_screen_.has_value() ? &*pending_score_screen_ : nullptr; }

bool MatchDirector::select_upgrade(const String &upgrade_id) {
    if (upgrade_id.is_empty() || !pending_score_screen_.has_value()) {
        return false;
    }

    const UpgradeCardViewModel *selected_upgrade = pending_score_screen_->reward.find_upgrade(upgrade_id);
    if (selected_upgrade == nullptr) {
        return false;
    }

    pending_score_screen_->reward.selected_upgrade = *selected_upgrade;
    return true;
}

bool MatchDirector::finalize_selected_upgrade() {
    if (!pending_score_screen_.has_value()) {
        return true;
    }

    auto &reward = pending_score_screen_->reward;
    if (reward.available_upgrades.empty()) {
        return true;
    }

    if (campaign_ == nullptr || !reward.selected_upgrade.has_value() || reward.source.is_empty() || reward.level_id.is_empty()) {
        return false;
    }

    bool claim_succeeded = false;
    if (reward.source == "first_clear") {
        claim_succeeded = campaign_->claim_level_upgrade(reward.level_id, reward.selected_upgrade->id);
    } else if (reward.source == "rescue") {
        claim_succeeded = campaign_->claim_rescue_draft(reward.level_id, reward.selected_upgrade->id);
    }

    if (!claim_succeeded) {
        return false;
    }

    campaign_->save();
    return true;
}

MatchUpdate MatchDirector::finish_match(bool victory) {
    MatchUpdate update_result;
    if (!match_session_.finish_game() || campaign_ == nullptr) {
        return update_result;
    }

    spawn_scheduler_.stop();
    const int level_score = match_session_.calculate_level_score(victory);

    campaign_->add_score(level_score);
    const ScoreScreenRewardModel reward = build_reward_model(victory);
    if (victory) {
        campaign_->mark_level_completed(level_id_, level_score);
    }

    const int new_total = campaign_->get_total_score();
    campaign_->save();

    const PackedStringArray new_unlocks = ProgressionPresentation::describe_new_unlocks(*campaign_, victory, level_id_);
    const String next_level_id = determine_next_level_id(victory);
    pending_score_screen_ = match_session_.build_end_game_summary(victory, new_total, level_id_, next_level_id, new_unlocks, reward);

    update_result.match_finished = true;
    update_result.hearts_changed = true;
    update_result.hearts = match_session_.get_base_integrity();
    update_result.score_screen = pending_score_screen_;
    return update_result;
}

MatchUpdate MatchDirector::make_resource_update() const {
    MatchUpdate update_result;
    update_result.resources_changed = true;
    update_result.core_resource = match_session_.get_core_resource();
    return update_result;
}

MatchUpdate MatchDirector::make_hearts_update() const {
    MatchUpdate update_result;
    update_result.hearts_changed = true;
    update_result.hearts = match_session_.get_base_integrity();
    return update_result;
}

ScoreScreenRewardModel MatchDirector::build_reward_model(bool victory) const {
    ScoreScreenRewardModel reward;
    if (campaign_ == nullptr) {
        return reward;
    }

    const String frontier_level_id = campaign_->get_frontier_level_id();
    if (victory && campaign_->can_claim_level_upgrade(level_id_)) {
        reward.available_upgrades = campaign_->build_upgrade_draft_for_level(level_id_);
        if (!reward.available_upgrades.empty()) {
            reward.source = "first_clear";
            reward.level_id = level_id_;
        }
    } else if (!victory && !frontier_level_id.is_empty() && campaign_->can_claim_rescue_draft(frontier_level_id)) {
        reward.available_upgrades = campaign_->build_rescue_draft_for_level(frontier_level_id);
        if (!reward.available_upgrades.empty()) {
            reward.source = "rescue";
            reward.level_id = frontier_level_id;
        }
    }

    if (!reward.source.is_empty() && !reward.level_id.is_empty()) {
        reward.title = ProgressionPresentation::format_reward_title(reward.source, reward.level_id);
        reward.subtitle = ProgressionPresentation::format_reward_subtitle(reward.source, reward.level_id);
    }

    return reward;
}

String MatchDirector::determine_next_level_id(bool victory) const {
    if (!victory || campaign_ == nullptr) {
        return {};
    }

    const auto &level_data = campaign_->get_level_unlock_data();
    for (size_t index = 0; index < level_data.size(); ++index) {
        if (level_data[index].level_id == level_id_ && index + 1 < level_data.size()) {
            const String candidate = level_data[index + 1].level_id;
            return campaign_->is_level_unlocked(candidate) ? candidate : String();
        }
    }

    return {};
}

} // namespace defn