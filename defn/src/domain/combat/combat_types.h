#ifndef COMBAT_TYPES_H
#define COMBAT_TYPES_H

#include "unit_data.h"

namespace defn {

enum class AttackMode { NONE, MELEE, RANGED };

struct CombatColor {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

struct CombatConfig {
    UnitSide side = UnitSide::FRIENDLY;
    int melee_damage = 0;
    double melee_attack_period_seconds = 0.0;
    int ranged_damage = 0;
    double ranged_attack_period_seconds = 0.0;
    float attack_range = 0.0F;
    float ranged_range = 0.0F;
    CombatColor melee_flash_color;
    CombatColor ranged_flash_color;
    std::optional<ProjectileAttackConfig> projectile_attack;
};

} // namespace defn

#endif