#ifndef COMBAT_TARGET_SELECTOR_H
#define COMBAT_TARGET_SELECTOR_H

#include "attack_target.h"
#include "combat_logic.h"
#include "combat_types.h"

#include <godot_cpp/classes/area2d.hpp>

namespace defn {

using namespace godot;

class Unit;

class CombatTargetSelector {
  public:
    static CombatTargetSelection select(Unit *unit, Area2D *detection_area, const CombatConfig &config, AttackTarget *current_target);
};

} // namespace defn

#endif