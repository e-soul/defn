// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "movement_component.h"

#include "grid_manager.h"
#include "reposition_logic.h"

namespace defn {

void MovementComponent::_bind_methods() {}

void MovementComponent::configure(Node2D *owner_node, UnitSide side, real_t speed_pixels_per_second) {
    owner_node_ = owner_node;
    side_ = side;
    speed_pixels_per_second_ = speed_pixels_per_second;
}

void MovementComponent::move(double delta) {
    if (owner_node_ == nullptr || speed_pixels_per_second_ <= 0.0F || delta <= 0.0) {
        stop();
        return;
    }

    auto *grid = GridManager::get_singleton();
    if (grid == nullptr) {
        stop();
        return;
    }

    const real_t displacement = speed_pixels_per_second_ * static_cast<real_t>(delta);
    Vector2 position = owner_node_->get_position();

    if (side_ == UnitSide::FRIENDLY) {
        const auto &rules = grid->get_rules();
        const real_t max_x = grid->get_world_width() - rules.friendly_world_margin;
        if (position.x < max_x) {
            position.x = std::min(position.x + displacement, max_x);
            owner_node_->set_position(position);
            return;
        }

        stop();
        return;
    }

    position.x -= displacement;
    owner_node_->set_position(position);
}

bool MovementComponent::move_toward_x(real_t destination_x, double delta) {
    if (owner_node_ == nullptr) {
        return false;
    }

    Vector2 position = owner_node_->get_position();
    const RepositionState state{.mode = UnitControlMode::REPOSITIONING, .destination_x = destination_x};
    const RepositionStep step = advance_reposition(state, position.x, speed_pixels_per_second_, delta, static_cast<float>(REPOSITION_ARRIVAL_EPSILON));
    position.x = step.next_x;
    owner_node_->set_position(position);
    return step.arrived;
}

void MovementComponent::stop() {}

} // namespace defn
