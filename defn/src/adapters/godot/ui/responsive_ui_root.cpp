// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "responsive_ui_root.h"
#include "ui_layout.h"
#include "ui_theme_provider.h"
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>

namespace defn {
void ResponsiveUiRoot::_ready() {
    set_name("ResponsiveUiRoot");
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    set_clip_contents(true);
    set_process_mode(PROCESS_MODE_ALWAYS);
    _process(0.0);
}

void ResponsiveUiRoot::_process(double /*delta*/) {
    godot::Vector2 display = get_window()->get_size();
    if (godot::OS::get_singleton()->has_feature("web")) {
        const double fitted_width = godot::JavaScriptBridge::get_singleton()->eval(
            "(()=>{const c=document.getElementById('canvas');return c?Math.min(c.clientWidth,c.clientHeight*16/9):1920;})()", true);
        display = {static_cast<float>(fitted_width), static_cast<float>(fitted_width * 9.0 / 16.0)};
    }
    const UiLayout layout = fit_ui(display.x, display.y);
    // Automatic font oversampling only sees the viewport stretch, not this UI transform.
    // Cache glyphs at their final rendering density to avoid enlarging tiny raster glyphs.
    get_viewport()->set_oversampling_override(std::max(1.0F, get_viewport()->get_screen_transform().get_scale().x * layout.scale));
    UiThemeProvider::set_compact(layout.compact);
    const godot::Vector2 size(layout.width, layout.height);
    if (get_size() != size) {
        set_scale({layout.scale, layout.scale});
        set_size(size);
    }
}
} // namespace defn
