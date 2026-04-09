#include "base_objective.h"

#include "collision_layers.h"
#include "health_component.h"

#include <algorithm>

#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

namespace {

constexpr real_t OBJECTIVE_RADIUS = 96.0F;
constexpr real_t OBJECTIVE_CORE_RADIUS = 32.0F;
constexpr real_t OBJECTIVE_OUTLINE_RADIUS = 108.0F;
constexpr real_t DAMAGE_FLASH_DURATION_SECONDS = 0.12F;

const Color OBJECTIVE_FILL_COLOR = Color(0.82, 0.11, 0.08);
const Color OBJECTIVE_OUTLINE_COLOR = Color(0.32, 0.03, 0.03);
const Color OBJECTIVE_CORE_COLOR = Color(1.0, 0.38, 0.36, 0.9);

} // namespace

BaseObjective::BaseObjective() { set_process(false); }

void BaseObjective::_bind_methods() {
    ADD_SIGNAL(MethodInfo("durability_changed", PropertyInfo(Variant::INT, "current_hp"), PropertyInfo(Variant::INT, "max_hp")));
    ADD_SIGNAL(MethodInfo("objective_destroyed"));
}

void BaseObjective::configure(int max_hp, const Vector2 &position) {
    set_position(position);
    ensure_health_component();
    ensure_hitbox();
    health_->configure(max_hp);
    queue_redraw();
}

void BaseObjective::take_damage(int amount) {
    if (health_ != nullptr) {
        health_->take_damage(amount);
    }
}

void BaseObjective::flash_damage(const Color &color) {
    if (is_dead()) {
        return;
    }

    flash_color_ = color;
    flash_time_remaining_ = DAMAGE_FLASH_DURATION_SECONDS;
    set_process(true);
    queue_redraw();
}

bool BaseObjective::is_dead() const { return health_ == nullptr || health_->is_dead(); }

int BaseObjective::get_current_hp() const { return health_ != nullptr ? health_->get_current_hp() : 0; }

int BaseObjective::get_max_hp() const { return health_ != nullptr ? health_->get_max_hp() : 0; }

void BaseObjective::_draw() {
    const bool flashing = flash_time_remaining_ > 0.0F;
    const Color fill_color = flashing ? OBJECTIVE_FILL_COLOR.lerp(flash_color_, 0.55F) : OBJECTIVE_FILL_COLOR;

    draw_circle(Vector2(), OBJECTIVE_OUTLINE_RADIUS, OBJECTIVE_OUTLINE_COLOR);
    draw_circle(Vector2(), OBJECTIVE_RADIUS, fill_color);
    draw_circle(Vector2(), OBJECTIVE_CORE_RADIUS, OBJECTIVE_CORE_COLOR);
}

void BaseObjective::_process(double delta) {
    if (flash_time_remaining_ <= 0.0F) {
        set_process(false);
        return;
    }

    flash_time_remaining_ = std::max(flash_time_remaining_ - static_cast<real_t>(delta), 0.0F);
    if (flash_time_remaining_ <= 0.0F) {
        set_process(false);
    }

    queue_redraw();
}

void BaseObjective::ensure_health_component() {
    if (health_ != nullptr) {
        return;
    }

    health_ = memnew(HealthComponent);
    health_->set_name("HealthComponent");
    add_child(health_);
    health_->connect("health_changed", callable_mp(this, &BaseObjective::on_health_changed));
    health_->connect("died", callable_mp(this, &BaseObjective::on_destroyed));
}

void BaseObjective::ensure_hitbox() {
    if (hitbox_ != nullptr) {
        return;
    }

    hitbox_ = memnew(Area2D);
    hitbox_->set_name("Hitbox");
    hitbox_->set_collision_layer(CollisionLayers::FRIENDLY_HITBOX);
    hitbox_->set_collision_mask(CollisionLayers::NONE);
    hitbox_->set_monitoring(false);
    hitbox_->set_monitorable(true);

    auto *shape_node = memnew(CollisionShape2D);
    Ref<CircleShape2D> circle_shape;
    circle_shape.instantiate();
    circle_shape->set_radius(static_cast<float>(OBJECTIVE_RADIUS));
    shape_node->set_shape(circle_shape);
    hitbox_->add_child(shape_node);

    add_child(hitbox_);
}

void BaseObjective::on_health_changed(int current_hp, int max_hp) { emit_signal("durability_changed", current_hp, max_hp); }

void BaseObjective::on_destroyed() {
    emit_signal("objective_destroyed");
    queue_free();
}

} // namespace defn