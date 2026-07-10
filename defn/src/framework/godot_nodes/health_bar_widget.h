#ifndef HEALTH_BAR_WIDGET_H
#define HEALTH_BAR_WIDGET_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class HealthComponent;

class HealthBarWidget : public Node2D {
    GDCLASS(HealthBarWidget, Node2D)

  public:
    void configure(HealthComponent *health, const godot::Color &fill_color, const Vector2 &offset);

  protected:
    static void _bind_methods();

  private:
    void on_health_changed(int current, int max);
    void setup_bar(int max_hp, const godot::Color &fill_color, const Vector2 &offset);

    ProgressBar *bar = nullptr;
    Ref<StyleBoxFlat> fill_style;
    Ref<StyleBoxFlat> bg_style;
};

} // namespace defn

#endif
