// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef COMBAT_TYPES_H
#define COMBAT_TYPES_H

#include "content_values.h"
#include "damage_rules.h"
#include "unit_side.h"

#include <array>
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
    // What this unit advertises itself as. Broadcast the same way `threat_weight` is -- the shooter reads it off the
    // snapshot -- because a role only means anything to somebody else.
    UnitRole role = UnitRole::NONE;
    // How this unit picks among the enemies it can reach. A property of the *shooter*.
    TargetPreference target_preference = TargetPreference::NEAREST;
    // How far this unit *notices* an enemy, as against how far it can hit one. Never smaller than `ranged_range`:
    // resolve_aggro_range clamps it, because a unit that could shoot further than it can see would stand idle beside a
    // target it was able to kill. Equal to `ranged_range` is the shipped default and means "no pursuit".
    //
    // The gap between the two is the whole mechanism. Everything walks forward and stops at the first thing it can
    // attack, so "advance on the target I actually want" needs no steering -- only a reason not to stop for a lesser
    // one, and a sensor wide enough to know the better one is out there before the lesser one is in reach.
    float aggro_range = 0.0F;
    // Per-role multipliers on a candidate's threat weight, so a role preference and a tank's pull compose instead of
    // overriding one another. All ones is "no preference", which leaves every score bit-identical.
    std::array<float, UNIT_ROLE_COUNT> role_bias{};
    Color melee_flash_color;
    Color ranged_flash_color;
    std::optional<ProjectileDamageConfig> projectile_attack;

    [[nodiscard]] float bias_for_role(UnitRole role) const {
        const float bias = role_bias.at(static_cast<std::size_t>(unit_role_index(role)));
        return bias > 0.0F ? bias : 1.0F;
    }

    // NONE is never preferred, whatever the table says. Otherwise one stray entry would make every unit that never
    // declared a role into a pursuit target, which is the opposite of an opt-in mechanic.
    [[nodiscard]] bool prefers_role(UnitRole role) const { return role != UnitRole::NONE && bias_for_role(role) > 1.0F; }
};

} // namespace defn

#endif