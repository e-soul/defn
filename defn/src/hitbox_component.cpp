#include "hitbox_component.h"

#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>

namespace defn {

void HitboxComponent::_bind_methods() {}

void HitboxComponent::configure(Node *owner_node, uint32_t hitbox_layer, real_t radius) {
    if (hitbox_ != nullptr) {
        return;
    }

    hitbox_ = memnew(Area2D);
    hitbox_->set_name("Hitbox");
    hitbox_->set_collision_layer(hitbox_layer);
    hitbox_->set_collision_mask(0U);
    hitbox_->set_monitoring(false);
    hitbox_->set_monitorable(true);

    auto *shape_node = memnew(CollisionShape2D);
    Ref<CircleShape2D> circle_shape;
    circle_shape.instantiate();
    circle_shape->set_radius(static_cast<float>(radius));
    shape_node->set_shape(circle_shape);
    hitbox_->add_child(shape_node);

    if (owner_node != nullptr) {
        owner_node->add_child(hitbox_);
    }
}

void HitboxComponent::set_local_position(const Vector2 &position) {
    if (hitbox_ != nullptr) {
        hitbox_->set_position(position);
    }
}

void HitboxComponent::disable() {
    if (hitbox_ == nullptr) {
        return;
    }

    hitbox_->set_collision_layer(0U);
    hitbox_->set_monitorable(false);
}

} // namespace defn