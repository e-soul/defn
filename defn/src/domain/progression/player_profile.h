// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef PLAYER_PROFILE_H
#define PLAYER_PROFILE_H

#include <map>
#include <set>
#include <string>

namespace defn {

struct PlayerProfile {
    int total_score = 0;
    std::set<std::string> completed_levels;
    std::map<std::string, int> best_level_scores;
    std::map<std::string, int> owned_upgrade_counts;
    std::map<std::string, std::string> claimed_level_upgrades;
    std::map<std::string, int> claimed_rescue_drafts;
    // Keyed by threat level so an ascension ladder can be added without another save migration. Absent from a
    // pre-endless save, which loads as empty.
    std::map<int, int> endless_best_wave;
    std::map<int, int> endless_best_score;
};

using ProgressionProfile = PlayerProfile;

} // namespace defn

#endif