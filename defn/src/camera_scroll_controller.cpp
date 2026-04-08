#include "camera_scroll_controller.h"

#include "grid_manager.h"

#include <algorithm>

namespace defn {

namespace {

constexpr real_t HALF = 2.0F;
constexpr real_t CAMERA_SMOOTH_FACTOR = 3.0F;

} // namespace

void CameraScrollController::configure(const GameplayRules &rules, real_t world_width) {
    rules_ = rules;
    world_width_ = world_width;
    camera_target_x_ = rules_.viewport_width / HALF;
}

real_t CameraScrollController::calculate_world_width(real_t background_display_width) const {
    return background_display_width * static_cast<real_t>(rules_.world_multiplier);
}

real_t CameraScrollController::get_trigger_height() const {
    return (rules_.belt_bottom_y - rules_.belt_top_y) + rules_.scroll_trigger_extra_height;
}

Vector2 CameraScrollController::get_camera_anchor_position() const { return {camera_target_x_, rules_.viewport_height / HALF}; }

Vector2 CameraScrollController::get_trigger_position() const {
    const real_t scroll_step = rules_.viewport_width * rules_.camera_scroll_step_factor;
    const real_t trigger_x = camera_target_x_ + (rules_.viewport_width / HALF) - scroll_step;
    const real_t trigger_y = (rules_.belt_top_y + rules_.belt_bottom_y) / HALF;
    return {trigger_x, trigger_y};
}

void CameraScrollController::update_camera(Camera2D *camera, GridManager *grid, double delta) const {
    if (camera == nullptr || grid == nullptr) {
        return;
    }

    const real_t current_x = camera->get_position().x;
    const real_t diff = camera_target_x_ - current_x;
    if (diff > 1.0F || diff < -1.0F) {
        auto factor = static_cast<real_t>(CAMERA_SMOOTH_FACTOR * delta);
        factor = std::min(factor, 1.0F);
        const real_t new_x = current_x + (diff * factor);
        camera->set_position({new_x, rules_.viewport_height / HALF});
    } else {
        camera->set_position(get_camera_anchor_position());
    }

    grid->set_camera_x(camera->get_position().x);
}

bool CameraScrollController::advance_target() {
    const real_t scroll_step = rules_.viewport_width * rules_.camera_scroll_step_factor;
    const real_t max_target = world_width_ - (rules_.viewport_width / HALF);
    const real_t next_target = std::min(camera_target_x_ + scroll_step, max_target);
    const bool changed = next_target != camera_target_x_;
    camera_target_x_ = next_target;
    return changed;
}

} // namespace defn