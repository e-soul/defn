// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_viewport_metrics.h"

#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/array.hpp>

namespace defn {

UiViewportMetrics measure_ui_viewport(godot::Viewport *viewport) {
    const godot::Vector2 size = viewport->get_visible_rect().size;
    UiViewportMetrics measured{.width = size.x, .height = size.y, .css_width = size.x, .css_height = size.y};
    measured.web = godot::OS::get_singleton()->has_feature("web");
    if (!measured.web) {
        return measured;
    }
    // Read only the displayed canvas. DPR is for rendering sharpness, and must never decide UI size. The shell
    // already excludes safe-area padding. Its fullscreen header is the only chrome that overlaps the canvas.
    const godot::Variant result = godot::JavaScriptBridge::get_singleton()->eval(R"JS((() => {
        const canvas = document.getElementById('canvas');
        if (!canvas) return '[]';
        const bounds = canvas.getBoundingClientRect();
        const button = document.getElementById('fullscreen');
        const chrome = document.fullscreenElement && button && !button.hidden ? button.getBoundingClientRect() : null;
        return JSON.stringify([bounds.width, bounds.height, matchMedia('(pointer: coarse)').matches,
            chrome ? Math.max(0, bounds.right - chrome.left + 6) : 0,
            chrome ? Math.max(0, chrome.bottom - bounds.top + 6) : 0]);
    })())JS",
                                                                                 true);
    if (result.get_type() != godot::Variant::STRING) {
        return measured;
    }
    const godot::Variant parsed = godot::JSON::parse_string(result);
    if (parsed.get_type() != godot::Variant::ARRAY) {
        return measured;
    }
    const godot::Array values = parsed;
    if (values.size() == 5 && static_cast<double>(values[0]) > 0.0 && static_cast<double>(values[1]) > 0.0) {
        measured.css_width = static_cast<float>(static_cast<double>(values[0]));
        measured.css_height = static_cast<float>(static_cast<double>(values[1]));
        measured.coarse_pointer = values[2];
        measured.overlay_width = static_cast<float>(static_cast<double>(values[3]));
        measured.overlay_height = static_cast<float>(static_cast<double>(values[4]));
    }
    return measured;
}

} // namespace defn
