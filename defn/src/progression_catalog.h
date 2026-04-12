#ifndef PROGRESSION_CATALOG_H
#define PROGRESSION_CATALOG_H

#include <godot_cpp/variant/string.hpp>
#include <vector>

namespace defn {

using namespace godot;

struct LevelUnlock {
    String level_id;
    String requires_completed;
    std::vector<int> rescue_thresholds;
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