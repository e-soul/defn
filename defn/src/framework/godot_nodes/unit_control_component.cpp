// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "unit_control_component.h"

#include "animation_controller.h"
#include "combat_component.h"
#include "movement_component.h"
#include "unit.h"

namespace defn {

void UnitControlComponent::_bind_methods() {}

void UnitControlComponent::configure(Unit *unit, MovementComponent *movement, AnimationController *animation, CombatComponent *combat) {
    unit_ = unit;
    movement_ = movement;
    animation_ = animation;
    combat_ = combat;
}

bool UnitControlComponent::request_reposition(real_t destination_x, float arrival_epsilon) {
    if (canceled_ || unit_ == nullptr || movement_ == nullptr || unit_->is_dead() || unit_->is_queued_for_deletion()) {
        return false;
    }

    const RepositionRequest request = defn::request_reposition(state_, unit_->get_position().x, destination_x);
    if (!request.accepted) {
        return false;
    }

    state_ = request.state;
    arrival_epsilon_ = arrival_epsilon;
    apply_request_intents(request.intents);
    return true;
}

void UnitControlComponent::cancel_without_combat_resume() {
    if (canceled_) {
        return;
    }

    state_ = cancel_reposition(state_);
    canceled_ = true;
    set_process(false);
    if (movement_ != nullptr) {
        movement_->stop();
    }
}

void UnitControlComponent::_process(double delta) {
    if (canceled_ || state_.mode != UnitControlMode::REPOSITIONING) {
        return;
    }
    if (unit_ == nullptr || movement_ == nullptr || unit_->is_dead() || unit_->is_queued_for_deletion()) {
        cancel_without_combat_resume();
        return;
    }

    const RepositionStep step = advance_reposition(state_, unit_->get_position().x, movement_->get_speed_pixels_per_second(), delta, arrival_epsilon_);
    (void)movement_->move_toward_x(state_.destination_x, delta);
    state_ = step.state;
    if (step.arrived) {
        finish_reposition(step);
    }
}

void UnitControlComponent::apply_request_intents(const RepositionIntents &intents) {
    if (intents.suspend_combat && combat_ != nullptr) {
        combat_->begin_manual_reposition();
    }
    if (animation_ != nullptr) {
        if (intents.face_backward) {
            animation_->set_facing(FacingDirection::BACKWARD);
        }
        if (intents.walk) {
            animation_->set_anim_state(AnimState::WALK);
        }
    }
}

void UnitControlComponent::finish_reposition(const RepositionStep &step) {
    if (animation_ != nullptr && step.intents.face_forward) {
        animation_->set_facing(FacingDirection::FORWARD);
    }
    if (combat_ != nullptr && step.intents.resume_combat) {
        combat_->end_manual_reposition();
    }
}

} // namespace defn
