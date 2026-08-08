// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef REPOSITION_DESTINATION_MARKER_H
#define REPOSITION_DESTINATION_MARKER_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>

namespace defn {

using namespace godot;

struct RepositionDestinationMarkerStyle {
    real_t radius_x = 0.0F;
    real_t radius_y = 0.0F;
    real_t border_width = 0.0F;
    real_t minimum_scale = 1.0F;
    real_t maximum_scale = 1.0F;
    double pulse_duration_seconds = 0.0;
    int pulse_count = 0;
    godot::Color fill_color{};
    godot::Color border_color{};
};

class RepositionDestinationMarker : public Node2D {
    GDCLASS(RepositionDestinationMarker, Node2D)

  public:
    void configure(const RepositionDestinationMarkerStyle &style);
    [[nodiscard]] real_t get_radius_x() const { return style_.radius_x; }
    [[nodiscard]] real_t get_radius_y() const { return style_.radius_y; }
    [[nodiscard]] real_t get_border_width() const { return style_.border_width; }
    [[nodiscard]] int get_pulse_count() const { return style_.pulse_count; }
    void _draw() override;
    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    RepositionDestinationMarkerStyle style_{};
    double elapsed_seconds_ = 0.0;
};

} // namespace defn

#endif
