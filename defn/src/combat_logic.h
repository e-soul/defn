#ifndef COMBAT_LOGIC_H
#define COMBAT_LOGIC_H

#include "attack_target.h"
#include "combat_types.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <span>

namespace defn {

using namespace godot;

struct CombatTargetSnapshot {
    AttackTarget *target = nullptr;
    UnitSide side = UnitSide::FRIENDLY;
    bool dead = false;
    Vector2 position;
};

struct CombatTargetSelection {
    bool engaged = false;
    AttackMode attack_mode = AttackMode::NONE;
    AttackTarget *target = nullptr;
};

enum class CombatMovementIntent { NONE, MOVE, STOP };
enum class CombatPoseState { WALK, ATTACK, SHOOT, OTHER };
enum class CombatPoseIntent { NONE, WALK, ATTACK, SHOOT };

struct CombatLogicState {
    double attack_cooldown_seconds = 0.0;
    AttackMode attack_mode = AttackMode::NONE;
    bool engaged = false;
    AttackTarget *target = nullptr;
};

struct CombatLogicInput {
    CombatLogicState state;
    CombatTargetSelection selection;
    CombatPoseState current_pose = CombatPoseState::OTHER;
    double delta = 0.0;
    bool unit_dead = false;
    bool projectile_pending = false;
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

real_t get_forward_distance(UnitSide side, const Vector2 &origin, const Vector2 &target_position);
AttackMode classify_target_by_distance(const CombatConfig &config, real_t distance);
CombatTargetSelection select_target_from_snapshots(const Vector2 &origin, const CombatConfig &config, AttackTarget *current_target,
                                                   std::span<const CombatTargetSnapshot> targets);
CombatLogicStep advance_combat_logic(const CombatConfig &config, const CombatLogicInput &input);

} // namespace defn

#endif