#include "entity.h"
#include "grid_manager.h"

namespace defn {

Entity::Entity() = default;

void Entity::_bind_methods() {
    ADD_SIGNAL(MethodInfo("entity_died", PropertyInfo(Variant::OBJECT, "entity")));
}

void Entity::init_stats(int p_max_hp, int p_damage, double p_attack_speed, double p_move_speed) {
    max_hp = p_max_hp;
    current_hp = p_max_hp;
    damage = p_damage;
    attack_speed = p_attack_speed;
    move_speed = p_move_speed;
}

void Entity::_ready() {
    sprite = memnew(AnimatedSprite2D);
    add_child(sprite);

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
        set_anim_state(AnimState::DEATH);
        emit_signal("entity_died", this);
    }
}

void Entity::set_anim_state(AnimState state) {
    if (anim_state == AnimState::DEATH) return; // can't leave death state
    anim_state = state;

    if (!sprite) return;

    switch (state) {
        case AnimState::WALK:
            sprite->play("walk");
            break;
        case AnimState::ATTACK:
            sprite->play("attack");
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

    health_bar_bg = memnew(ColorRect);
    health_bar_bg->set_size(Vector2(bar_width, bar_height));
    health_bar_bg->set_position(Vector2(-bar_width * 0.5, bar_offset_y));
    health_bar_bg->set_color(Color(0.3, 0.3, 0.3, 0.8));
    health_bar_bg->set_visible(false);
    add_child(health_bar_bg);

    health_bar_fill = memnew(ColorRect);
    health_bar_fill->set_size(Vector2(bar_width, bar_height));
    health_bar_fill->set_position(Vector2(0, 0));
    health_bar_fill->set_color(Color(0, 1, 0, 0.9)); // default green, overridden by subclass
    health_bar_bg->add_child(health_bar_fill);
}

void Entity::update_health_bar() {
    if (!health_bar_bg || !health_bar_fill) return;

    bool should_show = current_hp < max_hp && current_hp > 0;
    health_bar_bg->set_visible(should_show);

    if (should_show) {
        double ratio = static_cast<double>(current_hp) / max_hp;
        double bar_width = 102.0 * ratio;
        health_bar_fill->set_size(Vector2(bar_width, 6.0));
    }
}

} // namespace defn
