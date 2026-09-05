// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "animation_controller.h"

#include "godot_string.h"
#include "godot_vector.h"

#include <algorithm>

#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

void AnimationController::_bind_methods() { ADD_SIGNAL(MethodInfo("shoot_effect_triggered")); }

void AnimationController::configure(Node *owner_node, const UnitConfig &cfg, bool enable_sprite) {
    this->owner_node = Object::cast_to<Node2D>(owner_node);
    state_.configure(cfg.animations);
    muzzle_offset = to_godot_vector(muzzle_anchor(cfg));
    base_sprite_flip_h_ = cfg.sprite_flip_h;
    base_muzzle_flip_h_ = cfg.muzzle.flip_h;

    if (enable_sprite) {
        sprite = memnew(AnimatedSprite2D);
        if (sprite == nullptr) {
            return;
        }

        owner_node->add_child(sprite);
        setup_sprite_frames(cfg);
        original_modulate = sprite->get_modulate();
    }

    setup_muzzle_flash(owner_node, cfg);
    set_anim_state(UnitPose::WALK);
}

void AnimationController::_process(double delta) {
    if (flash_timer > 0.0) {
        flash_timer -= delta;
        if (flash_timer <= 0.0 && sprite) {
            sprite->set_modulate(original_modulate);
        }
    }

    const UnitAnimationUpdate update = state_.advance(delta);
    sync_presentation();
    if (update.shoot_effect_fired) {
        trigger_shoot_effects(show_muzzle_flash_on_shoot_effect);
    }
    update_death_presentation();
}

void AnimationController::setup_sprite_frames(const UnitConfig &cfg) {
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();

    for (const auto &[anim_name, anim_cfg] : cfg.animations) {
        const String animation_name = to_godot_string(anim_name);
        animation_offsets_[anim_name] = to_godot_vector(anim_cfg.offset);
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
    muzzle_flash->set_position(muzzle_offset);
    if (cfg.muzzle.flip_h) {
        muzzle_flash->set_flip_h(true);
    }
    muzzle_flash->set_visible(false);
    muzzle_flash->connect("animation_finished", callable_mp(this, &AnimationController::on_muzzle_flash_finished));
    owner_node->add_child(muzzle_flash);
}

void AnimationController::sync_presentation() {
    const std::string &animation = state_.get_current_animation();
    if (animation != presented_animation_) {
        presented_animation_ = animation;
        if (animation != SHOOT_ANIMATION) {
            hide_muzzle_flash();
        }
    }

    show_state_frame_on_sprite();
}

// The sprite never runs an animation of its own: it is parked on whichever frame the clock has reached, so the pose on
// screen and the timing combat reasons about cannot drift apart.
void AnimationController::show_state_frame_on_sprite() {
    if (sprite == nullptr || presented_animation_.empty()) {
        return;
    }

    const StringName animation_name = to_godot_string(presented_animation_);
    const Ref<SpriteFrames> frames = sprite->get_sprite_frames();
    if (!frames.is_valid() || !frames->has_animation(animation_name)) {
        return;
    }

    const int frame_count = frames->get_frame_count(animation_name);
    if (frame_count <= 0) {
        return;
    }

    if (sprite->get_animation() != animation_name) {
        sprite->set_animation(animation_name);
    }
    apply_sprite_offset();
    sprite->stop();
    sprite->set_frame_and_progress(std::min(state_.get_clock().frame(), frame_count - 1), 0.0);
}

// Godot mirrors the texture inside an unmoved destination rect, so `offset` is *not* flipped for us: a correction
// measured on the un-flipped art has to be mirrored by hand to keep pointing at the same spot on the body.
godot::Vector2 AnimationController::current_sprite_offset() const {
    if (sprite == nullptr) {
        return {};
    }
    const auto entry = animation_offsets_.find(presented_animation_);
    if (entry == animation_offsets_.end()) {
        return {};
    }
    const godot::Vector2 offset = entry->second;
    return sprite->is_flipped_h() ? godot::Vector2(-offset.x, offset.y) : offset;
}

void AnimationController::apply_sprite_offset() {
    if (sprite != nullptr) {
        sprite->set_offset(current_sprite_offset());
    }
}

void AnimationController::set_anim_state(UnitPose pose) {
    state_.set_pose(pose);
    sync_presentation();
}

void AnimationController::hold_anim_state(UnitPose pose) {
    state_.hold_pose(pose);
    sync_presentation();
}

bool AnimationController::play_named_animation(const StringName &animation_name, bool restart) {
    if (!state_.play_named(to_std_string(animation_name), restart)) {
        return false;
    }

    hide_muzzle_flash();
    sync_presentation();
    return true;
}

void AnimationController::play_attack_animation() {
    state_.play_attack();
    sync_presentation();
}

void AnimationController::play_shoot_animation(bool show_muzzle_flash, int effect_frame) {
    show_muzzle_flash_on_shoot_effect = show_muzzle_flash;
    const UnitAnimationUpdate update = state_.play_shoot(effect_frame);
    sync_presentation();
    if (update.shoot_effect_fired) {
        trigger_shoot_effects(show_muzzle_flash_on_shoot_effect);
    }
}

bool AnimationController::consume_shoot_effect_triggered() { return state_.consume_shoot_effect_triggered(); }

bool AnimationController::is_attack_animation_playing() const { return state_.is_attack_animation_playing(); }

bool AnimationController::is_attack_windup_active() const { return state_.is_attack_windup_active(); }

void AnimationController::cancel_pending_attack_presentation() {
    state_.cancel_pending_attack();
    hide_muzzle_flash();
    sync_presentation();
}

void AnimationController::set_facing(FacingDirection direction) {
    const bool backward = direction == FacingDirection::BACKWARD;
    if (sprite != nullptr) {
        sprite->set_flip_h(backward ? !base_sprite_flip_h_ : base_sprite_flip_h_);
        apply_sprite_offset();
    }
    if (muzzle_flash != nullptr) {
        muzzle_flash->set_flip_h(backward ? !base_muzzle_flip_h_ : base_muzzle_flip_h_);
        const real_t offset_x = backward ? -muzzle_offset.x : muzzle_offset.x;
        muzzle_flash->set_position({offset_x, muzzle_offset.y});
    }
}

void AnimationController::flash_damage(const godot::Color &color) {
    if (sprite) {
        sprite->set_modulate(color);
        flash_timer = 0.1;
    }
}

godot::Vector2 AnimationController::get_muzzle_global_position() const {
    if (muzzle_flash != nullptr) {
        return muzzle_flash->get_global_position();
    }

    if (owner_node != nullptr) {
        return owner_node->to_global(muzzle_offset);
    }

    return {};
}

godot::Rect2 AnimationController::get_sprite_local_bounds() const {
    if (sprite == nullptr) {
        return {};
    }
    const godot::Ref<godot::SpriteFrames> frames = sprite->get_sprite_frames();
    if (!frames.is_valid() || !frames->has_animation(sprite->get_animation()) || frames->get_frame_count(sprite->get_animation()) <= 0) {
        return {};
    }
    const godot::Ref<godot::Texture2D> texture = frames->get_frame_texture(sprite->get_animation(), sprite->get_frame());
    if (!texture.is_valid()) {
        return {};
    }
    const godot::Vector2 size = texture->get_size();
    return {current_sprite_offset() - (size * 0.5F), size};
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

void AnimationController::update_death_presentation() {
    if (death_fade_started_ || state_.get_pose() != UnitPose::DEATH || presented_animation_ != DEATH_ANIMATION || state_.get_clock().is_playing()) {
        return;
    }

    death_fade_started_ = true;
    start_death_fade();
}

void AnimationController::on_muzzle_flash_finished() {
    if (muzzle_flash) {
        muzzle_flash->set_visible(false);
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
