#ifndef HITBOX_COMPONENT_H
#define HITBOX_COMPONENT_H

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class HitboxComponent : public Node {
    GDCLASS(HitboxComponent, Node)

  public:
    void configure(Node *owner_node, uint32_t hitbox_layer, real_t radius);
    Area2D *get_hitbox() const { return hitbox_; }
    void set_local_position(const Vector2 &position);
    void disable();

  protected:
    static void _bind_methods();

  private:
    Area2D *hitbox_ = nullptr;
};

} // namespace defn

#endif