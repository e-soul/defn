#ifndef COMBAT_ATTACK_EXECUTOR_H
#define COMBAT_ATTACK_EXECUTOR_H

#include "attack_target.h"
#include "combat_types.h"

namespace defn {

class AnimationController;
class BattleEntity;

class CombatAttackExecutor {
  public:
    static void spawn_pending_projectile(const CombatConfig &config, BattleEntity *unit, AnimationController *animation, PendingProjectileSpawn &pending_projectile);
    static void trigger_attack(const CombatConfig &config, AttackMode attack_mode, AttackTarget *target, AnimationController *animation,
                               PendingProjectileSpawn &pending_projectile);
};

} // namespace defn

#endif