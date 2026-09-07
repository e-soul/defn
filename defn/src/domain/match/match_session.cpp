// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "match_session.h"

#include <algorithm>
#include <cmath>

namespace defn {

void MatchSession::start(const MatchConfig &config) {
    config_ = config;
    base_bounty_multiplier_ = config.bounty_multiplier;
    supply_cap_ceiling_ = config.supply_cap;
    const int base_max_health = std::max(config.initial_integrity, 0) * BASE_HEALTH_PER_HEART;
    state_ = MatchRuntimeState{
        .core_resource = config.starting_core_resource,
        .base_health = base_max_health,
        .base_max_health = base_max_health,
        .initial_integrity = config.initial_integrity,
    };
    // A level whose starting grant is already at or below its cap is capped from the first tick; one that hands out
    // more than the cap lets the player keep the overflow until they spend it.
    apply_energy_ceiling();
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
    // Spending is the ordinary way the reserve first reaches the cap, so the latch is checked here too.
    apply_energy_ceiling();
}

void MatchSession::tick_energy() { credit_energy(config_.energy_regen_rate); }

int MatchSession::credit_energy(int amount) {
    if (amount <= 0) {
        return 0;
    }

    const int before = state_.core_resource;
    state_.core_resource += amount;
    apply_energy_ceiling();
    return state_.core_resource - before;
}

void MatchSession::apply_energy_ceiling() {
    if (config_.energy_cap <= 0) {
        return;
    }

    // A ratchet rather than a clamp. The cap does nothing while the player is still above it, so a level may open
    // with a grant larger than the cap and that grant is kept until it is spent; the first time the reserve falls
    // to the cap the ceiling latches, and from then on it is a hard ceiling on regeneration and bounty alike.
    //
    // The point of the rule is that income stops compounding once the player is living inside it. A reserve that
    // can only ever be refilled to the same number turns every kill into "spend it or lose it", which is what stops
    // a long run banking a wave's worth of energy against the wave after it.
    if (state_.core_resource <= config_.energy_cap) {
        state_.energy_ceiling_engaged = true;
    }
    if (state_.energy_ceiling_engaged) {
        state_.core_resource = std::min(state_.core_resource, config_.energy_cap);
    }
}

bool MatchSession::has_supply_room() const { return config_.supply_cap <= 0 || state_.living_friendlies < config_.supply_cap; }

void MatchSession::set_supply_cap(int cap) {
    // A zero cap means uncapped, and growth must not invent a limit a level deliberately did not set.
    if (supply_cap_ceiling_ <= 0) {
        return;
    }

    // Bounded above by the level's own cap and below by the line already standing. Without the lower bound a
    // shrinking allowance would refuse every deployment until the player had been killed down to it, which reads as
    // the game being broken rather than as a rule being applied.
    config_.supply_cap = std::clamp(cap, std::max(state_.living_friendlies, 1), supply_cap_ceiling_);
}

void MatchSession::record_friendly_deployed() { ++state_.living_friendlies; }

void MatchSession::record_friendly_died() { state_.living_friendlies = std::max(state_.living_friendlies - 1, 0); }

void MatchSession::set_base_health(int current_health) { state_.base_health = std::clamp(current_health, 0, state_.base_max_health); }

void MatchSession::set_bounty_scale(double scale) { config_.bounty_multiplier = base_bounty_multiplier_ * std::max(scale, 0.0); }

void MatchSession::award_survival_bonus(int points) { state_.survival_bonus += std::max(points, 0); }

void MatchSession::record_wave_reached(int wave) { state_.wave_reached = std::max(state_.wave_reached, wave); }

void MatchSession::record_enemy_spawned() { ++state_.living_enemies; }

int MatchSession::record_enemy_died(int base_bounty) {
    state_.kill_score += base_bounty;
    ++state_.enemies_killed;

    const auto scaled_bounty = static_cast<double>(base_bounty) * config_.bounty_multiplier;
    const int awarded_bounty = credit_energy(static_cast<int>(std::ceil(scaled_bounty)));
    state_.living_enemies = std::max(state_.living_enemies - 1, 0);
    return awarded_bounty;
}

int MatchSession::get_base_integrity() const { return calculate_hearts_from_health(state_.base_health); }

bool MatchSession::should_end_with_victory() const { return !state_.game_over && state_.base_health > 0 && state_.all_spawned && state_.living_enemies <= 0; }

int MatchSession::calculate_integrity_bonus() const { return get_base_integrity() * 50; }

int MatchSession::calculate_completion_bonus(bool victory) { return victory ? 100 : 0; }

int MatchSession::calculate_level_score(bool victory) const {
    return state_.kill_score + calculate_integrity_bonus() + state_.survival_bonus + calculate_completion_bonus(victory);
}

MatchSummaryModel MatchSession::build_end_game_summary(bool victory, int new_total_score, const std::string &current_level_id, const std::string &next_level_id,
                                                       const std::vector<std::string> &new_unlocks) const {
    MatchSummaryModel summary;
    summary.victory = victory;
    summary.enemies_killed = state_.enemies_killed;
    summary.kill_score = state_.kill_score;
    summary.hearts_remaining = get_base_integrity();
    summary.hearts_total = state_.initial_integrity;
    summary.integrity_bonus = calculate_integrity_bonus();
    summary.survival_bonus = state_.survival_bonus;
    summary.completion_bonus = calculate_completion_bonus(victory);
    summary.level_score = calculate_level_score(victory);
    summary.new_total_score = new_total_score;
    summary.wave_reached = state_.wave_reached;
    summary.current_level_id = current_level_id;
    summary.next_level_id = next_level_id;
    summary.new_unlocks = new_unlocks;
    return summary;
}

int MatchSession::calculate_hearts_from_health(int health) {
    if (health <= 0) {
        return 0;
    }

    return (health + BASE_HEALTH_PER_HEART - 1) / BASE_HEALTH_PER_HEART;
}

} // namespace defn