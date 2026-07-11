#ifndef FIELD_PROMOTION_EFFECT_H
#define FIELD_PROMOTION_EFFECT_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn::vfx {

class FieldPromotionEffect : public godot::Node2D {
    GDCLASS(FieldPromotionEffect, godot::Node2D)

  public:
    void start();
    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    double elapsed_seconds_ = 0.0;
};

void spawn_field_promotion_effect(godot::Node2D *parent, const godot::Vector2 &world_position);

} // namespace defn::vfx

#endif
