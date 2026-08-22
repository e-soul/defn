// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

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
    // True when the target still exists and is alive, but sits outside every attack range, so it has to be chased.
    static bool is_target_out_of_range(const BattleEntity *unit, const CombatConfig &config, EntityId target_id);
};

} // namespace defn

#endif