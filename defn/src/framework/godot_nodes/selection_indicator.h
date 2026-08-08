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
    real_t radius_x = 26.0F;
    real_t radius_y = 8.0F;
    real_t border_width = 2.0F;
    real_t world_offset_y = 8.0F;
    Color fill_color = Color(0.12F, 0.85F, 0.48F, 0.24F);
    Color border_color = Color(0.75F, 1.0F, 0.86F, 0.95F);
};

class SelectionIndicator : public Node2D {
    GDCLASS(SelectionIndicator, Node2D)

  public:
    void configure(const SelectionIndicatorStyle &style = {});
    [[nodiscard]] real_t get_world_offset_y() const { return style_.world_offset_y; }
    void _draw() override;

  protected:
    static void _bind_methods();

  private:
    SelectionIndicatorStyle style_{};
};

} // namespace defn

#endif
