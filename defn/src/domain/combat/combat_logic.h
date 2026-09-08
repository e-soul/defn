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
    //
    // `target_position` is filled on this path too, holding the pursued candidate, while `target_id` stays invalid.
    // Every rule downstream gates on the id, so nothing else can see the difference.
    bool pursuing = false;
    // Where this unit is headed, whether or not it can attack what is there yet: the target it is fighting, the
    // candidate it is pursuing, or -- having neither -- the nearest enemy ahead of it inside the sensor.
    //
    // The belt slide steers by this rather than by `target_id`, and that is the whole difference between a curve and
    // a snap. A unit only *selects* a target once it can attack it, which for a melee rusher is the last stride of a
    // run that started hundreds of pixels back; steering by selection would leave it charging straight down its own
    // lane and then stepping sideways on arrival. Steering by what it is walking at bends the whole approach.
    bool has_approach_target = false;
    Vector2 approach_position;
    // Where the enemy *army* stands relative to this unit, which is a different question from where its next target
    // is: structures are left out of all three, because a base is never a reason to give up on the army and is
    // exactly what a unit that has walked past the line would otherwise wander off toward.
    //
    // `army_ahead` is set while any of it is still in front at all; `army_beyond_standoff` while some of it is far
    // enough in front to walk into rather than merely on the correct side; `unpassed_army_position` is the nearest
    // enemy this unit has not yet put properly in front of itself -- the first one it meets on the way back.
    //
    // Only filled for a unit that can decline a target it could attack, because declining is the only way to end up
    // behind one. See `CombatConfig::has_role_preference`.
    bool army_ahead = false;
    bool army_beyond_standoff = false;
    bool has_unpassed_army = false;
    Vector2 unpassed_army_position;
};

// FALL_BACK is MOVE with the sign flipped: the unit walks against its side's advance, back toward the line it has
// overrun. Nothing else in the game moves that way of its own accord, which is why it is a separate intent rather
// than a direction on MOVE -- the presentation has to turn the sprite round for it.
enum class CombatMovementIntent { NONE, MOVE, FALL_BACK, STOP };

// Where the unit wants to stand on the belt's depth axis. Only the destination: how fast it gets there is a property
// of whatever is doing the moving, exactly as the forward axis already splits.
struct BeltSlideIntent {
    bool active = false;
    float target_y = 0.0F;
};

enum class CombatPoseState { WALK, ATTACK, SHOOT, OTHER };
enum class CombatPoseIntent { NONE, WALK, ATTACK, SHOOT };

struct CombatLogicState {
    double attack_cooldown_seconds = 0.0;
    AttackMode attack_mode = AttackMode::NONE;
    bool engaged = false;
    EntityId target_id;
    // Walking backward toward the enemy line this unit has overrun, and holding fire until it is in front again.
    // A mode rather than a per-frame test because the rule that starts it and the rule that ends it are different
    // ones: see `advance_combat_logic`.
    bool falling_back = false;
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
    BeltSlideIntent belt_slide;
    CombatPoseIntent pose = CombatPoseIntent::NONE;
    bool hide_muzzle_flash = false;
    bool trigger_attack = false;
};

struct CombatLogicStep {
    CombatLogicState state;
    CombatLogicIntent intent;
};

// Close enough to a lane to stop nudging toward it. Shares the reposition epsilon's reasoning: it exists to absorb
// float noise, not to define an arrival distance, because the step below can never overshoot.
inline constexpr float BELT_SLIDE_ARRIVAL_EPSILON = 0.01F;

float get_forward_distance(UnitSide side, const Vector2 &origin, const Vector2 &target_position);

// One frame of the depth-axis slide: the y this unit should now be standing on. Monotone by construction -- the step
// is clamped to the remaining gap, so convergence never overshoots and cannot oscillate around the target. A speed of
// zero is "this unit does not slide", which is every unit that has not opted in.
float advance_belt_slide(float current_y, float target_y, float speed_pixels_per_second, double delta, float epsilon = BELT_SLIDE_ARRIVAL_EPSILON);

// How far this unit senses, which is its aggro range floored at its ranged range. The detection sensor on both the
// real unit and the kernel is built from this, so widening aggro widens what target selection is even shown.
float resolve_aggro_range(const CombatConfig &config);

// Where this unit comes to rest against a target: the furthest it can reach with anything, which is exactly where
// walking forward stops. Read as a *destination* by the fall-back rule, so a unit that overran the line ends the
// manoeuvre standing where an ordinary approach would have left it instead of on top of its victim.
float engagement_standoff(const CombatConfig &config);
AttackMode classify_target_by_distance(const CombatConfig &config, float distance);
CombatTargetSelection select_target_from_snapshots(const Vector2 &origin, const CombatConfig &config, EntityId current_target_id,
                                                   std::span<const CombatTargetSnapshot> targets);
CombatLogicStep advance_combat_logic(const CombatConfig &config, const CombatLogicInput &input);

} // namespace defn

#endif
