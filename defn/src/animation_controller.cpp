#include "animation_controller.h"
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

void AnimationController::_bind_methods() {}

void AnimationController::configure(Node *owner_node, const UnitConfig &cfg) {
    sprite = memnew(AnimatedSprite2D);
    owner_node->add_child(sprite);

    setup_sprite_frames(owner_node, cfg);
    setup_muzzle_flash(owner_node, cfg);
    set_anim_state(AnimState::WALK);

    sprite->connect("animation_finished", callable_mp(this, &AnimationController::on_animation_finished));
}

void AnimationController::_process(double delta) {
    if (flash_timer > 0.0) {
        flash_timer -= delta;
        if (flash_timer <= 0.0 && sprite) {
            sprite->set_modulate(original_modulate);
        }
    }
}

void AnimationController::setup_sprite_frames(Node * /*owner_node*/, const UnitConfig &cfg) {
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    for (const auto &[anim_name, anim_cfg] : cfg.animations) {
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
    sprite->set_flip_h(cfg.sprite_flip_h);
}

void AnimationController::setup_muzzle_flash(Node *owner_node, const UnitConfig &cfg) {
    if (cfg.muzzle.path_template.is_empty()) {
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
        String path = vformat(cfg.muzzle.path_template, i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("muzzle", tex);
        }
    }

    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }

    muzzle_flash->set_sprite_frames(frames);
    muzzle_flash->set_position(cfg.muzzle.offset);
    if (cfg.muzzle.flip_h) {
        muzzle_flash->set_flip_h(true);
    }
    muzzle_flash->set_visible(false);
    muzzle_flash->connect("animation_finished", callable_mp(this, &AnimationController::on_muzzle_flash_finished));
    owner_node->add_child(muzzle_flash);
}

void AnimationController::set_anim_state(AnimState state) {
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

void AnimationController::flash_damage(const Color &color) {
    if (sprite) {
        sprite->set_modulate(color);
        flash_timer = 0.1;
    }
}

void AnimationController::play_muzzle_flash() {
    if (muzzle_flash) {
        muzzle_flash->set_visible(true);
        muzzle_flash->play("muzzle");
        muzzle_flash->set_frame_and_progress(0, 0.0);
    }
}

void AnimationController::hide_muzzle_flash() {
    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
    }
}

void AnimationController::on_muzzle_flash_finished() {
    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
    }
}

void AnimationController::on_animation_finished() {
    if (anim_state == AnimState::DEATH) {
        start_death_fade();
    }
}

void AnimationController::start_death_fade() {
    Node *parent = get_parent();
    if (!parent) {
        return;
    }
    Ref<Tween> tween = parent->create_tween();
    tween->tween_property(parent, NodePath("modulate"), Color(1, 1, 1, 0), 0.5);
    tween->tween_callback(Callable(parent, "queue_free"));
}

} // namespace defn
