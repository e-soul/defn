#include "progression_use_cases.h"

#include <algorithm>

namespace defn {

namespace {

const ProgressionUpgradeCard *find_upgrade_card(const std::vector<ProgressionUpgradeCard> &cards, const std::string &upgrade_id) {
    for (const auto &card : cards) {
        if (card.id == upgrade_id) {
            return &card;
        }
    }
    return nullptr;
}

bool can_grant_upgrade(const ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards, const std::string &upgrade_id) {
    const ProgressionUpgradeCard *card = find_upgrade_card(cards, upgrade_id);
    return card != nullptr && get_owned_upgrade_count(profile, upgrade_id) < card->max_picks;
}

void grant_upgrade(ProgressionProfile &profile, const std::string &upgrade_id) { ++profile.owned_upgrade_counts[upgrade_id]; }

} // namespace

void ProgressionUseCases::add_score(ProgressionProfile &profile, int amount) { profile.total_score += amount; }

void ProgressionUseCases::mark_level_completed(ProgressionProfile &profile, const std::string &level_id, int level_score) {
    profile.completed_levels.insert(level_id);
    profile.best_level_scores[level_id] = std::max(level_score, get_highest_level_score(profile, level_id));
}

bool ProgressionUseCases::claim_level_upgrade(ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards, const std::string &level_id,
                                              const std::string &upgrade_id) {
    if (level_id.empty() || upgrade_id.empty() || !can_claim_level_upgrade(profile, level_id) || !can_grant_upgrade(profile, cards, upgrade_id)) {
        return false;
    }

    profile.claimed_level_upgrades[level_id] = upgrade_id;
    grant_upgrade(profile, upgrade_id);
    return true;
}

bool ProgressionUseCases::claim_rescue_draft(ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                             const std::vector<ProgressionUpgradeCard> &cards, const std::string &level_id,
                                             const std::string &upgrade_id) {
    if (level_id.empty() || upgrade_id.empty() || !can_claim_rescue_draft(profile, level_unlocks, level_id) || !can_grant_upgrade(profile, cards, upgrade_id)) {
        return false;
    }

    const int next_threshold = get_next_rescue_draft_threshold(profile, level_unlocks, level_id);
    if (next_threshold <= 0 || profile.total_score < next_threshold) {
        return false;
    }

    profile.claimed_rescue_drafts[level_id] = get_rescue_drafts_claimed(profile, level_id) + 1;
    grant_upgrade(profile, upgrade_id);
    return true;
}

std::vector<std::string> ProgressionUseCases::build_upgrade_draft(const ProgressionProfile &profile, const std::vector<UpgradeDraftCard> &cards,
                                                                  const UpgradeDraftConfig &config, RandomSource &random) {
    return defn::build_upgrade_draft(profile, cards, config, random);
}

} // namespace defn