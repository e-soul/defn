#ifndef COMBAT_ATTACK_EXECUTOR_H
#define COMBAT_ATTACK_EXECUTOR_H

#include "attack_target.h"
#include "combat_types.h"

#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

class AnimationController;
class BattleEntity;

struct PendingProjectileSpawn {
  bool active = false;
  ObjectID target_id{};
  Vector2 target_global_position;
};

class CombatAttackExecutor {
  public:
    static void spawn_pending_projectile(const CombatConfig &config, BattleEntity *unit, AnimationController *animation, PendingProjectileSpawn &pending_projectile);
    static void trigger_attack(const CombatConfig &config, AttackMode attack_mode, AttackTarget *target, AnimationController *animation,
                               PendingProjectileSpawn &pending_projectile);
};

} // namespace defn

#endif