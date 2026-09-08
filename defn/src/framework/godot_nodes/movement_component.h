// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MOVEMENT_COMPONENT_H
#define MOVEMENT_COMPONENT_H

#include "unit_side.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class MovementComponent : public Node {
    GDCLASS(MovementComponent, Node)

  public:
    void configure(Node2D *owner_node, UnitSide side, real_t speed_pixels_per_second, real_t belt_slide_speed_pixels_per_second = 0.0F);
    void move(double delta);
    // Against the advance: a unit walking back to the enemy line it has overrun. Facing is the caller's business,
    // because this component moves a node and knows nothing about sprites.
    void move_backward(double delta);
    // The depth axis, driven independently of `move`: an engaged unit has stopped walking and is still expected to
    // finish nosing onto its target's lane.
    void slide_toward_belt_y(real_t target_y, double delta);
    [[nodiscard]] bool move_toward_x(real_t destination_x, double delta);
    void stop();
    [[nodiscard]] real_t get_speed_pixels_per_second() const { return speed_pixels_per_second_; }
    [[nodiscard]] real_t get_belt_slide_speed_pixels_per_second() const { return belt_slide_speed_pixels_per_second_; }

  protected:
    static void _bind_methods();

  private:
    void walk(double delta, real_t direction);

    Node2D *owner_node_ = nullptr;
    UnitSide side_ = UnitSide::FRIENDLY;
    real_t speed_pixels_per_second_ = 0.0F;
    real_t belt_slide_speed_pixels_per_second_ = 0.0F;
};

} // namespace defn

#endif
