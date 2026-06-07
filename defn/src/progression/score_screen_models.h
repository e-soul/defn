#ifndef SCORE_SCREEN_MODELS_H
#define SCORE_SCREEN_MODELS_H

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <vector>

namespace defn {

using namespace godot;

struct UpgradeCardViewModel {
    String id;
    String name;
    String description;
    String emoji;
    String category;
    int owned_count = 0;
};

struct ScoreScreenRewardModel {
    String source;
    String level_id;
    String title;
    String subtitle;
    std::vector<UpgradeCardViewModel> available_upgrades;
    std::optional<UpgradeCardViewModel> selected_upgrade;

    bool requires_selection() const { return !available_upgrades.empty() && !selected_upgrade.has_value(); }

    const UpgradeCardViewModel *find_upgrade(const String &upgrade_id) const {
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
    String current_level_id;
    String next_level_id;
    PackedStringArray new_unlocks;
    ScoreScreenRewardModel reward;
    std::vector<UpgradeCardViewModel> owned_upgrades;
};

} // namespace defn

#endif