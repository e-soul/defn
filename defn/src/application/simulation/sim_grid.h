// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_GRID_H
#define SIM_GRID_H

#include "gameplay_rules.h"
#include "random_source.h"
#include "runtime_service_interfaces.h"

namespace defn {

// The engine-free twin of `GridManager`: the same spawn geometry, answered from the same rules. Both spawn positions
// are relative to where the camera is looking, which is why the camera has to be modelled at all.
class SimGrid final : public GridQueryService {
  public:
    SimGrid(const GameplayRules &rules, RandomSource &random) : rules_(rules), random_(&random) {
        world_width_ = rules_.viewport_width * static_cast<float>(rules_.world_multiplier);
        camera_x_ = rules_.viewport_width / 2.0F;
    }

    [[nodiscard]] const GameplayRules &get_rules() const { return rules_; }

    void set_world_width(float world_width) { world_width_ = world_width; }
    [[nodiscard]] float get_world_width() const { return world_width_; }
    void set_camera_x(float camera_x) { camera_x_ = camera_x; }
    [[nodiscard]] float get_camera_x() const { return camera_x_; }

    [[nodiscard]] double deploy_x() const override { return camera_x_ - (rules_.viewport_width / 2.0F) - rules_.spawn_offset; }
    [[nodiscard]] double spawn_x() const override { return camera_x_ + (rules_.viewport_width / 2.0F) + rules_.spawn_offset; }
    [[nodiscard]] double sample_belt_y() const override { return random_->range_real(rules_.belt_top_y, rules_.belt_bottom_y); }

  private:
    GameplayRules rules_;
    RandomSource *random_;
    float world_width_ = 0.0F;
    float camera_x_ = 0.0F;
};

} // namespace defn

#endif
