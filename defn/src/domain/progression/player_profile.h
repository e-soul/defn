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
};

using ProgressionProfile = PlayerProfile;

} // namespace defn

#endif