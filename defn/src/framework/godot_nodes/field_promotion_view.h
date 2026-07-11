#ifndef FIELD_PROMOTION_VIEW_H
#define FIELD_PROMOTION_VIEW_H

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

class AnimationController;
class HealthBarWidget;

class FieldPromotionView : public godot::Node2D {
    GDCLASS(FieldPromotionView, godot::Node2D)

  public:
    void configure(AnimationController *animation, HealthBarWidget *health_bar);
    void show_promotion();
    [[nodiscard]] bool is_shown() const { return shown_; }

  protected:
    static void _bind_methods();

  private:
    void create_insignia();
    void play_pulse();
    static void play_sound(godot::Node2D *effect_parent, const godot::Vector2 &world_position);

    AnimationController *animation_ = nullptr;
    HealthBarWidget *health_bar_ = nullptr;
    godot::Label *insignia_ = nullptr;
    bool shown_ = false;
};

} // namespace defn

#endif
