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
};

struct CombatTargetSelection {
    bool engaged = false;
    AttackMode attack_mode = AttackMode::NONE;
    EntityId target_id;
    Vector2 target_position;
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
AttackMode classify_target_by_distance(const CombatConfig &config, float distance);
CombatTargetSelection select_target_from_snapshots(const Vector2 &origin, const CombatConfig &config, EntityId current_target_id,
                                                   std::span<const CombatTargetSnapshot> targets);
CombatLogicStep advance_combat_logic(const CombatConfig &config, const CombatLogicInput &input);

} // namespace defn

#endif