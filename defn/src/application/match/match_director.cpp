#include "match_director.h"

#include "progression_presentation.h"

#include <godot_cpp/variant/packed_string_array.hpp>

namespace defn {

namespace {

std::string to_std_string(const String &value) { return value.utf8().get_data(); }

String to_godot_string(const std::string &value) { return {value.c_str()}; }

std::vector<std::string> to_std_strings(const PackedStringArray &values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto &value : values) {
        result.push_back(to_std_string(String(value)));
    }
    return result;
}

MatchUpgradeOption to_match_upgrade_option(const UpgradeCardViewModel &card) {
    return {
        .id = to_std_string(card.id),
        .name = to_std_string(card.name),
        .description = to_std_string(card.description),
        .emoji = to_std_string(card.emoji),
        .category = to_std_string(card.category),
        .owned_count = card.owned_count,
    };
}

std::vector<MatchUpgradeOption> to_match_upgrade_options(const std::vector<UpgradeCardViewModel> &cards) {
    std::vector<MatchUpgradeOption> result;
    result.reserve(cards.size());
    for (const auto &card : cards) {
        result.push_back(to_match_upgrade_option(card));
    }
    return result;
}

} // namespace

bool MatchDirector::configure(ProgressionService *campaign, const UnitCatalog *unit_catalog, const GridQueryService *grid) {
    campaign_ = campaign;
    unit_catalog_ = unit_catalog;
    grid_ = grid;
    spawn_scheduler_.configure(unit_catalog_, grid_);
    return campaign_ != nullptr && unit_catalog_ != nullptr && grid_ != nullptr;
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
    pending_match_end_.reset();
    deployment_service_.configure(&match_session_, unit_catalog_, campaign_, grid_);
    spawn_scheduler_.start();
}

std::vector<UnitConfig> MatchDirector::build_available_friendlies() const {
    std::vector<UnitConfig> friendlies;
    if (campaign_ == nullptr || unit_catalog_ == nullptr) {
        return friendlies;
    }

    const PackedStringArray unlocked_units = campaign_->get_unlocked_units();
    const auto all_friendlies = unit_catalog_->get_friendly_units();
    friendlies.reserve(all_friendlies.size());
    for (const auto &config : all_friendlies) {
        if (config.name == "base") {
            continue;
        }

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
    update_result.spawn_unit_intents = scheduler_update.spawn_unit_intents;
    update_result.wave_changed = scheduler_update.wave_changed;

    for (const SpawnUnitIntent &intent : update_result.spawn_unit_intents) {
        if (intent.side == MatchUnitSide::Hostile && !intent.unit_id.empty()) {
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

MatchUpdate MatchDirector::handle_deploy_request(const std::string &unit_id) {
    MatchUpdate update_result;
    const DeploymentResult result = deployment_service_.deploy_friendly(unit_id);
    if (result.succeeded && result.spawn_intent.has_value()) {
        update_result.spawn_unit_intents.push_back(*result.spawn_intent);
        update_result.resource_changed = ResourceChanged{.energy = result.remaining_energy};
    }

    return update_result;
}

MatchUpdate MatchDirector::handle_enemy_defeated(const EnemyDefeatedReport &report) {
    if (report.bounty <= 0) {
        return {};
    }

    const int bounty_awarded = match_session_.record_enemy_died(report.bounty);
    MatchUpdate update_result = make_resource_update();
    const int kill_score = match_session_.get_kill_score();
    const int career_score = campaign_ != nullptr ? campaign_->get_total_score() : 0;
    update_result.score_changed = ScoreChanged{.kill_score = kill_score, .total_score = career_score + kill_score, .bounty_awarded = bounty_awarded};
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

const MatchEnded *MatchDirector::get_pending_match_end() const { return pending_match_end_.has_value() ? &*pending_match_end_ : nullptr; }

bool MatchDirector::select_upgrade(const std::string &upgrade_id) {
    if (upgrade_id.empty() || !pending_match_end_.has_value()) {
        return false;
    }

    const MatchUpgradeOption *selected_upgrade = pending_match_end_->reward_options.find_upgrade(upgrade_id);
    if (selected_upgrade == nullptr) {
        return false;
    }

    pending_match_end_->reward_options.selected_upgrade = *selected_upgrade;
    return true;
}

bool MatchDirector::finalize_selected_upgrade() {
    if (!pending_match_end_.has_value()) {
        return true;
    }

    auto &reward = pending_match_end_->reward_options;
    if (reward.available_upgrades.empty()) {
        return true;
    }

    if (campaign_ == nullptr || !reward.selected_upgrade.has_value() || reward.source.empty() || reward.level_id.empty()) {
        return false;
    }

    bool claim_succeeded = false;
    if (reward.source == "first_clear") {
        claim_succeeded = campaign_->claim_level_upgrade(to_godot_string(reward.level_id), to_godot_string(reward.selected_upgrade->id));
    } else if (reward.source == "rescue") {
        claim_succeeded = campaign_->claim_rescue_draft(to_godot_string(reward.level_id), to_godot_string(reward.selected_upgrade->id));
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
    const MatchRewardOptions reward = build_reward_options(victory);
    if (victory) {
        campaign_->mark_level_completed(level_id_, level_score);
    }

    const int new_total = campaign_->get_total_score();
    campaign_->save();

    const PackedStringArray new_unlocks = ProgressionPresentation::describe_new_unlocks(*campaign_, victory, level_id_);
    const String next_level_id = determine_next_level_id(victory);
    pending_match_end_ = MatchEnded{
        .victory = victory,
        .summary_model =
            match_session_.build_end_game_summary(victory, new_total, to_std_string(level_id_), to_std_string(next_level_id), to_std_strings(new_unlocks)),
        .reward_options = reward,
        .owned_upgrades = to_match_upgrade_options(campaign_->build_owned_upgrade_cards()),
    };

    update_result.integrity_changed = IntegrityChanged{.hearts = match_session_.get_base_integrity()};
    update_result.match_ended = pending_match_end_;
    return update_result;
}

MatchUpdate MatchDirector::make_resource_update() const {
    MatchUpdate update_result;
    update_result.resource_changed = ResourceChanged{.energy = match_session_.get_core_resource()};
    return update_result;
}

MatchUpdate MatchDirector::make_hearts_update() const {
    MatchUpdate update_result;
    update_result.integrity_changed = IntegrityChanged{.hearts = match_session_.get_base_integrity()};
    return update_result;
}

MatchRewardOptions MatchDirector::build_reward_options(bool victory) const {
    MatchRewardOptions reward;
    if (campaign_ == nullptr) {
        return reward;
    }

    const String frontier_level_id = campaign_->get_frontier_level_id();
    if (victory && campaign_->can_claim_level_upgrade(level_id_)) {
        reward.available_upgrades = to_match_upgrade_options(campaign_->build_upgrade_draft_for_level(level_id_));
        if (!reward.available_upgrades.empty()) {
            reward.source = "first_clear";
            reward.level_id = to_std_string(level_id_);
        }
    } else if (!victory && !frontier_level_id.is_empty() && campaign_->can_claim_rescue_draft(frontier_level_id)) {
        reward.available_upgrades = to_match_upgrade_options(campaign_->build_rescue_draft_for_level(frontier_level_id));
        if (!reward.available_upgrades.empty()) {
            reward.source = "rescue";
            reward.level_id = to_std_string(frontier_level_id);
        }
    }

    if (!reward.source.empty() && !reward.level_id.empty()) {
        reward.title = to_std_string(ProgressionPresentation::format_reward_title(to_godot_string(reward.source), to_godot_string(reward.level_id)));
        reward.subtitle = to_std_string(ProgressionPresentation::format_reward_subtitle(to_godot_string(reward.source), to_godot_string(reward.level_id)));
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