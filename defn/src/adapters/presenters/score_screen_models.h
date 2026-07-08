#ifndef SCORE_SCREEN_MODELS_H
#define SCORE_SCREEN_MODELS_H

#include <optional>
#include <string>
#include <vector>

namespace defn {

struct UpgradeCardViewModel {
    std::string id;
    std::string name;
    std::string description;
    std::string emoji;
    std::string category;
    int owned_count = 0;
};

struct ScoreScreenRewardModel {
    std::string source;
    std::string level_id;
    std::string title;
    std::string subtitle;
    std::vector<UpgradeCardViewModel> available_upgrades;
    std::optional<UpgradeCardViewModel> selected_upgrade;

    bool requires_selection() const { return !available_upgrades.empty() && !selected_upgrade.has_value(); }

    const UpgradeCardViewModel *find_upgrade(const std::string &upgrade_id) const {
        for (const auto &upgrade : available_upgrades) {
            if (upgrade.id == upgrade_id) {
                return &upgrade;
            }
        }

        return nullptr;
    }
};

struct ScoreScreenModel {
    bool victory = false;
    int enemies_killed = 0;
    int kill_score = 0;
    int hearts_remaining = 0;
    int hearts_total = 0;
    int integrity_bonus = 0;
    int completion_bonus = 0;
    int level_score = 0;
    int new_total_score = 0;
    std::string current_level_id;
    std::string next_level_id;
    std::vector<std::string> new_unlocks;
    ScoreScreenRewardModel reward;
    std::vector<UpgradeCardViewModel> owned_upgrades;
};

} // namespace defn

#endif