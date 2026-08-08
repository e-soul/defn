// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SELECTION_INDICATOR_H
#define SELECTION_INDICATOR_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>

namespace defn {

using namespace godot;

struct SelectionIndicatorStyle {
    real_t radius_x = 0.0F;
    real_t radius_y = 0.0F;
    real_t border_width = 0.0F;
    real_t world_offset_y = 0.0F;
    godot::Color fill_color{};
    godot::Color border_color{};
};

class SelectionIndicator : public Node2D {
    GDCLASS(SelectionIndicator, Node2D)

  public:
    void configure(const SelectionIndicatorStyle &style);
    [[nodiscard]] real_t get_radius_x() const { return style_.radius_x; }
    [[nodiscard]] real_t get_radius_y() const { return style_.radius_y; }
    [[nodiscard]] real_t get_border_width() const { return style_.border_width; }
    [[nodiscard]] real_t get_world_offset_y() const { return style_.world_offset_y; }
    [[nodiscard]] godot::Color get_fill_color() const { return style_.fill_color; }
    [[nodiscard]] godot::Color get_border_color() const { return style_.border_color; }
    void _draw() override;

  protected:
    static void _bind_methods();

  private:
    SelectionIndicatorStyle style_{};
};

} // namespace defn

#endif
