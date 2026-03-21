#ifndef UNIT_H
#define UNIT_H

#include "unit_data.h"
#include <godot_cpp/classes/animated_sprite2d.hpp>
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/character_body2d.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

enum class AnimState { WALK, ATTACK, SHOOT, DEATH };

enum class AttackMode { NONE, MELEE, RANGED };

class Unit : public CharacterBody2D {
    GDCLASS(Unit, CharacterBody2D)

  public:
    Unit();
    ~Unit() override = default;

    void set_unit_config(const UnitConfig &cfg);

    void take_damage(int amount);
    bool is_dead() const { return current_hp <= 0; }

    int get_current_hp() const { return current_hp; }
    int get_max_hp() const { return max_hp; }
    int get_damage() const { return damage; }
    double get_attack_speed() const { return attack_speed; }
    double get_move_speed() const { return move_speed; }
    int get_cost() const { return unit_config_.cost; }
    int get_bounty() const { return unit_config_.bounty; }
    UnitSide get_side() const { return unit_config_.side; }

    AnimState get_anim_state() const { return anim_state; }
    void set_anim_state(AnimState state);

    void flash_damage(const Color &color);

    void _ready() override;
    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    void init_stats();
    void setup_health_bar();
    void update_health_bar();
    void setup_detection(uint32_t hitbox_layer, uint32_t sensor_mask);
    void setup_sprite_frames();
    void setup_muzzle_flash();

    // Targeting
    void find_target();
    bool try_keep_target();
    void find_new_target();
    double get_forward_distance(Unit *other) const;

    // Behavior
    void update_cooldowns(double delta);
    void update_state(double delta);
    void perform_behavior(double delta);
    void do_movement(double delta);
    void check_breach();

    // Animation callbacks
    void on_muzzle_flash_finished();
    void on_animation_finished();
    void start_death_fade();

    UnitConfig unit_config_;

    int max_hp = 100;
    int current_hp = 100;
    int damage = 15;
    double attack_speed = 1.0;
    double attack_cooldown = 0.0;
    double move_speed = 0.5;
    double attack_range = 128.0;
    double ranged_range = 384.0;
    int ranged_damage = 10;
    double ranged_attack_speed = 1.5;
    AttackMode attack_mode = AttackMode::NONE;

    AnimState anim_state = AnimState::WALK;
    AnimatedSprite2D *sprite = nullptr;

    // Health bar
    ProgressBar *health_bar = nullptr;
    Ref<StyleBoxFlat> health_bar_fill_style;
    Ref<StyleBoxFlat> health_bar_bg_style;

    // Collision detection
    Area2D *hitbox = nullptr;
    Area2D *detection_area = nullptr;
    AnimatedSprite2D *muzzle_flash = nullptr;

    // Flash effect
    double flash_timer = 0.0;
    Color original_modulate = Color(1, 1, 1, 1);

    bool engaged = false;
    Unit *target = nullptr;

    Color melee_flash_color = Color(1, 1, 1);
    Color ranged_flash_color = Color(1, 1, 1);
};

} // namespace defn

#endif
