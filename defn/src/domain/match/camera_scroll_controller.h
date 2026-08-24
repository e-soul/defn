// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef CAMERA_SCROLL_CONTROLLER_H
#define CAMERA_SCROLL_CONTROLLER_H

#include "content_values.h"
#include "gameplay_rules.h"

namespace defn {

// Where the camera is looking, and how it gets there. The camera is not a presentation detail: `GridManager` derives
// both spawn positions from it, so how far the belt has scrolled decides where enemies appear and where deployments
// land. Nothing here touches Godot, so the simulator scrolls exactly the way the game does.
//
// The target only moves when a unit crosses a trigger strip -- friendlies push it forward, hostiles pull it back. The
// player has no direct camera control.
class CameraScrollController {
  public:
    // The trigger strips are this wide; a unit's hitbox entering one scrolls the camera.
    static constexpr float TRIGGER_WIDTH = 20.0F;

    void configure(const GameplayRules &rules, float world_width);

    [[nodiscard]] float calculate_world_width(float background_display_width) const;
    [[nodiscard]] float get_world_width() const { return world_width_; }
    [[nodiscard]] float get_trigger_height() const;
    [[nodiscard]] Vector2 get_camera_anchor_position() const;
    [[nodiscard]] Vector2 get_left_trigger_position() const;
    [[nodiscard]] Vector2 get_right_trigger_position() const;

    // One smoothing step toward the current target. Returns where the camera should be after `delta`.
    [[nodiscard]] Vector2 next_camera_position(const Vector2 &current_position, double delta) const;
    bool advance_target();
    bool retreat_target();

  private:
    [[nodiscard]] float get_min_target_x() const;
    [[nodiscard]] float get_max_target_x() const;

    GameplayRules rules_{};
    float world_width_ = rules_.viewport_width * static_cast<float>(rules_.world_multiplier);
    float camera_target_x_ = rules_.viewport_width / 2.0F;
};

} // namespace defn

#endif
