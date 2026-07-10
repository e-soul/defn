#ifndef CAMERA_SCROLL_CONTROLLER_H
#define CAMERA_SCROLL_CONTROLLER_H

#include "gameplay_rules.h"

#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

class GridManager;

class CameraScrollController {
  public:
    void configure(const GameplayRules &rules, real_t world_width);

    real_t calculate_world_width(real_t background_display_width) const;
    real_t get_world_width() const { return world_width_; }
    real_t get_trigger_height() const;
    godot::Vector2 get_camera_anchor_position() const;
    godot::Vector2 get_left_trigger_position() const;
    godot::Vector2 get_right_trigger_position() const;

    void update_camera(Camera2D *camera, GridManager *grid, double delta) const;
    bool advance_target();
    bool retreat_target();

  private:
    real_t get_min_target_x() const;
    real_t get_max_target_x() const;

    GameplayRules rules_{};
    real_t world_width_ = rules_.viewport_width * static_cast<real_t>(rules_.world_multiplier);
    real_t camera_target_x_ = rules_.viewport_width / 2.0F;
};

} // namespace defn

#endif