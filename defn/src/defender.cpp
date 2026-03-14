#include "defender.h"
#include "hostile.h"
#include "grid_manager.h"
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/scene_tree.hpp>

namespace defn {

Defender::Defender() {
    cost = 25;
}

void Defender::_bind_methods() {}

void Defender::_ready() {
    Entity::_ready();
    init_stats(300, 15, 1.0, 0.5);

    // Health bar green for defenders
    if (health_bar_fill_style.is_valid()) {
        health_bar_fill_style->set_bg_color(Color(0.2, 0.9, 0.2, 0.9));
    }

    add_to_group("defenders");

    setup_sprite_frames();
    set_anim_state(AnimState::WALK);

    sprite->connect("animation_finished", callable_mp(this, &Defender::on_animation_finished));
    attack_timer_node->connect("timeout", callable_mp(this, &Defender::on_attack_timeout));
}

void Defender::_process(double delta) {
    Entity::_process(delta);

    if (anim_state == AnimState::DEATH) return;

    find_target();

    if (engaged && target && !target->is_dead()) {
        // Timer handles attack damage via on_attack_timeout
    } else {
        engaged = false;
        target = nullptr;
        attack_timer_node->stop();
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

    // Walk animation: Walk__000 through Walk__009 (10 frames)
    frames->add_animation("walk");
    frames->set_animation_speed("walk", 10.0);
    frames->set_animation_loop("walk", true);
    for (int i = 0; i <= 9; ++i) {
        String path = vformat("res://assets/Spec_Ops_-_Game_Sprites/png/Soldier1/Walk__%03d.png", i);
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
        String path = vformat("res://assets/Spec_Ops_-_Game_Sprites/png/Soldier1/Melee__%03d.png", i);
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
        String path = vformat("res://assets/Spec_Ops_-_Game_Sprites/png/Soldier1/Dead__%03d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("death", tex);
        }
    }

    sprite->set_sprite_frames(frames);
    // Scale to fit 128px tile: 128/475 ≈ 0.27
    set_scale(Vector2(0.27, 0.27));

    // Setup detection areas after scaling: hitbox on layer 1, detect hostiles on layer 2
    setup_detection(1, 2);

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
        if (dist >= 0 && dist <= attack_range) {
            return; // still engaged with valid target
        }
    }

    target = nullptr;
    engaged = false;

    double closest_dist = 1e9;
    TypedArray<Node> hostiles = get_tree()->get_nodes_in_group("hostiles");
    for (int i = 0; i < hostiles.size(); ++i) {
        auto *hostile = Object::cast_to<Hostile>(hostiles[i].operator Object *());
        if (!hostile || hostile->is_dead()) continue;

        double dist = hostile->get_position().x - get_position().x;
        if (dist >= 0 && dist <= attack_range && dist < closest_dist) {
            closest_dist = dist;
            target = hostile;
            engaged = true;
        }
    }

    if (engaged && anim_state != AnimState::ATTACK) {
        set_anim_state(AnimState::ATTACK);
        attack_timer_node->start();
    }
}

void Defender::on_attack_timeout() {
    if (target && !target->is_dead()) {
        target->take_damage(damage);
        target->flash_damage(Color(1, 0.2, 0.2));
    }
}

void Defender::do_movement(double delta) {
    double max_x = GridManager::SPAWN_X - 100.0; // don't walk off-screen right
    Vector2 pos = get_position();
    if (pos.x < max_x) {
        pos.x += move_speed * GridManager::ATTACK_RANGE * delta;
        if (pos.x > max_x) pos.x = max_x;
        set_position(pos);
    }
}

void Defender::on_animation_finished() {
    if (anim_state == AnimState::DEATH) {
        start_death_fade();
        return;
    }
    if (anim_state == AnimState::ATTACK && engaged && target && !target->is_dead()) {
        sprite->play("attack");
    }
}

void Defender::start_death_fade() {
    Ref<Tween> tween = create_tween();
    tween->tween_property(this, NodePath("modulate"), Color(1, 1, 1, 0), 0.5);
    tween->tween_callback(Callable(this, "queue_free"));
}

} // namespace defn
