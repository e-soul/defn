// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "display_adapter.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/os.hpp>

namespace defn {
DisplaySnapshot read_display(godot::Window *window) {
    DisplaySnapshot display;
    if (window == nullptr) {
        return normalize_display_snapshot(display);
    }
    const auto pixels = window->get_size();
    auto *server = godot::DisplayServer::get_singleton();
    auto *operating_system = godot::OS::get_singleton();
    float density = 1;
    display.touch = server->is_touchscreen_available();
    if (operating_system->has_feature("web")) {
        // Canvas bounds exclude the shell chrome and its CSS safe-area padding exactly once.
        auto *bridge = godot::JavaScriptBridge::get_singleton();
        const double width = bridge->eval("document.getElementById('canvas')?.getBoundingClientRect().width || 1280", true);
        const double height = bridge->eval("document.getElementById('canvas')?.getBoundingClientRect().height || 720", true);
        display.content = {.width = static_cast<float>(width), .height = static_cast<float>(height)};
        display.render_density = static_cast<float>(static_cast<double>(bridge->eval("window.devicePixelRatio || 1", true)));
        display.touch = static_cast<bool>(bridge->eval("navigator.maxTouchPoints > 0", true));
        display.fine_pointer = static_cast<bool>(bridge->eval("matchMedia('(any-pointer: fine)').matches", true));
        display.primary_coarse_pointer = static_cast<bool>(bridge->eval("matchMedia('(pointer: coarse)').matches", true));
        display.safe_area_applied = true;
        return normalize_display_snapshot(display);
    }
    const int screen = window->get_current_screen();
    if (operating_system->has_feature("android")) {
        // Android dp is defined against 160 dpi. Keep this seam separate from desktop OS scaling.
        density = std::max(1.0F, static_cast<float>(server->screen_get_dpi(screen)) / 160.0F);
        const godot::Rect2i safe = server->get_display_safe_area();
        display.safe_area = {.left = static_cast<float>(safe.position.x) / density,
                             .top = static_cast<float>(safe.position.y) / density,
                             .right = std::max(0.0F, static_cast<float>(pixels.x - safe.position.x - safe.size.x) / density),
                             .bottom = std::max(0.0F, static_cast<float>(pixels.y - safe.position.y - safe.size.y) / density)};
        display.touch = true;
        display.fine_pointer = false;
        display.primary_coarse_pointer = true;
    } else if (operating_system->has_feature("windows")) {
        // Godot 4.7 Windows window sizes are physical client pixels; screen DPI is the per-monitor OS DPI.
        // screen_get_scale is 1 on this backend. Missing/headless DPI falls back to 96, never phone heuristics.
        const int dpi = server->screen_get_dpi(screen);
        density = dpi > 0 ? std::max(1.0F, static_cast<float>(dpi) / 96.0F) : 1.0F;
    } else {
        density = std::max(1.0F, server->screen_get_scale(screen));
    }
    display.content = {.width = std::max(1.0F, static_cast<float>(pixels.x) / density), .height = std::max(1.0F, static_cast<float>(pixels.y) / density)};
    display.render_density = density;
    return normalize_display_snapshot(display);
}
void apply_display(godot::Window *window, const DisplaySnapshot &display) {
    if (window == nullptr) {
        return;
    }
    window->set_content_scale_mode(godot::Window::CONTENT_SCALE_MODE_CANVAS_ITEMS);
    window->set_content_scale_aspect(godot::Window::CONTENT_SCALE_ASPECT_IGNORE);
    window->set_content_scale_size({static_cast<int>(std::lround(display.content.width)), static_cast<int>(std::lround(display.content.height))});
    window->set_content_scale_factor(1);
    // Native touch takes the same GUI/mouse path as web: no independent touch deployment command.
    godot::Input::get_singleton()->set_emulate_mouse_from_touch(true);
    godot::Input::get_singleton()->set_emulate_touch_from_mouse(false);
}
} // namespace defn
