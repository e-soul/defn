#ifndef PROGRESSION_STATS_PRESENTER_H
#define PROGRESSION_STATS_PRESENTER_H

#include "progression_models.h"
#include "progression_stat_visualization.h"

#include <string>
#include <vector>

namespace defn {

struct ProgressionSelectorItemViewModel {
    std::string id;
    std::string label;
    std::string portrait_path_template;
    std::string locked_message;
    bool unlocked = false;
    bool selected = false;
};

struct ProgressionStatRowViewModel {
    std::string id;
    std::string label;
    std::string value;
    std::string detail;
    ProgressionStatVisualViewModel visual;
};

struct ProgressionUpgradeChipViewModel {
    std::string id;
    std::string label;
    std::string description;
    std::string emoji;
};

struct ProgressionStatsScreenViewModel {
    std::vector<ProgressionSelectorItemViewModel> selectors;
    std::string selected_entity_id;
    std::string title;
    std::string description;
    std::string portrait_path_template;
    bool locked = false;
    std::string locked_message;
    std::vector<ProgressionStatRowViewModel> stats;
    std::vector<ProgressionUpgradeChipViewModel> upgrades;
    std::string empty_upgrade_message;
    std::string all_upgrades_label = "All Owned Upgrades";
    std::string back_label = "Back";
};

class ProgressionStatsPresenter {
  public:
    ProgressionStatsPresenter() = delete;

    [[nodiscard]] static std::string default_selection(const ProgressionOverviewSnapshot &snapshot);
    [[nodiscard]] static ProgressionStatsScreenViewModel present(const ProgressionOverviewSnapshot &snapshot, const std::string &selected_entity_id);
};

} // namespace defn

#endif
