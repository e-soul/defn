#include "detection_component.h"
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>

namespace defn {

void DetectionComponent::_bind_methods() {}

void DetectionComponent::configure(Node *owner_node, uint32_t sensor_mask, real_t ranged_range, real_t scale_x) {
    detection_area = memnew(Area2D);
    detection_area->set_collision_layer(0);
    detection_area->set_collision_mask(sensor_mask);
    detection_area->set_monitoring(true);
    detection_area->set_monitorable(false);
    auto *sensor_shape = memnew(CollisionShape2D);
    Ref<CircleShape2D> sensor_circle;
    sensor_circle.instantiate();
    sensor_circle->set_radius(static_cast<float>(ranged_range / scale_x));
    sensor_shape->set_shape(sensor_circle);
    detection_area->add_child(sensor_shape);
    owner_node->add_child(detection_area);
}

void DetectionComponent::set_local_position(const godot::Vector2 &position) {
    if (detection_area != nullptr) {
        detection_area->set_position(position);
    }
}

} // namespace defn
