#ifndef PROGRESSION_SAVE_REPOSITORY_H
#define PROGRESSION_SAVE_REPOSITORY_H

#include <godot_cpp/variant/string.hpp>

#include <map>
#include <optional>
#include <set>

namespace defn {

using namespace godot;

struct PlayerProfile {
    int total_score = 0;
    std::set<String> completed_levels;
    std::map<String, int> best_level_scores;
    std::map<String, int> owned_upgrade_counts;
    std::map<String, String> claimed_level_upgrades;
    std::map<String, int> claimed_rescue_drafts;
};

class ProgressionSaveRepository {
  public:
    ProgressionSaveRepository() = delete;

    static std::optional<PlayerProfile> load(const String &path);
    static bool save(const String &path, const PlayerProfile &save_data);
};

} // namespace defn

#endif