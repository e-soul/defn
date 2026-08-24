// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_CAMERA_H
#define SIM_CAMERA_H

#include "camera_scroll_controller.h"
#include "sim_entity.h"
#include "sim_grid.h"

#include <span>

namespace defn {

enum class SimCameraMode {
    // Units push the camera along exactly as they do in game, so spawn positions move with the front line.
    MODELLED,
    // The camera never moves. Isolates everything else from scroll pacing.
    FIXED,
};

// The camera as a gameplay input rather than a view. `CameraScrollController` owns the scroll maths -- shared with the
// game -- and this adds the one thing the scene tree would otherwise provide: noticing that a unit has entered a
// trigger strip.
class SimCamera {
  public:
    void configure(const GameplayRules &rules, float world_width, SimCameraMode mode);

    // Fires triggers for anything that has just entered one, then takes a smoothing step and publishes the result to
    // the grid, which is what spawn and deploy positions are measured from.
    void update(double delta, SimGrid &grid, std::span<SimEntity> entities);

    [[nodiscard]] Vector2 get_position() const { return position_; }
    [[nodiscard]] int get_scroll_events() const { return scroll_events_; }

  private:
    [[nodiscard]] bool overlaps_trigger(const Vector2 &trigger_position, const Vector2 &unit_position) const;

    CameraScrollController controller_;
    SimCameraMode mode_ = SimCameraMode::MODELLED;
    Vector2 position_;
    int scroll_events_ = 0;
};

} // namespace defn

#endif
