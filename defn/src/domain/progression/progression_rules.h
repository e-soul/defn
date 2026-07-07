#ifndef PROGRESSION_RULES_H
#define PROGRESSION_RULES_H

#include "player_profile.h"

#include <string>
#include <vector>

namespace defn {

enum class ProgressionUpgradeEffectType {
    STARTING_ENERGY_DELTA,
    ENERGY_REGEN_DELTA,
    BOUNTY_MULTIPLIER_DELTA,
    BASE_INTEGRITY_DELTA,
    UNIT_HP_DELTA,
    UNIT_RANGED_DAMAGE_DELTA,
    UNIT_MOVE_SPEED_DELTA,
    UNIT_UNLOCK,
};

struct ProgressionLevelUnlock {
    std::string level_id;
    std::string requires_completed;
    std::vector<int> rescue_thresholds;
};

struct ProgressionUpgradeEffect {
    ProgressionUpgradeEffectType type = ProgressionUpgradeEffectType::STARTING_ENERGY_DELTA;
    float value = 0.0F;
    std::string unit_id;
};

struct ProgressionUpgradeCard {
    std::string id;
    int max_picks = 1;
    std::vector<ProgressionUpgradeEffect> effects;
};

struct ProgressionUnitStats {
    std::string unit_id;
    bool friendly = true;
    int hp = 100;
    int ranged_damage = 8;
    float move_speed = 64.0F;
    bool has_projectile_attack = false;
};

struct CampaignModifiers {
    int starting_energy_delta = 0;
    int energy_regen = 1;
    float bounty_multiplier = 1.0F;
    int base_integrity_delta = 0;
};

[[nodiscard]] const ProgressionLevelUnlock *find_level_unlock(const std::vector<ProgressionLevelUnlock> &level_unlocks, const std::string &level_id);
[[nodiscard]] bool is_level_completed(const ProgressionProfile &profile, const std::string &level_id);
[[nodiscard]] bool is_level_unlocked(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks, const std::string &level_id);
[[nodiscard]] std::vector<std::string> get_unlocked_levels(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks);
[[nodiscard]] std::string get_frontier_level_id(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks);
[[nodiscard]] std::string get_claimed_upgrade_for_level(const ProgressionProfile &profile, const std::string &level_id);
[[nodiscard]] bool can_claim_level_upgrade(const ProgressionProfile &profile, const std::string &level_id);
[[nodiscard]] int get_highest_level_score(const ProgressionProfile &profile, const std::string &level_id);
[[nodiscard]] int get_rescue_drafts_claimed(const ProgressionProfile &profile, const std::string &level_id);
[[nodiscard]] int get_next_rescue_draft_threshold(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                                  const std::string &level_id);
[[nodiscard]] bool can_claim_rescue_draft(const ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                          const std::string &level_id);
[[nodiscard]] int get_completed_level_count(const ProgressionProfile &profile);
[[nodiscard]] int get_owned_upgrade_count(const ProgressionProfile &profile, const std::string &upgrade_id);
[[nodiscard]] bool has_upgrade(const ProgressionProfile &profile, const std::string &upgrade_id);
[[nodiscard]] std::vector<std::string> get_owned_upgrades(const ProgressionProfile &profile);
[[nodiscard]] std::vector<std::string> get_unlocked_units(const ProgressionProfile &profile, const std::vector<std::string> &base_units,
                                                          const std::vector<ProgressionUpgradeCard> &cards);
[[nodiscard]] ProgressionUnitStats apply_owned_upgrade_effects(const ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards,
                                                               const ProgressionUnitStats &base_stats);
[[nodiscard]] CampaignModifiers calculate_campaign_modifiers(const ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards);

} // namespace defn

#endif