#include "menu_data_loader.h"

#include "json_file_loader.h"
#include "menu_style.h"
#include "variant_tools.h"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

MenuDefinitionType parse_menu_type(const Dictionary &menu_dict) {
    return String(menu_dict.get("type", "buttons")) == "options" ? MenuDefinitionType::OPTIONS : MenuDefinitionType::BUTTONS;
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
        .id = String(entry_dict.get("id", "")),
        .label = String(entry_dict.get("label", "???")),
        .action = String(entry_dict.get("action", "none")),
        .target = String(entry_dict.get("target", "")),
    };
}

MenuSetting parse_setting(const Dictionary &setting_dict) {
    MenuSetting setting;
    setting.id = String(setting_dict.get("id", ""));
    setting.label = String(setting_dict.get("label", ""));
    setting.setting_id = String(setting_dict.get("setting", ""));
    setting.bus_name = String(setting_dict.get("bus", "Master"));
    setting.kind = parse_setting_kind(setting_dict);
    setting.min_value = VariantTools::as_int(setting_dict.get("min", setting.min_value));
    setting.max_value = VariantTools::as_int(setting_dict.get("max", setting.max_value));
    setting.step_value = VariantTools::as_int(setting_dict.get("step", setting.step_value));

    const Array options = setting_dict.get("options", Array());
    setting.options.reserve(options.size());
    for (const Variant &option_variant : options) {
        const Dictionary option_dict = option_variant;
        setting.options.push_back({
            .label = String(option_dict.get("label", "???")),
            .value = String(option_dict.get("value", "")),
        });
    }

    return setting;
}

MenuDefinition parse_menu_definition(const String &menu_name, const Dictionary &menu_dict) {
    MenuDefinition definition;
    definition.name = menu_name;
    definition.type = parse_menu_type(menu_dict);
    definition.overlay_color = parse_color_array(menu_dict.get("overlay_color", Array()), definition.overlay_color);

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
    menu_data.background = String(data.get("background", ""));
    menu_data.style_data = data.get("style", Dictionary());

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