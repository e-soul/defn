#ifndef DETECTION_COMPONENT_H
#define DETECTION_COMPONENT_H

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class DetectionComponent : public Node {
    GDCLASS(DetectionComponent, Node)

  public:
    void configure(Node *owner_node, uint32_t sensor_mask, real_t ranged_range, real_t scale_x);
    Area2D *get_detection_area() const { return detection_area; }
    void set_local_position(const godot::Vector2 &position);

  protected:
    static void _bind_methods();

  private:
    Area2D *detection_area = nullptr;
};

} // namespace defn

#endif
