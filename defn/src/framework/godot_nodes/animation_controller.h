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

class AnimationController : public Node {
    GDCLASS(AnimationController, Node)

  public:
    void configure(Node *owner_node, const UnitConfig &cfg, bool enable_sprite = true);

    AnimState get_anim_state() const { return anim_state; }
    void set_anim_state(AnimState state);
    void hold_anim_state(AnimState state);
    void play_attack_animation();
    void play_shoot_animation(bool show_muzzle_flash = true, int effect_frame = 0);
    bool consume_shoot_effect_triggered();

    void flash_damage(const Color &color);
    Vector2 get_muzzle_global_position() const;
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
    Vector2 muzzle_offset = Vector2();
    bool shoot_effect_pending = false;
    bool shoot_effect_ready = false;
    bool show_muzzle_flash_on_shoot_effect = true;
    int shoot_effect_frame = 0;

    double flash_timer = 0.0;
    Color original_modulate = Color(1, 1, 1, 1);
};

} // namespace defn

#endif
