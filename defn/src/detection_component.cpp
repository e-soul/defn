#include "detection_component.h"
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>

namespace defn {

void DetectionComponent::_bind_methods() {}

void DetectionComponent::configure(Node *owner_node, uint32_t hitbox_layer, uint32_t sensor_mask, real_t ranged_range, real_t scale_x) {
    hitbox = memnew(Area2D);
    hitbox->set_collision_layer(hitbox_layer);
    hitbox->set_collision_mask(0);
    hitbox->set_monitoring(false);
    hitbox->set_monitorable(true);
    auto *hitbox_shape = memnew(CollisionShape2D);
    Ref<CircleShape2D> hitbox_circle;
    hitbox_circle.instantiate();
    hitbox_circle->set_radius(static_cast<float>(5.0 / scale_x));
    hitbox_shape->set_shape(hitbox_circle);
    hitbox->add_child(hitbox_shape);
    owner_node->add_child(hitbox);

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

} // namespace defn
