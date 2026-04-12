#ifndef PROGRESSION_CATALOG_H
#define PROGRESSION_CATALOG_H

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>

namespace defn {

using namespace godot;

struct RescueDraftConfig {
    real_t point_gain_multiplier = 0.0F;
    std::vector<int> draft_costs;

    bool has_drafts() const { return !draft_costs.empty(); }
};

struct LevelUnlock {
    String level_id;
    int score_required = 0;
    String requires_completed;
    RescueDraftConfig rescue;
};

class ProgressionCatalog {
  public:
    bool load(const String &path);

    const std::vector<LevelUnlock> &get_level_unlocks() const { return level_unlocks_; }

  private:
    std::vector<LevelUnlock> level_unlocks_;
};

} // namespace defn

#endif