// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MENU_MODELS_H
#define MENU_MODELS_H

#include <optional>
#include <string>
#include <vector>

namespace defn {

enum class MenuDefinitionType { BUTTONS, OPTIONS };
enum class MenuSettingKind { SECTION, DISPLAY_MODE, RESOLUTION, VSYNC, BUS_VOLUME, UNKNOWN };
enum class MenuActionType { NONE, GOTO_MENU, LEVEL_SELECT, PROGRESSION, START_GAME, QUIT, RESUME, MAIN_MENU };

struct MenuAction {
    std::string id;
    std::string label;
    MenuActionType action_type = MenuActionType::NONE;
    std::string target;
};

struct MenuOptionChoice {
    std::string label;
    std::string value;
};

struct MenuSetting {
    std::string id;
    std::string label;
    std::string setting_id;
    std::string bus_name;
    MenuSettingKind kind = MenuSettingKind::UNKNOWN;
    int min_value = 0;
    int max_value = 100;
    int step_value = 1;
    std::vector<MenuOptionChoice> options;
};

struct MenuDefinition {
    std::string name;
    /// Shown as the screen heading. Content owns what a menu is called; the theme owns how the heading looks.
    std::string title;
    MenuDefinitionType type = MenuDefinitionType::BUTTONS;
    std::vector<MenuAction> entries;
    std::vector<MenuSetting> settings;
    std::optional<MenuAction> back;
};

struct MenuContentData {
    std::string background;
    std::vector<MenuDefinition> menus;

    const MenuDefinition *find_menu(const std::string &menu_name) const {
        for (const auto &menu : menus) {
            if (menu.name == menu_name) {
                return &menu;
            }
        }
        return nullptr;
    }
};

} // namespace defn

#endif
