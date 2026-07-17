#ifndef MENU_MODELS_H
#define MENU_MODELS_H

#include "content_values.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

enum class MenuDefinitionType { BUTTONS, OPTIONS };
enum class MenuSettingKind { SECTION, DISPLAY_MODE, RESOLUTION, VSYNC, BUS_VOLUME, UNKNOWN };
enum class MenuActionType { NONE, GOTO_MENU, LEVEL_SELECT, PROGRESSION, START_GAME, QUIT, RESUME, MAIN_MENU };

struct MenuStyleBoxData {
    Color bg_color = {1.0F, 1.0F, 1.0F, 1.0F};
    Color border_color = {0.4F, 0.4F, 0.5F, 1.0F};
    int border_width = 2;
    int corner_radius = 8;
    Color font_color = {0.9F, 0.9F, 0.95F, 1.0F};
};

struct MenuOptionsStyleData {
    int label_font_size = 24;
    int label_min_width = 250;
    int control_min_width = 300;
    int control_min_height = 40;
    int row_separation = 12;
    int section_font_size = 28;
    int value_font_size = 20;
    Color section_font_color = {1.0F, 0.85F, 0.3F, 1.0F};
    Color label_font_color = {0.85F, 0.85F, 0.9F, 1.0F};
    Color value_font_color = {0.7F, 0.8F, 1.0F, 1.0F};
};

struct MenuStyleData {
    int button_font_size = 32;
    int button_min_width = 400;
    int button_min_height = 60;
    int button_separation = 16;
    MenuStyleBoxData normal;
    MenuStyleBoxData hover = {
        .bg_color = {1.0F, 1.0F, 1.0F, 1.0F},
        .border_color = {0.4F, 0.4F, 0.5F, 1.0F},
        .border_width = 2,
        .corner_radius = 8,
        .font_color = {1.0F, 1.0F, 1.0F, 1.0F},
    };
    MenuStyleBoxData pressed = {
        .bg_color = {1.0F, 1.0F, 1.0F, 1.0F},
        .border_color = {0.4F, 0.4F, 0.5F, 1.0F},
        .border_width = 2,
        .corner_radius = 8,
        .font_color = {0.8F, 0.8F, 0.9F, 1.0F},
    };
    MenuOptionsStyleData options;
};

struct UiSoundData {
    std::string path;
    float volume_linear = 0.2F;
};

struct UiSfxData {
    UiSoundData hover;
    UiSoundData click;
    UiSoundData deploy_card;
};

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
    MenuDefinitionType type = MenuDefinitionType::BUTTONS;
    Color overlay_color = {0.0F, 0.0F, 0.0F, 0.6F};
    std::vector<MenuAction> entries;
    std::vector<MenuSetting> settings;
    std::optional<MenuAction> back;
};

struct MenuContentData {
    std::string background;
    MenuStyleData style;
    UiSfxData sfx;
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
