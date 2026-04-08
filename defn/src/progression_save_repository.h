#ifndef PROGRESSION_SAVE_REPOSITORY_H
#define PROGRESSION_SAVE_REPOSITORY_H

#include <godot_cpp/variant/string.hpp>
#include <optional>
#include <vector>

namespace defn {

using namespace godot;

struct ProgressionSaveData {
    int total_score = 0;
    std::vector<String> levels_completed;
    std::vector<std::pair<String, int>> highest_level_scores;
};

class ProgressionSaveRepository {
  public:
    ProgressionSaveRepository() = delete;

    static std::optional<ProgressionSaveData> load(const String &path);
    static bool save(const String &path, const ProgressionSaveData &save_data);
};

} // namespace defn

#endif