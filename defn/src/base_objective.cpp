#include "base_objective.h"

#include "collision_layers.h"
#include "health_component.h"

#include <algorithm>

#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr real_t OBJECTIVE_RADIUS = 96.0F;
constexpr real_t OBJECTIVE_CORE_RADIUS = 32.0F;
constexpr real_t OBJECTIVE_OUTLINE_RADIUS = 108.0F;
constexpr real_t OBJECTIVE_DISPLAY_WIDTH = 192.0F;
constexpr real_t DAMAGE_FLASH_DURATION_SECONDS = 0.12F;

constexpr char OBJECTIVE_TEXTURE_PATH[] = "res://assets/tower.png";
constexpr char OBJECTIVE_DESTROYED_TEXTURE_PATH[] = "res://assets/tower_destroyed.png";

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
    ensure_sprite();
    ensure_target_anchor();
    ensure_health_component();
    ensure_hitbox();

    const Vector2 local_anchor = get_local_anchor_position();
    set_position(position - local_anchor);
    target_anchor_->set_position(local_anchor);
    hitbox_->set_position(local_anchor);

    health_->configure(max_hp);
    update_visual_state();
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
    update_visual_state();
}

bool BaseObjective::is_dead() const { return health_ == nullptr || health_->is_dead(); }

int BaseObjective::get_current_hp() const { return health_ != nullptr ? health_->get_current_hp() : 0; }

int BaseObjective::get_max_hp() const { return health_ != nullptr ? health_->get_max_hp() : 0; }

void BaseObjective::_draw() {
    if (sprite_ != nullptr && sprite_texture_.is_valid()) {
        return;
    }

    const bool flashing = flash_time_remaining_ > 0.0F;
    const Color fill_color = flashing ? OBJECTIVE_FILL_COLOR.lerp(flash_color_, 0.55F) : OBJECTIVE_FILL_COLOR;
    const Vector2 draw_origin = get_local_anchor_position();

    draw_circle(draw_origin, OBJECTIVE_OUTLINE_RADIUS, OBJECTIVE_OUTLINE_COLOR);
    draw_circle(draw_origin, OBJECTIVE_RADIUS, fill_color);
    draw_circle(draw_origin, OBJECTIVE_CORE_RADIUS, OBJECTIVE_CORE_COLOR);
}

void BaseObjective::_process(double delta) {
    if (flash_time_remaining_ <= 0.0F) {
        update_visual_state();
        set_process(false);
        return;
    }

    flash_time_remaining_ = std::max(flash_time_remaining_ - static_cast<real_t>(delta), 0.0F);
    update_visual_state();
    if (flash_time_remaining_ <= 0.0F) {
        set_process(false);
    }
}

void BaseObjective::ensure_sprite() {
    if (sprite_ != nullptr) {
        return;
    }

    auto *loader = ResourceLoader::get_singleton();
    if (loader == nullptr) {
        return;
    }

    sprite_texture_ = loader->load(OBJECTIVE_TEXTURE_PATH);
    if (!sprite_texture_.is_valid()) {
        UtilityFunctions::printerr("BaseObjective: Failed to load sprite: ", OBJECTIVE_TEXTURE_PATH);
        return;
    }

    sprite_ = memnew(Sprite2D);
    sprite_->set_name("TowerSprite");
    sprite_->set_texture(sprite_texture_);
    sprite_->set_centered(false);

    const Vector2 texture_size = sprite_texture_->get_size();
    if (texture_size.x > 0.0F) {
        const real_t scale_factor = OBJECTIVE_DISPLAY_WIDTH / texture_size.x;
        sprite_->set_scale(Vector2(scale_factor, scale_factor));
    }

    add_child(sprite_);
}

void BaseObjective::ensure_target_anchor() {
    if (target_anchor_ != nullptr) {
        return;
    }

    target_anchor_ = memnew(Node2D);
    target_anchor_->set_name("TargetAnchor");
    add_child(target_anchor_);
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

Vector2 BaseObjective::get_local_anchor_position() const {
    if (sprite_texture_.is_valid()) {
        const Vector2 texture_size = sprite_texture_->get_size();
        if (texture_size.x > 0.0F && texture_size.y > 0.0F) {
            const real_t scale_factor = OBJECTIVE_DISPLAY_WIDTH / texture_size.x;
            const Vector2 display_size = texture_size * scale_factor;
            return {
                display_size.x * 0.5F,
                std::max(display_size.y - (OBJECTIVE_RADIUS * 0.5F), OBJECTIVE_RADIUS),
            };
        }
    }

    return Vector2(OBJECTIVE_RADIUS, OBJECTIVE_RADIUS);
}

void BaseObjective::update_visual_state() {
    if (sprite_ != nullptr) {
        const bool flashing = flash_time_remaining_ > 0.0F;
        const Color tint = flashing ? Color(1.0, 1.0, 1.0).lerp(flash_color_, 0.35F) : Color(1.0, 1.0, 1.0);
        sprite_->set_modulate(tint);
        return;
    }

    queue_redraw();
}

void BaseObjective::on_health_changed(int current_hp, int max_hp) { emit_signal("durability_changed", current_hp, max_hp); }

void BaseObjective::on_destroyed() {
    flash_time_remaining_ = 0.0F;
    set_process(false);

    if (hitbox_ != nullptr) {
        hitbox_->set_collision_layer(CollisionLayers::NONE);
        hitbox_->set_monitorable(false);
    }

    if (sprite_ != nullptr) {
        auto *loader = ResourceLoader::get_singleton();
        if (loader != nullptr) {
            const Ref<Texture2D> destroyed_texture = loader->load(OBJECTIVE_DESTROYED_TEXTURE_PATH);
            if (destroyed_texture.is_valid()) {
                const Vector2 anchor_position = target_anchor_ != nullptr ? target_anchor_->get_position() : Vector2();
                sprite_texture_ = destroyed_texture;
                sprite_->set_texture(sprite_texture_);

                const Vector2 texture_size = sprite_texture_->get_size();
                if (texture_size.x > 0.0F) {
                    const real_t scale_factor = OBJECTIVE_DISPLAY_WIDTH / texture_size.x;
                    sprite_->set_scale(Vector2(scale_factor, scale_factor));
                }

                sprite_->set_position(anchor_position - get_local_anchor_position());
            } else {
                UtilityFunctions::printerr("BaseObjective: Failed to load destroyed sprite: ", OBJECTIVE_DESTROYED_TEXTURE_PATH);
            }
        }

        sprite_->set_modulate(Color(1.0, 1.0, 1.0));
    }

    emit_signal("objective_destroyed");
}

} // namespace defn