#ifndef COMBAT_TYPES_H
#define COMBAT_TYPES_H

#include "unit_data.h"

#include <godot_cpp/core/object_id.hpp>

namespace defn {

using namespace godot;

enum class AttackMode { NONE, MELEE, RANGED };

struct CombatConfig {
    UnitSide side = UnitSide::FRIENDLY;
    int melee_damage = 0;
    double melee_attack_period_seconds = 0.0;
    int ranged_damage = 0;
    double ranged_attack_period_seconds = 0.0;
    real_t attack_range = 0.0F;
    real_t ranged_range = 0.0F;
    Color melee_flash_color;
    Color ranged_flash_color;
    std::optional<ProjectileAttackConfig> projectile_attack;
};

struct PendingProjectileSpawn {
    bool active = false;
    ObjectID target_id{};
    Vector2 target_global_position;
};

} // namespace defn

#endif