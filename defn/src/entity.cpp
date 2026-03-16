#include "entity.h"
#include "grid_manager.h"
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
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

    attack_cooldown = 0.0;

    if (health_bar) {
        health_bar->set_max(max_hp);
        health_bar->set_value(max_hp);
    }
}

void Entity::init_ranged_stats(int p_ranged_damage, double p_ranged_attack_speed) {
    ranged_damage = p_ranged_damage;
    ranged_attack_speed = p_ranged_attack_speed;
}

void Entity::_ready() {
    sprite = memnew(AnimatedSprite2D);
    add_child(sprite);

    setup_health_bar();

    sprite->connect("animation_finished", callable_mp(this, &Entity::on_animation_finished));
}

void Entity::_process(double delta) {
    if (anim_state == AnimState::DEATH) {
        return;
    }

    // Handle flash effect
    if (flash_timer > 0.0) {
        flash_timer -= delta;
        if (flash_timer <= 0.0) {
            sprite->set_modulate(original_modulate);
        }
    }

    update_health_bar();

    update_cooldowns(delta);

    update_state(delta);

    perform_behavior(delta);
}

void Entity::update_cooldowns(double delta) {
    if (attack_cooldown > 0.0) {
        attack_cooldown -= delta;
    }
}

void Entity::update_state(double delta) { find_target(); }

void Entity::perform_behavior(double delta) {
    if (engaged && target && !target->is_dead()) {
        set_velocity(Vector2(0, 0));

        bool needs_pose_update = (anim_state == AnimState::WALK) || (attack_mode == AttackMode::MELEE && anim_state != AnimState::ATTACK) ||
                                 (attack_mode == AttackMode::RANGED && anim_state != AnimState::SHOOT);

        if (needs_pose_update) {
            if (attack_mode == AttackMode::MELEE) {
                set_anim_state(AnimState::ATTACK);
                sprite->set_frame_and_progress(0, 0.0);
                sprite->stop();
            } else if (attack_mode == AttackMode::RANGED) {
                set_anim_state(AnimState::SHOOT);
                sprite->set_frame_and_progress(0, 0.0);
                sprite->stop();
            }
        }

        if (attack_mode == AttackMode::MELEE && attack_cooldown <= 0.0) {
            attack_cooldown = 1.0 / attack_speed;
            set_anim_state(AnimState::ATTACK);
            sprite->play("attack");
            sprite->set_frame_and_progress(0, 0.0);
            target->take_damage(damage);
            target->flash_damage(melee_flash_color);
        } else if (attack_mode == AttackMode::RANGED && attack_cooldown <= 0.0) {
            attack_cooldown = 1.0 / ranged_attack_speed;
            set_anim_state(AnimState::SHOOT);
            sprite->play("shoot");
            sprite->set_frame_and_progress(0, 0.0);
            target->take_damage(ranged_damage);
            target->flash_damage(ranged_flash_color);
            if (muzzle_flash) {
                muzzle_flash->set_visible(true);
                muzzle_flash->play("muzzle");
                muzzle_flash->set_frame_and_progress(0, 0.0);
            }
        }
    } else {
        engaged = false;
        target = nullptr;
        if (muzzle_flash) {
            muzzle_flash->set_visible(false);
        }
        attack_mode = AttackMode::NONE;
        if (anim_state == AnimState::ATTACK || anim_state == AnimState::SHOOT) {
            set_anim_state(AnimState::WALK);
        }
        do_movement(delta);
    }
}

void Entity::take_damage(int amount) {
    if (is_dead()) {
        return;
    }

    current_hp -= amount;
    current_hp = std::max(current_hp, 0);

    if (current_hp <= 0) {
        set_velocity(Vector2(0, 0));
        set_anim_state(AnimState::DEATH);
        emit_signal("entity_died", this);
    }
}

void Entity::set_anim_state(AnimState state) {
    if (anim_state == AnimState::DEATH) {
        return;
    } // can't leave death state
    anim_state = state;

    if (!sprite) {
        return;
    }

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
    if (!health_bar) {
        return;
    }

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
    hitbox_circle->set_radius(static_cast<float>(5.0 / scale_x));
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
    sensor_circle->set_radius(static_cast<float>(ranged_range / scale_x));
    sensor_shape->set_shape(sensor_circle);
    detection_area->add_child(sensor_shape);
    add_child(detection_area);
}

void Entity::find_target() {
    if (anim_state == AnimState::DEATH) {
        return;
    }
    if (try_keep_target()) {
        return;
    }
    find_new_target();
}

bool Entity::try_keep_target() {
    if (!target || target->is_dead()) {
        return false;
    }

    double dist = get_forward_distance(target);
    if (dist < 0) {
        return false;
    }

    if (dist <= attack_range) {
        if (attack_mode != AttackMode::MELEE) {
            attack_mode = AttackMode::MELEE;
            if (muzzle_flash) {
                muzzle_flash->set_visible(false);
            }
            // Cooldown handles when the attack actually fires and animates
        }
        return true;
    }

    if (dist <= ranged_range) {
        if (attack_mode != AttackMode::RANGED) {
            attack_mode = AttackMode::RANGED;
            // Cooldown handles when the attack actually fires and animates
        }
        return true;
    }

    return false;
}

// Removed timeout callbacks

void Entity::on_muzzle_flash_finished() {
    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
    }
}

void Entity::on_animation_finished() {
    if (anim_state == AnimState::DEATH) {
        start_death_fade();
        return;
    }
    // We no longer automatically loop attack or shoot animations here.
    // They are triggered by the cooldown in perform_behavior().
    // We could return to WALK state if we wanted, or just wait in the last frame.
}

void Entity::start_death_fade() {
    Ref<Tween> tween = create_tween();
    tween->tween_property(this, NodePath("modulate"), Color(1, 1, 1, 0), 0.5);
    tween->tween_callback(Callable(this, "queue_free"));
}

void Entity::setup_muzzle_flash(const String &path_template, const Vector2 &offset, bool flip_h) {
    muzzle_flash = memnew(AnimatedSprite2D);
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    frames->add_animation("muzzle");
    frames->set_animation_speed("muzzle", 20.0);
    frames->set_animation_loop("muzzle", false);
    for (int i = 0; i <= 9; ++i) {
        String path = vformat(path_template, i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("muzzle", tex);
        }
    }

    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }

    muzzle_flash->set_sprite_frames(frames);
    muzzle_flash->set_position(offset);
    if (flip_h) {
        muzzle_flash->set_flip_h(true);
    }
    muzzle_flash->set_visible(false);
    if (!muzzle_flash->is_connected("animation_finished", callable_mp(this, &Entity::on_muzzle_flash_finished))) {
        muzzle_flash->connect("animation_finished", callable_mp(this, &Entity::on_muzzle_flash_finished));
    }
    add_child(muzzle_flash);
}

} // namespace defn