#ifndef MATCH_SUMMARY_H
#define MATCH_SUMMARY_H

#include <string>
#include <vector>

namespace defn {

struct MatchSummaryModel {
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
};

} // namespace defn

#endif