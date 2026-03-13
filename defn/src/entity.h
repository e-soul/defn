#ifndef ENTITY_H
#define ENTITY_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/animated_sprite2d.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

enum class AnimState {
    WALK,
    ATTACK,
    DEATH
};

class Entity : public Node2D {
    GDCLASS(Entity, Node2D)

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

    AnimState get_anim_state() const { return anim_state; }
    void set_anim_state(AnimState state);

    void flash_damage(const Color &color);

    AnimatedSprite2D *get_sprite() const { return sprite; }

    void _ready() override;
    void _process(double delta) override;

protected:
    static void _bind_methods();

    void setup_health_bar();
    void update_health_bar();

    int max_hp = 100;
    int current_hp = 100;
    int damage = 15;
    double attack_speed = 1.0;
    double attack_timer = 0.0;
    double move_speed = 0.5;
    double attack_range = 128.0;

    AnimState anim_state = AnimState::WALK;
    AnimatedSprite2D *sprite = nullptr;

    // Health bar
    ColorRect *health_bar_bg = nullptr;
    ColorRect *health_bar_fill = nullptr;

    // Flash effect
    double flash_timer = 0.0;
    Color original_modulate = Color(1, 1, 1, 1);
};

} // namespace defn

#endif
