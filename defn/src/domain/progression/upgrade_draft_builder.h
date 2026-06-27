#ifndef UPGRADE_DRAFT_BUILDER_H
#define UPGRADE_DRAFT_BUILDER_H

#include "progression_rules.h"
#include "random_source.h"

#include <string>
#include <vector>

namespace defn {

struct UpgradeDraftCard {
    std::string id;
    int minimum_completed_levels = 0;
    int weight = 1;
    int max_picks = 1;
    bool grants_unit_unlock = false;
    std::vector<std::string> prerequisites;
};

struct UpgradeDraftConfig {
    int draft_size = 3;
    bool reserve_unit_slot = true;
};

[[nodiscard]] std::vector<std::string> build_upgrade_draft(const ProgressionProfile &profile, const std::vector<UpgradeDraftCard> &cards,
                                                           const UpgradeDraftConfig &config, RandomSource &random);

} // namespace defn

#endif