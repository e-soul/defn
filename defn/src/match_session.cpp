#include "match_session.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace defn {

void MatchSession::start(const MatchConfig &config) {
    config_ = config;
    const int base_max_health = std::max(config.initial_integrity, 0) * BASE_HEALTH_PER_HEART;
    state_ = MatchRuntimeState{
        .core_resource = config.starting_core_resource,
        .base_health = base_max_health,
        .base_max_health = base_max_health,
        .initial_integrity = config.initial_integrity,
    };
}

bool MatchSession::finish_game() {
    if (state_.game_over) {
        return false;
    }

    state_.game_over = true;
    return true;
}

bool MatchSession::can_spend_energy(int amount) const { return !state_.game_over && amount >= 0 && amount <= state_.core_resource; }

void MatchSession::spend_energy(int amount) {
    if (amount <= 0) {
        return;
    }

    state_.core_resource = std::max(state_.core_resource - amount, 0);
}

void MatchSession::tick_energy() { state_.core_resource += config_.energy_regen_rate; }

void MatchSession::set_base_health(int current_health) { state_.base_health = std::clamp(current_health, 0, state_.base_max_health); }

void MatchSession::record_enemy_spawned() { ++state_.living_enemies; }

void MatchSession::record_enemy_died(int base_bounty) {
    state_.kill_score += base_bounty;
    ++state_.enemies_killed;

    const auto scaled_bounty = static_cast<double>(base_bounty) * static_cast<double>(config_.bounty_multiplier);
    state_.core_resource += static_cast<int>(std::ceil(scaled_bounty));
    state_.living_enemies = std::max(state_.living_enemies - 1, 0);
}

int MatchSession::get_base_integrity() const { return calculate_hearts_from_health(state_.base_health); }

bool MatchSession::should_end_with_victory() const { return !state_.game_over && state_.base_health > 0 && state_.all_spawned && state_.living_enemies <= 0; }

int MatchSession::calculate_integrity_bonus() const { return get_base_integrity() * 50; }

int MatchSession::calculate_completion_bonus(bool victory) { return victory ? 100 : 0; }

int MatchSession::calculate_level_score(bool victory) const { return state_.kill_score + calculate_integrity_bonus() + calculate_completion_bonus(victory); }

Dictionary MatchSession::build_end_game_stats(bool victory, int new_total_score, const String &current_level_id, const String &next_level_id,
                                              const PackedStringArray &new_unlocks, const Array &available_upgrades, const Dictionary &selected_upgrade,
                                              const Dictionary &reward_context) const {
    Dictionary stats;
    stats["victory"] = victory;
    stats["enemies_killed"] = state_.enemies_killed;
    stats["kill_score"] = state_.kill_score;
    stats["hearts_remaining"] = get_base_integrity();
    stats["hearts_total"] = state_.initial_integrity;
    stats["integrity_bonus"] = calculate_integrity_bonus();
    stats["completion_bonus"] = calculate_completion_bonus(victory);
    stats["level_score"] = calculate_level_score(victory);
    stats["new_total_score"] = new_total_score;
    stats["current_level_id"] = current_level_id;
    stats["next_level_id"] = next_level_id;

    Array unlocks_array;
    for (const auto &new_unlock : new_unlocks) {
        unlocks_array.push_back(new_unlock);
    }
    stats["new_unlocks"] = unlocks_array;
    stats["available_upgrades"] = available_upgrades;
    stats["selected_upgrade"] = selected_upgrade;

    const Array reward_keys = reward_context.keys();
    for (const Variant &key_variant : reward_keys) {
        const Variant value = reward_context.get(key_variant, Variant());
        stats[key_variant] = value;
    }

    return stats;
}

int MatchSession::calculate_hearts_from_health(int health) {
    if (health <= 0) {
        return 0;
    }

    return (health + BASE_HEALTH_PER_HEART - 1) / BASE_HEALTH_PER_HEART;
}

} // namespace defn