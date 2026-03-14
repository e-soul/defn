#include "entity.h"
#include "grid_manager.h"
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

Entity::Entity() {
    // ±20% random variation on base attack range to prevent perfect alignment
    double variation = UtilityFunctions::randf_range(0.8, 1.2);
    attack_range = GridManager::ATTACK_RANGE * variation;
    double ranged_variation = UtilityFunctions::randf_range(0.8, 1.2);
    ranged_range = GridManager::RANGED_RANGE * ranged_variation;
}

void Entity::_bind_methods() {
    ADD_SIGNAL(MethodInfo("entity_died", PropertyInfo(Variant::OBJECT, "entity")));
}

void Entity::init_stats(int p_max_hp, int p_damage, double p_attack_speed, double p_move_speed) {
    max_hp = p_max_hp;
    current_hp = p_max_hp;
    damage = p_damage;
    attack_speed = p_attack_speed;
    move_speed = p_move_speed;

    if (attack_timer_node) {
        attack_timer_node->set_wait_time(1.0 / attack_speed);
    }

    if (health_bar) {
        health_bar->set_max(max_hp);
        health_bar->set_value(max_hp);
    }
}

void Entity::init_ranged_stats(int p_ranged_damage, double p_ranged_attack_speed) {
    ranged_damage = p_ranged_damage;
    ranged_attack_speed = p_ranged_attack_speed;
    if (ranged_timer_node) {
        ranged_timer_node->set_wait_time(1.0 / ranged_attack_speed);
    }
}

void Entity::_ready() {
    sprite = memnew(AnimatedSprite2D);
    add_child(sprite);

    attack_timer_node = memnew(Timer);
    attack_timer_node->set_one_shot(false);
    attack_timer_node->set_autostart(false);
    add_child(attack_timer_node);

    ranged_timer_node = memnew(Timer);
    ranged_timer_node->set_one_shot(false);
    ranged_timer_node->set_autostart(false);
    add_child(ranged_timer_node);

    setup_health_bar();
}

void Entity::_process(double delta) {
    // Handle flash effect
    if (flash_timer > 0.0) {
        flash_timer -= delta;
        if (flash_timer <= 0.0) {
            sprite->set_modulate(original_modulate);
        }
    }

    update_health_bar();
}

void Entity::take_damage(int amount) {
    if (is_dead()) return;

    current_hp -= amount;
    if (current_hp < 0) current_hp = 0;

    if (current_hp <= 0) {
        set_velocity(Vector2(0, 0));
        set_anim_state(AnimState::DEATH);
        emit_signal("entity_died", this);
    }
}

void Entity::set_anim_state(AnimState state) {
    if (anim_state == AnimState::DEATH) return; // can't leave death state
    anim_state = state;

    if (state == AnimState::DEATH && attack_timer_node) {
        attack_timer_node->stop();
    }
    if (state == AnimState::DEATH && ranged_timer_node) {
        ranged_timer_node->stop();
    }

    if (!sprite) return;

    switch (state) {
        case AnimState::WALK:
            sprite->play("walk");
            break;
        case AnimState::ATTACK:
            sprite->play("attack");
            break;
        case AnimState::SHOOT:
            sprite->play("shoot");
            break;
        case AnimState::DEATH:
            sprite->play("death");
            break;
    }
}

void Entity::flash_damage(const Color &color) {
    if (sprite) {
        sprite->set_modulate(color);
        flash_timer = 0.1;
    }
}

void Entity::setup_health_bar() {
    constexpr double bar_width = 102.0;
    constexpr double bar_height = 6.0;
    constexpr double bar_offset_y = -56.0; // above sprite

    health_bar = memnew(ProgressBar);
    health_bar->set_custom_minimum_size(Vector2(bar_width, bar_height));
    health_bar->set_position(Vector2(-bar_width * 0.5, bar_offset_y));
    health_bar->set_min(0.0);
    health_bar->set_max(max_hp);
    health_bar->set_value(max_hp);
    health_bar->set_show_percentage(false);
    health_bar->set_visible(false);

    // Background style (grey)
    health_bar_bg_style.instantiate();
    health_bar_bg_style->set_bg_color(Color(0.3, 0.3, 0.3, 0.8));
    health_bar_bg_style->set_corner_radius_all(0);
    health_bar_bg_style->set_border_width_all(0);
    health_bar_bg_style->set_content_margin_all(0);
    health_bar->add_theme_stylebox_override("background", health_bar_bg_style);

    // Fill style (default green, overridden by subclass)
    health_bar_fill_style.instantiate();
    health_bar_fill_style->set_bg_color(Color(0, 1, 0, 0.9));
    health_bar_fill_style->set_corner_radius_all(0);
    health_bar_fill_style->set_border_width_all(0);
    health_bar_fill_style->set_content_margin_all(0);
    health_bar->add_theme_stylebox_override("fill", health_bar_fill_style);

    add_child(health_bar);
}

void Entity::update_health_bar() {
    if (!health_bar) return;

    bool should_show = current_hp < max_hp && current_hp > 0;
    health_bar->set_visible(should_show);

    if (should_show) {
        health_bar->set_value(current_hp);
    }
}

void Entity::setup_detection(uint32_t hitbox_layer, uint32_t sensor_mask) {
    double scale_x = get_scale().x;

    // Hitbox: small circle marking this entity's position
    hitbox = memnew(Area2D);
    hitbox->set_collision_layer(hitbox_layer);
    hitbox->set_collision_mask(0);
    hitbox->set_monitoring(false);
    hitbox->set_monitorable(true);
    auto *hitbox_shape = memnew(CollisionShape2D);
    Ref<CircleShape2D> hitbox_circle;
    hitbox_circle.instantiate();
    hitbox_circle->set_radius(5.0 / scale_x);
    hitbox_shape->set_shape(hitbox_circle);
    hitbox->add_child(hitbox_shape);
    add_child(hitbox);

    // Detection area: attack range sensor
    detection_area = memnew(Area2D);
    detection_area->set_collision_layer(0);
    detection_area->set_collision_mask(sensor_mask);
    detection_area->set_monitoring(true);
    detection_area->set_monitorable(false);
    auto *sensor_shape = memnew(CollisionShape2D);
    Ref<CircleShape2D> sensor_circle;
    sensor_circle.instantiate();
    sensor_circle->set_radius(ranged_range / scale_x);
    sensor_shape->set_shape(sensor_circle);
    detection_area->add_child(sensor_shape);
    add_child(detection_area);
}

} // namespace defn
