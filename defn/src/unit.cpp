#include "unit.h"
#include "grid_manager.h"
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

Unit::Unit() {
    // ±20% random variation on base attack range to prevent perfect alignment
    double variation = UtilityFunctions::randf_range(0.8, 1.2);
    attack_range = GridManager::ATTACK_RANGE * variation;
    double ranged_variation = UtilityFunctions::randf_range(0.8, 1.2);
    ranged_range = GridManager::RANGED_RANGE * ranged_variation;
}

void Unit::_bind_methods() {
    ADD_SIGNAL(MethodInfo("unit_died", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("enemy_breached"));
}

void Unit::set_unit_config(const UnitConfig &cfg) { unit_config_ = cfg; }

void Unit::init_stats() {
    max_hp = unit_config_.hp;
    current_hp = max_hp;
    damage = unit_config_.melee_damage;
    attack_speed = unit_config_.melee_attack_speed;
    move_speed = unit_config_.move_speed;
    ranged_damage = unit_config_.ranged_damage;
    ranged_attack_speed = unit_config_.ranged_attack_speed;
    attack_cooldown = 0.0;

    melee_flash_color = unit_config_.melee_flash_color;
    ranged_flash_color = unit_config_.ranged_flash_color;

    if (health_bar) {
        health_bar->set_max(max_hp);
        health_bar->set_value(max_hp);
    }
}

void Unit::_ready() {
    sprite = memnew(AnimatedSprite2D);
    add_child(sprite);

    setup_health_bar();
    init_stats();

    // Health bar color from config
    if (health_bar_fill_style.is_valid()) {
        health_bar_fill_style->set_bg_color(unit_config_.health_bar_color);
    }

    // Group assignment based on side
    if (unit_config_.side == UnitSide::FRIENDLY) {
        add_to_group("friendlies");
    } else {
        add_to_group("hostiles");
    }

    setup_sprite_frames();
    setup_muzzle_flash();
    set_anim_state(AnimState::WALK);

    sprite->connect("animation_finished", callable_mp(this, &Unit::on_animation_finished));
}

void Unit::_process(double delta) {
    if (anim_state == AnimState::DEATH) {
        return;
    }

    // Hostile breach check
    if (unit_config_.side == UnitSide::HOSTILE) {
        check_breach();
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

// --- Sprite setup from JSON animation config ---

void Unit::setup_sprite_frames() {
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    for (const auto &[anim_name, anim_cfg] : unit_config_.animations) {
        frames->add_animation(anim_name);
        frames->set_animation_speed(anim_name, anim_cfg.speed);
        frames->set_animation_loop(anim_name, anim_cfg.loop);
        for (int i = 0; i < anim_cfg.frame_count; ++i) {
            String path = vformat(anim_cfg.path_template, i);
            Ref<Texture2D> tex = loader->load(path);
            if (tex.is_valid()) {
                frames->add_frame(anim_name, tex);
            }
        }
    }

    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }

    sprite->set_sprite_frames(frames);
    sprite->set_flip_h(unit_config_.sprite_flip_h);
    set_scale(Vector2(static_cast<real_t>(unit_config_.scale), static_cast<real_t>(unit_config_.scale)));

    // Collision layers: friendly = hitbox layer 1, senses layer 2; hostile = opposite
    if (unit_config_.side == UnitSide::FRIENDLY) {
        setup_detection(1, 2);
    } else {
        setup_detection(2, 1);
    }
}

void Unit::setup_muzzle_flash() {
    if (unit_config_.muzzle.path_template.is_empty()) {
        return;
    }

    muzzle_flash = memnew(AnimatedSprite2D);
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    frames->add_animation("muzzle");
    frames->set_animation_speed("muzzle", 20.0);
    frames->set_animation_loop("muzzle", false);
    for (int i = 0; i <= 9; ++i) {
        String path = vformat(unit_config_.muzzle.path_template, i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("muzzle", tex);
        }
    }

    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }

    muzzle_flash->set_sprite_frames(frames);
    muzzle_flash->set_position(unit_config_.muzzle.offset);
    if (unit_config_.muzzle.flip_h) {
        muzzle_flash->set_flip_h(true);
    }
    muzzle_flash->set_visible(false);
    muzzle_flash->connect("animation_finished", callable_mp(this, &Unit::on_muzzle_flash_finished));
    add_child(muzzle_flash);
}

// --- Targeting ---

double Unit::get_forward_distance(Unit *other) const {
    if (unit_config_.side == UnitSide::FRIENDLY) {
        return other->get_position().x - get_position().x;
    }
    return get_position().x - other->get_position().x;
}

void Unit::find_target() {
    if (anim_state == AnimState::DEATH) {
        return;
    }
    if (try_keep_target()) {
        return;
    }
    find_new_target();
}

bool Unit::try_keep_target() {
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
        }
        return true;
    }

    if (dist <= ranged_range) {
        if (attack_mode != AttackMode::RANGED) {
            attack_mode = AttackMode::RANGED;
        }
        return true;
    }

    return false;
}

void Unit::find_new_target() {
    target = nullptr;
    engaged = false;

    // Target the opposite side
    String target_group = (unit_config_.side == UnitSide::FRIENDLY) ? "hostiles" : "friendlies";

    Unit *best_melee = nullptr;
    Unit *best_ranged = nullptr;
    double closest_melee = 1e9;
    double closest_ranged = 1e9;

    TypedArray<Node> targets = get_tree()->get_nodes_in_group(target_group);
    for (const auto &node_variant : targets) {
        auto *other = Object::cast_to<Unit>(node_variant.operator Object *());
        if (!other || other->is_dead()) {
            continue;
        }

        double dist = get_forward_distance(other);
        if (dist < 0) {
            continue;
        }

        if (dist <= attack_range && dist < closest_melee) {
            closest_melee = dist;
            best_melee = other;
        }
        if (dist <= ranged_range && dist < closest_ranged) {
            closest_ranged = dist;
            best_ranged = other;
        }
    }

    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
    }
    attack_mode = AttackMode::NONE;

    if (best_melee) {
        target = best_melee;
        engaged = true;
        attack_mode = AttackMode::MELEE;
    } else if (best_ranged) {
        target = best_ranged;
        engaged = true;
        attack_mode = AttackMode::RANGED;
    }
}

// --- Movement ---

void Unit::do_movement(double delta) {
    double speed = move_speed * GridManager::ATTACK_RANGE;

    if (unit_config_.side == UnitSide::FRIENDLY) {
        double max_x = GridManager::get_world_width() - 100.0;
        if (get_position().x < max_x) {
            set_velocity(Vector2(static_cast<real_t>(speed), 0));
            move_and_slide();
            if (get_position().x > max_x) {
                set_position(Vector2(static_cast<real_t>(max_x), get_position().y));
                set_velocity(Vector2(0, 0));
            }
        } else {
            set_velocity(Vector2(0, 0));
        }
    } else {
        set_velocity(Vector2(static_cast<real_t>(-speed), 0));
        move_and_slide();
    }
}

void Unit::check_breach() {
    if (get_position().x < GridManager::BREACH_X) {
        emit_signal("enemy_breached");
        queue_free();
    }
}

// --- Combat behavior ---

void Unit::update_cooldowns(double delta) {
    if (attack_cooldown > 0.0) {
        attack_cooldown -= delta;
    }
}

void Unit::update_state(double /*delta*/) { find_target(); }

void Unit::perform_behavior(double delta) {
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

// --- Damage & animation ---

void Unit::take_damage(int amount) {
    if (is_dead()) {
        return;
    }

    current_hp -= amount;
    current_hp = std::max(current_hp, 0);

    if (current_hp <= 0) {
        set_velocity(Vector2(0, 0));
        set_anim_state(AnimState::DEATH);
        emit_signal("unit_died", this);
    }
}

void Unit::set_anim_state(AnimState state) {
    if (anim_state == AnimState::DEATH) {
        return;
    }
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

void Unit::flash_damage(const Color &color) {
    if (sprite) {
        sprite->set_modulate(color);
        flash_timer = 0.1;
    }
}

void Unit::on_muzzle_flash_finished() {
    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
    }
}

void Unit::on_animation_finished() {
    if (anim_state == AnimState::DEATH) {
        start_death_fade();
    }
}

void Unit::start_death_fade() {
    Ref<Tween> tween = create_tween();
    tween->tween_property(this, NodePath("modulate"), Color(1, 1, 1, 0), 0.5);
    tween->tween_callback(Callable(this, "queue_free"));
}

// --- Health bar ---

void Unit::setup_health_bar() {
    constexpr double bar_width = 102.0;
    constexpr double bar_height = 6.0;
    constexpr double bar_offset_y = -56.0;

    health_bar = memnew(ProgressBar);
    health_bar->set_custom_minimum_size(Vector2(bar_width, bar_height));
    health_bar->set_position(Vector2(-bar_width * 0.5, bar_offset_y));
    health_bar->set_min(0.0);
    health_bar->set_max(max_hp);
    health_bar->set_value(max_hp);
    health_bar->set_show_percentage(false);
    health_bar->set_visible(false);

    health_bar_bg_style.instantiate();
    health_bar_bg_style->set_bg_color(Color(0.3, 0.3, 0.3, 0.8));
    health_bar_bg_style->set_corner_radius_all(0);
    health_bar_bg_style->set_border_width_all(0);
    health_bar_bg_style->set_content_margin_all(0);
    health_bar->add_theme_stylebox_override("background", health_bar_bg_style);

    health_bar_fill_style.instantiate();
    health_bar_fill_style->set_bg_color(Color(0, 1, 0, 0.9));
    health_bar_fill_style->set_corner_radius_all(0);
    health_bar_fill_style->set_border_width_all(0);
    health_bar_fill_style->set_content_margin_all(0);
    health_bar->add_theme_stylebox_override("fill", health_bar_fill_style);

    add_child(health_bar);
}

void Unit::update_health_bar() {
    if (!health_bar) {
        return;
    }

    bool should_show = current_hp < max_hp && current_hp > 0;
    health_bar->set_visible(should_show);

    if (should_show) {
        health_bar->set_value(current_hp);
    }
}

// --- Detection areas ---

void Unit::setup_detection(uint32_t hitbox_layer, uint32_t sensor_mask) {
    double scale_x = get_scale().x;

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

} // namespace defn
