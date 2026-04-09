#ifndef PROJECTILE_ATTACK_H
#define PROJECTILE_ATTACK_H

#include "unit_data.h"

#include <godot_cpp/classes/animated_sprite2d.hpp>
#include <godot_cpp/classes/audio_stream_player2d.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object_id.hpp>

namespace defn {

using namespace godot;

class ProjectileAttack : public Node2D {
    GDCLASS(ProjectileAttack, Node2D)

  public:
    void configure(const ProjectileAttackConfig &config, UnitSide shooter_side, const Color &flash_color, const Vector2 &start_global_position,
                   const Vector2 &target_global_position, Node2D *direct_target, int fallback_damage);

    void _process(double delta) override;

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
    Node2D *resolve_direct_target() const;
    int resolve_impact_damage() const;
    int resolve_splash_damage() const;
    int compute_affected_target_count(int candidate_count) const;
    void try_finish();

    ProjectileAttackConfig config_{};
    UnitSide shooter_side_ = UnitSide::FRIENDLY;
    Color flash_color_{};
    AnimatedSprite2D *sprite_ = nullptr;
    AudioStreamPlayer2D *explosion_player_ = nullptr;
    Vector2 target_global_position_{};
    Vector2 travel_direction_{};
    Vector2 flight_scale_ = Vector2(1.0, 1.0);
    Vector2 explosion_scale_ = Vector2(1.0, 1.0);
    real_t total_travel_distance_ = 0.0F;
    real_t travelled_distance_ = 0.0F;
    int fallback_damage_ = 0;
    ObjectID direct_target_id_{};
    bool exploding_ = false;
    bool explosion_animation_finished_ = false;
    bool explosion_sfx_finished_ = true;
};

} // namespace defn

#endif