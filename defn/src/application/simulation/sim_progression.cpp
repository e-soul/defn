// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_progression.h"

#include <algorithm>
#include <utility>

namespace defn {

void SimProgression::configure(std::vector<std::string> base_unit_ids, std::vector<ProgressionUpgradeCard> upgrade_cards,
                               const std::vector<std::string> &owned_upgrades) {
    base_unit_ids_ = std::move(base_unit_ids);
    upgrade_cards_ = std::move(upgrade_cards);
    profile_ = {};
    for (const std::string &upgrade_id : owned_upgrades) {
        ++profile_.owned_upgrade_counts[upgrade_id];
    }
}

std::vector<std::string> SimProgression::get_unlocked_units() const { return defn::get_unlocked_units(profile_, base_unit_ids_, upgrade_cards_); }

bool SimProgression::is_level_completed(const std::string &level_id) const { return defn::is_level_completed(profile_, level_id); }

int SimProgression::get_highest_level_score(const std::string &level_id) const { return defn::get_highest_level_score(profile_, level_id); }

ProgressionUnitStats SimProgression::get_effective_friendly_unit_stats(const ProgressionUnitStats &base_stats) const {
    return apply_owned_upgrade_effects(profile_, upgrade_cards_, base_stats);
}

int SimProgression::get_effective_starting_energy(int base) const {
    return base + calculate_campaign_modifiers(profile_, upgrade_cards_).starting_energy_delta;
}

int SimProgression::get_effective_energy_regen() const { return calculate_campaign_modifiers(profile_, upgrade_cards_).energy_regen; }

float SimProgression::get_effective_bounty_multiplier() const { return calculate_campaign_modifiers(profile_, upgrade_cards_).bounty_multiplier; }

int SimProgression::get_effective_base_integrity(int base) const { return base + calculate_campaign_modifiers(profile_, upgrade_cards_).base_integrity_delta; }

bool SimProgression::select_level(const std::string &level_id) {
    current_level_id_ = level_id;
    return true;
}

ProgressionMatchResult SimProgression::complete_level(const std::string &level_id, int level_score, bool victory) {
    if (victory) {
        profile_.completed_levels.insert(level_id);
    }
    profile_.total_score += level_score;
    int &best = profile_.best_level_scores[level_id];
    best = std::max(best, level_score);

    // No draft is offered: the simulator measures the match, not the card it would have won.
    return {.new_total_score = profile_.total_score};
}

} // namespace defn
