#ifndef COMBAT_TARGET_SELECTOR_H
#define COMBAT_TARGET_SELECTOR_H

#include "attack_target.h"
#include "combat_types.h"

#include <godot_cpp/classes/area2d.hpp>

namespace defn {

using namespace godot;

class Unit;

struct CombatTargetSelection {
    bool engaged = false;
    AttackMode attack_mode = AttackMode::NONE;
    AttackTarget *target = nullptr;
};

class CombatTargetSelector {
  public:
    static CombatTargetSelection select(Unit *unit, Area2D *detection_area, const CombatConfig &config, AttackTarget *current_target);

  private:
    static AttackMode classify_target(Unit *unit, const CombatConfig &config, const AttackTarget *target);
    static real_t get_forward_distance(UnitSide side, const Vector2 &origin, const AttackTarget *target);
};

} // namespace defn

#endif