#ifndef PROGRESSION_USE_CASES_H
#define PROGRESSION_USE_CASES_H

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

} // namespace defn

#endif