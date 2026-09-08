// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MENU_BACKDROP_H
#define MENU_BACKDROP_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

/// The menu's backdrop, drawn rather than loaded: a dusk wash, a leaning hatch, and the two opposing tints that
/// make the screen read as contested ground. Every colour comes from the shared palette, so the backdrop moves
/// with the theme instead of pinning the menu to one piece of artwork at one resolution.
class MenuBackdrop : public godot::Control {
    GDCLASS(MenuBackdrop, godot::Control)

  public:
    MenuBackdrop();

    void _draw() override;

  protected:
    static void _bind_methods();
    void _notification(int what);
};

} // namespace defn

#endif
