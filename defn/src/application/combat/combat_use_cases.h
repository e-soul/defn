// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef COMBAT_USE_CASES_H
#define COMBAT_USE_CASES_H

#include "combat_logic.h"

#include <vector>

namespace defn {

// SLIDE_BELT carries its destination in `target_position.y` and is emitted alongside STOP or MOVE rather than instead
// of either: the belt's depth axis moves independently of the forward one.
//
// MOVE_BACKWARD is MOVE against the side's advance: a unit that has overrun the enemy line walking back to it. Whoever
// carries it out also turns the unit round, because it is the only time anything walks the wrong way on its own.
enum class CombatCommandType { STOP, MOVE, MOVE_BACKWARD, SLIDE_BELT, PLAY_POSE, HIDE_MUZZLE_FLASH, DEAL_DAMAGE, SPAWN_PROJECTILE, PLAY_EFFECT };
enum class CombatEffectType { NONE, MELEE_ATTACK, RANGED_SHOOT, DAMAGE_FLASH };

struct CombatCommand {
    CombatCommandType type = CombatCommandType::STOP;
    CombatPoseIntent pose = CombatPoseIntent::NONE;
    CombatEffectType effect = CombatEffectType::NONE;
    EntityId target_id;
    Vector2 target_position;
    int damage = 0;
    // How a DEAL_DAMAGE hit arrives, so the target's mitigation can tell a round from a swing. Meaningless on every
    // other command type.
    DamageDelivery delivery = DamageDelivery::RANGED;
    Color color;
    ProjectileDamageConfig projectile;
};

struct AdvanceCombatOutput {
    CombatLogicState state;
    std::vector<CombatCommand> commands;
};

AdvanceCombatOutput advance_combat(const CombatConfig &config, const CombatLogicInput &input);

} // namespace defn

#endif