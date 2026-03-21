#include "health_bar_widget.h"
#include "health_component.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

void HealthBarWidget::_bind_methods() {}

void HealthBarWidget::configure(HealthComponent *health, const Color &fill_color) {
    setup_bar(health->get_max_hp(), fill_color);
    health->connect("health_changed", callable_mp(this, &HealthBarWidget::on_health_changed));
}

void HealthBarWidget::setup_bar(int max_hp, const Color &fill_color) {
    constexpr double bar_width = 102.0;
    constexpr double bar_height = 6.0;

    bar = memnew(ProgressBar);
    bar->set_custom_minimum_size(Vector2(bar_width, bar_height));
    bar->set_position(Vector2(-bar_width * 0.5, 0));
    bar->set_min(0.0);
    bar->set_max(max_hp);
    bar->set_value(max_hp);
    bar->set_show_percentage(false);
    bar->set_visible(false);

    bg_style.instantiate();
    bg_style->set_bg_color(Color(0.3, 0.3, 0.3, 0.8));
    bg_style->set_corner_radius_all(0);
    bg_style->set_border_width_all(0);
    bg_style->set_content_margin_all(0);
    bar->add_theme_stylebox_override("background", bg_style);

    fill_style.instantiate();
    fill_style->set_bg_color(fill_color);
    fill_style->set_corner_radius_all(0);
    fill_style->set_border_width_all(0);
    fill_style->set_content_margin_all(0);
    bar->add_theme_stylebox_override("fill", fill_style);

    add_child(bar);
}

void HealthBarWidget::on_health_changed(int current, int max) {
    if (!bar) {
        return;
    }

    bar->set_max(max);
    bool should_show = current < max && current > 0;
    bar->set_visible(should_show);

    if (should_show) {
        bar->set_value(current);
    }
}

} // namespace defn
