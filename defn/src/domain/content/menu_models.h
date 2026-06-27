#ifndef MENU_MODELS_H
#define MENU_MODELS_H

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <vector>

namespace defn {

using namespace godot;

enum class MenuDefinitionType { BUTTONS, OPTIONS };
enum class MenuSettingKind { SECTION, DISPLAY_MODE, RESOLUTION, VSYNC, BUS_VOLUME, UNKNOWN };

struct MenuAction {
    String id;
    String label;
    String action;
    String target;
};

struct MenuOptionChoice {
    String label;
    String value;
};

struct MenuSetting {
    String id;
    String label;
    String setting_id;
    String bus_name;
    MenuSettingKind kind = MenuSettingKind::UNKNOWN;
    int min_value = 0;
    int max_value = 100;
    int step_value = 1;
    std::vector<MenuOptionChoice> options;
};

struct MenuDefinition {
    String name;
    MenuDefinitionType type = MenuDefinitionType::BUTTONS;
    Color overlay_color = Color(0, 0, 0, 0.6);
    std::vector<MenuAction> entries;
    std::vector<MenuSetting> settings;
    std::optional<MenuAction> back;
};

struct MenuContentData {
    String background;
    Dictionary style_data;
    std::vector<MenuDefinition> menus;

    const MenuDefinition *find_menu(const String &menu_name) const {
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