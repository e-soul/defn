#ifndef PROGRESSION_PRESENTATION_H
#define PROGRESSION_PRESENTATION_H

#include "progression_models.h"
#include "progression_rules.h"

#include <string>
#include <vector>

namespace defn {

class ProgressionPresentation {
  public:
    ProgressionPresentation() = delete;

    static std::string format_level_name(const std::string &level_id);
    static std::string format_reward_title(ProgressionRewardSource reward_source, const std::string &reward_level_id);
    static std::string format_reward_subtitle(ProgressionRewardSource reward_source, const std::string &reward_level_id);
    static std::vector<std::string> describe_new_unlocks(const std::vector<std::string> &unlocked_level_ids);
    static std::string format_level_button_label(const ProgressionLevelUnlock &level_unlock, bool unlocked, bool completed, int best_score);
    static ProgressionUpgradeCardViewModel build_upgrade_card_view_model(const ProgressionUpgradePresentation &card, int owned_count = 0);
    static ProgressionRewardViewModel build_reward_view_model(const ProgressionRewardDraft &draft,
                                                              const std::vector<ProgressionUpgradePresentation> &upgrade_presentations);
};

} // namespace defn

#endif