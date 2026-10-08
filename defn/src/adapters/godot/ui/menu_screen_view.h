// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef MENU_SCREEN_VIEW_H
#define MENU_SCREEN_VIEW_H

#include "menu_view_model.h"
#include "ui_screen_scaffold.h"
#include <vector>

namespace defn {
class MenuScreenView : public UiScreenControl {
    GDCLASS(MenuScreenView, UiScreenControl)
  public:
    void add_action(godot::Button *button, const MenuButtonViewModel &model);

  protected:
    static void _bind_methods() {}
    void context_changed() override;

  private:
    struct DesktopAction {
        godot::Button *button;
        bool enabled;
    };
    void refresh_actions();
    std::vector<DesktopAction> desktop_actions_;
};
} // namespace defn
#endif
