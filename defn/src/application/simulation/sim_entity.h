// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_ENTITY_H
#define SIM_ENTITY_H

#include "combat_logic.h"
#include "combat_types.h"
#include "content_values.h"
#include "field_promotion_runtime.h"
#include "sim_projectile.h"
#include "unit_animation_state.h"
#include "unit_definition.h"
#include "unit_side.h"

#include <cstdint>
#include <optional>
#include <string>

namespace defn {

// One combatant in the simulated world. Everything the shipped game spreads across Unit, HealthComponent,
// MovementComponent, CombatComponent and AnimationController, flattened into a value the kernel can iterate.
struct SimEntity {
    EntityId id;
    std::string unit_id;
    UnitSide side = UnitSide::FRIENDLY;
    Vector2 position;

    int hp = 0;
    int max_hp = 0;
    // Flat reduction on every point of damage taken, from any source.
    int armour = 0;
    bool dead = false;
    // Godot copies the process group before walking it, so a node added during a frame first runs on the next one.
    std::uint64_t spawn_tick = 0;
    double spawn_time_seconds = 0.0;
    double death_time_seconds = 0.0;

    // Resolved per spawn, exactly as UnitFactory builds it: -1 ranges mean the unit has no attack of that kind.
    CombatConfig combat;
    // The sensor radius is the resolved ranged range even for melee-only units, mirroring DetectionComponent.
    float detection_radius = 0.0F;
    std::optional<ProjectileAttackConfig> projectile_attack;
    // Where a shot leaves the unit: the configured muzzle offset, already scaled by the unit's sprite scale. Zero for
    // every shipped unit today, but a long-range shooter with a real offset would otherwise mistime its flights.
    Vector2 muzzle_offset;
    float move_speed_pixels_per_second = 0.0F;
    int bounty = 0;
    bool combat_enabled = true;
    bool movement_enabled = true;
    // The scene tree reports trigger entry, not overlap, so the camera model needs the previous frame's answer.
    bool in_right_scroll_trigger = false;
    bool in_left_scroll_trigger = false;

    CombatLogicState combat_state;
    // Survives disengagement so a target that fled during the committed windup is still recognised as a chase.
    EntityId last_target_id;
    UnitAnimationState animation;
    FieldPromotionRuntime field_promotion;
    SimPendingProjectile pending_projectile;

    // Lab metrics. Kill credit has no counterpart in the shipped game, which only reports that a unit died.
    int damage_dealt = 0;
    int damage_taken = 0;
    int kills = 0;
    int attacks_landed = 0;
    int projectiles_launched = 0;

    [[nodiscard]] bool is_alive() const { return !dead; }
};

} // namespace defn

#endif
