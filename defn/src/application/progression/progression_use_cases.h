#ifndef PROGRESSION_USE_CASES_H
#define PROGRESSION_USE_CASES_H

#include "progression_models.h"
#include "progression_ports.h"
#include "progression_rules.h"
#include "upgrade_draft_builder.h"

#include <string>
#include <vector>

namespace defn {

class ProgressionUseCases {
  public:
    ProgressionUseCases() = delete;

    static void add_score(ProgressionProfile &profile, int amount);
    static void mark_level_completed(ProgressionProfile &profile, const std::string &level_id, int level_score);
    static bool claim_level_upgrade(ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards, const std::string &level_id,
                                    const std::string &upgrade_id);
    static bool claim_rescue_draft(ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                   const std::vector<ProgressionUpgradeCard> &cards, const std::string &level_id, const std::string &upgrade_id);
    static std::vector<std::string> build_upgrade_draft(const ProgressionProfile &profile, const std::vector<UpgradeDraftCard> &cards,
                                                        const UpgradeDraftConfig &config, RandomSource &random);
};

class ProgressionCampaignUseCases {
  public:
    ProgressionCampaignUseCases(ProfileRepository &profile_repository, const ProgressionCatalogPort &progression_catalog,
                                const UpgradeCatalogPort &upgrade_catalog, RandomSource &random);

    [[nodiscard]] PlayerProfile load_campaign() const;
    bool save_campaign(const PlayerProfile &profile) const;
    [[nodiscard]] bool can_select_level(const PlayerProfile &profile, const std::string &level_id) const;
    [[nodiscard]] ProgressionMatchResult complete_level(PlayerProfile &profile, const std::string &level_id, int level_score, bool victory);
    [[nodiscard]] ProgressionRewardDraft build_first_clear_reward_draft(const PlayerProfile &profile, const std::string &level_id);
    [[nodiscard]] ProgressionRewardDraft build_rescue_reward_draft(const PlayerProfile &profile, const std::string &level_id);
    [[nodiscard]] ProgressionRewardDraft build_reward_draft(const PlayerProfile &profile, const std::string &level_id, bool victory);
    bool claim_upgrade(PlayerProfile &profile, const ProgressionRewardClaim &claim) const;
    [[nodiscard]] std::vector<std::string> build_available_roster(const PlayerProfile &profile) const;

  private:
    [[nodiscard]] std::vector<std::string> build_upgrade_ids(const PlayerProfile &profile);

    ProfileRepository &profile_repository_;
    const ProgressionCatalogPort &progression_catalog_;
    const UpgradeCatalogPort &upgrade_catalog_;
    RandomSource &random_;
};

} // namespace defn

#endif