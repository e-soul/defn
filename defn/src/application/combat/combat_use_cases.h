#ifndef COMBAT_USE_CASES_H
#define COMBAT_USE_CASES_H

#include "combat_logic.h"

#include <vector>

namespace defn {

enum class CombatCommandType { STOP, MOVE, PLAY_POSE, HIDE_MUZZLE_FLASH, DEAL_DAMAGE, SPAWN_PROJECTILE, PLAY_EFFECT };
enum class CombatEffectType { NONE, MELEE_ATTACK, RANGED_SHOOT, DAMAGE_FLASH };

struct CombatCommand {
    CombatCommandType type = CombatCommandType::STOP;
    CombatPoseIntent pose = CombatPoseIntent::NONE;
    CombatEffectType effect = CombatEffectType::NONE;
    EntityId target_id;
    Vector2 target_position;
    int damage = 0;
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