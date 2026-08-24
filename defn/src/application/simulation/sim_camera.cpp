// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_camera.h"

#include <cmath>

namespace defn {

namespace {

// HitboxComponent gives every unit a 5-pixel hitbox in world space, whatever its sprite scale.
constexpr float UNIT_HITBOX_RADIUS = 5.0F;

} // namespace

void SimCamera::configure(const GameplayRules &rules, float world_width, SimCameraMode mode) {
    controller_.configure(rules, world_width);
    mode_ = mode;
    position_ = controller_.get_camera_anchor_position();
    scroll_events_ = 0;
}

bool SimCamera::overlaps_trigger(const Vector2 &trigger_position, const Vector2 &unit_position) const {
    // Rectangle against circle, the shape pairing the shipped Area2D trigger and unit hitbox actually use.
    const float half_width = (CameraScrollController::TRIGGER_WIDTH / 2.0F) + UNIT_HITBOX_RADIUS;
    const float half_height = (controller_.get_trigger_height() / 2.0F) + UNIT_HITBOX_RADIUS;
    return std::fabs(unit_position.x - trigger_position.x) <= half_width && std::fabs(unit_position.y - trigger_position.y) <= half_height;
}

void SimCamera::update(double delta, SimGrid &grid, std::span<SimEntity> entities) {
    if (mode_ == SimCameraMode::FIXED) {
        grid.set_camera_x(position_.x);
        return;
    }

    const Vector2 right_trigger = controller_.get_right_trigger_position();
    const Vector2 left_trigger = controller_.get_left_trigger_position();

    // Area2D reports entry, not overlap, so each unit only scrolls the camera on the frame it crosses in.
    for (SimEntity &entity : entities) {
        const bool alive = !entity.dead;
        const bool in_right = alive && entity.side == UnitSide::FRIENDLY && overlaps_trigger(right_trigger, entity.position);
        const bool in_left = alive && entity.side == UnitSide::HOSTILE && overlaps_trigger(left_trigger, entity.position);

        if (in_right && !entity.in_right_scroll_trigger && controller_.advance_target()) {
            ++scroll_events_;
        }
        if (in_left && !entity.in_left_scroll_trigger && controller_.retreat_target()) {
            ++scroll_events_;
        }

        entity.in_right_scroll_trigger = in_right;
        entity.in_left_scroll_trigger = in_left;
    }

    position_ = controller_.next_camera_position(position_, delta);
    grid.set_camera_x(position_.x);
}

} // namespace defn
