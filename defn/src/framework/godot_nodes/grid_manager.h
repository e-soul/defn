// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef GRID_MANAGER_H
#define GRID_MANAGER_H

#include "gameplay_rules.h"
#include "random_source.h"
#include "runtime_service_interfaces.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class GridManager : public Object, public GridQueryService {
    GDCLASS(GridManager, Object)

  public:
    GridManager() = default;

    static GridManager *get_singleton();
    static void register_singleton();
    static void unregister_singleton();

    void configure(const GameplayRules &rules, float belt_endpoint_a_ratio, float belt_endpoint_b_ratio);
    const GameplayRules &get_rules() const { return rules_; }

    // Belt-Y sampling is the one place the grid draws randomness. It goes through the port so a run can be seeded.
    void set_random_source(RandomSource *random);

    [[nodiscard]] double deploy_x() const override; // friendly spawn: just left of camera
    [[nodiscard]] double spawn_x() const override;  // hostile spawn: just right of camera
    [[nodiscard]] double sample_belt_y() const override;

    void set_world_width(real_t w);
    real_t get_world_width() const;
    void set_camera_x(real_t x);
    real_t get_camera_x() const;

  protected:
    static void _bind_methods();

  private:
    static GridManager *singleton_;

    GameplayRules rules_{};
    StdRandomSource default_random_;
    RandomSource *random_ = &default_random_;
    real_t world_width_ = rules_.viewport_width * static_cast<real_t>(rules_.world_multiplier);
    real_t camera_x_ = rules_.viewport_width / 2.0F;
};

} // namespace defn

#endif
