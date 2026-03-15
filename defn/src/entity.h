#ifndef ENTITY_H
#define ENTITY_H

#include <godot_cpp/classes/animated_sprite2d.hpp>
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/character_body2d.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

enum class AnimState { WALK, ATTACK, SHOOT, DEATH };

enum class AttackMode { NONE, MELEE, RANGED };

class Entity : public CharacterBody2D {
    GDCLASS(Entity, CharacterBody2D)

  public:
    Entity();
    ~Entity() override = default;

    void init_stats(int p_max_hp, int p_damage, double p_attack_speed, double p_move_speed);

    void take_damage(int amount);
    bool is_dead() const { return current_hp <= 0; }

    int get_current_hp() const { return current_hp; }
    int get_max_hp() const { return max_hp; }
    int get_damage() const { return damage; }
    double get_attack_speed() const { return attack_speed; }
    double get_move_speed() const { return move_speed; }
    double get_attack_range() const { return attack_range; }
    double get_ranged_range() const { return ranged_range; }
    int get_ranged_damage() const { return ranged_damage; }
    AttackMode get_attack_mode() const { return attack_mode; }

    void init_ranged_stats(int p_ranged_damage, double p_ranged_attack_speed);

    AnimState get_anim_state() const { return anim_state; }
    void set_anim_state(AnimState state);

    void flash_damage(const Color &color);

    AnimatedSprite2D *get_sprite() const { return sprite; }
    Area2D *get_detection_area() const { return detection_area; }

    bool is_engaged() const { return engaged; }

    void _ready() override;
    void _process(double delta) override;

  protected:
    static void _bind_methods();

    void setup_health_bar();
    void update_health_bar();
    void setup_detection(uint32_t hitbox_layer, uint32_t sensor_mask);

    int max_hp = 100;
    int current_hp = 100;
    int damage = 15;
    double attack_speed = 1.0;
    Timer *attack_timer_node = nullptr;
    Timer *ranged_timer_node = nullptr;
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
    Entity *target = nullptr;

    Color melee_flash_color = Color(1, 1, 1);
    Color ranged_flash_color = Color(1, 1, 1);

    void find_target();
    bool try_keep_target();
    virtual void find_new_target() {}
    virtual double get_forward_distance(Entity *t) const { return -1.0; }
    virtual void do_movement(double delta) {}

    void on_attack_timeout();
    void on_ranged_timeout();
    void on_muzzle_flash_finished();
    void on_animation_finished();
    void start_death_fade();
    void setup_muzzle_flash(const String &path_template, const Vector2 &offset, bool flip_h);
};

} // namespace defn

#endif