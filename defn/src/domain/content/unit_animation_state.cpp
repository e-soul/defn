// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "unit_animation_state.h"

#include <algorithm>

namespace defn {

namespace {

std::string_view animation_name_for(UnitPose pose) {
    switch (pose) {
    case UnitPose::WALK:
        return WALK_ANIMATION;
    case UnitPose::ATTACK:
        return ATTACK_ANIMATION;
    case UnitPose::SHOOT:
        return SHOOT_ANIMATION;
    case UnitPose::DEATH:
        return DEATH_ANIMATION;
    }

    return WALK_ANIMATION;
}

} // namespace

void UnitAnimationState::configure(std::vector<std::pair<std::string, AnimConfig>> animations) {
    animations_ = std::move(animations);
    current_animation_.clear();
    clock_ = {};
    pose_ = UnitPose::WALK;
    shoot_effect_pending_ = false;
    shoot_effect_ready_ = false;
    shoot_effect_frame_ = 0;
}

const AnimConfig *UnitAnimationState::find_animation(std::string_view name) const {
    for (const auto &[anim_name, anim_cfg] : animations_) {
        if (anim_name == name) {
            return &anim_cfg;
        }
    }

    return nullptr;
}

void UnitAnimationState::apply_animation(std::string_view name, Start start) {
    const AnimConfig *config = find_animation(name);
    if (config == nullptr || config->frame_count <= 0) {
        // Mirrors AnimatedSprite2D::play, which refuses an unknown or empty animation and leaves the current one alone.
        return;
    }

    const bool changed = current_animation_ != name;
    if (changed) {
        current_animation_ = name;
        if (name != SHOOT_ANIMATION) {
            shoot_effect_pending_ = false;
        }
    }

    switch (start) {
    case Start::HOLD:
        clock_.hold(*config);
        break;
    case Start::RESTART:
        clock_.play(*config);
        break;
    case Start::RESUME:
        // A switch of animation always starts from the top, exactly as AnimatedSprite2D::play does.
        if (changed) {
            clock_.play(*config);
        } else {
            clock_.resume(*config);
        }
        break;
    }
}

void UnitAnimationState::set_pose(UnitPose pose) {
    if (pose_ == UnitPose::DEATH) {
        return;
    }
    pose_ = pose;
    apply_animation(animation_name_for(pose), Start::RESUME);
}

void UnitAnimationState::hold_pose(UnitPose pose) {
    if (pose_ == UnitPose::DEATH) {
        return;
    }
    pose_ = pose;
    apply_animation(animation_name_for(pose), Start::HOLD);
}

void UnitAnimationState::play_attack() {
    if (pose_ == UnitPose::DEATH) {
        return;
    }
    pose_ = UnitPose::ATTACK;
    apply_animation(ATTACK_ANIMATION, Start::RESTART);
}

UnitAnimationUpdate UnitAnimationState::play_shoot(int effect_frame) {
    if (pose_ == UnitPose::DEATH) {
        return {};
    }
    pose_ = UnitPose::SHOOT;
    shoot_effect_ready_ = false;
    shoot_effect_pending_ = false;

    const AnimConfig *config = find_animation(SHOOT_ANIMATION);
    if (config == nullptr || config->frame_count <= 0) {
        // There is no animation to time the shot against, so it is released at once.
        shoot_effect_ready_ = true;
        return {.shoot_effect_fired = true};
    }

    apply_animation(SHOOT_ANIMATION, Start::RESTART);
    shoot_effect_frame_ = std::clamp(effect_frame, 0, config->frame_count - 1);
    shoot_effect_pending_ = true;
    return update_shoot_effect();
}

bool UnitAnimationState::play_named(std::string_view name, bool restart) {
    const AnimConfig *config = find_animation(name);
    if (config == nullptr || config->frame_count <= 0) {
        return false;
    }

    shoot_effect_pending_ = false;
    shoot_effect_ready_ = false;
    apply_animation(name, restart ? Start::RESTART : Start::RESUME);
    return true;
}

void UnitAnimationState::cancel_pending_attack() {
    shoot_effect_pending_ = false;
    shoot_effect_ready_ = false;
    set_pose(UnitPose::WALK);
}

UnitAnimationUpdate UnitAnimationState::advance(double delta) {
    clock_.advance(delta);
    return update_shoot_effect();
}

UnitAnimationUpdate UnitAnimationState::update_shoot_effect() {
    if (!shoot_effect_pending_) {
        return {};
    }

    if (current_animation_ != SHOOT_ANIMATION) {
        shoot_effect_pending_ = false;
        return {};
    }

    if (clock_.frame() < shoot_effect_frame_) {
        return {};
    }

    shoot_effect_pending_ = false;
    shoot_effect_ready_ = true;
    return {.shoot_effect_fired = true};
}

bool UnitAnimationState::consume_shoot_effect_triggered() {
    if (!shoot_effect_ready_) {
        return false;
    }

    shoot_effect_ready_ = false;
    return true;
}

bool UnitAnimationState::is_attack_animation_playing() const {
    return clock_.is_playing() && (current_animation_ == ATTACK_ANIMATION || current_animation_ == SHOOT_ANIMATION);
}

bool UnitAnimationState::is_attack_windup_active() const { return is_attack_animation_playing() && clock_.is_windup_active(); }

CombatPoseState to_combat_pose_state(UnitPose pose) {
    switch (pose) {
    case UnitPose::WALK:
        return CombatPoseState::WALK;
    case UnitPose::ATTACK:
        return CombatPoseState::ATTACK;
    case UnitPose::SHOOT:
        return CombatPoseState::SHOOT;
    case UnitPose::DEATH:
        return CombatPoseState::OTHER;
    }

    return CombatPoseState::OTHER;
}

} // namespace defn
