#include "combat_runtime.h"

#include "animation_controller.h"
#include "battle_entity.h"
#include "combat_attack_executor.h"
#include "combat_target_selector.h"
#include "health_component.h"
#include "movement_component.h"

#include <algorithm>

namespace defn {

void CombatRuntime::configure(BattleEntity *unit, HealthComponent *health, AnimationController *animation, Area2D *detection_area, const CombatConfig &config,
                              const std::optional<ProjectileAttackConfig> &projectile_attack) {
    unit_ = unit;
    health_ = health;
    animation_ = animation;
    detection_area_ = detection_area;
    config_ = config;
    projectile_attack_ = projectile_attack;
    selection_ = {};
    state_ = {};
    pending_projectile_ = {};
}

void CombatRuntime::update(double delta) {
    if (health_ == nullptr || health_->is_dead()) {
        return;
    }

    try_spawn_pending_projectile();
    update_target();

    CombatLogicInput input;
    input.state = state_;
    input.selection = selection_;
    input.current_pose = animation_ != nullptr ? map_pose_state(animation_->get_anim_state()) : CombatPoseState::OTHER;
    input.delta = delta;
    input.unit_dead = health_->is_dead();
    input.projectile_pending = pending_projectile_.active;

    apply_commands(advance_combat(config_, input), delta);
}

void CombatRuntime::update_target() { selection_ = CombatTargetSelector::select(unit_, detection_area_, config_, state_.target_id); }

void CombatRuntime::try_spawn_pending_projectile() {
    CombatAttackExecutor::spawn_pending_projectile(projectile_attack_, unit_, animation_, pending_projectile_);
}

void CombatRuntime::apply_commands(const AdvanceCombatOutput &output, double delta) {
    state_ = output.state;

    for (const CombatCommand &command : output.commands) {
        apply_command(command, delta);
    }

    try_spawn_pending_projectile();
}

void CombatRuntime::apply_command(const CombatCommand &command, double delta) {
    if (unit_ == nullptr) {
        return;
    }

    switch (command.type) {
    case CombatCommandType::STOP:
        if (auto *movement = unit_->get_movement_component(); movement != nullptr) {
            movement->stop();
        }
        break;
    case CombatCommandType::MOVE:
        if (auto *movement = unit_->get_movement_component(); movement != nullptr) {
            movement->move(delta);
        }
        break;
    case CombatCommandType::PLAY_POSE:
        if (animation_ == nullptr) {
            break;
        }
        switch (command.pose) {
        case CombatPoseIntent::WALK:
            animation_->set_anim_state(AnimState::WALK);
            break;
        case CombatPoseIntent::ATTACK:
            animation_->hold_anim_state(AnimState::ATTACK);
            break;
        case CombatPoseIntent::SHOOT:
            animation_->hold_anim_state(AnimState::SHOOT);
            break;
        case CombatPoseIntent::NONE:
            break;
        }
        break;
    case CombatCommandType::HIDE_MUZZLE_FLASH:
        if (animation_ != nullptr) {
            animation_->hide_muzzle_flash();
        }
        break;
    case CombatCommandType::DEAL_DAMAGE:
    case CombatCommandType::SPAWN_PROJECTILE:
    case CombatCommandType::PLAY_EFFECT:
        CombatAttackExecutor::apply_command(command, projectile_attack_, config_.side, animation_, pending_projectile_);
        break;
    }
}

CombatPoseState CombatRuntime::map_pose_state(AnimState state) {
    switch (state) {
    case AnimState::WALK:
        return CombatPoseState::WALK;
    case AnimState::ATTACK:
        return CombatPoseState::ATTACK;
    case AnimState::SHOOT:
        return CombatPoseState::SHOOT;
    case AnimState::DEATH:
        return CombatPoseState::OTHER;
    }

    return CombatPoseState::OTHER;
}

} // namespace defn