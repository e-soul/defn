#include "defender.h"
#include "grid_manager.h"
#include "hostile.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {
constexpr double DEFENDER_MUZZLE_OFFSET_X = 220.0;
constexpr double DEFENDER_MUZZLE_OFFSET_Y = -40.0;
} // namespace

Defender::Defender() { cost = 25; }

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

    melee_flash_color = Color(1, 0.2, 0.2);
    ranged_flash_color = Color(1, 0.8, 0.2);

    setup_sprite_frames();
    setup_muzzle_flash("res://assets/Spec_Ops_-_Game_Sprites/png/Objects/Muzzle/YellowMuzzle__%03d.png",
                       Vector2(DEFENDER_MUZZLE_OFFSET_X, DEFENDER_MUZZLE_OFFSET_Y), false);
    set_anim_state(AnimState::WALK);
}

void Defender::_process(double delta) {
    if (anim_state == AnimState::DEATH) {
        return;
    }
    Entity::_process(delta);
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

double Defender::get_forward_distance(Entity *other) const { return other->get_position().x - get_position().x; }

void Defender::find_new_target() {
    target = nullptr;
    engaged = false;

    Entity *best_melee = nullptr;
    Entity *best_ranged = nullptr;
    double closest_melee = 1e9;
    double closest_ranged = 1e9;

    TypedArray<Node> hostiles = get_tree()->get_nodes_in_group("hostiles");
    for (const auto &node_variant : hostiles) {
        auto *hostile = Object::cast_to<Entity>(node_variant.operator Object *());
        if (!hostile || hostile->is_dead()) {
            continue;
        }

        double dist = get_forward_distance(hostile);
        if (dist < 0) {
            continue;
        }

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
    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
    }
    attack_mode = AttackMode::NONE;

    // Melee has priority
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

} // namespace defn