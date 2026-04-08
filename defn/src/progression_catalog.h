#ifndef PROGRESSION_CATALOG_H
#define PROGRESSION_CATALOG_H

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>

namespace defn {

using namespace godot;

struct UnitUnlock {
    String unit_id;
    int score_required = 0;
};

struct LevelUnlock {
    String level_id;
    int score_required = 0;
    String requires_completed;
};

struct UpgradeEntry {
    String id;
    int score_required = 0;
    String type;
    real_t value = 0.0;
};

class ProgressionCatalog {
  public:
    bool load(const String &path);

    const std::vector<UnitUnlock> &get_unit_unlocks() const { return unit_unlocks_; }
    const std::vector<LevelUnlock> &get_level_unlocks() const { return level_unlocks_; }
    const std::vector<UpgradeEntry> &get_upgrades() const { return upgrades_; }

  private:
    std::vector<UnitUnlock> unit_unlocks_;
    std::vector<LevelUnlock> level_unlocks_;
    std::vector<UpgradeEntry> upgrades_;
};

} // namespace defn

#endif