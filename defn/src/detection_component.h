#ifndef DETECTION_COMPONENT_H
#define DETECTION_COMPONENT_H

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class DetectionComponent : public Node {
    GDCLASS(DetectionComponent, Node)

  public:
    void configure(Node *owner_node, uint32_t hitbox_layer, uint32_t sensor_mask, double ranged_range, double scale_x);

    Area2D *get_hitbox() const { return hitbox; }
    Area2D *get_detection_area() const { return detection_area; }

  protected:
    static void _bind_methods();

  private:
    Area2D *hitbox = nullptr;
    Area2D *detection_area = nullptr;
};

} // namespace defn

#endif
