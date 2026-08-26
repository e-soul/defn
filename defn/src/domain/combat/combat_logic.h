// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef COMBAT_LOGIC_H
#define COMBAT_LOGIC_H

#include "combat_types.h"

#include <span>

namespace defn {

struct CombatTargetSnapshot {
    EntityId id;
    UnitSide side = UnitSide::FRIENDLY;
    bool dead = false;
    Vector2 position;
    // What the shooter needs about this candidate beyond where it stands: how hard it pulls fire, how much of it is
    // left, and what kind of thing it is. All three default to "makes no difference", so a snapshot builder that
    // ignores them selects as before.
    float threat_weight = 1.0F;
    int health = 0;
    UnitRole role = UnitRole::NONE;
};

struct CombatTargetSelection {
    bool engaged = false;
    AttackMode attack_mode = AttackMode::NONE;
    EntityId target_id;
    Vector2 target_position;
    // Set when the shooter deliberately declined a target it could have attacked, because something it prefers is
    // sensed further ahead. `engaged` is false either way and the caller walks forward regardless -- this only exists
    // so a test, or a debug overlay, can tell "nothing to shoot" apart from "not stopping for that".
    bool pursuing = false;
};

enum class CombatMovementIntent { NONE, MOVE, STOP };
enum class CombatPoseState { WALK, ATTACK, SHOOT, OTHER };
enum class CombatPoseIntent { NONE, WALK, ATTACK, SHOOT };

struct CombatLogicState {
    double attack_cooldown_seconds = 0.0;
    AttackMode attack_mode = AttackMode::NONE;
    bool engaged = false;
    EntityId target_id;
};

struct CombatLogicInput {
    CombatLogicState state;
    CombatTargetSelection selection;
    CombatPoseState current_pose = CombatPoseState::OTHER;
    double delta = 0.0;
    bool unit_dead = false;
    bool projectile_pending = false;
    bool manual_repositioning = false;
    // Observed from the sprite: an attack animation is on screen, and it is still inside its committed windup frames.
    bool attack_animation_playing = false;
    bool attack_windup_active = false;
    // The unit's most recent target is still alive but no longer within any attack range, so it must be chased.
    bool target_out_of_range = false;
};

struct CombatLogicIntent {
    CombatMovementIntent movement = CombatMovementIntent::NONE;
    CombatPoseIntent pose = CombatPoseIntent::NONE;
    bool hide_muzzle_flash = false;
    bool trigger_attack = false;
};

struct CombatLogicStep {
    CombatLogicState state;
    CombatLogicIntent intent;
};

float get_forward_distance(UnitSide side, const Vector2 &origin, const Vector2 &target_position);

// How far this unit senses, which is its aggro range floored at its ranged range. The detection sensor on both the
// real unit and the kernel is built from this, so widening aggro widens what target selection is even shown.
float resolve_aggro_range(const CombatConfig &config);
AttackMode classify_target_by_distance(const CombatConfig &config, float distance);
CombatTargetSelection select_target_from_snapshots(const Vector2 &origin, const CombatConfig &config, EntityId current_target_id,
                                                   std::span<const CombatTargetSnapshot> targets);
CombatLogicStep advance_combat_logic(const CombatConfig &config, const CombatLogicInput &input);

} // namespace defn

#endif
