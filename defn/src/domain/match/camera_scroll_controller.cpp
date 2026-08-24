// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "camera_scroll_controller.h"

#include <algorithm>

namespace defn {

namespace {

constexpr float HALF = 2.0F;
constexpr float CAMERA_SMOOTH_FACTOR = 3.0F;
// Below this the camera is treated as having arrived, and snaps to the target rather than creeping toward it.
constexpr float SNAP_DISTANCE = 1.0F;

} // namespace

void CameraScrollController::configure(const GameplayRules &rules, float world_width) {
    rules_ = rules;
    world_width_ = world_width;
    camera_target_x_ = get_min_target_x();
}

float CameraScrollController::calculate_world_width(float background_display_width) const {
    return background_display_width * static_cast<float>(rules_.world_multiplier);
}

float CameraScrollController::get_trigger_height() const { return (rules_.belt_bottom_y - rules_.belt_top_y) + rules_.scroll_trigger_extra_height; }

Vector2 CameraScrollController::get_camera_anchor_position() const { return {.x = camera_target_x_, .y = rules_.viewport_height / HALF}; }

Vector2 CameraScrollController::get_left_trigger_position() const {
    const float scroll_step = rules_.viewport_width * rules_.camera_scroll_step_factor;
    return {
        .x = camera_target_x_ - (rules_.viewport_width / HALF) + scroll_step,
        .y = (rules_.belt_top_y + rules_.belt_bottom_y) / HALF,
    };
}

Vector2 CameraScrollController::get_right_trigger_position() const {
    const float scroll_step = rules_.viewport_width * rules_.camera_scroll_step_factor;
    return {
        .x = camera_target_x_ + (rules_.viewport_width / HALF) - scroll_step,
        .y = (rules_.belt_top_y + rules_.belt_bottom_y) / HALF,
    };
}

Vector2 CameraScrollController::next_camera_position(const Vector2 &current_position, double delta) const {
    const float diff = camera_target_x_ - current_position.x;
    if (diff > SNAP_DISTANCE || diff < -SNAP_DISTANCE) {
        const float factor = std::min(static_cast<float>(CAMERA_SMOOTH_FACTOR * delta), 1.0F);
        return {.x = current_position.x + (diff * factor), .y = rules_.viewport_height / HALF};
    }

    return get_camera_anchor_position();
}

bool CameraScrollController::advance_target() {
    const float scroll_step = rules_.viewport_width * rules_.camera_scroll_step_factor;
    const float next_target = std::min(camera_target_x_ + scroll_step, get_max_target_x());
    const bool changed = next_target != camera_target_x_;
    camera_target_x_ = next_target;
    return changed;
}

bool CameraScrollController::retreat_target() {
    const float scroll_step = rules_.viewport_width * rules_.camera_scroll_step_factor;
    const float next_target = std::max(camera_target_x_ - scroll_step, get_min_target_x());
    const bool changed = next_target != camera_target_x_;
    camera_target_x_ = next_target;
    return changed;
}

float CameraScrollController::get_min_target_x() const { return rules_.viewport_width / HALF; }

float CameraScrollController::get_max_target_x() const { return std::max(get_min_target_x(), world_width_ - (rules_.viewport_width / HALF)); }

} // namespace defn
