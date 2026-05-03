#ifndef MOVEMENT_COMPONENT_H
#define MOVEMENT_COMPONENT_H

#include "unit_data.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class MovementComponent : public Node {
    GDCLASS(MovementComponent, Node)

  public:
    void configure(Node2D *owner_node, UnitSide side, real_t speed_pixels_per_second);
    void move(double delta);
    void stop();

  protected:
    static void _bind_methods();

  private:
    Node2D *owner_node_ = nullptr;
    UnitSide side_ = UnitSide::FRIENDLY;
    real_t speed_pixels_per_second_ = 0.0F;
};

} // namespace defn

#endif