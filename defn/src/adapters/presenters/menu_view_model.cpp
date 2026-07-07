#include "menu_view_model.h"

namespace defn {

namespace {

MenuButtonViewModel build_button_view_model(const MenuActionPresentationInput &input) {
    const MenuIntent intent = build_menu_intent(input.intent_type, input.target);
    return {
        .id = input.id,
        .label = input.label.empty() ? "???" : input.label,
        .intent = intent,
        .enabled = intent.type != MenuIntentType::None,
    };
}

MenuSettingViewModel build_setting_view_model(const MenuSettingPresentationInput &input) {
    MenuSettingViewModel view_model;
    view_model.id = input.id;
    view_model.label = input.label;
    view_model.setting_id = input.setting_id;
    view_model.bus_name = input.bus_name.empty() ? "Master" : input.bus_name;
    view_model.kind = input.kind;
    view_model.min_value = input.min_value;
    view_model.max_value = input.max_value;
    view_model.step_value = input.step_value;
    view_model.options.reserve(input.options.size());
    for (const auto &option : input.options) {
        view_model.options.push_back({
            .label = option.label.empty() ? "???" : option.label,
            .value = option.value,
        });
    }
    return view_model;
}

MenuButtonViewModel make_back_button(const std::string &target) {
    return {
        .id = "back",
        .label = "Back",
        .intent = {.type = MenuIntentType::GotoMenu, .target = target},
        .enabled = true,
    };
}

} // namespace

MenuIntent build_menu_intent(MenuIntentType intent_type, const std::string &target) {
    return {
        .type = intent_type,
        .target = intent_type == MenuIntentType::GotoMenu ? target : std::string(),
    };
}

MenuScreenViewModel build_menu_screen_view_model(const MenuScreenPresentationInput &input) {
    MenuScreenViewModel view_model;
    view_model.name = input.name;
    view_model.type = input.type;
    view_model.buttons.reserve(input.entries.size());
    for (const auto &entry : input.entries) {
        view_model.buttons.push_back(build_button_view_model(entry));
    }

    view_model.settings.reserve(input.settings.size());
    for (const auto &setting : input.settings) {
        view_model.settings.push_back(build_setting_view_model(setting));
    }

    if (input.back.has_value()) {
        view_model.back_button = build_button_view_model(*input.back);
        if (view_model.back_button->label.empty() || view_model.back_button->label == "???") {
            view_model.back_button->label = "Back";
        }
    }
    return view_model;
}

LevelSelectViewModel build_level_select_view_model(std::vector<LevelSelectRowViewModel> levels) {
    LevelSelectViewModel view_model;
    view_model.levels = std::move(levels);
    view_model.back_button = make_back_button("game_menu");
    return view_model;
}

ProgressionScreenViewModel build_progression_screen_view_model() {
    ProgressionScreenViewModel view_model;
    view_model.back_button = make_back_button("game_menu");
    return view_model;
}

} // namespace defn