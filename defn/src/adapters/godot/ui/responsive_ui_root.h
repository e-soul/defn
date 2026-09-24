// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef DEFN_RESPONSIVE_UI_ROOT_H
#define DEFN_RESPONSIVE_UI_ROOT_H

#include <godot_cpp/classes/control.hpp>

namespace defn {
// A UI-only transform. The viewport, world canvas and camera stay untouched.
class ResponsiveUiRoot : public godot::Control {
    GDCLASS(ResponsiveUiRoot, godot::Control)
  public:
    void _ready() override;
    void _process(double delta) override;

  protected:
    static void _bind_methods() {}
};
} // namespace defn
#endif
