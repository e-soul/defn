// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef COMBAT_TYPES_H
#define COMBAT_TYPES_H

#include "content_values.h"
#include "damage_rules.h"
#include "unit_side.h"

#include <cstdint>
#include <optional>

namespace defn {

struct EntityId {
    uint64_t value = 0;

    [[nodiscard]] bool is_valid() const { return value != 0; }
    bool operator==(const EntityId &other) const { return value == other.value; }
};

enum class AttackMode { NONE, MELEE, RANGED };

enum class SplashTargetRoundingMode { FLOOR, NEAREST, CEIL };

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
    // A dead zone inside which the ranged attack cannot be used at all. Zero means no minimum, which is every unit
    // shipped before this existed.
    //
    // This is the first rule in the game that charges a unit for an advantage. Reach, damage, speed and durability
    // are all strictly-more-is-better, which is why the roster is a ladder in whichever of them dominates: a total
    // order cannot produce a matchup. A minimum range gives the long shot a weakness that a short-ranged unit stands
    // in front of and covers -- which is a pair that is worth more than either half, and the only shape the
    // composition premium can actually detect.
    float minimum_ranged_range = 0.0F;
    // How hard this unit pulls enemy fire toward itself. A property of the *target*, read off the snapshot rather
    // than off the shooter, and the whole of the tank role: unit A changes where damage lands on unit B.
    float threat_weight = 1.0F;
    // How this unit picks among the enemies it can reach. A property of the *shooter*.
    TargetPreference target_preference = TargetPreference::NEAREST;
    Color melee_flash_color;
    Color ranged_flash_color;
    std::optional<ProjectileDamageConfig> projectile_attack;
};

} // namespace defn

#endif