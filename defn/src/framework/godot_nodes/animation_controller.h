// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include "unit_definition.h"
#include <godot_cpp/classes/animated_sprite2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

enum class AnimState { WALK, ATTACK, SHOOT, DEATH };
enum class FacingDirection { FORWARD, BACKWARD };

class AnimationController : public Node {
    GDCLASS(AnimationController, Node)

  public:
    void configure(Node *owner_node, const UnitConfig &cfg, bool enable_sprite = true);

    AnimState get_anim_state() const { return anim_state; }
    void set_anim_state(AnimState state);
    void hold_anim_state(AnimState state);
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

    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    void setup_sprite_frames(Node *owner_node, const UnitConfig &cfg);
    void setup_muzzle_flash(Node *owner_node, const UnitConfig &cfg);
    void update_shoot_effect_state();
    void on_muzzle_flash_finished();
    void on_animation_changed();
    void on_animation_finished();
    void start_death_fade();
    void trigger_shoot_effects(bool show_muzzle_flash);

    AnimatedSprite2D *sprite = nullptr;
    AnimatedSprite2D *muzzle_flash = nullptr;
    Node2D *owner_node = nullptr;
    AnimState anim_state = AnimState::WALK;
    godot::Vector2 muzzle_offset = godot::Vector2();
    bool base_sprite_flip_h_ = false;
    bool base_muzzle_flip_h_ = false;
    bool shoot_effect_pending = false;
    bool shoot_effect_ready = false;
    bool show_muzzle_flash_on_shoot_effect = true;
    int shoot_effect_frame = 0;
    int attack_windup_frames_ = 0;
    int shoot_windup_frames_ = 0;

    double flash_timer = 0.0;
    godot::Color original_modulate = godot::Color(1, 1, 1, 1);
};

} // namespace defn

#endif
