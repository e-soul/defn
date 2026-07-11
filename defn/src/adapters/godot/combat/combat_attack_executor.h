#ifndef COMBAT_ATTACK_EXECUTOR_H
#define COMBAT_ATTACK_EXECUTOR_H

#include "combat_use_cases.h"
#include "unit_definition.h"

#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <optional>

namespace defn {

using namespace godot;

class AnimationController;
class BattleEntity;

struct PendingProjectileSpawn {
    bool active = false;
    EntityId target_id{};
    UnitSide shooter_side = UnitSide::FRIENDLY;
    godot::ObjectID source_id{};
    godot::Vector2 target_global_position;
    int fallback_damage = 0;
    Color flash_color;
};

class CombatAttackExecutor {
  public:
    static void spawn_pending_projectile(const std::optional<ProjectileAttackConfig> &projectile_config, BattleEntity *unit, AnimationController *animation,
                                         PendingProjectileSpawn &pending_projectile);
    static void apply_command(const CombatCommand &command, const std::optional<ProjectileAttackConfig> &projectile_config, BattleEntity *source,
                              UnitSide shooter_side, AnimationController *animation, PendingProjectileSpawn &pending_projectile);
};

} // namespace defn

#endif
