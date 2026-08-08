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
    real_t radius_x = 14.0F;
    real_t radius_y = 4.0F;
    real_t border_width = 1.5F;
    real_t minimum_scale = 0.65F;
    real_t maximum_scale = 1.15F;
    double pulse_duration_seconds = 0.28;
    int pulse_count = 3;
    Color fill_color = Color(0.2F, 0.9F, 0.58F, 0.18F);
    Color border_color = Color(0.8F, 1.0F, 0.88F, 0.9F);
};

class RepositionDestinationMarker : public Node2D {
    GDCLASS(RepositionDestinationMarker, Node2D)

  public:
    void configure(const RepositionDestinationMarkerStyle &style = {});
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
