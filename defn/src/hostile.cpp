#include "hostile.h"
#include "defender.h"
#include "grid_manager.h"
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

Hostile::Hostile() = default;

void Hostile::_bind_methods() {
    ADD_SIGNAL(MethodInfo("enemy_breached"));
}

void Hostile::_ready() {
    Entity::_ready();
    init_stats(100, 15, 1.0, 0.5);
    bounty = 5;

    // Health bar red for enemies
    if (health_bar_fill_style.is_valid()) {
        health_bar_fill_style->set_bg_color(Color(0.9, 0.15, 0.15, 0.9));
    }

    setup_sprite_frames();
    set_anim_state(AnimState::WALK);

    sprite->connect("animation_finished", callable_mp(this, &Hostile::on_animation_finished));
}

void Hostile::_process(double delta) {
    Entity::_process(delta);

    if (fading) {
        fade_timer -= delta;
        double alpha = fade_timer > 0.0 ? fade_timer / 0.5 : 0.0;
        set_modulate(Color(1, 1, 1, alpha));
        if (fade_timer <= 0.0) {
            queue_free();
        }
        return;
    }

    if (anim_state == AnimState::DEATH) return;

    check_breach();
    find_target();

    if (engaged && target && !target->is_dead()) {
        do_attack(delta);
    } else {
        engaged = false;
        target = nullptr;
        if (anim_state == AnimState::ATTACK) {
            set_anim_state(AnimState::WALK);
        }
        do_movement(delta);
    }
}

void Hostile::setup_sprite_frames() {
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    // Walk animation: Walk__000 through Walk__009 (10 frames)
    frames->add_animation("walk");
    frames->set_animation_speed("walk", 10.0);
    frames->set_animation_loop("walk", true);
    for (int i = 0; i <= 9; ++i) {
        String path = vformat("res://assets/The_Guerrila_-_Game_Sprites/png/Soldier4/Walk__%03d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("walk", tex);
        }
    }

    // Attack animation: Melee__000 through Melee__009 (10 frames)
    frames->add_animation("attack");
    frames->set_animation_speed("attack", 10.0);
    frames->set_animation_loop("attack", false);
    for (int i = 0; i <= 9; ++i) {
        String path = vformat("res://assets/The_Guerrila_-_Game_Sprites/png/Soldier4/Melee__%03d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("attack", tex);
        }
    }

    // Death animation: Dead__000 through Dead__009 (10 frames)
    frames->add_animation("death");
    frames->set_animation_speed("death", 8.0);
    frames->set_animation_loop("death", false);
    for (int i = 0; i <= 9; ++i) {
        String path = vformat("res://assets/The_Guerrila_-_Game_Sprites/png/Soldier4/Dead__%03d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("death", tex);
        }
    }

    sprite->set_sprite_frames(frames);
    // Flip horizontally so hostile faces left
    sprite->set_flip_h(true);
    // Scale to fit 128px tile: 128/518 ≈ 0.247
    set_scale(Vector2(0.247, 0.247));

    // Setup detection areas after scaling: hitbox on layer 2, detect defenders on layer 1
    setup_detection(2, 1);

    // Remove default animation if it exists
    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }
}

void Hostile::find_target() {
    if (anim_state == AnimState::DEATH) return;

    // Check if current target is still valid
    if (target && !target->is_dead()) {
        double dist = get_position().x - target->get_position().x;
        if (dist >= 0 && dist <= attack_range) {
            return;
        }
    }

    target = nullptr;
    engaged = false;

    if (!detection_area) return;

    double closest_dist = 1e9;
    TypedArray<Area2D> areas = detection_area->get_overlapping_areas();
    for (int i = 0; i < areas.size(); ++i) {
        auto *area = Object::cast_to<Area2D>(areas[i].operator Object *());
        if (!area) continue;
        auto *defender = Object::cast_to<Defender>(area->get_parent());
        if (!defender || defender->is_dead()) continue;

        // Defender must be ahead (to the left, lower X)
        double dist = get_position().x - defender->get_position().x;
        if (dist >= 0 && dist < closest_dist) {
            closest_dist = dist;
            target = defender;
            engaged = true;
        }
    }

    if (engaged && anim_state != AnimState::ATTACK) {
        set_anim_state(AnimState::ATTACK);
        attack_timer = 0.0;
    }
}

void Hostile::do_attack(double delta) {
    attack_timer += delta;
    if (attack_timer >= 1.0 / attack_speed) {
        attack_timer -= 1.0 / attack_speed;
        if (target && !target->is_dead()) {
            target->take_damage(damage);
            target->flash_damage(Color(1, 1, 1));
        }
    }
}

void Hostile::do_movement(double delta) {
    Vector2 pos = get_position();
    pos.x -= move_speed * GridManager::ATTACK_RANGE * delta;
    set_position(pos);
}

void Hostile::check_breach() {
    if (get_position().x < GridManager::BREACH_X) {
        emit_signal("enemy_breached");
        queue_free();
    }
}

void Hostile::on_animation_finished() {
    if (anim_state == AnimState::DEATH) {
        start_death_fade();
        return;
    }
    if (anim_state == AnimState::ATTACK && engaged && target && !target->is_dead()) {
        sprite->play("attack");
    }
}

void Hostile::start_death_fade() {
    fading = true;
    fade_timer = 0.5;
}

} // namespace defn
