// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "movement_component.h"

#include "combat_logic.h"
#include "reposition_logic.h"

namespace defn {

void MovementComponent::_bind_methods() {}

void MovementComponent::configure(Node2D *owner_node, UnitSide side, real_t speed_pixels_per_second, real_t belt_slide_speed_pixels_per_second) {
    owner_node_ = owner_node;
    side_ = side;
    speed_pixels_per_second_ = speed_pixels_per_second;
    belt_slide_speed_pixels_per_second_ = belt_slide_speed_pixels_per_second;
}

void MovementComponent::move(double delta) {
    if (owner_node_ == nullptr || speed_pixels_per_second_ <= 0.0F || delta <= 0.0) {
        stop();
        return;
    }

    // Each side walks toward the other, and neither runs out of ground: the belt is unbounded to the right, and a
    // friendly that advances into the trigger strip takes the camera with it.
    const real_t displacement = speed_pixels_per_second_ * static_cast<real_t>(delta);
    godot::Vector2 position = owner_node_->get_position();
    position.x += side_ == UnitSide::FRIENDLY ? displacement : -displacement;
    owner_node_->set_position(position);
}

void MovementComponent::slide_toward_belt_y(real_t target_y, double delta) {
    if (owner_node_ == nullptr || belt_slide_speed_pixels_per_second_ <= 0.0F) {
        return;
    }

    godot::Vector2 position = owner_node_->get_position();
    position.y =
        advance_belt_slide(static_cast<float>(position.y), static_cast<float>(target_y), static_cast<float>(belt_slide_speed_pixels_per_second_), delta);
    owner_node_->set_position(position);
}

bool MovementComponent::move_toward_x(real_t destination_x, double delta) {
    if (owner_node_ == nullptr) {
        return false;
    }

    godot::Vector2 position = owner_node_->get_position();
    const RepositionState state{.mode = UnitControlMode::REPOSITIONING, .destination_x = destination_x};
    const RepositionStep step = advance_reposition(state, position.x, speed_pixels_per_second_, delta, static_cast<float>(REPOSITION_ARRIVAL_EPSILON));
    position.x = step.next_x;
    owner_node_->set_position(position);
    return step.arrived;
}

void MovementComponent::stop() {}

} // namespace defn
