// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_PROJECTILE_H
#define SIM_PROJECTILE_H

#include "combat_types.h"
#include "content_values.h"
#include "projectile_flight.h"
#include "unit_side.h"

namespace defn {

// A shot in the air, from launch to detonation. Its position was frozen when it left the muzzle, so a target that
// keeps walking is missed -- the same behaviour ProjectileAttack has in the shipped game.
struct SimProjectile {
    EntityId id;
    EntityId source_id;
    EntityId direct_target_id;
    UnitSide shooter_side = UnitSide::FRIENDLY;
    ProjectileDamageConfig damage_config;
    int fallback_damage = 0;
    ProjectileFlight flight;
    bool exploded = false;
};

// What the shipped game keeps as a pending spawn on CombatRuntime: a shot that combat has committed to but that has
// not left the muzzle yet, because the shoot animation has not reached its spawn frame. Its shooter is frozen
// meanwhile: no attack, no movement.
struct SimPendingProjectile {
    bool active = false;
    EntityId target_id;
    Vector2 target_position;
    int fallback_damage = 0;
};

} // namespace defn

#endif
