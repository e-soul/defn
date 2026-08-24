// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef PROJECTILE_ATTACK_H
#define PROJECTILE_ATTACK_H

#include "attack_target.h"
#include "projectile_flight.h"
#include "unit_definition.h"

#include <godot_cpp/classes/animated_sprite2d.hpp>
#include <godot_cpp/classes/audio_stream_player2d.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object_id.hpp>

namespace defn {

using namespace godot;

// Presentation and lifetime for one shot. The flight itself is `ProjectileFlight`, an engine-free model the simulator
// runs too; this node only shows where that flight has got to, plays the explosion, and frees itself afterwards.
class ProjectileAttack : public Node2D {
    GDCLASS(ProjectileAttack, Node2D)

  public:
    void configure(const ProjectileAttackConfig &config, UnitSide shooter_side, godot::ObjectID source_id, const godot::Color &flash_color,
                   const godot::Vector2 &start_global_position, const godot::Vector2 &target_global_position, AttackTarget *direct_target, int fallback_damage);

    void _process(double delta) override;

    // False once the shot has gone off. The node outlives that moment to play its explosion, but it has already
    // applied its damage and stopped affecting the world.
    [[nodiscard]] bool is_in_flight() const { return !exploding_; }

  protected:
    static void _bind_methods();

  private:
    static Ref<SpriteFrames> build_frames(const AnimConfig &animation);
    void ensure_sprite();
    void start_flight_animation();
    void start_explosion_animation();
    void ensure_explosion_audio_player();
    void play_explosion_sfx();
    void explode();
    void apply_splash_damage();
    void on_animation_finished();
    void on_explosion_sfx_finished();
    AttackTarget *resolve_direct_target() const;
    void try_finish();

    ProjectileAttackConfig config_{};
    UnitSide shooter_side_ = UnitSide::FRIENDLY;
    godot::Color flash_color_{};
    AnimatedSprite2D *sprite_ = nullptr;
    AudioStreamPlayer2D *explosion_player_ = nullptr;
    ProjectileFlight flight_{};
    godot::Vector2 flight_scale_ = godot::Vector2(1.0, 1.0);
    godot::Vector2 explosion_scale_ = godot::Vector2(1.0, 1.0);
    int fallback_damage_ = 0;
    ObjectID direct_target_id_{};
    ObjectID source_id_{};
    bool exploding_ = false;
    bool explosion_animation_finished_ = false;
    bool explosion_sfx_finished_ = true;
};

} // namespace defn

#endif
