#include "defender.h"
#include "hostile.h"
#include "grid_manager.h"
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

Defender::Defender() {
    cost = 25;
}

void Defender::_bind_methods() {}

void Defender::_ready() {
    Entity::_ready();
    init_stats(300, 15, 1.0, 0.5);

    // Health bar green for defenders
    if (health_bar_fill) {
        health_bar_fill->set_color(Color(0.2, 0.9, 0.2, 0.9));
    }

    setup_sprite_frames();
    set_anim_state(AnimState::WALK);

    sprite->connect("animation_finished", callable_mp(this, &Defender::on_animation_finished));
}

void Defender::_process(double delta) {
    Entity::_process(delta);

    if (anim_state == AnimState::DEATH) return;

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

void Defender::setup_sprite_frames() {
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    // Walk animation: sword_walk_0009 through sword_walk_0016 (8 frames)
    frames->add_animation("walk");
    frames->set_animation_speed("walk", 8.0);
    frames->set_animation_loop("walk", true);
    for (int i = 9; i <= 16; ++i) {
        String path = vformat("res://assets/stick_figure/Sword_sprites/sword_walk_%04d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("walk", tex);
        }
    }

    // Attack animation: sword_combo_0065 through sword_combo_0075 (11 frames)
    frames->add_animation("attack");
    frames->set_animation_speed("attack", 11.0);
    frames->set_animation_loop("attack", false);
    for (int i = 65; i <= 75; ++i) {
        String path = vformat("res://assets/stick_figure/Sword_sprites/sword_combo_%04d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("attack", tex);
        }
    }

    // Death animation: sword_death_0052 through sword_death_0061 (10 frames)
    frames->add_animation("death");
    frames->set_animation_speed("death", 8.0);
    frames->set_animation_loop("death", false);
    for (int i = 52; i <= 61; ++i) {
        String path = vformat("res://assets/stick_figure/Sword_sprites/sword_death_%04d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("death", tex);
        }
    }

    sprite->set_sprite_frames(frames);
    // Scale to fit 128px tile: 128/512 = 0.25
    set_scale(Vector2(0.25, 0.25));

    // Remove default animation if it exists
    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }
}

void Defender::find_target() {
    if (anim_state == AnimState::DEATH) return;

    // Check if current target is still valid
    if (target && !target->is_dead()) {
        double dist = target->get_position().x - get_position().x;
        if (dist >= 0 && dist <= GridManager::TILE_SIZE) {
            return; // still engaged with valid target
        }
    }

    // Scan for hostiles in same lane
    target = nullptr;
    engaged = false;

    Node *parent = get_parent();
    if (!parent) return;

    double closest_dist = 1e9;
    int child_count = parent->get_child_count();
    for (int i = 0; i < child_count; ++i) {
        auto *hostile = Object::cast_to<Hostile>(parent->get_child(i));
        if (!hostile || hostile->is_dead() || hostile->get_lane() != lane) continue;

        double dist = hostile->get_position().x - get_position().x;
        if (dist >= 0 && dist <= GridManager::TILE_SIZE && dist < closest_dist) {
            closest_dist = dist;
            target = hostile;
            engaged = true;
        }
    }

    if (engaged && anim_state != AnimState::ATTACK) {
        set_anim_state(AnimState::ATTACK);
        attack_timer = 0.0;
    }
}

void Defender::do_attack(double delta) {
    attack_timer += delta;
    if (attack_timer >= 1.0 / attack_speed) {
        attack_timer -= 1.0 / attack_speed;
        if (target && !target->is_dead()) {
            target->take_damage(damage);
            target->flash_damage(Color(1, 0.2, 0.2));
        }
    }
}

void Defender::do_movement(double delta) {
    double max_x = GridManager::column_center_x(GridManager::COLUMN_COUNT - 1);
    Vector2 pos = get_position();
    if (pos.x < max_x) {
        pos.x += move_speed * GridManager::TILE_SIZE * delta;
        if (pos.x > max_x) pos.x = max_x;
        set_position(pos);
    }
}

void Defender::on_animation_finished() {
    if (anim_state == AnimState::DEATH) {
        queue_free();
        return;
    }
    if (anim_state == AnimState::ATTACK && engaged && target && !target->is_dead()) {
        sprite->play("attack");
    }
}

} // namespace defn
