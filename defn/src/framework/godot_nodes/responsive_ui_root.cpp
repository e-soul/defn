// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "responsive_ui_root.h"
#include "display_adapter.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {
void ResponsiveUiRoot::_ready() {
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    set_process_mode(PROCESS_MODE_ALWAYS);
    if (get_window() != nullptr) {
        get_window()->connect("size_changed", callable_mp(this, &ResponsiveUiRoot::request_layout));
    }
    refresh_display();
}
void ResponsiveUiRoot::request_layout() { dirty_ = true; }
void ResponsiveUiRoot::_process(double delta) {
    poll_seconds_ += delta;
    // Web DPR/host CSS bounds and desktop monitor DPI have no common engine notification.
    if (dirty_ || poll_seconds_ >= 0.25) {
        poll_seconds_ = 0;
        refresh_display();
    }
}
void ResponsiveUiRoot::refresh_display() { apply_snapshot(read_display(get_window())); }
void ResponsiveUiRoot::apply_snapshot(const DisplaySnapshot &next) {
    const bool changed = initial_ || dirty_ || next != display_ || theme_revision_ != UiThemeProvider::revision();
    if (!changed) {
        return;
    }
    display_ = next;
    dirty_ = false;
    const auto &data = UiThemeProvider::data().responsive;
    auto profile = resolve_ui_profile(next, data, UiThemeProvider::profile());
    if (initial_) {
        const auto area = usable_rect(next);
        profile = std::min(area.width, area.height) < data.small_short_edge ? UiProfile::Small : UiProfile::Standard;
        profile = resolve_ui_profile(next, data, profile);
    }
    initial_ = false;
    apply_display(get_window(), display_);
    UiThemeProvider::resolve_profile(profile, next.touch);
    UiThemeProvider::install(get_tree());
    // Safe-area geometry is consumed here; children resolve inside this rectangle without subtracting it again.
    const auto area = usable_rect(next);
    set_position({area.x, area.y});
    set_size({area.width, area.height});
    context_ = resolve_ui_context(next, data, profile);
    theme_revision_ = UiThemeProvider::revision();
    notify_content(this);
    apply_layout();
}
void ResponsiveUiRoot::notify_content(godot::Node *node) {
    if (auto *view = godot::Object::cast_to<UiContextControl>(node); view != nullptr) {
        view->set_ui_context(context_);
    }
    for (int i = 0; i < node->get_child_count(); ++i) {
        notify_content(node->get_child(i));
    }
}
} // namespace defn
