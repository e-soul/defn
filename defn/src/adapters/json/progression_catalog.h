#ifndef PROGRESSION_CATALOG_H
#define PROGRESSION_CATALOG_H

#include "progression_ports.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <vector>

namespace defn {

using namespace godot;

struct LevelUnlock {
    String level_id;
    String requires_completed;
    std::vector<int> rescue_thresholds;
};

class ProgressionCatalog : public ProgressionCatalogPort {
  public:
    bool load(const String &path);
    bool load_from_data(const Dictionary &data);

    const std::vector<LevelUnlock> &get_level_unlocks() const { return level_unlocks_; }
    [[nodiscard]] std::vector<ProgressionLevelUnlock> get_progression_level_unlocks() const override;

  private:
    std::vector<LevelUnlock> level_unlocks_;
};

} // namespace defn

#endif