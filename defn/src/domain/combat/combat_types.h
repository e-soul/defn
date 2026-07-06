#ifndef COMBAT_TYPES_H
#define COMBAT_TYPES_H

#include "unit_side.h"

#include <cstdint>
#include <optional>

namespace defn {

struct EntityId {
    uint64_t value = 0;

    [[nodiscard]] bool is_valid() const { return value != 0; }
    friend bool operator==(EntityId, EntityId) = default;
};

struct CombatPoint {
    float x = 0.0F;
    float y = 0.0F;
};

enum class AttackMode { NONE, MELEE, RANGED };

enum class SplashTargetRoundingMode { FLOOR, NEAREST, CEIL };

struct CombatColor {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

struct ProjectileDamageConfig {
    float splash_radius = 0.0F;
    float affected_fraction = 1.0F;
    int min_affected_targets = 1;
    SplashTargetRoundingMode affected_target_rounding = SplashTargetRoundingMode::NEAREST;
    bool include_direct_target = true;
    std::optional<int> impact_damage;
    std::optional<int> splash_damage;
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
    std::optional<ProjectileDamageConfig> projectile_attack;
};

} // namespace defn

#endif