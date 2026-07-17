#include "menu_data_loader.h"

#include "godot_string.h"
#include "json_file_loader.h"
#include "variant_tools.h"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr float DEFAULT_ALPHA = 1.0F;

Color parse_content_color(const Array &arr, const Color &fallback) {
    if (arr.size() >= 3) {
        return {
            .r = VariantTools::as_float(arr[0]),
            .g = VariantTools::as_float(arr[1]),
            .b = VariantTools::as_float(arr[2]),
            .a = arr.size() >= 4 ? VariantTools::as_float(arr[3]) : DEFAULT_ALPHA,
        };
    }
    return fallback;
}

MenuStyleBoxData parse_style_box_data(const Dictionary &style_dict, const MenuStyleBoxData &fallback) {
    MenuStyleBoxData style = fallback;
    style.bg_color = parse_content_color(style_dict.get("bg_color", Array()), style.bg_color);
    style.border_color = parse_content_color(style_dict.get("border_color", Array()), style.border_color);
    style.border_width = VariantTools::as_int(style_dict.get("border_width", style.border_width));
    style.corner_radius = VariantTools::as_int(style_dict.get("corner_radius", style.corner_radius));
    style.font_color = parse_content_color(style_dict.get("font_color", Array()), style.font_color);
    return style;
}

MenuOptionsStyleData parse_options_style_data(const Dictionary &options_dict, const MenuOptionsStyleData &fallback) {
    MenuOptionsStyleData options = fallback;
    options.label_font_size = VariantTools::as_int(options_dict.get("label_font_size", options.label_font_size));
    options.label_min_width = VariantTools::as_int(options_dict.get("label_min_width", options.label_min_width));
    options.control_min_width = VariantTools::as_int(options_dict.get("control_min_width", options.control_min_width));
    options.control_min_height = VariantTools::as_int(options_dict.get("control_min_height", options.control_min_height));
    options.row_separation = VariantTools::as_int(options_dict.get("row_separation", options.row_separation));
    options.section_font_size = VariantTools::as_int(options_dict.get("section_font_size", options.section_font_size));
    options.value_font_size = VariantTools::as_int(options_dict.get("value_font_size", options.value_font_size));
    options.section_font_color = parse_content_color(options_dict.get("section_font_color", Array()), options.section_font_color);
    options.label_font_color = parse_content_color(options_dict.get("label_font_color", Array()), options.label_font_color);
    options.value_font_color = parse_content_color(options_dict.get("value_font_color", Array()), options.value_font_color);
    return options;
}

MenuStyleData parse_style_data(const Dictionary &style_dict) {
    MenuStyleData style;
    style.button_font_size = VariantTools::as_int(style_dict.get("button_font_size", style.button_font_size));
    style.button_min_width = VariantTools::as_int(style_dict.get("button_min_width", style.button_min_width));
    style.button_min_height = VariantTools::as_int(style_dict.get("button_min_height", style.button_min_height));
    style.button_separation = VariantTools::as_int(style_dict.get("button_separation", style.button_separation));
    style.normal = parse_style_box_data(style_dict.get("normal", Dictionary()), style.normal);
    style.hover = parse_style_box_data(style_dict.get("hover", Dictionary()), style.hover);
    style.pressed = parse_style_box_data(style_dict.get("pressed", Dictionary()), style.pressed);
    style.options = parse_options_style_data(style_dict.get("options", Dictionary()), style.options);
    return style;
}

UiSoundData parse_ui_sound_data(const Dictionary &sound_dict) {
    return {
        .path = to_std_string(String(sound_dict.get("path", ""))),
        .volume_linear = VariantTools::as_float(sound_dict.get("volume_linear", 0.2)),
    };
}

UiSfxData parse_ui_sfx_data(const Dictionary &sfx_dict) {
    return {
        .hover = parse_ui_sound_data(sfx_dict.get("hover", Dictionary())),
        .click = parse_ui_sound_data(sfx_dict.get("click", Dictionary())),
        .deploy_card = parse_ui_sound_data(sfx_dict.get("deploy_card", Dictionary())),
    };
}

} // namespace

MenuDefinitionType parse_menu_type(const Dictionary &menu_dict) {
    return String(menu_dict.get("type", "buttons")) == "options" ? MenuDefinitionType::OPTIONS : MenuDefinitionType::BUTTONS;
}

MenuActionType parse_menu_action_type(const String &action) {
    const String normalized = action.to_lower();
    if (normalized == "goto_menu") {
        return MenuActionType::GOTO_MENU;
    }
    if (normalized == "level_select") {
        return MenuActionType::LEVEL_SELECT;
    }
    if (normalized == "progression") {
        return MenuActionType::PROGRESSION;
    }
    if (normalized == "start_game") {
        return MenuActionType::START_GAME;
    }
    if (normalized == "quit") {
        return MenuActionType::QUIT;
    }
    if (normalized == "resume") {
        return MenuActionType::RESUME;
    }
    if (normalized == "main_menu") {
        return MenuActionType::MAIN_MENU;
    }
    return MenuActionType::NONE;
}

MenuSettingKind parse_setting_kind(const Dictionary &setting_dict) {
    const String type = String(setting_dict.get("type", "")).to_lower();
    const String setting_id = String(setting_dict.get("setting", "")).to_lower();
    if (type == "section") {
        return MenuSettingKind::SECTION;
    }
    if (type == "dropdown" && setting_id == "display_mode") {
        return MenuSettingKind::DISPLAY_MODE;
    }
    if (type == "dropdown" && setting_id == "resolution") {
        return MenuSettingKind::RESOLUTION;
    }
    if (type == "checkbox" && setting_id == "vsync") {
        return MenuSettingKind::VSYNC;
    }
    if (type == "slider" && setting_id == "bus_volume") {
        return MenuSettingKind::BUS_VOLUME;
    }
    return MenuSettingKind::UNKNOWN;
}

MenuAction parse_action(const Dictionary &entry_dict) {
    return {
        .id = to_std_string(String(entry_dict.get("id", ""))),
        .label = to_std_string(String(entry_dict.get("label", "???"))),
        .action_type = parse_menu_action_type(String(entry_dict.get("action", "none"))),
        .target = to_std_string(String(entry_dict.get("target", ""))),
    };
}

MenuSetting parse_setting(const Dictionary &setting_dict) {
    MenuSetting setting;
    setting.id = to_std_string(String(setting_dict.get("id", "")));
    setting.label = to_std_string(String(setting_dict.get("label", "")));
    setting.setting_id = to_std_string(String(setting_dict.get("setting", "")));
    setting.bus_name = to_std_string(String(setting_dict.get("bus", "Master")));
    setting.kind = parse_setting_kind(setting_dict);
    setting.min_value = VariantTools::as_int(setting_dict.get("min", setting.min_value));
    setting.max_value = VariantTools::as_int(setting_dict.get("max", setting.max_value));
    setting.step_value = VariantTools::as_int(setting_dict.get("step", setting.step_value));

    const Array options = setting_dict.get("options", Array());
    setting.options.reserve(options.size());
    for (const Variant &option_variant : options) {
        const Dictionary option_dict = option_variant;
        setting.options.push_back({
            .label = to_std_string(String(option_dict.get("label", "???"))),
            .value = to_std_string(String(option_dict.get("value", ""))),
        });
    }

    return setting;
}

MenuDefinition parse_menu_definition(const String &menu_name, const Dictionary &menu_dict) {
    MenuDefinition definition;
    definition.name = to_std_string(menu_name);
    definition.type = parse_menu_type(menu_dict);
    definition.overlay_color = parse_content_color(menu_dict.get("overlay_color", Array()), definition.overlay_color);

    const Array entry_array = menu_dict.get("entries", Array());
    definition.entries.reserve(entry_array.size());
    for (const Variant &entry_variant : entry_array) {
        definition.entries.push_back(parse_action(entry_variant));
    }

    const Array setting_array = menu_dict.get("settings", Array());
    definition.settings.reserve(setting_array.size());
    for (const Variant &setting_variant : setting_array) {
        definition.settings.push_back(parse_setting(setting_variant));
    }

    const Dictionary back_dict = menu_dict.get("back", Dictionary());
    if (!back_dict.is_empty()) {
        definition.back = parse_action(back_dict);
    }

    return definition;
}

std::optional<MenuContentData> MenuDataLoader::load(const String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "MenuDataLoader");
    return data ? load_from_data(*data) : std::nullopt;
}

std::optional<MenuContentData> MenuDataLoader::load_from_data(const Dictionary &data) {
    MenuContentData menu_data;
    menu_data.background = to_std_string(String(data.get("background", "")));
    menu_data.style = parse_style_data(data.get("style", Dictionary()));
    menu_data.sfx = parse_ui_sfx_data(data.get("sfx", Dictionary()));

    const Dictionary menus = data.get("menus", Dictionary());
    const Array menu_names = menus.keys();
    menu_data.menus.reserve(menu_names.size());
    for (const Variant &menu_name_variant : menu_names) {
        const String menu_name = menu_name_variant;
        menu_data.menus.push_back(parse_menu_definition(menu_name, menus[menu_name]));
    }

    return menu_data;
}

} // namespace defn
