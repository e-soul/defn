#include "animation_controller.h"

#include "godot_string.h"

#include <algorithm>

#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

namespace {

Vector2 to_godot_vector(const ContentVector2 &vector) { return {vector.x, vector.y}; }

} // namespace

void AnimationController::_bind_methods() { ADD_SIGNAL(MethodInfo("shoot_effect_triggered")); }

void AnimationController::configure(Node *owner_node, const UnitConfig &cfg, bool enable_sprite) {
    this->owner_node = Object::cast_to<Node2D>(owner_node);
    muzzle_offset = to_godot_vector(cfg.muzzle.offset);

    if (enable_sprite) {
        sprite = memnew(AnimatedSprite2D);
        if (sprite == nullptr) {
            return;
        }

        owner_node->add_child(sprite);
        setup_sprite_frames(owner_node, cfg);
        original_modulate = sprite->get_modulate();
        sprite->connect("animation_finished", callable_mp(this, &AnimationController::on_animation_finished));
        sprite->connect("animation_changed", callable_mp(this, &AnimationController::on_animation_changed));
    }

    setup_muzzle_flash(owner_node, cfg);
    set_anim_state(AnimState::WALK);
}

void AnimationController::_process(double delta) {
    if (flash_timer > 0.0) {
        flash_timer -= delta;
        if (flash_timer <= 0.0 && sprite) {
            sprite->set_modulate(original_modulate);
        }
    }

    update_shoot_effect_state();
}

void AnimationController::setup_sprite_frames(Node * /*owner_node*/, const UnitConfig &cfg) {
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    for (const auto &[anim_name, anim_cfg] : cfg.animations) {
        const String animation_name = to_godot_string(anim_name);
        frames->add_animation(animation_name);
        frames->set_animation_speed(animation_name, anim_cfg.speed);
        frames->set_animation_loop(animation_name, anim_cfg.loop);
        for (int i = 0; i < anim_cfg.frame_count; ++i) {
            String path = vformat(to_godot_string(anim_cfg.path_template), i);
            Ref<Texture2D> tex = loader->load(path);
            if (tex.is_valid()) {
                frames->add_frame(animation_name, tex);
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
    if (cfg.muzzle.path_template.empty()) {
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
        String path = vformat(to_godot_string(cfg.muzzle.path_template), i);
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            frames->add_frame("muzzle", tex);
        }
    }

    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }

    muzzle_flash->set_sprite_frames(frames);
    muzzle_flash->set_position(to_godot_vector(cfg.muzzle.offset));
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

void AnimationController::hold_anim_state(AnimState state) {
    set_anim_state(state);
    if (!sprite) {
        return;
    }

    sprite->set_frame_and_progress(0, 0.0);
    sprite->stop();
}

bool AnimationController::play_named_animation(const StringName &animation_name, bool restart) {
    if (sprite == nullptr) {
        return false;
    }

    Ref<SpriteFrames> frames = sprite->get_sprite_frames();
    if (!frames.is_valid() || !frames->has_animation(animation_name) || frames->get_frame_count(animation_name) <= 0) {
        return false;
    }

    shoot_effect_pending = false;
    shoot_effect_ready = false;
    hide_muzzle_flash();
    sprite->play(animation_name);
    if (restart) {
        sprite->set_frame_and_progress(0, 0.0);
    }
    return true;
}

void AnimationController::play_attack_animation() {
    set_anim_state(AnimState::ATTACK);
    if (!sprite) {
        return;
    }

    sprite->play("attack");
    sprite->set_frame_and_progress(0, 0.0);
}

void AnimationController::play_shoot_animation(bool show_muzzle_flash, int effect_frame) {
    set_anim_state(AnimState::SHOOT);
    show_muzzle_flash_on_shoot_effect = show_muzzle_flash;
    shoot_effect_ready = false;
    shoot_effect_pending = false;

    if (!sprite) {
        shoot_effect_ready = true;
        trigger_shoot_effects(show_muzzle_flash_on_shoot_effect);
        return;
    }

    sprite->play("shoot");
    sprite->set_frame_and_progress(0, 0.0);
    shoot_effect_pending = true;

    int max_effect_frame = 0;
    Ref<SpriteFrames> frames = sprite->get_sprite_frames();
    if (frames.is_valid() && frames->has_animation("shoot")) {
        max_effect_frame = std::max(frames->get_frame_count("shoot") - 1, 0);
    }
    shoot_effect_frame = std::clamp(effect_frame, 0, max_effect_frame);

    if (shoot_effect_frame == 0) {
        shoot_effect_pending = false;
        shoot_effect_ready = true;
        trigger_shoot_effects(show_muzzle_flash_on_shoot_effect);
    }
}

bool AnimationController::consume_shoot_effect_triggered() {
    if (!shoot_effect_ready) {
        return false;
    }

    shoot_effect_ready = false;
    return true;
}

void AnimationController::flash_damage(const godot::Color &color) {
    if (sprite) {
        sprite->set_modulate(color);
        flash_timer = 0.1;
    }
}

Vector2 AnimationController::get_muzzle_global_position() const {
    if (muzzle_flash != nullptr) {
        return muzzle_flash->get_global_position();
    }

    if (owner_node != nullptr) {
        return owner_node->to_global(muzzle_offset);
    }

    return {};
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

void AnimationController::update_shoot_effect_state() {
    if (!shoot_effect_pending || sprite == nullptr) {
        return;
    }

    if (sprite->get_animation() != StringName("shoot")) {
        shoot_effect_pending = false;
        return;
    }

    if (sprite->get_frame() < shoot_effect_frame) {
        return;
    }

    shoot_effect_pending = false;
    shoot_effect_ready = true;
    trigger_shoot_effects(show_muzzle_flash_on_shoot_effect);
}

void AnimationController::on_muzzle_flash_finished() {
    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
    }
}

void AnimationController::on_animation_changed() {
    if (sprite && sprite->get_animation() != StringName("shoot")) {
        shoot_effect_pending = false;
        hide_muzzle_flash();
    }
}

void AnimationController::on_animation_finished() {
    if (anim_state == AnimState::DEATH) {
        start_death_fade();
    }
}

void AnimationController::trigger_shoot_effects(bool show_muzzle_flash) {
    if (show_muzzle_flash) {
        play_muzzle_flash();
    } else {
        hide_muzzle_flash();
    }
    emit_signal("shoot_effect_triggered");
}

void AnimationController::start_death_fade() {
    Node *parent = get_parent();
    if (!parent) {
        return;
    }
    Ref<Tween> tween = parent->create_tween();
    tween->tween_property(parent, NodePath("modulate"), godot::Color(1, 1, 1, 0), 0.5);
    tween->tween_callback(callable_mp(parent, &Node::queue_free));
}

} // namespace defn
