#include "match_session.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace defn {

void MatchSession::start(const MatchConfig &config) {
    config_ = config;
    state_ = MatchRuntimeState{
        .core_resource = config.starting_core_resource,
        .base_integrity = config.initial_integrity,
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

void MatchSession::record_enemy_spawned() { ++state_.living_enemies; }

void MatchSession::record_enemy_died(int base_bounty) {
    state_.kill_score += base_bounty;
    ++state_.enemies_killed;

    const auto scaled_bounty = static_cast<double>(base_bounty) * static_cast<double>(config_.bounty_multiplier);
    state_.core_resource += static_cast<int>(std::ceil(scaled_bounty));
    state_.living_enemies = std::max(state_.living_enemies - 1, 0);
}

bool MatchSession::record_enemy_breached() {
    state_.base_integrity = std::max(state_.base_integrity - 1, 0);
    state_.living_enemies = std::max(state_.living_enemies - 1, 0);
    return state_.base_integrity <= 0;
}

bool MatchSession::should_end_with_victory() const { return !state_.game_over && state_.all_spawned && state_.living_enemies <= 0; }

int MatchSession::calculate_integrity_bonus() const { return state_.base_integrity * 50; }

int MatchSession::calculate_completion_bonus(bool victory) { return victory ? 100 : 0; }

int MatchSession::calculate_level_score(bool victory) const { return state_.kill_score + calculate_integrity_bonus() + calculate_completion_bonus(victory); }

Dictionary MatchSession::build_end_game_stats(bool victory, int new_total_score, const String &current_level_id, const String &next_level_id,
                                              const PackedStringArray &new_unlocks) const {
    Dictionary stats;
    stats["victory"] = victory;
    stats["enemies_killed"] = state_.enemies_killed;
    stats["kill_score"] = state_.kill_score;
    stats["hearts_remaining"] = state_.base_integrity;
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

    return stats;
}

} // namespace defn