#ifndef MENU_DATA_LOADER_H
#define MENU_DATA_LOADER_H

#include "menu_models.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

using namespace godot;

class MenuDataLoader {
  public:
    MenuDataLoader() = delete;

    static std::optional<MenuContentData> load(const String &path);
    static std::optional<MenuContentData> load_from_data(const Dictionary &data);
};

MenuDefinitionType parse_menu_type(const Dictionary &menu_dict);
MenuActionType parse_menu_action_type(const String &action);
MenuSettingKind parse_setting_kind(const Dictionary &setting_dict);

} // namespace defn

#endif