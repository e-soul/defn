// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "menu_screen_view.h"

namespace defn {
void MenuScreenView::add_action(godot::Button *button, const MenuButtonViewModel &model) {
    body->add_child(button);
    apply_enabled(button, model.enabled);
    if (model.intent.type == MenuIntentType::ShowProgression) {
        desktop_actions_.push_back({.button = button, .enabled = model.enabled});
        refresh_actions();
    }
}
void MenuScreenView::context_changed() {
    refresh_actions();
    UiScreenControl::context_changed();
}
void MenuScreenView::refresh_actions() {
    const bool mobile = uses_mobile_menu_layout(context_.display);
    for (const auto &action : desktop_actions_) {
        apply_enabled(action.button, action.enabled && !mobile);
    }
}
} // namespace defn
