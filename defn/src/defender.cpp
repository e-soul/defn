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

namespace {

constexpr double DEFENDER_MUZZLE_OFFSET_X = 220.0;
constexpr double DEFENDER_MUZZLE_OFFSET_Y = -40.0;

} // namespace

Defender::Defender() {
    cost = 25;
}

void Defender::_bind_methods() {}

void Defender::_ready() {
    Entity::_ready();
    init_stats(300, 15, 1.0, 0.5);
    init_ranged_stats(8, 1.5);

    // Health bar green for defenders
    if (health_bar_fill_style.is_valid()) {
        health_bar_fill_style->set_bg_color(Color(0.2, 0.9, 0.2, 0.9));
    }

    add_to_group("defenders");

    setup_sprite_frames();
    setup_muzzle_flash();
    set_anim_state(AnimState::WALK);

    sprite->connect("animation_finished", callable_mp(this, &Defender::on_animation_finished));
    attack_timer_node->connect("timeout", callable_mp(this, &Defender::on_attack_timeout));
    ranged_timer_node->connect("timeout", callable_mp(this, &Defender::on_ranged_timeout));
}

void Defender::_process(double delta) {
    Entity::_process(delta);

    if (anim_state == AnimState::DEATH) { return; }

    find_target();

    if (engaged && target && !target->is_dead()) {
        set_velocity(Vector2(0, 0));
    } else {
        engaged = false;
        target = nullptr;
        attack_timer_node->stop();
        ranged_timer_node->stop();
        if (muzzle_flash) { muzzle_flash->set_visible(false); }
        attack_mode = AttackMode::NONE;
        if (anim_state == AnimState::ATTACK || anim_state == AnimState::SHOOT) {
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

    // Shoot animation: Shoot__000 through Shoot__009 (10 frames)
    frames->add_animation("shoot");
    frames->set_animation_speed("shoot", 10.0);
    frames->set_animation_loop("shoot", false);
    for (int i = 0; i <= 9; ++i) {
        String path = vformat("res://assets/Spec_Ops_-_Game_Sprites/png/Soldier1/Shoot__%03d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("shoot", tex);
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
    if (anim_state == AnimState::DEATH) { return; }
    if (try_keep_target()) { return; }
    find_new_target();
}

bool Defender::try_keep_target() {
    if (!target || target->is_dead()) { return false; }

    double dist = target->get_position().x - get_position().x;
    if (dist < 0) { return false; }

    if (dist <= attack_range) {
        if (attack_mode != AttackMode::MELEE) {
            attack_mode = AttackMode::MELEE;
            ranged_timer_node->stop();
            if (muzzle_flash) { muzzle_flash->set_visible(false); }
            set_anim_state(AnimState::ATTACK);
            attack_timer_node->start();
        }
        return true;
    }
    
    if (dist <= ranged_range) {
        if (attack_mode != AttackMode::RANGED) {
            attack_mode = AttackMode::RANGED;
            attack_timer_node->stop();
            set_anim_state(AnimState::SHOOT);
            ranged_timer_node->start();
        }
        return true;
    }

    return false;
}

void Defender::find_new_target() {
    target = nullptr;
    engaged = false;

    Hostile *best_melee = nullptr;
    Hostile *best_ranged = nullptr;
    double closest_melee = 1e9;
    double closest_ranged = 1e9;

    TypedArray<Node> hostiles = get_tree()->get_nodes_in_group("hostiles");
    for (const auto & node_variant : hostiles) {
        auto *hostile = Object::cast_to<Hostile>(node_variant.operator Object *());
        if (!hostile || hostile->is_dead()) { continue; }

        double dist = hostile->get_position().x - get_position().x;
        if (dist < 0) { continue; }

        if (dist <= attack_range && dist < closest_melee) {
            closest_melee = dist;
            best_melee = hostile;
        }
        if (dist <= ranged_range && dist < closest_ranged) {
            closest_ranged = dist;
            best_ranged = hostile;
        }
    }

    // Stop all attacks
    attack_timer_node->stop();
    ranged_timer_node->stop();
    if (muzzle_flash) { muzzle_flash->set_visible(false); }
    attack_mode = AttackMode::NONE;

    // Melee has priority
    if (best_melee) {
        target = best_melee;
        engaged = true;
        attack_mode = AttackMode::MELEE;
        set_anim_state(AnimState::ATTACK);
        attack_timer_node->start();
    } else if (best_ranged) {
        target = best_ranged;
        engaged = true;
        attack_mode = AttackMode::RANGED;
        set_anim_state(AnimState::SHOOT);
        ranged_timer_node->start();
    }
}

void Defender::on_attack_timeout() {
    if (target && !target->is_dead()) {
        target->take_damage(damage);
        target->flash_damage(Color(1, 0.2, 0.2));
    }
}

void Defender::on_ranged_timeout() {
    if (target && !target->is_dead()) {
        target->take_damage(ranged_damage);
        target->flash_damage(Color(1, 0.8, 0.2));
        if (muzzle_flash) {
            muzzle_flash->set_visible(true);
            muzzle_flash->play("muzzle");
        }
    }
}

void Defender::on_muzzle_flash_finished() {
    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
    }
}

void Defender::setup_muzzle_flash() {
    muzzle_flash = memnew(AnimatedSprite2D);
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    frames->add_animation("muzzle");
    frames->set_animation_speed("muzzle", 20.0);
    frames->set_animation_loop("muzzle", false);
    for (int i = 0; i <= 9; ++i) {
        String path = vformat("res://assets/Spec_Ops_-_Game_Sprites/png/Objects/Muzzle/YellowMuzzle__%03d.png", i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("muzzle", tex);
        }
    }

    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }

    muzzle_flash->set_sprite_frames(frames);
    muzzle_flash->set_position(Vector2(DEFENDER_MUZZLE_OFFSET_X, DEFENDER_MUZZLE_OFFSET_Y));
    muzzle_flash->set_visible(false);
    muzzle_flash->connect("animation_finished", callable_mp(this, &Defender::on_muzzle_flash_finished));
    add_child(muzzle_flash);
}

void Defender::do_movement(double delta) {
    double max_x = GridManager::get_world_width() - 100.0;
    if (get_position().x < max_x) {
        double speed = move_speed * GridManager::ATTACK_RANGE;
        set_velocity(Vector2(static_cast<real_t>(speed), 0));
        move_and_slide();
        // Clamp to max_x after sliding
        if (get_position().x > max_x) {
            set_position(Vector2(static_cast<real_t>(max_x), get_position().y));
            set_velocity(Vector2(0, 0));
        }
    } else {
        set_velocity(Vector2(0, 0));
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
    if (anim_state == AnimState::SHOOT && engaged && target && !target->is_dead()) {
        sprite->play("shoot");
    }
}

void Defender::start_death_fade() {
    Ref<Tween> tween = create_tween();
    tween->tween_property(this, NodePath("modulate"), Color(1, 1, 1, 0), 0.5);
    tween->tween_callback(Callable(this, "queue_free"));
}

} // namespace defn
