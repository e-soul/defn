#include "progression_rules.h"

#include <algorithm>
#include <cmath>

namespace defn {

namespace {

int round_effect_delta(float value) { return static_cast<int>(std::lround(static_cast<double>(value))); }

const ProgressionUpgradeCard *find_card(const std::vector<ProgressionUpgradeCard> &cards, const std::string &card_id) {
    for (const auto &card : cards) {
        if (card.id == card_id) {
            return &card;
        }
    }
    return nullptr;
}

template <typename Func> void for_each_owned_upgrade_effect(const ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards, Func &&func) {
    for (const auto &[owned_upgrade, count] : profile.owned_upgrade_counts) {
        const ProgressionUpgradeCard *card = find_card(cards, owned_upgrade);
        if (card == nullptr || count <= 0) {
            continue;
        }

        for (int pick = 0; pick < count; ++pick) {
            for (const auto &effect : card->effects) {
                func(*card, effect);
            }
        }
    }
}

void push_unique(std::vector<std::string> &values, const std::string &value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.push_back(value);
    }
}

} // namespace

const ProgressionLevelUnlock *find_level_unlock(const std::vector<ProgressionLevelUnlock> &level_unlocks, const std::string &level_id) {
    for (const auto &unlock : level_unlocks) {
        if (unlock.level_id == level_id) {
            return &unlock;
        }
    }
    return nullptr;
}

bool is_level_completed(const ProgressionProfile &profile, const std::string &level_id) { return profile.completed_levels.contains(level_id); }

bool is_level_unlocked(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks, const std::string &level_id) {
    const ProgressionLevelUnlock *unlock = find_level_unlock(level_unlocks, level_id);
    if (unlock == nullptr) {
        return false;
    }

    if (!unlock->requires_completed.empty() && !is_level_completed(profile, unlock->requires_completed)) {
        return false;
    }
    return true;
}

std::vector<std::string> get_unlocked_levels(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks) {
    std::vector<std::string> result;
    for (const auto &unlock : level_unlocks) {
        if (is_level_unlocked(profile, level_unlocks, unlock.level_id)) {
            result.push_back(unlock.level_id);
        }
    }
    return result;
}

std::string get_frontier_level_id(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks) {
    for (const auto &unlock : level_unlocks) {
        if (is_level_unlocked(profile, level_unlocks, unlock.level_id) && !is_level_completed(profile, unlock.level_id)) {
            return unlock.level_id;
        }
    }
    return {};
}

std::string get_claimed_upgrade_for_level(const ProgressionProfile &profile, const std::string &level_id) {
    if (const auto found = profile.claimed_level_upgrades.find(level_id); found != profile.claimed_level_upgrades.end()) {
        return found->second;
    }

    return {};
}

bool can_claim_level_upgrade(const ProgressionProfile &profile, const std::string &level_id) {
    return !level_id.empty() && is_level_completed(profile, level_id) && get_claimed_upgrade_for_level(profile, level_id).empty();
}

int get_highest_level_score(const ProgressionProfile &profile, const std::string &level_id) {
    if (const auto found = profile.best_level_scores.find(level_id); found != profile.best_level_scores.end()) {
        return found->second;
    }

    return 0;
}

int get_rescue_drafts_claimed(const ProgressionProfile &profile, const std::string &level_id) {
    if (const auto found = profile.claimed_rescue_drafts.find(level_id); found != profile.claimed_rescue_drafts.end()) {
        return std::max(0, found->second);
    }

    return 0;
}

int get_next_rescue_draft_threshold(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks, const std::string &level_id) {
    const ProgressionLevelUnlock *unlock = find_level_unlock(level_unlocks, level_id);
    if (unlock == nullptr) {
        return 0;
    }

    const int claimed_count = get_rescue_drafts_claimed(profile, level_id);
    if (!std::cmp_less(claimed_count, unlock->rescue_thresholds.size())) {
        return 0;
    }

    return unlock->rescue_thresholds[static_cast<size_t>(claimed_count)];
}

bool can_claim_rescue_draft(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks, const std::string &level_id) {
    if (level_id.empty() || level_id != get_frontier_level_id(profile, level_unlocks) || is_level_completed(profile, level_id)) {
        return false;
    }

    const int next_threshold = get_next_rescue_draft_threshold(profile, level_unlocks, level_id);
    return next_threshold > 0 && profile.total_score >= next_threshold;
}

int get_completed_level_count(const ProgressionProfile &profile) { return static_cast<int>(profile.completed_levels.size()); }

int get_owned_upgrade_count(const ProgressionProfile &profile, const std::string &upgrade_id) {
    if (const auto found = profile.owned_upgrade_counts.find(upgrade_id); found != profile.owned_upgrade_counts.end()) {
        return found->second;
    }

    return 0;
}

bool has_upgrade(const ProgressionProfile &profile, const std::string &upgrade_id) { return get_owned_upgrade_count(profile, upgrade_id) > 0; }

std::vector<std::string> get_owned_upgrades(const ProgressionProfile &profile) {
    std::vector<std::string> result;
    for (const auto &[upgrade_id, count] : profile.owned_upgrade_counts) {
        for (int pick = 0; pick < count; ++pick) {
            result.push_back(upgrade_id);
        }
    }
    return result;
}

std::vector<std::string> get_unlocked_units(const ProgressionProfile &profile, const std::vector<std::string> &base_units,
                                            const std::vector<ProgressionUpgradeCard> &cards) {
    std::vector<std::string> result;
    for (const auto &unit_id : base_units) {
        push_unique(result, unit_id);
    }

    for_each_owned_upgrade_effect(profile, cards, [&result](const ProgressionUpgradeCard &, const ProgressionUpgradeEffect &effect) {
        if (effect.type == ProgressionUpgradeEffectType::UNIT_UNLOCK && !effect.unit_id.empty()) {
            push_unique(result, effect.unit_id);
        }
    });
    return result;
}

ProgressionUnitStats apply_owned_upgrade_effects(const ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards,
                                                 const ProgressionUnitStats &base_stats) {
    ProgressionUnitStats effective_stats = base_stats;
    if (!effective_stats.friendly) {
        return effective_stats;
    }

    for_each_owned_upgrade_effect(profile, cards, [&effective_stats](const ProgressionUpgradeCard &, const ProgressionUpgradeEffect &effect) {
        if (effect.unit_id != effective_stats.unit_id) {
            return;
        }

        if (effect.type == ProgressionUpgradeEffectType::UNIT_HP_DELTA) {
            effective_stats.hp += round_effect_delta(effect.value);
        } else if (effect.type == ProgressionUpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA) {
            effective_stats.ranged_damage += round_effect_delta(effect.value);
        } else if (effect.type == ProgressionUpgradeEffectType::UNIT_MOVE_SPEED_DELTA) {
            effective_stats.move_speed += effect.value;
        }
    });

    effective_stats.hp = std::max(effective_stats.hp, 1);
    if (base_stats.ranged_damage > 0 || effective_stats.ranged_damage > 0 || base_stats.has_projectile_attack) {
        effective_stats.ranged_damage = std::max(effective_stats.ranged_damage, 1);
    } else {
        effective_stats.ranged_damage = std::max(effective_stats.ranged_damage, 0);
    }

    if (base_stats.move_speed > 0.0F || effective_stats.move_speed > 0.0F) {
        effective_stats.move_speed = std::max(effective_stats.move_speed, 1.0F);
    } else {
        effective_stats.move_speed = std::max(effective_stats.move_speed, 0.0F);
    }

    return effective_stats;
}

CampaignModifiers calculate_campaign_modifiers(const ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards) {
    CampaignModifiers modifiers;
    for_each_owned_upgrade_effect(profile, cards, [&modifiers](const ProgressionUpgradeCard &, const ProgressionUpgradeEffect &effect) {
        if (effect.type == ProgressionUpgradeEffectType::STARTING_ENERGY_DELTA) {
            modifiers.starting_energy_delta += static_cast<int>(effect.value);
        } else if (effect.type == ProgressionUpgradeEffectType::ENERGY_REGEN_DELTA) {
            modifiers.energy_regen += static_cast<int>(effect.value);
        } else if (effect.type == ProgressionUpgradeEffectType::BOUNTY_MULTIPLIER_DELTA) {
            modifiers.bounty_multiplier += effect.value;
        } else if (effect.type == ProgressionUpgradeEffectType::BASE_INTEGRITY_DELTA) {
            modifiers.base_integrity_delta += static_cast<int>(effect.value);
        }
    });
    return modifiers;
}

} // namespace defn