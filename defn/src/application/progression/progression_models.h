#ifndef PROGRESSION_MODELS_H
#define PROGRESSION_MODELS_H

#include <string>
#include <vector>

namespace defn {

enum class ProgressionRewardSource {
    NONE,
    FIRST_CLEAR,
    RESCUE,
};

inline std::string to_progression_reward_source_id(ProgressionRewardSource source) {
    switch (source) {
    case ProgressionRewardSource::FIRST_CLEAR:
        return "first_clear";
    case ProgressionRewardSource::RESCUE:
        return "rescue";
    case ProgressionRewardSource::NONE:
        return {};
    }

    return {};
}

inline ProgressionRewardSource progression_reward_source_from_id(const std::string &source_id) {
    if (source_id == "first_clear") {
        return ProgressionRewardSource::FIRST_CLEAR;
    }
    if (source_id == "rescue") {
        return ProgressionRewardSource::RESCUE;
    }
    return ProgressionRewardSource::NONE;
}

struct ProgressionUpgradePresentation {
    std::string id;
    std::string name;
    std::string description;
    std::string emoji;
    std::string category;
};

struct ProgressionUpgradeCardViewModel {
    std::string id;
    std::string name;
    std::string description;
    std::string emoji;
    std::string category;
    int owned_count = 0;
};

struct ProgressionRewardDraft {
    ProgressionRewardSource source = ProgressionRewardSource::NONE;
    std::string level_id;
    std::vector<std::string> upgrade_ids;

    [[nodiscard]] bool has_reward() const { return source != ProgressionRewardSource::NONE && !level_id.empty() && !upgrade_ids.empty(); }
};

struct ProgressionRewardClaim {
    ProgressionRewardSource source = ProgressionRewardSource::NONE;
    std::string level_id;
    std::string upgrade_id;
};

struct ProgressionRewardViewModel {
    std::string source;
    std::string level_id;
    std::string title;
    std::string subtitle;
    std::vector<ProgressionUpgradeCardViewModel> available_upgrades;
};

struct ProgressionMatchResult {
    int new_total_score = 0;
    std::vector<std::string> new_unlock_level_ids;
    std::string next_level_id;
    ProgressionRewardDraft reward_draft;
};

} // namespace defn

#endif