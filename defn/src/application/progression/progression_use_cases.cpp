#include "progression_use_cases.h"

#include <algorithm>
#include <optional>

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

bool contains_string(const std::vector<std::string> &values, const std::string &candidate) { return std::ranges::find(values, candidate) != values.end(); }

std::vector<std::string> find_new_unlocks(const PlayerProfile &before, const PlayerProfile &after, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                          const std::string &completed_level_id) {
    std::vector<std::string> result;
    if (completed_level_id.empty()) {
        return result;
    }

    const std::vector<std::string> before_unlocked = get_unlocked_levels(before, level_unlocks);
    for (const auto &unlock : level_unlocks) {
        if (unlock.requires_completed == completed_level_id && is_level_unlocked(after, level_unlocks, unlock.level_id) &&
            !contains_string(before_unlocked, unlock.level_id)) {
            result.push_back(unlock.level_id);
        }
    }
    return result;
}

std::string find_next_unlocked_level(const PlayerProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                     const std::string &current_level_id) {
    for (size_t index = 0; index < level_unlocks.size(); ++index) {
        if (level_unlocks[index].level_id == current_level_id && index + 1 < level_unlocks.size()) {
            const std::string &candidate = level_unlocks[index + 1].level_id;
            return is_level_unlocked(profile, level_unlocks, candidate) ? candidate : std::string();
        }
    }
    return {};
}

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
                                             const std::vector<ProgressionUpgradeCard> &cards, const std::string &level_id, const std::string &upgrade_id) {
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

ProgressionCampaignUseCases::ProgressionCampaignUseCases(ProfileRepository &profile_repository, const ProgressionCatalogPort &progression_catalog,
                                                         const UpgradeCatalogPort &upgrade_catalog, RandomSource &random)
    : profile_repository_(profile_repository), progression_catalog_(progression_catalog), upgrade_catalog_(upgrade_catalog), random_(random) {}

PlayerProfile ProgressionCampaignUseCases::load_campaign() const {
    if (const std::optional<PlayerProfile> loaded_profile = profile_repository_.load_profile(); loaded_profile.has_value()) {
        return *loaded_profile;
    }

    PlayerProfile default_profile;
    profile_repository_.save_profile(default_profile);
    return default_profile;
}

bool ProgressionCampaignUseCases::save_campaign(const PlayerProfile &profile) const { return profile_repository_.save_profile(profile); }

bool ProgressionCampaignUseCases::can_select_level(const PlayerProfile &profile, const std::string &level_id) const {
    return is_level_unlocked(profile, progression_catalog_.get_progression_level_unlocks(), level_id);
}

ProgressionMatchResult ProgressionCampaignUseCases::complete_level(PlayerProfile &profile, const std::string &level_id, int level_score, bool victory) {
    const PlayerProfile before = profile;
    const std::vector<ProgressionLevelUnlock> level_unlocks = progression_catalog_.get_progression_level_unlocks();

    ProgressionUseCases::add_score(profile, level_score);
    if (victory) {
        ProgressionUseCases::mark_level_completed(profile, level_id, level_score);
    }

    ProgressionMatchResult result;
    result.new_total_score = profile.total_score;
    if (victory) {
        result.new_unlock_level_ids = find_new_unlocks(before, profile, level_unlocks, level_id);
        result.next_level_id = find_next_unlocked_level(profile, level_unlocks, level_id);
    }
    result.reward_draft = build_reward_draft(profile, level_id, victory);
    profile_repository_.save_profile(profile);
    return result;
}

ProgressionRewardDraft ProgressionCampaignUseCases::build_first_clear_reward_draft(const PlayerProfile &profile, const std::string &level_id) {
    if (!can_claim_level_upgrade(profile, level_id)) {
        return {};
    }

    return {
        .source = ProgressionRewardSource::FIRST_CLEAR,
        .level_id = level_id,
        .upgrade_ids = build_upgrade_ids(profile),
    };
}

ProgressionRewardDraft ProgressionCampaignUseCases::build_rescue_reward_draft(const PlayerProfile &profile, const std::string &level_id) {
    if (!can_claim_rescue_draft(profile, progression_catalog_.get_progression_level_unlocks(), level_id)) {
        return {};
    }

    return {
        .source = ProgressionRewardSource::RESCUE,
        .level_id = level_id,
        .upgrade_ids = build_upgrade_ids(profile),
    };
}

ProgressionRewardDraft ProgressionCampaignUseCases::build_reward_draft(const PlayerProfile &profile, const std::string &level_id, bool victory) {
    if (victory) {
        return build_first_clear_reward_draft(profile, level_id);
    }

    const std::string frontier_level_id = get_frontier_level_id(profile, progression_catalog_.get_progression_level_unlocks());
    if (frontier_level_id.empty()) {
        return {};
    }
    return build_rescue_reward_draft(profile, frontier_level_id);
}

bool ProgressionCampaignUseCases::claim_upgrade(PlayerProfile &profile, const ProgressionRewardClaim &claim) const {
    bool claimed = false;
    if (claim.source == ProgressionRewardSource::FIRST_CLEAR) {
        claimed = ProgressionUseCases::claim_level_upgrade(profile, upgrade_catalog_.get_progression_upgrade_cards(), claim.level_id, claim.upgrade_id);
    } else if (claim.source == ProgressionRewardSource::RESCUE) {
        claimed = ProgressionUseCases::claim_rescue_draft(profile, progression_catalog_.get_progression_level_unlocks(),
                                                          upgrade_catalog_.get_progression_upgrade_cards(), claim.level_id, claim.upgrade_id);
    }

    if (claimed) {
        profile_repository_.save_profile(profile);
    }
    return claimed;
}

std::vector<std::string> ProgressionCampaignUseCases::build_available_roster(const PlayerProfile &profile) const {
    return get_unlocked_units(profile, upgrade_catalog_.get_base_unit_ids(), upgrade_catalog_.get_progression_upgrade_cards());
}

std::vector<std::string> ProgressionCampaignUseCases::build_upgrade_ids(const PlayerProfile &profile) {
    return ProgressionUseCases::build_upgrade_draft(profile, upgrade_catalog_.get_upgrade_draft_cards(), upgrade_catalog_.get_upgrade_draft_config(), random_);
}

} // namespace defn