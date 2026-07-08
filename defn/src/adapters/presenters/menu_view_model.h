#ifndef MENU_VIEW_MODEL_H
#define MENU_VIEW_MODEL_H

#include "menu_intents.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

enum class MenuScreenType { Buttons, Options };
enum class MenuSettingViewKind { Section, DisplayMode, Resolution, Vsync, BusVolume, Unknown };

struct MenuActionPresentationInput {
    std::string id;
    std::string label;
    MenuIntentType intent_type = MenuIntentType::None;
    std::string target;
};

struct MenuOptionChoicePresentationInput {
    std::string label;
    std::string value;
};

struct MenuSettingPresentationInput {
    std::string id;
    std::string label;
    std::string setting_id;
    std::string bus_name;
    MenuSettingViewKind kind = MenuSettingViewKind::Unknown;
    int min_value = 0;
    int max_value = 100;
    int step_value = 1;
    std::vector<MenuOptionChoicePresentationInput> options;
};

struct MenuScreenPresentationInput {
    std::string name;
    MenuScreenType type = MenuScreenType::Buttons;
    std::vector<MenuActionPresentationInput> entries;
    std::vector<MenuSettingPresentationInput> settings;
    std::optional<MenuActionPresentationInput> back;
};

struct MenuButtonViewModel {
    std::string id;
    std::string label;
    MenuIntent intent;
    bool enabled = true;
};

struct MenuOptionChoiceViewModel {
    std::string label;
    std::string value;
};

struct MenuSettingViewModel {
    std::string id;
    std::string label;
    std::string setting_id;
    std::string bus_name;
    MenuSettingViewKind kind = MenuSettingViewKind::Unknown;
    int min_value = 0;
    int max_value = 100;
    int step_value = 1;
    std::vector<MenuOptionChoiceViewModel> options;
};

struct MenuScreenViewModel {
    std::string name;
    MenuScreenType type = MenuScreenType::Buttons;
    std::vector<MenuButtonViewModel> buttons;
    std::vector<MenuSettingViewModel> settings;
    std::optional<MenuButtonViewModel> back_button;
};

struct LevelSelectRowViewModel {
    std::string level_id;
    std::string label;
    bool unlocked = false;
};

struct LevelSelectViewModel {
    std::string title = "SELECT LEVEL";
    std::vector<LevelSelectRowViewModel> levels;
    MenuButtonViewModel back_button;
};

struct ProgressionScreenViewModel {
    std::string title = "YOUR UPGRADES";
    MenuButtonViewModel back_button;
};

[[nodiscard]] MenuIntent build_menu_intent(MenuIntentType intent_type, const std::string &target = {});
[[nodiscard]] MenuScreenViewModel build_menu_screen_view_model(const MenuScreenPresentationInput &input);
[[nodiscard]] LevelSelectViewModel build_level_select_view_model(std::vector<LevelSelectRowViewModel> levels);
[[nodiscard]] ProgressionScreenViewModel build_progression_screen_view_model();

} // namespace defn

#endif