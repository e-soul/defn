// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include "unit_animation_state.h"
#include "unit_definition.h"
#include <godot_cpp/classes/animated_sprite2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <map>
#include <string>

namespace defn {

using namespace godot;

enum class FacingDirection { FORWARD, BACKWARD };

// Presentation for a unit's animation. All timing lives in the engine-free `UnitAnimationState`; this node only shows
// the frame that state has reached and fires the sprite, muzzle-flash and tween side effects around it.
class AnimationController : public Node {
    GDCLASS(AnimationController, Node)

  public:
    void configure(Node *owner_node, const UnitConfig &cfg, bool enable_sprite = true);

    UnitPose get_anim_state() const { return state_.get_pose(); }
    void set_anim_state(UnitPose pose);
    void hold_anim_state(UnitPose pose);
    bool play_named_animation(const StringName &animation_name, bool restart = true);
    void play_attack_animation();
    void play_shoot_animation(bool show_muzzle_flash = true, int effect_frame = 0);
    bool consume_shoot_effect_triggered();
    // True while an attack or shoot animation is on screen, and while it is still inside its committed windup frames.
    [[nodiscard]] bool is_attack_animation_playing() const;
    [[nodiscard]] bool is_attack_windup_active() const;
    void cancel_pending_attack_presentation();
    void set_facing(FacingDirection direction);

    void flash_damage(const godot::Color &color);
    godot::Vector2 get_muzzle_global_position() const;
    [[nodiscard]] godot::Rect2 get_sprite_local_bounds() const;
    void play_muzzle_flash();
    void hide_muzzle_flash();

    [[nodiscard]] const UnitAnimationState &get_animation_state() const { return state_; }

    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    void setup_sprite_frames(const UnitConfig &cfg);
    void setup_muzzle_flash(Node *owner_node, const UnitConfig &cfg);
    void sync_presentation();
    void show_state_frame_on_sprite();
    // The anchor correction for the clip on screen, already mirrored to match the sprite's current facing.
    [[nodiscard]] godot::Vector2 current_sprite_offset() const;
    void apply_sprite_offset();
    void update_death_presentation();
    void on_muzzle_flash_finished();
    void start_death_fade();
    void trigger_shoot_effects(bool show_muzzle_flash);

    UnitAnimationState state_;
    std::string presented_animation_;
    // Per-clip anchor corrections, keyed by animation name. See `AnimConfig::offset`.
    std::map<std::string, godot::Vector2> animation_offsets_;

    AnimatedSprite2D *sprite = nullptr;
    AnimatedSprite2D *muzzle_flash = nullptr;
    Node2D *owner_node = nullptr;
    godot::Vector2 muzzle_offset = godot::Vector2();
    bool base_sprite_flip_h_ = false;
    bool base_muzzle_flip_h_ = false;
    bool show_muzzle_flash_on_shoot_effect = true;
    bool death_fade_started_ = false;

    double flash_timer = 0.0;
    godot::Color original_modulate = godot::Color(1, 1, 1, 1);
};

} // namespace defn

#endif
