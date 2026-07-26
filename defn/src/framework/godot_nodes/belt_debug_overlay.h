// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef BELT_DEBUG_OVERLAY_H
#define BELT_DEBUG_OVERLAY_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

class BeltDebugOverlay : public godot::Node2D {
    GDCLASS(BeltDebugOverlay, godot::Node2D)

  public:
    BeltDebugOverlay();

    void configure(godot::real_t world_width, godot::real_t upper_y, godot::real_t lower_y);
    void toggle_visibility();
    void _draw() override;

  protected:
    static void _bind_methods();

  private:
    godot::real_t world_width_ = 0.0;
    godot::real_t upper_y_ = 0.0;
    godot::real_t lower_y_ = 0.0;
};

} // namespace defn

#endif
