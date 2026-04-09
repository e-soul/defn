#ifndef BASE_OBJECTIVE_H
#define BASE_OBJECTIVE_H

#include "unit_data.h"

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

class HealthComponent;

class BaseObjective : public Node2D {
    GDCLASS(BaseObjective, Node2D)

  public:
    BaseObjective();

    void configure(int max_hp, const Vector2 &position);
    void take_damage(int amount);
    void flash_damage(const Color &color);
    bool is_dead() const;

    UnitSide get_side() const { return UnitSide::FRIENDLY; }
    int get_current_hp() const;
    int get_max_hp() const;
    Area2D *get_hitbox() const { return hitbox_; }

    void _draw() override;
    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    void ensure_health_component();
    void ensure_hitbox();
    void on_health_changed(int current_hp, int max_hp);
    void on_destroyed();

    HealthComponent *health_ = nullptr;
    Area2D *hitbox_ = nullptr;
    Color flash_color_ = Color(1.0, 1.0, 1.0);
    real_t flash_time_remaining_ = 0.0F;
};

} // namespace defn

#endif