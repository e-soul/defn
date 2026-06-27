#ifndef COMBAT_TARGET_SELECTOR_H
#define COMBAT_TARGET_SELECTOR_H

#include "combat_logic.h"
#include "combat_types.h"

#include <godot_cpp/classes/area2d.hpp>

namespace defn {

using namespace godot;

class BattleEntity;

class CombatTargetSelector {
  public:
    static CombatTargetSelection select(const BattleEntity *unit, Area2D *detection_area, const CombatConfig &config, EntityId current_target_id);
};

} // namespace defn

#endif